#define MODULE_NAME "DispensingWorkflowUseCase"

#include "DispensingWorkflowUseCase.h"

#include "application/services/dispensing/DispensingExecutionCompatibilityService.h"
#include "application/services/dispensing/WorkflowPreviewSnapshotService.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Siligen::Application::UseCases::Dispensing {

using Domain::Motion::Ports::IMotionStatePort;
using Domain::Safety::Ports::IInterlockSignalPort;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;

namespace {

constexpr double kPreviewGluePointSpacingMm = 3.0;
constexpr double kPreviewDuplicateEpsilonMm = 1e-4;
constexpr double kPreviewTailPositionEpsilonMm = 2e-2;
constexpr double kPreviewTailLegMaxMm = 4e-1;
constexpr double kPreviewCornerMinTurnDeg = 20.0;
constexpr double kPreviewCornerMinLegMm = 2e-1;

double DistanceSquared(const Point2D& lhs, const Point2D& rhs) {
    const double dx = static_cast<double>(lhs.x) - static_cast<double>(rhs.x);
    const double dy = static_cast<double>(lhs.y) - static_cast<double>(rhs.y);
    return dx * dx + dy * dy;
}

double Distance(const Point2D& lhs, const Point2D& rhs) {
    return std::sqrt(DistanceSquared(lhs, rhs));
}

bool NearlyEqualPoint(const Point2D& lhs, const Point2D& rhs, double epsilon_mm) {
    const double epsilon_sq = epsilon_mm * epsilon_mm;
    return DistanceSquared(lhs, rhs) <= epsilon_sq;
}

std::vector<Point2D> ToPointVector(const std::vector<Siligen::TrajectoryPoint>& points) {
    std::vector<Point2D> converted;
    converted.reserve(points.size());
    for (const auto& trajectory_point : points) {
        Point2D point;
        point.x = trajectory_point.position.x;
        point.y = trajectory_point.position.y;
        converted.push_back(point);
    }
    return converted;
}

std::vector<Point2D> RemoveConsecutiveNearDuplicates(
    const std::vector<Point2D>& points,
    double epsilon_mm = kPreviewDuplicateEpsilonMm) {
    std::vector<Point2D> deduped;
    if (points.empty()) {
        return deduped;
    }
    deduped.reserve(points.size());
    deduped.push_back(points.front());
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (!NearlyEqualPoint(deduped.back(), points[i], epsilon_mm)) {
            deduped.push_back(points[i]);
        }
    }
    return deduped;
}

std::vector<Point2D> RemoveShortABABacktracks(const std::vector<Point2D>& points) {
    std::vector<Point2D> reduced;
    reduced.reserve(points.size());
    for (const auto& point : points) {
        reduced.push_back(point);
        while (reduced.size() >= 3U) {
            const auto& a = reduced[reduced.size() - 3U];
            const auto& b = reduced[reduced.size() - 2U];
            const auto& c = reduced[reduced.size() - 1U];
            const bool is_backtrack =
                NearlyEqualPoint(a, c, kPreviewTailPositionEpsilonMm) &&
                Distance(a, b) <= kPreviewTailLegMaxMm &&
                Distance(b, c) <= kPreviewTailLegMaxMm;
            if (!is_backtrack) {
                break;
            }
            reduced.pop_back();
            reduced.pop_back();
        }
    }
    return reduced;
}

std::vector<std::size_t> DetectCornerAnchorIndices(const std::vector<Point2D>& points) {
    std::vector<std::size_t> anchors;
    if (points.size() < 3U) {
        return anchors;
    }
    anchors.reserve(points.size() / 4U + 2U);
    for (std::size_t i = 1; i + 1 < points.size(); ++i) {
        const auto& prev = points[i - 1];
        const auto& cur = points[i];
        const auto& next = points[i + 1];
        const double leg1 = Distance(prev, cur);
        const double leg2 = Distance(cur, next);
        if (leg1 < kPreviewCornerMinLegMm || leg2 < kPreviewCornerMinLegMm) {
            continue;
        }
        const double vx1 = static_cast<double>(cur.x) - static_cast<double>(prev.x);
        const double vy1 = static_cast<double>(cur.y) - static_cast<double>(prev.y);
        const double vx2 = static_cast<double>(next.x) - static_cast<double>(cur.x);
        const double vy2 = static_cast<double>(next.y) - static_cast<double>(cur.y);
        const double dot = vx1 * vx2 + vy1 * vy2;
        const double cos_theta = std::clamp(dot / (leg1 * leg2), -1.0, 1.0);
        const double turn_deg = std::acos(cos_theta) * 180.0 / 3.14159265358979323846;
        if (turn_deg >= kPreviewCornerMinTurnDeg) {
            anchors.push_back(i);
        }
    }
    return anchors;
}

