#define MODULE_NAME "DispensingWorkflowUseCase"

#include "DispensingWorkflowUseCase.h"

#include "domain/safety/domain-services/InterlockPolicy.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

namespace Siligen::Application::UseCases::Dispensing {

using Domain::Machine::Ports::IHardwareConnectionPort;
using Domain::Machine::ValueObjects::MachineMode;
using Domain::Motion::Ports::IMotionStatePort;
using Domain::Motion::Ports::MotionState;
using Domain::Motion::Ports::MotionStatus;
using Domain::Dispensing::ValueObjects::JobExecutionMode;
using Domain::Dispensing::ValueObjects::ProcessOutputPolicy;
using Domain::Safety::DomainServices::InterlockPolicy;
using Domain::Safety::Ports::IInterlockSignalPort;
using Domain::Safety::ValueObjects::InterlockCause;
using Domain::Safety::ValueObjects::InterlockPolicyConfig;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;

namespace {

constexpr auto kJobPollInterval = std::chrono::milliseconds(100);
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

bool IsTerminal(DispensingWorkflowUseCase::WorkflowJobState state) {
    return state == DispensingWorkflowUseCase::WorkflowJobState::COMPLETED ||
           state == DispensingWorkflowUseCase::WorkflowJobState::FAILED ||
           state == DispensingWorkflowUseCase::WorkflowJobState::CANCELLED;
}

bool IsTaskStateActive(const std::string& state) {
    return state == "pending" || state == "running" || state == "paused";
}

}  // namespace

DispensingWorkflowUseCase::DispensingWorkflowUseCase(
    std::shared_ptr<UploadFileUseCase> upload_use_case,
    std::shared_ptr<PlanningUseCase> planning_use_case,
    std::shared_ptr<DispensingExecutionUseCase> execution_use_case,
    std::shared_ptr<IHardwareConnectionPort> connection_port,
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

DispensingWorkflowUseCase::~DispensingWorkflowUseCase() {
    shutting_down_.store(true);
    JobID active_job;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        active_job = active_job_id_;
    }
    if (!active_job.empty()) {
        (void)StopJob(active_job);
    }

    std::thread worker_to_join;
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        if (worker_thread_.joinable()) {
            worker_to_join = std::move(worker_thread_);
        }
    }
    if (worker_to_join.joinable()) {
        worker_to_join.join();
    }
}

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

    auto execution_request = request.execution_request;
    execution_request.dxf_filepath = artifact.upload_response.filepath;
    auto validation = execution_request.Validate();
    if (validation.IsError()) {
        return Result<PreparePlanResponse>::Failure(validation.GetError());
    }

    auto planning_request = BuildPlanningRequest(artifact.upload_response.filepath, execution_request);
    auto planning_result = planning_use_case_->Execute(planning_request);
    if (planning_result.IsError()) {
        return Result<PreparePlanResponse>::Failure(planning_result.GetError());
    }

    const auto& planning = planning_result.Value();
    PreparePlanResponse response;
    response.success = true;
    response.artifact_id = request.artifact_id;
    response.plan_id = GenerateId("plan");
    response.plan_fingerprint = BuildPlanFingerprint(request.artifact_id, planning, execution_request);
    response.filepath = artifact.upload_response.filepath;
    response.segment_count = static_cast<std::uint32_t>(std::max(0, planning.segment_count));
    response.point_count = static_cast<std::uint32_t>(planning.trajectory_points.size());
    response.total_length_mm = planning.total_length;
    response.estimated_time_s = planning.estimated_time;
    response.generated_at = ToIso8601UtcNow();

    PlanRecord record;
    record.response = response;
    record.execution_request = execution_request;
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

    PlanRecord plan_record;
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
        plan_record = it->second;
    }

    auto precondition_result = ValidateExecutionPreconditions();
    if (precondition_result.IsError()) {
        return Result<JobID>::Failure(precondition_result.GetError());
    }

    const auto job_id = GenerateId("job");
    auto context = std::make_shared<JobContext>();
    context->job_id = job_id;
    context->plan_id = request.plan_id;
    context->plan_fingerprint = plan_record.response.plan_fingerprint;
    context->execution_request = plan_record.execution_request;
    context->state.store(WorkflowJobState::PENDING);
    context->target_count.store(request.target_count);
    context->dry_run =
        plan_record.execution_request.ResolveOutputPolicy() == ProcessOutputPolicy::Inhibited;
    context->start_time = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        if (!active_job_id_.empty()) {
            auto active_it = jobs_.find(active_job_id_);
            if (active_it != jobs_.end() && !IsTerminal(active_it->second->state.load())) {
                return Result<JobID>::Failure(
                    Error(ErrorCode::INVALID_STATE, "another job is already active", "DispensingWorkflowUseCase"));
            }
        }
        jobs_[job_id] = context;
        active_job_id_ = job_id;
    }

    std::thread new_worker_thread;
    try {
        new_worker_thread = std::thread([this, context, plan_record]() { RunJob(context, plan_record); });
    } catch (const std::exception& ex) {
        {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            auto job_it = jobs_.find(job_id);
            if (job_it != jobs_.end() && job_it->second == context) {
                jobs_.erase(job_it);
            }
            if (active_job_id_ == job_id) {
                active_job_id_.clear();
            }
        }
        return Result<JobID>::Failure(
            Error(
                ErrorCode::THREAD_START_FAILED,
                "failure_stage=start_job_thread_start;failure_code=THREAD_START_FAILED;message=" + std::string(ex.what()),
                "DispensingWorkflowUseCase"));
    } catch (...) {
        {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            auto job_it = jobs_.find(job_id);
            if (job_it != jobs_.end() && job_it->second == context) {
                jobs_.erase(job_it);
            }
            if (active_job_id_ == job_id) {
                active_job_id_.clear();
            }
        }
        return Result<JobID>::Failure(
            Error(
                ErrorCode::THREAD_START_FAILED,
                "failure_stage=start_job_thread_start;failure_code=THREAD_START_FAILED;message=unknown",
                "DispensingWorkflowUseCase"));
    }

    std::thread worker_to_join;
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        if (worker_thread_.joinable()) {
            worker_to_join = std::move(worker_thread_);
        }
        worker_thread_ = std::move(new_worker_thread);
    }
    if (worker_to_join.joinable()) {
        worker_to_join.join();
    }
    return Result<JobID>::Success(job_id);
}