std::vector<std::size_t> PickUniformIndices(const std::vector<std::size_t>& candidates, std::size_t target_count) {
    if (target_count == 0U || candidates.empty()) {
        return {};
    }
    if (candidates.size() <= target_count) {
        return candidates;
    }
    if (target_count == 1U) {
        return {candidates.front()};
    }

    std::vector<std::size_t> picked;
    picked.reserve(target_count);
    const std::size_t last_pos = candidates.size() - 1U;
    for (std::size_t i = 0; i < target_count; ++i) {
        const double ratio = static_cast<double>(i) / static_cast<double>(target_count - 1U);
        const std::size_t pos = static_cast<std::size_t>(std::llround(ratio * static_cast<double>(last_pos)));
        const std::size_t value = candidates[pos];
        if (picked.empty() || picked.back() != value) {
            picked.push_back(value);
        }
    }
    if (picked.size() < target_count) {
        for (const auto value : candidates) {
            if (picked.size() >= target_count) {
                break;
            }
            if (std::find(picked.begin(), picked.end(), value) == picked.end()) {
                picked.push_back(value);
            }
        }
        std::sort(picked.begin(), picked.end());
    }
    if (picked.size() > target_count) {
        picked.resize(target_count);
    }
    return picked;
}

std::vector<Point2D> BuildUniformStepSample(
    const std::vector<Point2D>& points,
    std::size_t max_points) {
    std::vector<Point2D> polyline;
    if (points.empty() || max_points == 0U) {
        return polyline;
    }

    const std::size_t safe_max_points = std::max<std::size_t>(2U, max_points);
    const std::size_t source_point_count = points.size();
    std::size_t step = 1U;
    if (source_point_count > safe_max_points) {
        const std::size_t numerator = source_point_count - 1U;
        const std::size_t denominator = safe_max_points - 1U;
        step = (numerator + denominator - 1U) / denominator;
        step = std::max<std::size_t>(1U, step);
    }

    polyline.reserve(std::min(source_point_count, safe_max_points));
    for (std::size_t i = 0U; i < source_point_count; i += step) {
        polyline.push_back(points[i]);
    }

    const auto& last_source = points.back();
    const bool should_append_last =
        polyline.empty() ||
        !NearlyEqualPoint(polyline.back(), last_source, kPreviewDuplicateEpsilonMm);
    if (should_append_last) {
        polyline.push_back(last_source);
    }
    return polyline;
}

Point2D InterpolatePoint(const Point2D& start, const Point2D& end, double ratio) {
    Point2D point;
    point.x = static_cast<float32>(
        static_cast<double>(start.x) + (static_cast<double>(end.x) - static_cast<double>(start.x)) * ratio);
    point.y = static_cast<float32>(
        static_cast<double>(start.y) + (static_cast<double>(end.y) - static_cast<double>(start.y)) * ratio);
    return point;
}

std::vector<Point2D> BuildFixedSpacingSample(
    const std::vector<Point2D>& points,
    double spacing_mm,
    const std::vector<std::size_t>& anchor_indices) {
    std::vector<Point2D> sampled;
    if (points.empty()) {
        return sampled;
    }

    sampled.reserve(points.size());
    sampled.push_back(points.front());
    if (points.size() == 1U) {
        return sampled;
    }

    std::vector<bool> force_vertex(points.size(), false);
    for (const auto idx : anchor_indices) {
        if (idx < force_vertex.size()) {
            force_vertex[idx] = true;
        }
    }
    force_vertex.back() = true;

    const double safe_spacing = std::max(spacing_mm, kPreviewDuplicateEpsilonMm);
    double distance_since_last_emit = 0.0;

    for (std::size_t seg_idx = 1U; seg_idx < points.size(); ++seg_idx) {
        const auto& seg_start = points[seg_idx - 1U];
        const auto& seg_end = points[seg_idx];
        const double segment_length = Distance(seg_start, seg_end);
        if (segment_length <= kPreviewDuplicateEpsilonMm) {
            continue;
        }

        double traveled = 0.0;
        while (traveled + kPreviewDuplicateEpsilonMm < segment_length) {
            const double remaining_to_emit = std::max(safe_spacing - distance_since_last_emit, kPreviewDuplicateEpsilonMm);
            const double remaining_in_segment = segment_length - traveled;
            if (remaining_in_segment + kPreviewDuplicateEpsilonMm < remaining_to_emit) {
                distance_since_last_emit += remaining_in_segment;
                traveled = segment_length;
                break;
            }

            traveled += remaining_to_emit;
            const double ratio = std::clamp(traveled / segment_length, 0.0, 1.0);
            const auto interpolated = InterpolatePoint(seg_start, seg_end, ratio);
            if (!NearlyEqualPoint(sampled.back(), interpolated, kPreviewDuplicateEpsilonMm)) {
                sampled.push_back(interpolated);
            }
            distance_since_last_emit = 0.0;
        }

        if (force_vertex[seg_idx]) {
            const auto& vertex = points[seg_idx];
            if (!NearlyEqualPoint(sampled.back(), vertex, kPreviewDuplicateEpsilonMm)) {
                sampled.push_back(vertex);
            }
            distance_since_last_emit = 0.0;
        }
    }

    if (!NearlyEqualPoint(sampled.back(), points.back(), kPreviewDuplicateEpsilonMm)) {
        sampled.push_back(points.back());
    }
    return sampled;
}

std::vector<Point2D> ClampPolylineByMaxPointsPreserveCorners(
    const std::vector<Point2D>& points,
    std::size_t max_points) {
    if (points.empty() || max_points == 0U) {
        return {};
    }

    const std::size_t safe_max_points = std::max<std::size_t>(2U, max_points);
    if (points.size() <= safe_max_points) {
        return points;
    }

    const auto corner_anchors = DetectCornerAnchorIndices(points);
    std::vector<std::size_t> mandatory_indices;
    mandatory_indices.reserve(corner_anchors.size() + 2U);
    mandatory_indices.push_back(0U);
    for (const auto idx : corner_anchors) {
        if (idx + 1U < points.size()) {
            mandatory_indices.push_back(idx);
        }
    }
    mandatory_indices.push_back(points.size() - 1U);
    std::sort(mandatory_indices.begin(), mandatory_indices.end());
    mandatory_indices.erase(std::unique(mandatory_indices.begin(), mandatory_indices.end()), mandatory_indices.end());

    std::vector<std::size_t> sample_indices;
    if (mandatory_indices.size() >= safe_max_points) {
        sample_indices = PickUniformIndices(mandatory_indices, safe_max_points);
    } else {
        sample_indices = mandatory_indices;
        std::vector<bool> is_mandatory(points.size(), false);
        for (const auto idx : mandatory_indices) {
            if (idx < is_mandatory.size()) {
                is_mandatory[idx] = true;
            }
        }
        std::vector<std::size_t> supplemental_indices;
        supplemental_indices.reserve(points.size() - mandatory_indices.size());
        for (std::size_t idx = 0U; idx < points.size(); ++idx) {
            if (!is_mandatory[idx]) {
                supplemental_indices.push_back(idx);
            }
        }
        const std::size_t remaining = safe_max_points - sample_indices.size();
        const auto picked_supplemental = PickUniformIndices(supplemental_indices, remaining);
        sample_indices.insert(sample_indices.end(), picked_supplemental.begin(), picked_supplemental.end());
        std::sort(sample_indices.begin(), sample_indices.end());
        sample_indices.erase(std::unique(sample_indices.begin(), sample_indices.end()), sample_indices.end());
    }

    std::vector<Point2D> polyline;
    polyline.reserve(sample_indices.size());
    for (const auto idx : sample_indices) {
        if (idx < points.size()) {
            polyline.push_back(points[idx]);
        }
    }
    if (polyline.size() < 2U) {
        return BuildUniformStepSample(points, safe_max_points);
    }
    return polyline;
}

std::vector<Point2D> BuildPreviewPolyline(
    const std::vector<Siligen::TrajectoryPoint>& trajectory_points,
    std::size_t max_points) {
    const auto raw_points = ToPointVector(trajectory_points);
    auto fallback_polyline = BuildUniformStepSample(raw_points, max_points);
    if (raw_points.empty()) {
        return fallback_polyline;
    }

    auto deduped_points = RemoveConsecutiveNearDuplicates(raw_points);
    if (deduped_points.size() < 2U) {
        return fallback_polyline;
    }

    auto reduced_points = RemoveShortABABacktracks(deduped_points);
    if (reduced_points.size() < 2U) {
        reduced_points = deduped_points;
    } else if (deduped_points.size() >= 20U && reduced_points.size() * 5U < deduped_points.size()) {
        // Backtrack suppression dropped too many points; keep original deduped geometry as fallback.
        reduced_points = deduped_points;
    }
    reduced_points = RemoveConsecutiveNearDuplicates(reduced_points);

    const auto corner_anchors = DetectCornerAnchorIndices(reduced_points);
    auto spacing_sample = BuildFixedSpacingSample(reduced_points, kPreviewGluePointSpacingMm, corner_anchors);
    if (spacing_sample.size() < 2U) {
        spacing_sample = BuildUniformStepSample(reduced_points, max_points);
    }
    if (spacing_sample.size() < 2U) {
        return fallback_polyline;
    }

    auto polyline = ClampPolylineByMaxPointsPreserveCorners(spacing_sample, max_points);
    if (polyline.size() < 2U) {
        return fallback_polyline;
    }
    return polyline;
}

std::string ToIso8601UtcNow() {
    const auto now = std::chrono::system_clock::now();
    const auto time_value = std::chrono::system_clock::to_time_t(now);
    std::tm tm_value{};
#ifdef _WIN32
    gmtime_s(&tm_value, &time_value);
#else
    gmtime_r(&time_value, &tm_value);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_value, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string HexEncodeUint64(std::uint64_t value) {
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << value;
    return oss.str();
}

std::uint64_t Fnv1a64(const std::string& text) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : text) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool IsRuntimeTerminalState(const std::string& state) {
    return state == "completed" || state == "failed" || state == "cancelled";
}

}  // namespace