Result<JobStatusResponse> DispensingWorkflowUseCase::GetJobStatus(const JobID& job_id) const {
    std::shared_ptr<JobContext> context;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            return Result<JobStatusResponse>::Failure(
                Error(ErrorCode::NOT_FOUND, "job not found", "DispensingWorkflowUseCase"));
        }
        context = it->second;
    }
    return Result<JobStatusResponse>::Success(BuildJobStatusResponse(context));
}

Result<void> DispensingWorkflowUseCase::PauseJob(const JobID& job_id) {
    std::shared_ptr<JobContext> context;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            return Result<void>::Failure(
                Error(ErrorCode::NOT_FOUND, "job not found", "DispensingWorkflowUseCase"));
        }
        context = it->second;
    }

    const auto state = context->state.load();
    if (state == WorkflowJobState::PAUSED) {
        return Result<void>::Success();
    }
    if (state == WorkflowJobState::STOPPING) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "job is stopping", "DispensingWorkflowUseCase"));
    }
    if (IsTerminal(state)) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "job already finished", "DispensingWorkflowUseCase"));
    }

    context->pause_requested.store(true);
    std::string active_task_id;
    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        active_task_id = context->active_task_id;
    }
    if (!active_task_id.empty()) {
        auto pause_result = execution_use_case_->PauseTask(active_task_id);
        if (pause_result.IsError()) {
            return pause_result;
        }
    }
    return Result<void>::Success();
}