DispensingWorkflowUseCase::DispensingWorkflowUseCase(
    std::shared_ptr<IUploadFilePort> upload_use_case,
    std::shared_ptr<PlanningUseCase> planning_use_case,
    std::shared_ptr<DispensingExecutionUseCase> execution_use_case,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<IMotionStatePort> motion_state_port,
    std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port,
    std::shared_ptr<IInterlockSignalPort> interlock_signal_port)
    : upload_use_case_(std::move(upload_use_case)),
      planning_use_case_(std::move(planning_use_case)),
      execution_use_case_(std::move(execution_use_case)),
      connection_port_(std::move(connection_port)),
      motion_state_port_(std::move(motion_state_port)),
      homing_port_(std::move(homing_port)),
      interlock_signal_port_(std::move(interlock_signal_port)) {
    if (!upload_use_case_ || !planning_use_case_ || !execution_use_case_) {
        throw std::invalid_argument("DispensingWorkflowUseCase: dependencies cannot be null");
    }
}

DispensingWorkflowUseCase::~DispensingWorkflowUseCase() = default;

Result<CreateArtifactResponse> DispensingWorkflowUseCase::CreateArtifact(const UploadRequest& request) {
    auto upload_result = upload_use_case_->Execute(request);
    if (upload_result.IsError()) {
        return Result<CreateArtifactResponse>::Failure(upload_result.GetError());
    }

    const auto& upload = upload_result.Value();
    CreateArtifactResponse response;
    response.success = true;
    response.artifact_id = GenerateId("artifact");
    response.filepath = upload.filepath;
    response.original_name = upload.original_name;
    response.generated_filename = upload.generated_filename;
    response.size = upload.size;
    response.timestamp = upload.timestamp;

    ArtifactRecord record;
    record.response = response;
    record.upload_response = upload;

    {
        std::lock_guard<std::mutex> lock(artifacts_mutex_);
        artifacts_[response.artifact_id] = record;
    }

    return Result<CreateArtifactResponse>::Success(std::move(response));
}

Result<PreparePlanResponse> DispensingWorkflowUseCase::PreparePlan(const PreparePlanRequest& request) {
    Services::Dispensing::DispensingExecutionCompatibilityService compatibility_service;
    ArtifactRecord artifact;
    {
        std::lock_guard<std::mutex> lock(artifacts_mutex_);
        auto it = artifacts_.find(request.artifact_id);
        if (it == artifacts_.end()) {
            return Result<PreparePlanResponse>::Failure(
                Error(ErrorCode::NOT_FOUND, "artifact not found", "DispensingWorkflowUseCase"));
        }
        artifact = it->second;
    }

    auto planning_request_result =
        compatibility_service.BuildPlanningRequest(request.execution_request, artifact.upload_response.filepath);
    if (planning_request_result.IsError()) {
        return Result<PreparePlanResponse>::Failure(planning_request_result.GetError());
    }

    auto planning_result = planning_use_case_->Execute(planning_request_result.Value());
    if (planning_result.IsError()) {
        return Result<PreparePlanResponse>::Failure(planning_result.GetError());
    }

    const auto& planning = planning_result.Value();
    if (!planning.execution_package) {
        return Result<PreparePlanResponse>::Failure(
            Error(ErrorCode::INVALID_STATE, "execution package unavailable", "DispensingWorkflowUseCase"));
    }
    auto execution_request_result =
        compatibility_service.BuildExecutionRequest(planning, request.execution_request, artifact.upload_response.filepath);
    if (execution_request_result.IsError()) {
        return Result<PreparePlanResponse>::Failure(execution_request_result.GetError());
    }

    PlanExecutionLaunch execution_launch;
    execution_launch.execution_package = execution_request_result.Value().execution_package;
    execution_launch.runtime_overrides.source_path = execution_request_result.Value().source_path;
    execution_launch.runtime_overrides.use_hardware_trigger = execution_request_result.Value().use_hardware_trigger;
    execution_launch.runtime_overrides.dry_run = execution_request_result.Value().dry_run;
    execution_launch.runtime_overrides.machine_mode = execution_request_result.Value().machine_mode;
    execution_launch.runtime_overrides.execution_mode = execution_request_result.Value().execution_mode;
    execution_launch.runtime_overrides.output_policy = execution_request_result.Value().output_policy;
    execution_launch.runtime_overrides.max_jerk = execution_request_result.Value().max_jerk;
    execution_launch.runtime_overrides.arc_tolerance_mm = execution_request_result.Value().arc_tolerance_mm;
    execution_launch.runtime_overrides.dispensing_speed_mm_s = execution_request_result.Value().dispensing_speed_mm_s;
    execution_launch.runtime_overrides.dry_run_speed_mm_s = execution_request_result.Value().dry_run_speed_mm_s;
    execution_launch.runtime_overrides.rapid_speed_mm_s = execution_request_result.Value().rapid_speed_mm_s;
    execution_launch.runtime_overrides.acceleration_mm_s2 = execution_request_result.Value().acceleration_mm_s2;
    execution_launch.runtime_overrides.velocity_trace_enabled = execution_request_result.Value().velocity_trace_enabled;
    execution_launch.runtime_overrides.velocity_trace_interval_ms =
        execution_request_result.Value().velocity_trace_interval_ms;
    execution_launch.runtime_overrides.velocity_trace_path = execution_request_result.Value().velocity_trace_path;
    execution_launch.runtime_overrides.velocity_guard_enabled = execution_request_result.Value().velocity_guard_enabled;
    execution_launch.runtime_overrides.velocity_guard_ratio = execution_request_result.Value().velocity_guard_ratio;
    execution_launch.runtime_overrides.velocity_guard_abs_mm_s = execution_request_result.Value().velocity_guard_abs_mm_s;
    execution_launch.runtime_overrides.velocity_guard_min_expected_mm_s =
        execution_request_result.Value().velocity_guard_min_expected_mm_s;
    execution_launch.runtime_overrides.velocity_guard_grace_ms = execution_request_result.Value().velocity_guard_grace_ms;
    execution_launch.runtime_overrides.velocity_guard_interval_ms =
        execution_request_result.Value().velocity_guard_interval_ms;
    execution_launch.runtime_overrides.velocity_guard_max_consecutive =
        execution_request_result.Value().velocity_guard_max_consecutive;
    execution_launch.runtime_overrides.velocity_guard_stop_on_violation =
        execution_request_result.Value().velocity_guard_stop_on_violation;

    PreparePlanResponse response;
    response.success = true;
    response.artifact_id = request.artifact_id;
    response.plan_id = GenerateId("plan");
    response.plan_fingerprint = BuildPlanFingerprint(request.artifact_id, planning, execution_launch);
    response.filepath = artifact.upload_response.filepath;
    response.segment_count = static_cast<std::uint32_t>(std::max(0, planning.segment_count));
    response.point_count = static_cast<std::uint32_t>(planning.trajectory_points.size());
    response.total_length_mm = planning.total_length;
    response.estimated_time_s = planning.estimated_time;
    response.generated_at = ToIso8601UtcNow();

    PlanRecord record;
    record.response = response;
    record.execution_launch = std::move(execution_launch);
    record.trajectory_points = planning.trajectory_points;
    record.preview_state = PlanPreviewState::PREPARED;
    record.latest = true;

    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        for (auto& [existing_plan_id, existing_record] : plans_) {
            if (existing_plan_id == response.plan_id) {
                continue;
            }
            if (existing_record.response.artifact_id == request.artifact_id && existing_record.latest) {
                existing_record.latest = false;
                existing_record.preview_state = PlanPreviewState::STALE;
                existing_record.confirmed_at.clear();
            }
        }
        plans_[response.plan_id] = record;
    }

    return Result<PreparePlanResponse>::Success(std::move(response));
}

Result<PreviewSnapshotResponse> DispensingWorkflowUseCase::GetPreviewSnapshot(const PreviewSnapshotRequest& request) {
    if (request.plan_id.empty()) {
        return Result<PreviewSnapshotResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "plan_id is required", "DispensingWorkflowUseCase"));
    }

    PlanRecord snapshot_record;
    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        auto it = plans_.find(request.plan_id);
        if (it == plans_.end()) {
            return Result<PreviewSnapshotResponse>::Failure(
                Error(ErrorCode::NOT_FOUND, "plan not found", "DispensingWorkflowUseCase"));
        }
        if (!it->second.latest) {
            return Result<PreviewSnapshotResponse>::Failure(
                Error(ErrorCode::INVALID_STATE, "plan is stale", "DispensingWorkflowUseCase"));
        }
        if (it->second.trajectory_points.empty()) {
            it->second.preview_state = PlanPreviewState::FAILED;
            it->second.failure_message = "trajectory points unavailable";
            return Result<PreviewSnapshotResponse>::Failure(
                Error(ErrorCode::INVALID_STATE, "trajectory points unavailable", "DispensingWorkflowUseCase"));
        }

        const bool keep_confirmed_state =
            it->second.preview_state == PlanPreviewState::CONFIRMED &&
            !it->second.preview_snapshot_hash.empty() &&
            it->second.preview_snapshot_hash == it->second.response.plan_fingerprint;

        it->second.preview_snapshot_id = it->second.response.plan_id;
        it->second.preview_snapshot_hash = it->second.response.plan_fingerprint;
        it->second.preview_generated_at = ToIso8601UtcNow();
        if (!keep_confirmed_state) {
            it->second.preview_state = PlanPreviewState::SNAPSHOT_READY;
            it->second.confirmed_at.clear();
        }
        it->second.failure_message.clear();
        snapshot_record = it->second;
    }

    return Result<PreviewSnapshotResponse>::Success(
        BuildPreviewSnapshotResponse(snapshot_record, request.max_polyline_points));
}