Result<void> DispensingWorkflowUseCase::ResumeJob(const JobID& job_id) {
    std::shared_ptr<JobContext> context;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            return Result<void>::Failure(
                Error(ErrorCode::NOT_FOUND, "job not found", "DispensingWorkflowUseCase"));
        }
        context = it->second;
    }

    const auto state = context->state.load();
    if (state == WorkflowJobState::STOPPING) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "job is stopping", "DispensingWorkflowUseCase"));
    }
    if (IsTerminal(state)) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "job already finished", "DispensingWorkflowUseCase"));
    }

    auto precondition_result = ValidateExecutionPreconditions();
    if (precondition_result.IsError()) {
        return Result<void>::Failure(precondition_result.GetError());
    }

    context->pause_requested.store(false);
    std::string active_task_id;
    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        active_task_id = context->active_task_id;
    }
    if (!active_task_id.empty()) {
        auto resume_result = execution_use_case_->ResumeTask(active_task_id);
        if (resume_result.IsError()) {
            return resume_result;
        }
    }
    return Result<void>::Success();
}

Result<void> DispensingWorkflowUseCase::StopJob(const JobID& job_id) {
    std::shared_ptr<JobContext> context;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            return Result<void>::Failure(
                Error(ErrorCode::NOT_FOUND, "job not found", "DispensingWorkflowUseCase"));
        }
        context = it->second;
    }

    if (IsTerminal(context->state.load())) {
        return Result<void>::Success();
    }

    context->stop_requested.store(true);
    context->pause_requested.store(false);
    context->state.store(WorkflowJobState::STOPPING);

    std::string active_task_id;
    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        active_task_id = context->active_task_id;
    }
    if (!active_task_id.empty()) {
        auto cancel_result = execution_use_case_->CancelTask(active_task_id);
        if (cancel_result.IsError()) {
            auto status_result = execution_use_case_->GetTaskStatus(active_task_id);
            if (status_result.IsError()) {
                if (IsTerminal(context->state.load())) {
                    return Result<void>::Success();
                }
                return Result<void>::Failure(
                    Error(
                        cancel_result.GetError().GetCode(),
                        "failure_stage=stop_cancel_forward;failure_code=" +
                            std::to_string(static_cast<int>(cancel_result.GetError().GetCode())) +
                            ";message=" + cancel_result.GetError().GetMessage() +
                            ";status_query_failure_code=" +
                            std::to_string(static_cast<int>(status_result.GetError().GetCode())) +
                            ";status_query_failure_message=" + status_result.GetError().GetMessage(),
                        "DispensingWorkflowUseCase"));
            }

            const auto& task_status = status_result.Value();
            if (task_status.state == "cancelled") {
                FinalizeJob(context, WorkflowJobState::CANCELLED, task_status.error_message);
                return Result<void>::Success();
            }
            if (task_status.state == "completed") {
                FinalizeJob(context, WorkflowJobState::COMPLETED, task_status.error_message);
                return Result<void>::Success();
            }
            if (task_status.state == "failed") {
                FinalizeJob(context, WorkflowJobState::FAILED, task_status.error_message);
                return Result<void>::Failure(
                    Error(
                        ErrorCode::HARDWARE_ERROR,
                        "failure_stage=stop_cancel_forward;failure_code=TASK_FAILED;message=" +
                            task_status.error_message,
                        "DispensingWorkflowUseCase"));
            }

            return Result<void>::Failure(
                Error(
                    cancel_result.GetError().GetCode(),
                    "failure_stage=stop_cancel_forward;failure_code=" +
                        std::to_string(static_cast<int>(cancel_result.GetError().GetCode())) +
                        ";message=" + cancel_result.GetError().GetMessage() +
                        ";task_state=" + task_status.state,
                    "DispensingWorkflowUseCase"));
        }
    } else {
        FinalizeJob(context, WorkflowJobState::CANCELLED, "job cancelled");
    }

    return Result<void>::Success();
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

Result<void> DispensingWorkflowUseCase::ValidateExecutionPreconditions() const {
    if (!connection_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "hardware connection port not available", "DispensingWorkflowUseCase"));
    }
    if (!connection_port_->IsConnected()) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "hardware not connected", "DispensingWorkflowUseCase"));
    }
    if (!motion_state_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "motion state port not available", "DispensingWorkflowUseCase"));
    }

    const LogicalAxisId required_axes[] = {LogicalAxisId::X, LogicalAxisId::Y};
    for (LogicalAxisId axis : required_axes) {
        auto status_result = motion_state_port_->GetAxisStatus(axis);
        if (status_result.IsError()) {
            return Result<void>::Failure(status_result.GetError());
        }
        const MotionStatus& status = status_result.Value();
        if (status.state == MotionState::ESTOP) {
            return Result<void>::Failure(
                Error(ErrorCode::EMERGENCY_STOP_ACTIVATED, "axis in emergency stop state", "DispensingWorkflowUseCase"));
        }
        if (!status.enabled || status.state == MotionState::DISABLED) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "required axis not enabled", "DispensingWorkflowUseCase"));
        }
        bool axis_homed = false;
        if (homing_port_) {
            auto homed_result = homing_port_->IsAxisHomed(axis);
            if (homed_result.IsError()) {
                return Result<void>::Failure(homed_result.GetError());
            }
            axis_homed = homed_result.Value();
        } else {
            axis_homed = (status.state == MotionState::HOMED);
        }
        if (!axis_homed) {
            return Result<void>::Failure(
                Error(ErrorCode::AXIS_NOT_HOMED, "required axis not homed", "DispensingWorkflowUseCase"));
        }
        if (status.has_error || status.servo_alarm || status.following_error) {
            return Result<void>::Failure(
                Error(ErrorCode::HARDWARE_ERROR, "required axis has active error", "DispensingWorkflowUseCase"));
        }
    }

    if (interlock_signal_port_) {
        InterlockPolicyConfig interlock_config;
        auto interlock_result = InterlockPolicy::Evaluate(*interlock_signal_port_, interlock_config);
        if (interlock_result.IsError()) {
            return Result<void>::Failure(interlock_result.GetError());
        }

        const auto& decision = interlock_result.Value();
        if (decision.triggered) {
            const std::string reason = decision.reason == nullptr ? "interlock triggered" : decision.reason;
            switch (decision.cause) {
                case InterlockCause::EMERGENCY_STOP:
                    return Result<void>::Failure(
                        Error(ErrorCode::EMERGENCY_STOP_ACTIVATED, reason, "DispensingWorkflowUseCase"));
                case InterlockCause::SERVO_ALARM:
                    return Result<void>::Failure(
                        Error(ErrorCode::HARDWARE_ERROR, reason, "DispensingWorkflowUseCase"));
                case InterlockCause::SAFETY_DOOR_OPEN:
                case InterlockCause::PRESSURE_ABNORMAL:
                case InterlockCause::TEMPERATURE_ABNORMAL:
                case InterlockCause::VOLTAGE_ABNORMAL:
                    return Result<void>::Failure(
                        Error(ErrorCode::INVALID_STATE, reason, "DispensingWorkflowUseCase"));
                case InterlockCause::NONE:
                default:
                    return Result<void>::Failure(
                        Error(ErrorCode::INVALID_STATE, "interlock triggered", "DispensingWorkflowUseCase"));
            }
        }
    }

    return Result<void>::Success();
}