Result<ConfirmPreviewResponse> DispensingWorkflowUseCase::ConfirmPreview(const ConfirmPreviewRequest& request) {
    if (request.plan_id.empty()) {
        return Result<ConfirmPreviewResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "plan_id is required", "DispensingWorkflowUseCase"));
    }
    if (request.snapshot_hash.empty()) {
        return Result<ConfirmPreviewResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "snapshot_hash is required", "DispensingWorkflowUseCase"));
    }

    ConfirmPreviewResponse response;
    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        auto it = plans_.find(request.plan_id);
        if (it == plans_.end()) {
            return Result<ConfirmPreviewResponse>::Failure(
                Error(ErrorCode::NOT_FOUND, "plan not found", "DispensingWorkflowUseCase"));
        }
        if (!it->second.latest) {
            return Result<ConfirmPreviewResponse>::Failure(
                Error(ErrorCode::INVALID_STATE, "plan is stale", "DispensingWorkflowUseCase"));
        }
        if (it->second.preview_state != PlanPreviewState::SNAPSHOT_READY &&
            it->second.preview_state != PlanPreviewState::CONFIRMED) {
            return Result<ConfirmPreviewResponse>::Failure(
                Error(ErrorCode::INVALID_STATE, "preview snapshot not prepared", "DispensingWorkflowUseCase"));
        }
        if (request.snapshot_hash != it->second.preview_snapshot_hash) {
            return Result<ConfirmPreviewResponse>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "snapshot hash mismatch", "DispensingWorkflowUseCase"));
        }

        it->second.preview_state = PlanPreviewState::CONFIRMED;
        if (it->second.confirmed_at.empty()) {
            it->second.confirmed_at = ToIso8601UtcNow();
        }
        it->second.failure_message.clear();

        response.confirmed = true;
        response.plan_id = it->second.response.plan_id;
        response.snapshot_hash = it->second.preview_snapshot_hash;
        response.preview_state = PreviewStateToString(it->second.preview_state);
        response.confirmed_at = it->second.confirmed_at;
    }

    return Result<ConfirmPreviewResponse>::Success(std::move(response));
}

Result<JobID> DispensingWorkflowUseCase::StartJob(const StartJobRequest& request) {
    if (request.plan_id.empty()) {
        return Result<JobID>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "plan_id is required", "DispensingWorkflowUseCase"));
    }
    if (request.plan_fingerprint.empty()) {
        return Result<JobID>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "plan_fingerprint is required", "DispensingWorkflowUseCase"));
    }
    if (request.target_count == 0) {
        return Result<JobID>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "target_count must be greater than 0", "DispensingWorkflowUseCase"));
    }

    PlanExecutionLaunch execution_launch;
    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        auto it = plans_.find(request.plan_id);
        if (it == plans_.end()) {
            return Result<JobID>::Failure(
                Error(ErrorCode::NOT_FOUND, "plan not found", "DispensingWorkflowUseCase"));
        }
        if (!it->second.latest) {
            return Result<JobID>::Failure(
                Error(ErrorCode::INVALID_STATE, "plan is stale", "DispensingWorkflowUseCase"));
        }
        if (it->second.preview_state != PlanPreviewState::CONFIRMED) {
            return Result<JobID>::Failure(
                Error(ErrorCode::INVALID_STATE, "preview not confirmed", "DispensingWorkflowUseCase"));
        }
        if (request.plan_fingerprint != it->second.response.plan_fingerprint) {
            return Result<JobID>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "plan fingerprint mismatch", "DispensingWorkflowUseCase"));
        }
        execution_launch = it->second.execution_launch;
    }

    RuntimeStartJobRequest runtime_request;
    runtime_request.plan_id = request.plan_id;
    runtime_request.execution_request = BuildExecutionRequest(execution_launch);
    runtime_request.plan_fingerprint = request.plan_fingerprint;
    runtime_request.target_count = request.target_count;

    auto runtime_result = execution_use_case_->StartJob(runtime_request);
    if (runtime_result.IsError()) {
        return Result<JobID>::Failure(runtime_result.GetError());
    }

    const auto& job_id = runtime_result.Value();
    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        auto it = plans_.find(request.plan_id);
        if (it != plans_.end() && it->second.latest) {
            it->second.runtime_job_id = job_id;
        }
    }
    {
        std::lock_guard<std::mutex> lock(job_plan_index_mutex_);
        job_plan_index_[job_id] = request.plan_id;
    }

    return Result<JobID>::Success(job_id);
}

Result<JobStatusResponse> DispensingWorkflowUseCase::GetJobStatus(const JobID& job_id) const {
    auto runtime_result = execution_use_case_->GetJobStatus(job_id);
    if (runtime_result.IsError()) {
        return Result<JobStatusResponse>::Failure(runtime_result.GetError());
    }

    const auto& runtime_status = runtime_result.Value();
    SyncPlanStateFromRuntimeStatus(job_id, runtime_status);

    JobStatusResponse response;
    response.job_id = runtime_status.job_id;
    response.plan_id = runtime_status.plan_id;
    response.plan_fingerprint = runtime_status.plan_fingerprint;
    response.state = runtime_status.state;
    response.target_count = runtime_status.target_count;
    response.completed_count = runtime_status.completed_count;
    response.current_cycle = runtime_status.current_cycle;
    response.current_segment = runtime_status.current_segment;
    response.total_segments = runtime_status.total_segments;
    response.cycle_progress_percent = runtime_status.cycle_progress_percent;
    response.overall_progress_percent = runtime_status.overall_progress_percent;
    response.elapsed_seconds = runtime_status.elapsed_seconds;
    response.error_message = runtime_status.error_message;
    response.active_task_id = runtime_status.active_task_id;
    response.dry_run = runtime_status.dry_run;
    return Result<JobStatusResponse>::Success(std::move(response));
}

Result<void> DispensingWorkflowUseCase::PauseJob(const JobID& job_id) {
    return execution_use_case_->PauseJob(job_id);
}

Result<void> DispensingWorkflowUseCase::ResumeJob(const JobID& job_id) {
    return execution_use_case_->ResumeJob(job_id);
}

Result<void> DispensingWorkflowUseCase::StopJob(const JobID& job_id) {
    auto result = execution_use_case_->StopJob(job_id);
    if (result.IsSuccess()) {
        PlanID plan_id;
        {
            std::lock_guard<std::mutex> lock(job_plan_index_mutex_);
            auto it = job_plan_index_.find(job_id);
            if (it != job_plan_index_.end()) {
                plan_id = it->second;
            }
        }
        if (!plan_id.empty()) {
            ReleaseConfirmedPreviewForPlan(plan_id, &job_id);
        }
    }
    return result;
}

Result<Domain::Safety::ValueObjects::InterlockSignals> DispensingWorkflowUseCase::ReadInterlockSignals() const {
    if (!interlock_signal_port_) {
        return Result<Domain::Safety::ValueObjects::InterlockSignals>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "interlock signal port not available", "DispensingWorkflowUseCase"));
    }
    return interlock_signal_port_->ReadSignals();
}

bool DispensingWorkflowUseCase::IsInterlockLatched() const {
    if (!interlock_signal_port_) {
        return false;
    }
    return interlock_signal_port_->IsLatched();
}

PreviewSnapshotResponse DispensingWorkflowUseCase::BuildPreviewSnapshotResponse(
    const PlanRecord& plan_record,
    std::size_t max_polyline_points) {
    Siligen::Application::Services::Dispensing::WorkflowPreviewSnapshotInput input;
    input.snapshot_id = plan_record.preview_snapshot_id;
    input.snapshot_hash = plan_record.preview_snapshot_hash;
    input.plan_id = plan_record.response.plan_id;
    input.preview_state = PreviewStateToString(plan_record.preview_state);
    input.confirmed_at = plan_record.confirmed_at;
    input.segment_count = plan_record.response.segment_count;
    input.point_count = plan_record.response.point_count;
    input.total_length_mm = plan_record.response.total_length_mm;
    input.estimated_time_s = plan_record.response.estimated_time_s;
    input.generated_at = plan_record.preview_generated_at;
    input.trajectory_points = &plan_record.trajectory_points;

    Siligen::Application::Services::Dispensing::WorkflowPreviewSnapshotService snapshot_service;
    return snapshot_service.BuildResponse(input, max_polyline_points);
}

std::string DispensingWorkflowUseCase::GenerateId(const char* prefix) {
    const auto seq = id_sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return std::string(prefix) + "-" + std::to_string(millis) + "-" + std::to_string(seq);
}

DispensingExecutionRequest DispensingWorkflowUseCase::BuildExecutionRequest(const PlanExecutionLaunch& launch) const {
    DispensingExecutionRequest request;
    request.execution_package = launch.execution_package;
    request.source_path = launch.runtime_overrides.source_path;
    request.use_hardware_trigger = launch.runtime_overrides.use_hardware_trigger;
    request.dry_run = launch.runtime_overrides.dry_run;
    request.machine_mode = launch.runtime_overrides.machine_mode;
    request.execution_mode = launch.runtime_overrides.execution_mode;
    request.output_policy = launch.runtime_overrides.output_policy;
    request.max_jerk = launch.runtime_overrides.max_jerk;
    request.arc_tolerance_mm = launch.runtime_overrides.arc_tolerance_mm;
    request.dispensing_speed_mm_s = launch.runtime_overrides.dispensing_speed_mm_s;
    request.dry_run_speed_mm_s = launch.runtime_overrides.dry_run_speed_mm_s;
    request.rapid_speed_mm_s = launch.runtime_overrides.rapid_speed_mm_s;
    request.acceleration_mm_s2 = launch.runtime_overrides.acceleration_mm_s2;
    request.velocity_trace_enabled = launch.runtime_overrides.velocity_trace_enabled;
    request.velocity_trace_interval_ms = launch.runtime_overrides.velocity_trace_interval_ms;
    request.velocity_trace_path = launch.runtime_overrides.velocity_trace_path;
    request.velocity_guard_enabled = launch.runtime_overrides.velocity_guard_enabled;
    request.velocity_guard_ratio = launch.runtime_overrides.velocity_guard_ratio;
    request.velocity_guard_abs_mm_s = launch.runtime_overrides.velocity_guard_abs_mm_s;
    request.velocity_guard_min_expected_mm_s = launch.runtime_overrides.velocity_guard_min_expected_mm_s;
    request.velocity_guard_grace_ms = launch.runtime_overrides.velocity_guard_grace_ms;
    request.velocity_guard_interval_ms = launch.runtime_overrides.velocity_guard_interval_ms;
    request.velocity_guard_max_consecutive = launch.runtime_overrides.velocity_guard_max_consecutive;
    request.velocity_guard_stop_on_violation = launch.runtime_overrides.velocity_guard_stop_on_violation;
    return request;
}