PlanningRequest DispensingWorkflowUseCase::BuildPlanningRequest(
    const std::string& filepath,
    const DispensingMVPRequest& request) const {
    PlanningRequest planning_request;
    planning_request.dxf_filepath = filepath;
    planning_request.optimize_path = request.optimize_path;
    planning_request.start_x = request.start_x;
    planning_request.start_y = request.start_y;
    planning_request.approximate_splines = request.approximate_splines;
    planning_request.two_opt_iterations = request.two_opt_iterations;
    planning_request.spline_max_step_mm = request.spline_max_step_mm;
    planning_request.spline_max_error_mm = request.spline_max_error_mm;
    planning_request.continuity_tolerance_mm = request.continuity_tolerance_mm;
    planning_request.curve_chain_angle_deg = request.curve_chain_angle_deg;
    planning_request.curve_chain_max_segment_mm = request.curve_chain_max_segment_mm;
    planning_request.use_hardware_trigger = request.use_hardware_trigger;
    planning_request.use_interpolation_planner = request.use_interpolation_planner;
    planning_request.interpolation_algorithm = request.interpolation_algorithm;
    planning_request.trajectory_config.arc_tolerance = request.arc_tolerance_mm > 0.0f
                                                           ? request.arc_tolerance_mm
                                                           : planning_request.trajectory_config.arc_tolerance;
    planning_request.trajectory_config.max_jerk = request.max_jerk > 0.0f
                                                      ? request.max_jerk
                                                      : planning_request.trajectory_config.max_jerk;

    if (request.acceleration_mm_s2.has_value() && request.acceleration_mm_s2.value() > 0.0f) {
        planning_request.trajectory_config.max_acceleration = request.acceleration_mm_s2.value();
    }

    const auto execution_mode = request.ResolveExecutionMode();

    if (execution_mode == JobExecutionMode::ValidationDryCycle &&
        request.dry_run_speed_mm_s.has_value() &&
        request.dry_run_speed_mm_s.value() > 0.0f) {
        planning_request.trajectory_config.max_velocity = request.dry_run_speed_mm_s.value();
    } else if (request.dispensing_speed_mm_s.has_value() && request.dispensing_speed_mm_s.value() > 0.0f) {
        planning_request.trajectory_config.max_velocity = request.dispensing_speed_mm_s.value();
    } else if (request.rapid_speed_mm_s.has_value() && request.rapid_speed_mm_s.value() > 0.0f) {
        planning_request.trajectory_config.max_velocity = request.rapid_speed_mm_s.value();
    }

    return planning_request;
}

JobStatusResponse DispensingWorkflowUseCase::BuildJobStatusResponse(const std::shared_ptr<JobContext>& context) const {
    JobStatusResponse response;
    response.job_id = context->job_id;
    response.plan_id = context->plan_id;
    response.plan_fingerprint = context->plan_fingerprint;
    response.state = JobStateToString(context->state.load());
    response.target_count = context->target_count.load();
    response.completed_count = context->completed_count.load();
    response.current_cycle = context->current_cycle.load();
    response.current_segment = context->current_segment.load();
    response.total_segments = context->total_segments.load();
    response.cycle_progress_percent = context->cycle_progress_percent.load();
    response.dry_run = context->dry_run;

    if (response.target_count > 0) {
        const std::uint64_t numerator =
            static_cast<std::uint64_t>(response.completed_count) * 100ULL + response.cycle_progress_percent;
        response.overall_progress_percent = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(100ULL, numerator / response.target_count));
    }
    if (response.state == "completed") {
        response.overall_progress_percent = 100;
        response.cycle_progress_percent = 100;
    }

    const auto end_point = IsTerminal(context->state.load()) ? context->end_time : std::chrono::steady_clock::now();
    response.elapsed_seconds =
        std::chrono::duration<float32>(end_point - context->start_time).count();

    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        response.error_message = context->error_message;
        response.active_task_id = context->active_task_id;
    }
    return response;
}

PreviewSnapshotResponse DispensingWorkflowUseCase::BuildPreviewSnapshotResponse(
    const PlanRecord& plan_record,
    std::size_t max_polyline_points) {
    PreviewSnapshotResponse response;
    response.snapshot_id = plan_record.preview_snapshot_id;
    response.snapshot_hash = plan_record.preview_snapshot_hash;
    response.plan_id = plan_record.response.plan_id;
    response.preview_state = PreviewStateToString(plan_record.preview_state);
    response.confirmed_at = plan_record.confirmed_at;
    response.segment_count = plan_record.response.segment_count;
    response.point_count = plan_record.response.point_count;
    response.polyline_source_point_count = static_cast<std::uint32_t>(plan_record.trajectory_points.size());
    response.total_length_mm = plan_record.response.total_length_mm;
    response.estimated_time_s = plan_record.response.estimated_time_s;
    response.generated_at = plan_record.preview_generated_at;

    auto polyline = BuildPreviewPolyline(plan_record.trajectory_points, max_polyline_points);
    response.polyline_point_count = static_cast<std::uint32_t>(polyline.size());
    response.trajectory_polyline.reserve(polyline.size());
    for (const auto& point : polyline) {
        PreviewSnapshotPoint snapshot_point;
        snapshot_point.x = point.x;
        snapshot_point.y = point.y;
        response.trajectory_polyline.push_back(snapshot_point);
    }
    return response;
}