std::string DispensingWorkflowUseCase::BuildPlanFingerprint(
    const ArtifactID& artifact_id,
    const PlanningResponse& planning,
    const PlanExecutionLaunch& execution_launch) const {
    std::ostringstream oss;
    const auto runtime_request = BuildExecutionRequest(execution_launch);
    const auto machine_mode = runtime_request.ResolveMachineMode();
    const auto execution_mode = runtime_request.ResolveExecutionMode();
    const auto output_policy = runtime_request.ResolveOutputPolicy();
    const auto& package = execution_launch.execution_package;
    const auto& execution_plan = package.execution_plan;

    oss << artifact_id << '|'
        << runtime_request.dry_run << '|'
        << Domain::Machine::ValueObjects::ToString(machine_mode) << '|'
        << Domain::Dispensing::ValueObjects::ToString(execution_mode) << '|'
        << Domain::Dispensing::ValueObjects::ToString(output_policy) << '|'
        << planning.segment_count << '|'
        << planning.trajectory_points.size() << '|'
        << planning.total_length << '|'
        << planning.estimated_time << '|'
        << package.total_length_mm << '|'
        << package.estimated_time_s << '|'
        << package.source_path << '|'
        << package.source_fingerprint << '|'
        << execution_plan.motion_trajectory.points.size() << '|'
        << execution_plan.interpolation_segments.size() << '|'
        << execution_plan.interpolation_points.size() << '|'
        << execution_plan.trigger_distances_mm.size() << '|'
        << execution_plan.trigger_interval_ms << '|'
        << execution_plan.trigger_interval_mm << '|'
        << runtime_request.use_hardware_trigger << '|'
        << runtime_request.max_jerk << '|'
        << runtime_request.arc_tolerance_mm << '|'
        << runtime_request.dispensing_speed_mm_s.value_or(0.0f) << '|'
        << runtime_request.dry_run_speed_mm_s.value_or(0.0f) << '|'
        << runtime_request.rapid_speed_mm_s.value_or(0.0f) << '|'
        << runtime_request.acceleration_mm_s2.value_or(0.0f) << '|'
        << runtime_request.velocity_trace_enabled << '|'
        << runtime_request.velocity_trace_interval_ms << '|'
        << runtime_request.velocity_trace_path << '|'
        << runtime_request.velocity_guard_enabled << '|'
        << runtime_request.velocity_guard_ratio << '|'
        << runtime_request.velocity_guard_abs_mm_s << '|'
        << runtime_request.velocity_guard_min_expected_mm_s << '|'
        << runtime_request.velocity_guard_grace_ms << '|'
        << runtime_request.velocity_guard_interval_ms << '|'
        << runtime_request.velocity_guard_max_consecutive << '|'
        << runtime_request.velocity_guard_stop_on_violation;
    return HexEncodeUint64(Fnv1a64(oss.str()));
}

std::string DispensingWorkflowUseCase::PreviewStateToString(PlanPreviewState state) const {
    switch (state) {
        case PlanPreviewState::PREPARED:
            return "prepared";
        case PlanPreviewState::SNAPSHOT_READY:
            return "snapshot_ready";
        case PlanPreviewState::CONFIRMED:
            return "confirmed";
        case PlanPreviewState::STALE:
            return "stale";
        case PlanPreviewState::FAILED:
            return "failed";
        default:
            return "unknown";
    }
}

void DispensingWorkflowUseCase::ReleaseConfirmedPreviewForPlan(
    const PlanID& plan_id,
    const JobID* runtime_job_id) const {
    if (plan_id.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(plans_mutex_);
    auto plan_it = plans_.find(plan_id);
    if (plan_it == plans_.end() || !plan_it->second.latest) {
        return;
    }

    if (runtime_job_id != nullptr &&
        !plan_it->second.runtime_job_id.empty() &&
        plan_it->second.runtime_job_id != *runtime_job_id) {
        return;
    }

    if (plan_it->second.preview_state == PlanPreviewState::CONFIRMED) {
        plan_it->second.preview_state = PlanPreviewState::SNAPSHOT_READY;
    }
    plan_it->second.confirmed_at.clear();
    if (runtime_job_id != nullptr && plan_it->second.runtime_job_id == *runtime_job_id) {
        plan_it->second.runtime_job_id.clear();
    }
}

void DispensingWorkflowUseCase::SyncPlanStateFromRuntimeStatus(
    const JobID& job_id,
    const RuntimeJobStatusResponse& runtime_status) const {
    if (!IsRuntimeTerminalState(runtime_status.state)) {
        return;
    }

    PlanID plan_id;
    {
        std::lock_guard<std::mutex> lock(job_plan_index_mutex_);
        auto it = job_plan_index_.find(job_id);
        if (it != job_plan_index_.end()) {
            plan_id = it->second;
            job_plan_index_.erase(it);
        }
    }

    if (plan_id.empty()) {
        plan_id = runtime_status.plan_id;
    }
    ReleaseConfirmedPreviewForPlan(plan_id, &job_id);
}

}  // namespace Siligen::Application::UseCases::Dispensing