std::string DispensingWorkflowUseCase::GenerateId(const char* prefix) {
    const auto seq = id_sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return std::string(prefix) + "-" + std::to_string(millis) + "-" + std::to_string(seq);
}

std::string DispensingWorkflowUseCase::BuildPlanFingerprint(
    const ArtifactID& artifact_id,
    const PlanningResponse& planning,
    const DispensingMVPRequest& execution_request) const {
    std::ostringstream oss;
    const auto machine_mode = execution_request.ResolveMachineMode();
    const auto execution_mode = execution_request.ResolveExecutionMode();
    const auto output_policy = execution_request.ResolveOutputPolicy();
    oss << artifact_id << '|'
        << execution_request.dry_run << '|'
        << Domain::Machine::ValueObjects::ToString(machine_mode) << '|'
        << Domain::Dispensing::ValueObjects::ToString(execution_mode) << '|'
        << Domain::Dispensing::ValueObjects::ToString(output_policy) << '|'
        << execution_request.optimize_path << '|'
        << execution_request.start_x << '|'
        << execution_request.start_y << '|'
        << execution_request.approximate_splines << '|'
        << execution_request.two_opt_iterations << '|'
        << execution_request.spline_max_step_mm << '|'
        << execution_request.spline_max_error_mm << '|'
        << execution_request.arc_tolerance_mm << '|'
        << execution_request.continuity_tolerance_mm << '|'
        << execution_request.curve_chain_angle_deg << '|'
        << execution_request.curve_chain_max_segment_mm << '|'
        << execution_request.use_hardware_trigger << '|'
        << execution_request.use_interpolation_planner << '|'
        << static_cast<int>(execution_request.interpolation_algorithm) << '|'
        << execution_request.dispensing_speed_mm_s.value_or(0.0f) << '|'
        << execution_request.dry_run_speed_mm_s.value_or(0.0f) << '|'
        << execution_request.rapid_speed_mm_s.value_or(0.0f) << '|'
        << execution_request.acceleration_mm_s2.value_or(0.0f) << '|'
        << planning.segment_count << '|'
        << planning.trajectory_points.size() << '|'
        << planning.total_length << '|'
        << planning.estimated_time;
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

std::string DispensingWorkflowUseCase::JobStateToString(WorkflowJobState state) const {
    switch (state) {
        case WorkflowJobState::PENDING:
            return "pending";
        case WorkflowJobState::RUNNING:
            return "running";
        case WorkflowJobState::STOPPING:
            return "stopping";
        case WorkflowJobState::PAUSED:
            return "paused";
        case WorkflowJobState::COMPLETED:
            return "completed";
        case WorkflowJobState::FAILED:
            return "failed";
        case WorkflowJobState::CANCELLED:
            return "cancelled";
        default:
            return "unknown";
    }
}

void DispensingWorkflowUseCase::RunJob(
    const std::shared_ptr<JobContext>& context,
    const PlanRecord& plan_record) {
    if (context->stop_requested.load()) {
        FinalizeJob(context, WorkflowJobState::CANCELLED, "job cancelled");
        return;
    }

    const auto target_count = context->target_count.load();

    for (std::uint32_t cycle = 0; cycle < target_count; ++cycle) {
        if (context->final_state_committed.load()) {
            return;
        }
        if (context->stop_requested.load()) {
            FinalizeJob(context, WorkflowJobState::CANCELLED, "job cancelled");
            return;
        }

        context->current_cycle.store(cycle + 1);
        context->current_segment.store(0);
        context->total_segments.store(0);
        context->cycle_progress_percent.store(0);

        while (context->pause_requested.load()) {
            if (context->stop_requested.load()) {
                FinalizeJob(context, WorkflowJobState::CANCELLED, "job cancelled");
                return;
            }
            if (!context->final_state_committed.load()) {
                context->state.store(WorkflowJobState::PAUSED);
            }
            std::this_thread::sleep_for(kJobPollInterval);
        }

        auto precondition_result = ValidateExecutionPreconditions();
        if (precondition_result.IsError()) {
            FinalizeJob(context, WorkflowJobState::FAILED, precondition_result.GetError().GetMessage());
            return;
        }
        if (context->stop_requested.load()) {
            FinalizeJob(context, WorkflowJobState::CANCELLED, "job cancelled");
            return;
        }
        if (!context->final_state_committed.load()) {
            context->state.store(WorkflowJobState::RUNNING);
        }
        if (context->final_state_committed.load()) {
            return;
        }
        if (context->stop_requested.load()) {
            FinalizeJob(context, WorkflowJobState::CANCELLED, "job cancelled");
            return;
        }

        auto task_result = execution_use_case_->ExecuteAsync(plan_record.execution_request);
        if (task_result.IsError()) {
            FinalizeJob(context, WorkflowJobState::FAILED, task_result.GetError().GetMessage());
            return;
        }

        const auto task_id = task_result.Value();
        {
            std::lock_guard<std::mutex> lock(context->mutex_);
            context->active_task_id = task_id;
        }

        bool pause_forwarded = false;
        bool cancel_forwarded = false;
        while (true) {
            if (context->final_state_committed.load()) {
                return;
            }
            if (context->stop_requested.load() && !cancel_forwarded) {
                if (!context->final_state_committed.load()) {
                    context->state.store(WorkflowJobState::STOPPING);
                }
                auto cancel_result = execution_use_case_->CancelTask(task_id);
                if (cancel_result.IsError()) {
                    auto task_status_after_cancel_result = execution_use_case_->GetTaskStatus(task_id);
                    if (task_status_after_cancel_result.IsError()) {
                        FinalizeJob(
                            context,
                            WorkflowJobState::FAILED,
                            "failure_stage=cancel_forward;cancel_failure_code=" +
                                std::to_string(static_cast<int>(cancel_result.GetError().GetCode())) +
                                ";cancel_failure_message=" + cancel_result.GetError().GetMessage() +
                                ";status_query_failure_code=" +
                                std::to_string(static_cast<int>(task_status_after_cancel_result.GetError().GetCode())) +
                                ";status_query_failure_message=" + task_status_after_cancel_result.GetError().GetMessage());
                        return;
                    }

                    const auto& task_status_after_cancel = task_status_after_cancel_result.Value();
                    if (task_status_after_cancel.state == "cancelled") {
                        FinalizeJob(context, WorkflowJobState::CANCELLED, task_status_after_cancel.error_message);
                        return;
                    }
                    if (task_status_after_cancel.state == "completed") {
                        context->completed_count.store(cycle + 1);
                        context->cycle_progress_percent.store(100);
                        if (cycle + 1 >= context->target_count.load()) {
                            break;
                        }
                        FinalizeJob(context, WorkflowJobState::CANCELLED, "job cancelled");
                        return;
                    }
                    if (task_status_after_cancel.state == "failed") {
                        FinalizeJob(context, WorkflowJobState::FAILED, task_status_after_cancel.error_message);
                        return;
                    }
                    if (IsTaskStateActive(task_status_after_cancel.state)) {
                        FinalizeJob(
                            context,
                            WorkflowJobState::FAILED,
                            "failure_stage=cancel_confirm;failure_code=TASK_STILL_ACTIVE;message=" +
                                cancel_result.GetError().GetMessage() +
                                ";task_state=" + task_status_after_cancel.state);
                        return;
                    }

                    FinalizeJob(
                        context,
                        WorkflowJobState::FAILED,
                        "failure_stage=cancel_confirm;failure_code=UNKNOWN_TASK_STATE;task_state=" +
                            task_status_after_cancel.state);
                    return;
                }
                cancel_forwarded = true;
            }

            auto task_status_result = execution_use_case_->GetTaskStatus(task_id);
            if (task_status_result.IsError()) {
                FinalizeJob(context, WorkflowJobState::FAILED, task_status_result.GetError().GetMessage());
                return;
            }
            const auto& task_status = task_status_result.Value();
            context->current_segment.store(task_status.executed_segments);
            context->total_segments.store(task_status.total_segments);
            context->cycle_progress_percent.store(task_status.progress_percent);

            if (context->pause_requested.load()) {
                if (!pause_forwarded && task_status.state == "running") {
                    auto pause_result = execution_use_case_->PauseTask(task_id);
                    if (pause_result.IsError()) {
                        FinalizeJob(context, WorkflowJobState::FAILED, pause_result.GetError().GetMessage());
                        return;
                    }
                    pause_forwarded = true;
                }
            } else if (pause_forwarded && task_status.state == "paused") {
                auto resume_result = execution_use_case_->ResumeTask(task_id);
                if (resume_result.IsError()) {
                    FinalizeJob(context, WorkflowJobState::FAILED, resume_result.GetError().GetMessage());
                    return;
                }
                pause_forwarded = false;
            }

            if (task_status.state == "paused") {
                if (!context->final_state_committed.load()) {
                    context->state.store(WorkflowJobState::PAUSED);
                }
            } else if (task_status.state == "running" || task_status.state == "pending") {
                if (!context->final_state_committed.load()) {
                    context->state.store(
                        context->stop_requested.load() ? WorkflowJobState::STOPPING : WorkflowJobState::RUNNING);
                }
            } else if (task_status.state == "completed") {
                context->completed_count.store(cycle + 1);
                context->cycle_progress_percent.store(100);
                if (context->stop_requested.load() && cycle + 1 < context->target_count.load()) {
                    FinalizeJob(context, WorkflowJobState::CANCELLED, "job cancelled");
                    return;
                }
                break;
            } else if (task_status.state == "cancelled") {
                FinalizeJob(context, WorkflowJobState::CANCELLED, task_status.error_message);
                return;
            } else if (task_status.state == "failed") {
                FinalizeJob(context, WorkflowJobState::FAILED, task_status.error_message);
                return;
            }

            std::this_thread::sleep_for(kJobPollInterval);
        }

        {
            std::lock_guard<std::mutex> lock(context->mutex_);
            context->active_task_id.clear();
        }
    }

    FinalizeJob(context, WorkflowJobState::COMPLETED);
}

void DispensingWorkflowUseCase::FinalizeJob(
    const std::shared_ptr<JobContext>& context,
    WorkflowJobState final_state,
    const std::string& error_message) {
    bool expected = false;
    if (!context->final_state_committed.compare_exchange_strong(expected, true)) {
        return;
    }

    context->state.store(final_state);
    context->end_time = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        context->active_task_id.clear();
        context->error_message = error_message;
    }
    if (final_state == WorkflowJobState::COMPLETED) {
        context->cycle_progress_percent.store(100);
        context->current_cycle.store(context->target_count.load());
        context->completed_count.store(context->target_count.load());
    }
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        if (active_job_id_ == context->job_id) {
            active_job_id_.clear();
        }
    }

    if (final_state == WorkflowJobState::COMPLETED ||
        final_state == WorkflowJobState::FAILED ||
        final_state == WorkflowJobState::CANCELLED) {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        auto plan_it = plans_.find(context->plan_id);
        if (plan_it != plans_.end() && plan_it->second.latest) {
            if (plan_it->second.preview_state == PlanPreviewState::CONFIRMED) {
                plan_it->second.preview_state = PlanPreviewState::SNAPSHOT_READY;
            }
            plan_it->second.confirmed_at.clear();
        }
    }
}

}  // namespace Siligen::Application::UseCases::Dispensing
