#include "application/services/dispensing/AuthorityPreviewAssemblyService.h"
#include "application/services/dispensing/ExecutionAssemblyService.h"

#include "domain/dispensing/planning/domain-services/AuthorityTriggerLayoutPlanner.h"
#include "domain/dispensing/planning/domain-services/CurveFlatteningService.h"

#include "domain/dispensing/domain-services/TriggerPlanner.h"
#include "domain/motion/CMPCoordinatedInterpolator.h"
#include "domain/motion/domain-services/TimeTrajectoryPlanner.h"
#include "domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h"
#include "process_path/contracts/GeometryUtils.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/types/CMPTypes.h"
#include "shared/types/TrajectoryTriggerUtils.h"
#include "shared/types/VisualizationTypes.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <limits>
#include <sstream>

namespace Siligen::Application::Services::Dispensing {

using Siligen::Application::Services::Dispensing::AuthorityPreviewBuildInput;
using Siligen::Application::Services::Dispensing::AuthorityPreviewBuildResult;
using Siligen::Application::Services::Dispensing::ExecutionAssemblyBuildInput;
using Siligen::Application::Services::Dispensing::ExecutionAssemblyBuildResult;
using Siligen::Domain::Dispensing::Contracts::ExecutionPackageBuilt;
using Siligen::Domain::Dispensing::DomainServices::AuthorityTriggerLayoutPlanner;
using Siligen::Domain::Dispensing::DomainServices::AuthorityTriggerLayoutPlannerRequest;
using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;
using Siligen::Domain::Dispensing::DomainServices::TriggerPlanner;
using Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout;
using Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayoutState;
using Siligen::Domain::Dispensing::ValueObjects::InterpolationTriggerBinding;
using Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerPoint;
using Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerSourceKind;
using Siligen::Domain::Dispensing::ValueObjects::SpacingValidationClassification;
using Siligen::Domain::Dispensing::ValueObjects::TriggerPlan;
using Siligen::Domain::Motion::CMPCoordinatedInterpolator;
using Siligen::Domain::Motion::InterpolationAlgorithm;
using Siligen::Domain::Motion::TrajectoryInterpolatorFactory;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint;
using Siligen::Domain::Motion::DomainServices::TimeTrajectoryPlanner;
using Siligen::MotionPlanning::Contracts::MotionPlan;
using Siligen::ProcessPath::Contracts::ArcPoint;
using Siligen::ProcessPath::Contracts::ComputeArcLength;
using Siligen::ProcessPath::Contracts::ComputeArcSweep;
using Siligen::ProcessPath::Contracts::Segment;
using Siligen::ProcessPath::Contracts::SegmentEnd;
using Siligen::ProcessPath::Contracts::SegmentStart;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::VisualizationTriggerPoint;

namespace {

constexpr float32 kEpsilon = 1e-6f;
constexpr float32 kGluePointDedupEpsilonMm = 1e-4f;

struct PlanningArtifactsAssemblyInput {
    ProcessPath process_path;
    ProcessPath authority_process_path;
    MotionPlan motion_plan;
    std::string source_path;
    std::string dxf_filename;
    float32 dispensing_velocity = 0.0f;
    float32 acceleration = 0.0f;
    uint32 dispenser_interval_ms = 0;
    uint32 dispenser_duration_ms = 0;
    float32 trigger_spatial_interval_mm = 0.0f;
    float32 valve_response_ms = 0.0f;
    float32 safety_margin_ms = 0.0f;
    float32 min_interval_ms = 0.0f;
    float32 max_jerk = 0.0f;
    float32 sample_dt = 0.01f;
    float32 sample_ds = 0.0f;
    float32 spline_max_step_mm = 0.0f;
    float32 spline_max_error_mm = 0.0f;
    float32 estimated_time_s = 0.0f;
    Siligen::Shared::Types::DispensingStrategy dispensing_strategy =
        Siligen::Shared::Types::DispensingStrategy::BASELINE;
    int subsegment_count = 8;
    bool dispense_only_cruise = false;
    bool downgrade_on_violation = true;
    bool use_interpolation_planner = false;
    InterpolationAlgorithm interpolation_algorithm = InterpolationAlgorithm::LINEAR;
    Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile compensation_profile{};
    float32 spacing_tol_ratio = 0.0f;
    float32 spacing_min_mm = 0.0f;
    float32 spacing_max_mm = 0.0f;
};

struct TriggerArtifacts {
    std::vector<float32> distances;
    std::vector<Point2D> positions;
    std::vector<AuthorityTriggerPoint> authority_trigger_points;
    std::vector<SpacingValidationGroup> spacing_validation_groups;
    AuthorityTriggerLayout authority_trigger_layout;
    uint32 interval_ms = 0;
    float32 interval_mm = 0.0f;
    bool downgraded = false;
    bool binding_ready = false;
    bool spacing_valid = false;
    bool has_short_segment_exceptions = false;
    std::string validation_classification;
    std::string exception_reason;
    std::string failure_reason;
};

struct ExecutionTrajectorySelection {
    std::vector<TrajectoryPoint> motion_trajectory_points;
    const std::vector<TrajectoryPoint>* execution_trajectory = nullptr;
    bool authority_shared = false;
};

struct TriggerBindingCandidate {
    bool found = false;
    std::size_t enabled_position = 0;
    std::size_t interpolation_index = 0;
    float32 distance_error_mm = std::numeric_limits<float32>::max();
    float32 match_error_mm = std::numeric_limits<float32>::max();
    bool monotonic = true;
};

struct TriggerBindingTraceRow {
    std::size_t trigger_index = 0;
    std::string trigger_id;
    std::string span_ref;
    std::size_t source_segment_index = 0;
    LayoutTriggerSourceKind source_kind = LayoutTriggerSourceKind::Generated;
    Point2D trigger_position;
    float32 authority_distance_mm = 0.0f;
    bool matched = false;
    std::size_t interpolation_index = 0;
    uint32 execution_sequence_id = 0;
    Point2D execution_position;
    float32 execution_trigger_position_mm = 0.0f;
    float32 distance_delta_mm = 0.0f;
    float32 position_delta_mm = 0.0f;
    bool monotonic = false;
    Siligen::SegmentType execution_segment_type = Siligen::SegmentType::LINEAR;
};

std::string FormatPoint(const Point2D& point) {
    std::ostringstream oss;
    oss << '(' << point.x << ',' << point.y << ')';
    return oss.str();
}

const char* ToString(LayoutTriggerSourceKind source_kind) {
    switch (source_kind) {
        case LayoutTriggerSourceKind::Anchor:
            return "anchor";
        case LayoutTriggerSourceKind::Generated:
            return "generated";
        case LayoutTriggerSourceKind::SharedVertex:
            return "shared_vertex";
    }
    return "generated";
}

const char* ToString(Siligen::SegmentType segment_type) {
    switch (segment_type) {
        case Siligen::SegmentType::LINEAR:
            return "linear";
        case Siligen::SegmentType::ARC_CW:
            return "arc_cw";
        case Siligen::SegmentType::ARC_CCW:
            return "arc_ccw";
    }
    return "linear";
}

template <typename ClockPoint>
long long ElapsedMs(const ClockPoint& start_time) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start_time)
        .count();
}

void LogBindingTraceWindow(const AuthorityTriggerLayout& layout,
                           const std::vector<TriggerBindingTraceRow>& rows,
                           std::size_t failure_index) {
    if (rows.empty()) {
        return;
    }

    const std::size_t window_start = 0;
    const std::size_t requested_end = std::max<std::size_t>(failure_index + 1, 30);
    const std::size_t window_end = std::min(rows.size(), requested_end);

    {
        std::ostringstream oss;
        oss << "preview_binding_trace_window"
            << " layout_id=" << layout.layout_id
            << " failed_trigger_index=" << failure_index
            << " row_start=" << window_start
            << " row_end=" << window_end
            << " available_rows=" << rows.size()
            << " total_triggers=" << layout.trigger_points.size();
        SILIGEN_LOG_INFO(oss.str());
    }

    for (std::size_t row_index = window_start; row_index < window_end; ++row_index) {
        const auto& row = rows[row_index];
        std::ostringstream oss;
        oss << "preview_binding_trace"
            << " trigger_index=" << row.trigger_index
            << " trigger_id=" << row.trigger_id
            << " span_ref=" << row.span_ref
            << " source_segment_index=" << row.source_segment_index
            << " source_kind=" << ToString(row.source_kind)
            << " trigger_position=" << FormatPoint(row.trigger_position)
            << " authority_distance_mm=" << row.authority_distance_mm
            << " matched=" << (row.matched ? 1 : 0);
        if (row.matched) {
            oss << " execution_index=" << row.interpolation_index
                << " execution_sequence_id=" << row.execution_sequence_id
                << " execution_position=" << FormatPoint(row.execution_position)
                << " execution_trigger_position_mm=" << row.execution_trigger_position_mm
                << " distance_delta_mm=" << row.distance_delta_mm
                << " position_delta_mm=" << row.position_delta_mm
                << " monotonic=" << (row.monotonic ? 1 : 0)
                << " execution_segment_type=" << ToString(row.execution_segment_type);
        }
        SILIGEN_LOG_INFO(oss.str());
    }
}

bool ApplyTriggerMarkersByPositionWithDiagnostics(std::vector<TrajectoryPoint>& points,
                                                  const std::vector<Point2D>& trigger_positions,
                                                  const std::vector<float32>& trigger_distances_mm,
                                                  float32 dedup_tolerance_mm,
                                                  float32 position_tolerance_mm,
                                                  uint16 pulse_width_us =
                                                      Siligen::Shared::Types::kDefaultTriggerPulseWidthUs) {
    if (!trigger_distances_mm.empty() && trigger_positions.size() != trigger_distances_mm.size()) {
        SILIGEN_LOG_WARNING("build_interp_marker_stage=invalid_input trigger_positions and trigger_distances size mismatch");
        return false;
    }

    const auto started_at = std::chrono::steady_clock::now();
    {
        std::ostringstream oss;
        oss << "build_interp_marker_stage=start"
            << " trajectory_points=" << points.size()
            << " trigger_positions=" << trigger_positions.size()
            << " strategy=batch_distance_guided"
            << " dedup_tolerance_mm=" << dedup_tolerance_mm
            << " position_tolerance_mm=" << position_tolerance_mm;
        SILIGEN_LOG_INFO(oss.str());
    }

    if (!Siligen::Shared::Types::ApplyTriggerMarkersByPosition(
            points,
            trigger_positions,
            trigger_distances_mm,
            dedup_tolerance_mm,
            position_tolerance_mm,
            pulse_width_us)) {
        std::ostringstream oss;
        oss << "build_interp_marker_stage=failed"
            << " trigger_positions=" << trigger_positions.size()
            << " final_points=" << points.size()
            << " elapsed_ms=" << ElapsedMs(started_at);
        SILIGEN_LOG_WARNING(oss.str());
        return false;
    }

    {
        std::ostringstream oss;
        oss << "build_interp_marker_stage=complete"
            << " inserted_markers=" << Siligen::Shared::Types::CountTriggerMarkers(points)
            << " final_points=" << points.size()
            << " elapsed_ms=" << ElapsedMs(started_at);
        SILIGEN_LOG_INFO(oss.str());
    }
    return true;
}

bool IsBetterTriggerBindingCandidate(
    const TriggerBindingCandidate& candidate,
    const TriggerBindingCandidate& current) {
    if (!candidate.found) {
        return false;
    }
    if (!current.found) {
        return true;
    }

    if (candidate.distance_error_mm + Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm <
        current.distance_error_mm) {
        return true;
    }
    if (std::fabs(candidate.distance_error_mm - current.distance_error_mm) >
        Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm) {
        return false;
    }

    if (candidate.match_error_mm + Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm <
        current.match_error_mm) {
        return true;
    }
    if (std::fabs(candidate.match_error_mm - current.match_error_mm) >
        Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm) {
        return false;
    }

    if (candidate.monotonic != current.monotonic) {
        return candidate.monotonic;
    }

    return candidate.interpolation_index < current.interpolation_index;
}

float32 SegmentLength(const Segment& segment) {
    if (segment.length > kEpsilon) {
        return segment.length;
    }

    switch (segment.type) {
        case SegmentType::Line:
            return segment.line.start.DistanceTo(segment.line.end);
        case SegmentType::Arc:
            return ComputeArcLength(segment.arc);
        case SegmentType::Spline: {
            if (segment.spline.control_points.size() < 2) {
                return 0.0f;
            }
            float32 total = 0.0f;
            for (size_t i = 1; i < segment.spline.control_points.size(); ++i) {
                total += segment.spline.control_points[i - 1].DistanceTo(segment.spline.control_points[i]);
            }
            return total;
        }
        default:
            return 0.0f;
    }
}

float32 ComputeProcessPathLength(const ProcessPath& process_path) {
    float32 total_length = 0.0f;
    for (const auto& segment : process_path.segments) {
        total_length += SegmentLength(segment.geometry);
    }
    return total_length;
}

bool PointsNear(const Point2D& lhs, const Point2D& rhs, float32 epsilon_mm) {
    return lhs.DistanceTo(rhs) <= epsilon_mm;
}

bool ContainsNearPoint(const std::vector<Point2D>& points, const Point2D& point, float32 epsilon_mm) {
    for (const auto& existing : points) {
        if (PointsNear(existing, point, epsilon_mm)) {
            return true;
        }
    }
    return false;
}

float32 ResolveSpacingMin(const PlanningArtifactsAssemblyInput& input, float32 target_spacing) {
    if (input.spacing_min_mm > 0.0f) {
        return input.spacing_min_mm;
    }
    (void)target_spacing;
    return 2.7f;
}

float32 ResolveSpacingMax(const PlanningArtifactsAssemblyInput& input, float32 target_spacing) {
    if (input.spacing_max_mm > 0.0f) {
        return input.spacing_max_mm;
    }
    (void)target_spacing;
    return 3.3f;
}

bool ResolveSplinePosition(const Segment& segment, float32 local_distance, Point2D& out) {
    if (segment.spline.control_points.empty()) {
        out = SegmentEnd(segment);
        return true;
    }
    if (segment.spline.control_points.size() == 1) {
        out = segment.spline.control_points.front();
        return true;
    }

    const float32 total_length = SegmentLength(segment);
    if (total_length <= kEpsilon) {
        out = segment.spline.control_points.back();
        return true;
    }

    float32 accumulated = 0.0f;
    for (size_t index = 1; index < segment.spline.control_points.size(); ++index) {
        const auto& start = segment.spline.control_points[index - 1];
        const auto& end = segment.spline.control_points[index];
        const float32 edge_length = start.DistanceTo(end);
        if (edge_length <= kEpsilon) {
            continue;
        }
        if (accumulated + edge_length >= local_distance - kEpsilon) {
            const float32 ratio = std::clamp((local_distance - accumulated) / edge_length, 0.0f, 1.0f);
            out = start + (end - start) * ratio;
            return true;
        }
        accumulated += edge_length;
    }

    out = segment.spline.control_points.back();
    return true;
}

bool ResolveSegmentPosition(const Segment& segment, float32 local_distance, Point2D& out) {
    const float32 segment_length = SegmentLength(segment);
    if (segment_length <= kEpsilon) {
        out = SegmentEnd(segment);
        return true;
    }

    const float32 ratio = std::clamp(local_distance / segment_length, 0.0f, 1.0f);
    switch (segment.type) {
        case SegmentType::Line:
            out = segment.line.start + (segment.line.end - segment.line.start) * ratio;
            return true;
        case SegmentType::Arc: {
            const float32 sweep = ComputeArcSweep(segment.arc.start_angle_deg,
                                                  segment.arc.end_angle_deg,
                                                  segment.arc.clockwise);
            const float32 direction = segment.arc.clockwise ? -1.0f : 1.0f;
            const float32 angle = segment.arc.start_angle_deg + direction * sweep * ratio;
            out = ArcPoint(segment.arc, angle);
            return true;
        }
        case SegmentType::Spline:
            return ResolveSplinePosition(segment, local_distance, out);
        default:
            out = SegmentEnd(segment);
            return true;
    }
}

void AppendAuthorityPoint(TriggerArtifacts& artifacts, AuthorityTriggerPoint point) {
    if (!artifacts.authority_trigger_points.empty()) {
        auto& previous = artifacts.authority_trigger_points.back();
        if (PointsNear(previous.position, point.position, kGluePointDedupEpsilonMm) &&
            std::fabs(previous.trigger_distance_mm - point.trigger_distance_mm) <= kEpsilon) {
            previous.shared_vertex = true;
            return;
        }
        if (PointsNear(previous.position, point.position, kGluePointDedupEpsilonMm)) {
            previous.shared_vertex = true;
            point.shared_vertex = true;
        }
    }

    artifacts.distances.push_back(point.trigger_distance_mm);
    artifacts.positions.push_back(point.position);
    artifacts.authority_trigger_points.push_back(std::move(point));
}

Result<void> AppendAnchoredAuthorityForSegment(
    TriggerArtifacts& artifacts,
    const Segment& segment,
    std::size_t segment_index,
    float32 path_offset_mm,
    float32 target_spacing_mm,
    float32 spacing_min_mm,
    float32 spacing_max_mm) {
    const float32 segment_length = SegmentLength(segment);
    if (segment_length <= kEpsilon) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "dispense_on 几何线段长度为0", "DispensePackagingAssembly"));
    }
    if (target_spacing_mm <= kEpsilon) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "trigger_spatial_interval_mm 无效", "DispensePackagingAssembly"));
    }

    const int division_count = std::max(1, static_cast<int>(std::lround(segment_length / target_spacing_mm)));
    const float32 actual_spacing_mm = segment_length / static_cast<float32>(division_count);

    SpacingValidationGroup group;
    group.segment_index = segment_index;
    group.actual_spacing_mm = actual_spacing_mm;
    group.short_segment_exception =
        actual_spacing_mm < spacing_min_mm - kEpsilon || actual_spacing_mm > spacing_max_mm + kEpsilon;
    group.within_window =
        actual_spacing_mm >= spacing_min_mm - kEpsilon && actual_spacing_mm <= spacing_max_mm + kEpsilon;

    group.points.reserve(static_cast<std::size_t>(division_count) + 1U);
    for (int division = 0; division <= division_count; ++division) {
        const float32 local_distance =
            (division == division_count) ? segment_length : actual_spacing_mm * static_cast<float32>(division);
        Point2D point;
        if (!ResolveSegmentPosition(segment, local_distance, point)) {
            return Result<void>::Failure(
                Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "authority trigger 位置解析失败", "DispensePackagingAssembly"));
        }
        group.points.push_back(point);

        AuthorityTriggerPoint authority_point;
        authority_point.position = point;
        authority_point.trigger_distance_mm = path_offset_mm + local_distance;
        authority_point.segment_index = segment_index;
        authority_point.short_segment_exception = group.short_segment_exception;
        AppendAuthorityPoint(artifacts, std::move(authority_point));
    }

    artifacts.spacing_validation_groups.push_back(std::move(group));
    return Result<void>::Success();
}

std::vector<VisualizationTriggerPoint> BuildPlannedTriggerPoints(
    const std::vector<TrajectoryPoint>& interpolation_points) {
    std::vector<VisualizationTriggerPoint> triggers;
    if (interpolation_points.empty()) {
        return triggers;
    }

    std::vector<Point2D> preview_positions;
    const float32 nan_value = std::numeric_limits<float32>::quiet_NaN();
    for (const auto& point : interpolation_points) {
        if (!point.enable_position_trigger) {
            continue;
        }
        if (ContainsNearPoint(preview_positions, point.position, kGluePointDedupEpsilonMm)) {
            continue;
        }
        preview_positions.push_back(point.position);
        triggers.emplace_back(point.position, nan_value);
    }
    return triggers;
}

std::vector<TrajectoryPoint> BuildTrajectoryFromMotion(const MotionTrajectory& trajectory) {
    std::vector<TrajectoryPoint> base;
    if (trajectory.points.empty()) {
        return base;
    }

    base.reserve(trajectory.points.size());
    for (const auto& pt : trajectory.points) {
        TrajectoryPoint out;
        out.position = Siligen::Shared::Types::Point2D(pt.position);
        out.velocity = std::sqrt(pt.velocity.x * pt.velocity.x + pt.velocity.y * pt.velocity.y);
        out.timestamp = pt.t;
        // MotionTrajectory::dispense_on only means "glue is enabled on this path region".
        // Preview glue points must be reconstructed from trigger_distances_mm instead.
        out.enable_position_trigger = false;
        base.push_back(out);
    }
    return base;
}

ExecutionTrajectorySelection SelectExecutionTrajectory(
    const ExecutionPackageValidated& execution_package) {
    ExecutionTrajectorySelection selection;
    selection.motion_trajectory_points = BuildTrajectoryFromMotion(execution_package.execution_plan.motion_trajectory);
    if (!execution_package.execution_plan.interpolation_points.empty()) {
        selection.execution_trajectory = &execution_package.execution_plan.interpolation_points;
        selection.authority_shared = true;
        return selection;
    }
    return selection;
}

std::vector<Siligen::Shared::Types::Point2D> CollectTriggerPositions(
    const std::vector<VisualizationTriggerPoint>& preview_triggers) {
    std::vector<Siligen::Shared::Types::Point2D> final_glue_points;
    final_glue_points.reserve(preview_triggers.size());
    for (const auto& trigger : preview_triggers) {
        final_glue_points.push_back(trigger.position);
    }
    return final_glue_points;
}

const ProcessPath& ResolveAuthorityProcessPath(const PlanningArtifactsAssemblyInput& input) {
    if (!input.authority_process_path.segments.empty()) {
        return input.authority_process_path;
    }
    return input.process_path;
}

const ProcessPath& ResolveAuthorityProcessPath(const AuthorityPreviewBuildInput& input) {
    if (!input.authority_process_path.segments.empty()) {
        return input.authority_process_path;
    }
    return input.process_path;
}

bool ValidateGlueSpacing(
    const PlanningArtifactsAssemblyInput& input,
    TriggerArtifacts& artifacts,
    float32 target_spacing_mm) {
    if (artifacts.spacing_validation_groups.empty()) {
        artifacts.spacing_valid = false;
        if (artifacts.failure_reason.empty()) {
            artifacts.failure_reason = "authority spacing groups unavailable";
        }
        return false;
    }

    const float32 min_allowed = ResolveSpacingMin(input, target_spacing_mm);
    const float32 max_allowed = ResolveSpacingMax(input, target_spacing_mm);

    bool valid = true;
    std::size_t invalid_group_count = 0;
    std::size_t short_exception_count = 0;
    for (const auto& group : artifacts.spacing_validation_groups) {
        if (group.short_segment_exception) {
            ++short_exception_count;
            continue;
        }
        if (!group.within_window) {
            valid = false;
            ++invalid_group_count;
        }
    }

    artifacts.has_short_segment_exceptions = short_exception_count > 0;
    artifacts.spacing_valid = valid;
    if (!valid && artifacts.failure_reason.empty()) {
        std::ostringstream oss;
        oss << "spacing validation failed: invalid_groups=" << invalid_group_count
            << ", exceptions=" << short_exception_count
            << ", target=" << target_spacing_mm
            << ", window=[" << min_allowed << ',' << max_allowed << ']';
        artifacts.failure_reason = oss.str();
        SILIGEN_LOG_WARNING(artifacts.failure_reason);
    }
    return valid;
}

float32 EstimateExecutionTime(
    const PlanningArtifactsAssemblyInput& input,
    const ExecutionPackageBuilt& built) {
    if (input.estimated_time_s > kEpsilon) {
        return input.estimated_time_s;
    }
    if (built.estimated_time_s > kEpsilon) {
        return built.estimated_time_s;
    }
    if (input.motion_plan.total_time > kEpsilon) {
        return input.motion_plan.total_time;
    }
    if (built.total_length_mm > kEpsilon && input.dispensing_velocity > kEpsilon) {
        return built.total_length_mm / input.dispensing_velocity;
    }
    return 0.0f;
}

float32 ResolveInterpolationStep(const PlanningArtifactsAssemblyInput& input) {
    if (input.sample_ds > kEpsilon) {
        return input.sample_ds;
    }
    if (input.spline_max_step_mm > kEpsilon) {
        return input.spline_max_step_mm;
    }
    return 1.0f;
}

float32 SpeedMagnitude(const MotionTrajectoryPoint& point) {
    const float32 vx = point.velocity.x;
    const float32 vy = point.velocity.y;
    return std::sqrt(vx * vx + vy * vy);
}

Siligen::Domain::Motion::ValueObjects::TimePlanningConfig BuildInterpolationPlanningConfig(
    const Siligen::InterpolationConfig& config) {
    Siligen::Domain::Motion::ValueObjects::TimePlanningConfig motion_config;
    motion_config.vmax = config.max_velocity;
    motion_config.amax = config.max_acceleration;
    motion_config.jmax = config.max_jerk;
    motion_config.sample_dt = config.time_step;
    motion_config.sample_ds = 0.0f;
    motion_config.curvature_speed_factor = config.curvature_speed_factor;
    motion_config.corner_speed_factor = 1.0f;
    motion_config.start_speed_factor = 1.0f;
    motion_config.end_speed_factor = 1.0f;
    motion_config.rapid_speed_factor = 1.0f;
    motion_config.arc_tolerance_mm = std::max(config.position_tolerance, 0.0f);
    motion_config.enforce_jerk_limit = true;
    return motion_config;
}

std::vector<TrajectoryPoint> ConvertMotionTrajectoryToTrajectoryPoints(const MotionTrajectory& trajectory) {
    std::vector<TrajectoryPoint> points;
    points.reserve(trajectory.points.size());
    for (size_t index = 0; index < trajectory.points.size(); ++index) {
        const auto& sample = trajectory.points[index];
        TrajectoryPoint point;
        point.position = Point2D(sample.position.x, sample.position.y);
        point.timestamp = sample.t;
        point.sequence_id = static_cast<uint32>(index);
        point.velocity = SpeedMagnitude(sample);
        points.push_back(point);
    }
    return points;
}

void AppendUniquePoint(std::vector<Point2D>& points, const Point2D& point) {
    if (points.empty() || points.back().DistanceTo(point) > kEpsilon) {
        points.push_back(point);
    }
}

void SampleLinePoints(
    const Point2D& start,
    const Point2D& end,
    float32 step,
    std::vector<Point2D>& points) {
    const float32 length = start.DistanceTo(end);
    if (length <= kEpsilon) {
        return;
    }

    if (step <= kEpsilon || length <= step) {
        AppendUniquePoint(points, start);
        AppendUniquePoint(points, end);
        return;
    }

    int slices = static_cast<int>(std::ceil(length / step));
    slices = std::max(1, slices);
    for (int i = 0; i <= slices; ++i) {
        const float32 ratio = static_cast<float32>(i) / static_cast<float32>(slices);
        AppendUniquePoint(points, start + (end - start) * ratio);
    }
}

void SampleArcPoints(
    const Siligen::ProcessPath::Contracts::ArcPrimitive& arc,
    float32 step,
    std::vector<Point2D>& points) {
    const float32 length = ComputeArcLength(arc);
    if (length <= kEpsilon) {
        return;
    }

    float32 sweep = ComputeArcSweep(arc.start_angle_deg, arc.end_angle_deg, arc.clockwise);
    int slices = 1;
    if (step > kEpsilon) {
        slices = static_cast<int>(std::ceil(length / step));
        slices = std::max(1, slices);
    }

    const float32 direction = arc.clockwise ? -1.0f : 1.0f;
    for (int i = 0; i <= slices; ++i) {
        const float32 ratio = static_cast<float32>(i) / static_cast<float32>(slices);
        const float32 angle = arc.start_angle_deg + direction * sweep * ratio;
        AppendUniquePoint(points, ArcPoint(arc, angle));
    }
}

Result<std::vector<Point2D>> BuildInterpolationSeedPoints(
    const ProcessPath& path,
    float32 spline_max_error_mm,
    float32 step_mm) {
    std::vector<Point2D> points;
    Siligen::Domain::Dispensing::DomainServices::CurveFlatteningService flattening_service;
    const float32 sample_step_mm = step_mm > kEpsilon ? step_mm : 1.0f;
    const auto started_at = std::chrono::steady_clock::now();
    const std::size_t total_segments = path.segments.size();
    for (std::size_t segment_index = 0; segment_index < total_segments; ++segment_index) {
        const auto& segment = path.segments[segment_index];
        const auto& geometry = segment.geometry;
        if (geometry.is_point) {
            AppendUniquePoint(points, geometry.line.start);
        } else {
            auto flattened_result = flattening_service.Flatten(geometry, spline_max_error_mm, step_mm);
            if (flattened_result.IsError()) {
                return Result<std::vector<Point2D>>::Failure(flattened_result.GetError());
            }
            const auto& flattened_points = flattened_result.Value().points;
            if (!flattened_points.empty()) {
                AppendUniquePoint(points, flattened_points.front());
                for (std::size_t index = 1; index < flattened_points.size(); ++index) {
                    SampleLinePoints(flattened_points[index - 1], flattened_points[index], sample_step_mm, points);
                }
            }
        }

        if (total_segments >= 200 &&
            (((segment_index + 1) % 100U) == 0U || (segment_index + 1) == total_segments)) {
            std::ostringstream oss;
            oss << "build_interp_seed_stage=progress"
                << " processed_segments=" << (segment_index + 1)
                << " total_segments=" << total_segments
                << " sampled_points=" << points.size()
                << " elapsed_ms=" << ElapsedMs(started_at);
            SILIGEN_LOG_INFO(oss.str());
        }
    }
    return Result<std::vector<Point2D>>::Success(std::move(points));
}

std::vector<TrajectoryPoint> ConvertPreviewPointsToTrajectory(
    const std::vector<Point2D>& preview_points) {
    std::vector<TrajectoryPoint> points;
    points.reserve(preview_points.size());
    for (std::size_t index = 0; index < preview_points.size(); ++index) {
        TrajectoryPoint point;
        point.position = preview_points[index];
        point.sequence_id = static_cast<uint32>(index);
        points.push_back(point);
    }
    return points;
}

float32 EstimatePreviewTime(
    const AuthorityPreviewBuildInput& input,
    float32 total_length_mm) {
    if (input.dispensing_velocity <= kEpsilon || total_length_mm <= kEpsilon) {
        return 0.0f;
    }
    return total_length_mm / input.dispensing_velocity;
}

std::vector<AuthorityTriggerPoint> ConvertAuthorityTriggerPoints(const AuthorityTriggerLayout& layout) {
    std::vector<AuthorityTriggerPoint> points;
    points.reserve(layout.trigger_points.size());
    for (const auto& trigger : layout.trigger_points) {
        AuthorityTriggerPoint point;
        point.position = trigger.position;
        point.trigger_distance_mm = trigger.distance_mm_global;
        point.segment_index = trigger.source_segment_index;
        point.short_segment_exception = false;
        point.shared_vertex = trigger.shared_vertex;
        points.push_back(point);
    }
    return points;
}

std::vector<SpacingValidationGroup> ConvertSpacingValidationGroups(const AuthorityTriggerLayout& layout) {
    std::vector<SpacingValidationGroup> groups;
    groups.reserve(layout.validation_outcomes.size());
    for (const auto& outcome : layout.validation_outcomes) {
        SpacingValidationGroup group;
        auto span_it = std::find_if(
            layout.spans.begin(),
            layout.spans.end(),
            [&](const auto& span) { return span.span_id == outcome.span_ref; });
        if (span_it != layout.spans.end() && !span_it->source_segment_indices.empty()) {
            group.segment_index = span_it->source_segment_indices.front();
            group.actual_spacing_mm = span_it->actual_spacing_mm;
        }
        group.points = outcome.points;
        group.short_segment_exception =
            outcome.classification == SpacingValidationClassification::PassWithException;
        group.within_window = outcome.classification == SpacingValidationClassification::Pass;
        groups.push_back(std::move(group));
    }
    return groups;
}

std::string ResolveValidationClassification(const AuthorityTriggerLayout& layout) {
    if (layout.validation_outcomes.empty()) {
        return layout.authority_ready ? "pass" : "fail";
    }
    bool has_exception = false;
    for (const auto& outcome : layout.validation_outcomes) {
        if (outcome.classification == SpacingValidationClassification::Fail) {
            return "fail";
        }
        if (outcome.classification == SpacingValidationClassification::PassWithException) {
            has_exception = true;
        }
    }
    return has_exception ? "pass_with_exception" : "pass";
}

std::string ResolveExceptionReason(const AuthorityTriggerLayout& layout) {
    for (const auto& outcome : layout.validation_outcomes) {
        if (!outcome.exception_reason.empty()) {
            return outcome.exception_reason;
        }
    }
    return {};
}

std::string ResolveBlockingReason(const AuthorityTriggerLayout& layout) {
    for (const auto& outcome : layout.validation_outcomes) {
        if (!outcome.blocking_reason.empty()) {
            return outcome.blocking_reason;
        }
    }
    return {};
}

std::vector<Point2D> CollectAuthorityPositions(const AuthorityTriggerLayout& layout) {
    std::vector<Point2D> positions;
    positions.reserve(layout.trigger_points.size());
    for (const auto& trigger : layout.trigger_points) {
        positions.push_back(trigger.position);
    }
    return positions;
}

void LogAuthoritySpanDiagnostics(const AuthorityTriggerLayout& layout) {
    {
        std::ostringstream oss;
        oss << "authority_layout_diagnostic"
            << " layout_id=" << layout.layout_id
            << " dispatch_type=" << Siligen::Domain::Dispensing::ValueObjects::ToString(layout.dispatch_type)
            << " effective_components=" << layout.effective_component_count
            << " ignored_components=" << layout.ignored_component_count
            << " total_components=" << layout.components.size()
            << " spans=" << layout.spans.size();
        SILIGEN_LOG_INFO(oss.str());
    }
    for (const auto& component : layout.components) {
        std::ostringstream oss;
        oss << "authority_component_diagnostic"
            << " layout_id=" << layout.layout_id
            << " component_id=" << component.component_id
            << " component_index=" << component.component_index
            << " dispatch_type=" << Siligen::Domain::Dispensing::ValueObjects::ToString(component.dispatch_type)
            << " ignored=" << (component.ignored ? 1 : 0)
            << " ignored_reason="
            << (component.ignored_reason.empty() ? "none" : component.ignored_reason)
            << " source_segments=" << component.source_segment_indices.size()
            << " span_refs=" << component.span_refs.size();
        SILIGEN_LOG_INFO(oss.str());
    }
    for (const auto& span : layout.spans) {
        const auto outcome_it = std::find_if(
            layout.validation_outcomes.begin(),
            layout.validation_outcomes.end(),
            [&](const auto& outcome) { return outcome.span_ref == span.span_id; });
        std::ostringstream oss;
        oss << "authority_span_diagnostic"
            << " layout_id=" << layout.layout_id
            << " layout_dispatch_type=" << Siligen::Domain::Dispensing::ValueObjects::ToString(layout.dispatch_type)
            << " component_id=" << span.component_id
            << " component_index=" << span.component_index
            << " component_dispatch_type="
            << Siligen::Domain::Dispensing::ValueObjects::ToString(span.dispatch_type)
            << " span_id=" << span.span_id
            << " order_index=" << span.order_index
            << " topology_type=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.topology_type)
            << " split_reason=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.split_reason)
            << " anchor_policy=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.anchor_policy)
            << " phase_strategy=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.phase_strategy)
            << " curve_mode=" << static_cast<int>(span.curve_mode)
            << " closed=" << (span.closed ? 1 : 0)
            << " source_segments=" << span.source_segment_indices.size()
            << " strong_anchors=" << span.strong_anchors.size()
            << " candidate_corners=" << span.candidate_corner_count
            << " accepted_corners=" << span.accepted_corner_count
            << " suppressed_corners=" << span.suppressed_corner_count
            << " dense_corner_clusters=" << span.dense_corner_cluster_count
            << " anchor_constraints_satisfied=" << (span.anchor_constraints_satisfied ? 1 : 0)
            << " validation_state=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.validation_state)
            << " anchor_constraint_state="
            << (outcome_it != layout.validation_outcomes.end() && !outcome_it->anchor_constraint_state.empty()
                    ? outcome_it->anchor_constraint_state
                    : "not_applicable")
            << " total_length_mm=" << span.total_length_mm
            << " actual_spacing_mm=" << span.actual_spacing_mm
            << " phase_mm=" << span.phase_mm;
        if (!span.strong_anchors.empty()) {
            oss << " anchor_roles=";
            for (std::size_t index = 0; index < span.strong_anchors.size(); ++index) {
                if (index > 0) {
                    oss << ',';
                }
                oss << Siligen::Domain::Dispensing::ValueObjects::ToString(span.strong_anchors[index].role);
            }
        }
        SILIGEN_LOG_INFO(oss.str());
    }
}

TriggerArtifacts BuildTriggerArtifactsFromAuthorityPreview(
    const AuthorityPreviewBuildResult& authority_preview) {
    TriggerArtifacts artifacts;
    artifacts.authority_trigger_layout = authority_preview.authority_trigger_layout;
    artifacts.authority_trigger_points = authority_preview.authority_trigger_points;
    artifacts.spacing_validation_groups = authority_preview.spacing_validation_groups;
    artifacts.positions = authority_preview.glue_points;
    artifacts.validation_classification = authority_preview.preview_validation_classification;
    artifacts.exception_reason = authority_preview.preview_exception_reason;
    artifacts.failure_reason = authority_preview.preview_failure_reason;
    artifacts.spacing_valid = authority_preview.preview_spacing_valid;
    artifacts.has_short_segment_exceptions = authority_preview.preview_has_short_segment_exceptions;
    artifacts.binding_ready = authority_preview.preview_binding_ready;
    artifacts.authority_trigger_layout.binding_ready = authority_preview.preview_binding_ready;
    for (const auto& trigger : authority_preview.authority_trigger_points) {
        artifacts.distances.push_back(trigger.trigger_distance_mm);
    }
    return artifacts;
}

Result<TriggerArtifacts> BuildTriggerArtifacts(
    const ProcessPath& path,
    const PlanningArtifactsAssemblyInput& input) {
    TriggerArtifacts artifacts;
    if (path.segments.empty()) {
        artifacts.failure_reason = "process path is empty";
        return Result<TriggerArtifacts>::Success(std::move(artifacts));
    }

    AuthorityTriggerLayoutPlanner planner;
    AuthorityTriggerLayoutPlannerRequest request;
    request.process_path = path;
    request.plan_id = input.source_path;
    request.plan_fingerprint = input.dxf_filename;
    request.layout_id_seed = input.source_path + "|" + input.dxf_filename + "|" + std::to_string(path.segments.size());
    request.target_spacing_mm = input.trigger_spatial_interval_mm;
    request.min_spacing_mm = ResolveSpacingMin(input, input.trigger_spatial_interval_mm);
    request.max_spacing_mm = ResolveSpacingMax(input, input.trigger_spatial_interval_mm);
    request.dispensing_velocity = input.dispensing_velocity;
    request.acceleration = input.acceleration;
    request.dispenser_interval_ms = input.dispenser_interval_ms;
    request.dispenser_duration_ms = input.dispenser_duration_ms;
    request.valve_response_ms = input.valve_response_ms;
    request.safety_margin_ms = input.safety_margin_ms;
    request.min_interval_ms = input.min_interval_ms;
    request.downgrade_on_violation = input.downgrade_on_violation;
    request.dispensing_strategy = input.dispensing_strategy;
    request.subsegment_count = input.subsegment_count;
    request.dispense_only_cruise = input.dispense_only_cruise;
    request.enable_branch_revisit_split = true;
    request.emit_topology_diagnostics = true;
    request.compensation_profile = input.compensation_profile;
    request.spline_max_error_mm = input.spline_max_error_mm;
    request.spline_max_step_mm = input.spline_max_step_mm;

    auto layout_result = planner.Plan(request);
    if (layout_result.IsError()) {
        return Result<TriggerArtifacts>::Failure(layout_result.GetError());
    }

    artifacts.authority_trigger_layout = layout_result.Value();
    artifacts.authority_trigger_points = ConvertAuthorityTriggerPoints(artifacts.authority_trigger_layout);
    artifacts.spacing_validation_groups = ConvertSpacingValidationGroups(artifacts.authority_trigger_layout);
    artifacts.positions = CollectAuthorityPositions(artifacts.authority_trigger_layout);
    artifacts.validation_classification = ResolveValidationClassification(artifacts.authority_trigger_layout);
    artifacts.exception_reason = ResolveExceptionReason(artifacts.authority_trigger_layout);
    artifacts.failure_reason = ResolveBlockingReason(artifacts.authority_trigger_layout);
    LogAuthoritySpanDiagnostics(artifacts.authority_trigger_layout);

    for (const auto& trigger : artifacts.authority_trigger_points) {
        artifacts.distances.push_back(trigger.trigger_distance_mm);
    }

    TriggerPlan trigger_plan;
    trigger_plan.strategy = input.dispensing_strategy;
    trigger_plan.interval_mm = input.trigger_spatial_interval_mm;
    trigger_plan.subsegment_count = input.subsegment_count;
    trigger_plan.dispense_only_cruise = input.dispense_only_cruise;
    trigger_plan.safety.duration_ms = static_cast<int32>(input.dispenser_duration_ms);
    trigger_plan.safety.valve_response_ms = static_cast<int32>(input.valve_response_ms);
    trigger_plan.safety.margin_ms = static_cast<int32>(input.safety_margin_ms);
    trigger_plan.safety.min_interval_ms = static_cast<int32>(input.min_interval_ms);
    trigger_plan.safety.downgrade_on_violation = input.downgrade_on_violation;

    TriggerPlanner trigger_planner;
    for (const auto& span : artifacts.authority_trigger_layout.spans) {
        auto timing_result = trigger_planner.Plan(
            span.total_length_mm,
            input.dispensing_velocity,
            input.acceleration,
            input.trigger_spatial_interval_mm,
            input.dispenser_interval_ms,
            0.0f,
            trigger_plan,
            input.compensation_profile);
        if (timing_result.IsError()) {
            return Result<TriggerArtifacts>::Failure(timing_result.GetError());
        }
        const auto& trigger_result = timing_result.Value();
        artifacts.interval_ms = std::max<uint32>(artifacts.interval_ms, trigger_result.interval_ms);
        artifacts.interval_mm = std::max(artifacts.interval_mm, trigger_result.plan.interval_mm);
        artifacts.downgraded = artifacts.downgraded || trigger_result.downgrade_applied;
    }

    if (artifacts.interval_mm <= kEpsilon) {
        artifacts.interval_mm = input.trigger_spatial_interval_mm;
    }
    if (artifacts.interval_ms == 0) {
        artifacts.interval_ms = input.dispenser_interval_ms;
    }
    if (artifacts.authority_trigger_points.empty() && artifacts.failure_reason.empty()) {
        artifacts.failure_reason = "explicit trigger authority unavailable";
    }
    ValidateGlueSpacing(input, artifacts, artifacts.interval_mm);
    return Result<TriggerArtifacts>::Success(std::move(artifacts));
}

Result<std::vector<TrajectoryPoint>> BuildInterpolationPoints(
    const PlanningArtifactsAssemblyInput& input,
    const ProcessPath& path,
    const TriggerArtifacts& trigger_artifacts) {
    auto log_stage = [&](const char* stage, const std::string& detail = std::string()) {
        std::ostringstream oss;
        oss << "build_interp_stage=" << stage
            << " dxf=" << input.dxf_filename
            << " algorithm=" << static_cast<int>(input.interpolation_algorithm)
            << " process_segments=" << path.segments.size()
            << " authority_triggers=" << trigger_artifacts.positions.size();
        if (!detail.empty()) {
            oss << ' ' << detail;
        }
        SILIGEN_LOG_INFO(oss.str());
    };
    const auto started_at = std::chrono::steady_clock::now();
    log_stage("enter");

    if (!input.use_interpolation_planner) {
        log_stage("skip_interpolation_planner_disabled");
        return Result<std::vector<TrajectoryPoint>>::Success({});
    }

    if (trigger_artifacts.validation_classification == "fail") {
        log_stage("skip_validation_fail");
        return Result<std::vector<TrajectoryPoint>>::Success({});
    }

    const auto seed_started_at = std::chrono::steady_clock::now();
    log_stage("seed_start");
    const auto seed_points_result = BuildInterpolationSeedPoints(
        path,
        input.spline_max_error_mm,
        ResolveInterpolationStep(input));
    if (seed_points_result.IsError()) {
        log_stage("seed_failed", "elapsed_ms=" + std::to_string(ElapsedMs(seed_started_at)) +
                                     " reason=" + seed_points_result.GetError().GetMessage());
        return Result<std::vector<TrajectoryPoint>>::Failure(seed_points_result.GetError());
    }
    const auto& seed_points = seed_points_result.Value();
    log_stage("seed_done",
              "seed_points=" + std::to_string(seed_points.size()) +
                  " elapsed_ms=" + std::to_string(ElapsedMs(seed_started_at)));
    if (seed_points.size() < 2) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensePackagingAssembly"));
    }

    Siligen::InterpolationConfig config{};
    config.max_velocity = input.dispensing_velocity;
    config.max_acceleration = input.acceleration;
    config.max_jerk = input.max_jerk;
    config.time_step = (input.sample_dt > kEpsilon) ? input.sample_dt : 0.001f;
    if (input.spline_max_error_mm > kEpsilon) {
        config.position_tolerance = input.spline_max_error_mm;
    }
    if (input.compensation_profile.curvature_speed_factor > kEpsilon) {
        config.curvature_speed_factor = input.compensation_profile.curvature_speed_factor;
    }
    if (input.trigger_spatial_interval_mm > kEpsilon) {
        config.trigger_spacing_mm = input.trigger_spatial_interval_mm;
    }

    if (config.max_velocity <= 0.0f ||
        config.max_acceleration <= 0.0f ||
        config.time_step <= 0.0f ||
        config.trigger_spacing_mm < 0.0f) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensePackagingAssembly"));
    }

    if ((input.interpolation_algorithm == InterpolationAlgorithm::LINEAR ||
         input.interpolation_algorithm == InterpolationAlgorithm::CMP_COORDINATED) &&
        config.max_jerk <= 0.0f) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensePackagingAssembly"));
    }

    std::vector<TrajectoryPoint> points;
    if (input.interpolation_algorithm == InterpolationAlgorithm::CMP_COORDINATED &&
        !trigger_artifacts.positions.empty()) {
        Siligen::CMPConfiguration cmp_config;
        cmp_config.trigger_mode = Siligen::CMPTriggerMode::POSITION_SYNC;
        cmp_config.cmp_channel = 1;
        cmp_config.pulse_width_us = 2000;
        cmp_config.trigger_position_tolerance = 0.1f;
        cmp_config.time_tolerance_ms = 1.0f;
        cmp_config.enable_compensation = true;
        cmp_config.compensation_factor = 1.0f;
        cmp_config.enable_multi_channel = false;

        std::vector<Siligen::DispensingTriggerPoint> triggers;
        triggers.reserve(trigger_artifacts.positions.size());
        for (std::size_t index = 0; index < trigger_artifacts.positions.size(); ++index) {
            Siligen::DispensingTriggerPoint trigger;
            trigger.position = trigger_artifacts.positions[index];
            trigger.trigger_distance = trigger_artifacts.distances[index];
            trigger.sequence_id = static_cast<uint32>(index);
            trigger.pulse_width_us = cmp_config.pulse_width_us;
            trigger.is_enabled = true;
            triggers.push_back(trigger);
        }

        CMPCoordinatedInterpolator cmp_interpolator;
        const auto cmp_started_at = std::chrono::steady_clock::now();
        log_stage("cmp_plan_start", "seed_points=" + std::to_string(seed_points.size()));
        points = cmp_interpolator.PositionTriggeredDispensing(path, triggers, cmp_config, config);
        log_stage("cmp_plan_done",
                  "points=" + std::to_string(points.size()) +
                      " elapsed_ms=" + std::to_string(ElapsedMs(cmp_started_at)));
    } else {
        if (input.interpolation_algorithm == InterpolationAlgorithm::CMP_COORDINATED) {
            return Result<std::vector<TrajectoryPoint>>::Failure(
                Error(ErrorCode::TRAJECTORY_GENERATION_FAILED,
                      "CMP插补缺少显式 trigger authority，不能退化为默认轨迹采样触发",
                      "DispensePackagingAssembly"));
        }

        if (input.interpolation_algorithm == InterpolationAlgorithm::LINEAR) {
            TimeTrajectoryPlanner trajectory_planner;
            const auto linear_plan_started_at = std::chrono::steady_clock::now();
            log_stage("linear_plan_start", "seed_points=" + std::to_string(seed_points.size()));
            auto planned_trajectory = trajectory_planner.Plan(path, BuildInterpolationPlanningConfig(config));
            log_stage("linear_plan_done",
                      "motion_points=" + std::to_string(planned_trajectory.points.size()) +
                          " elapsed_ms=" + std::to_string(ElapsedMs(linear_plan_started_at)));
            const auto convert_started_at = std::chrono::steady_clock::now();
            log_stage("linear_convert_start");
            points = ConvertMotionTrajectoryToTrajectoryPoints(
                planned_trajectory);
            log_stage("linear_convert_done",
                      "points=" + std::to_string(points.size()) +
                          " elapsed_ms=" + std::to_string(ElapsedMs(convert_started_at)));
        } else {
            auto interpolator = TrajectoryInterpolatorFactory::CreateInterpolator(input.interpolation_algorithm);
            if (!interpolator) {
                return Result<std::vector<TrajectoryPoint>>::Failure(
                    Error(ErrorCode::NOT_IMPLEMENTED, "插补算法未实现", "DispensePackagingAssembly"));
            }
            if (!interpolator->ValidateParameters(seed_points, config)) {
                return Result<std::vector<TrajectoryPoint>>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensePackagingAssembly"));
            }
            points = interpolator->CalculateInterpolation(seed_points, config);
        }

        if (!trigger_artifacts.positions.empty() &&
            !ApplyTriggerMarkersByPositionWithDiagnostics(
                points,
                trigger_artifacts.positions,
                trigger_artifacts.distances,
                Siligen::Shared::Types::kTriggerMarkerDedupToleranceMm,
                std::max(config.position_tolerance, Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm))) {
            return Result<std::vector<TrajectoryPoint>>::Failure(
                Error(ErrorCode::TRAJECTORY_GENERATION_FAILED,
                      "显式 trigger authority 映射到插补轨迹失败",
                      "DispensePackagingAssembly"));
        }
    }

    if (points.empty()) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "插补结果为空", "DispensePackagingAssembly"));
    }

    log_stage("complete",
              "points=" + std::to_string(points.size()) +
                  " elapsed_ms=" + std::to_string(ElapsedMs(started_at)));
    return Result<std::vector<TrajectoryPoint>>::Success(std::move(points));
}

void BindAuthorityLayoutToExecutionTrajectory(
    TriggerArtifacts& artifacts,
    const std::vector<TrajectoryPoint>* execution_trajectory) {
    artifacts.authority_trigger_layout.bindings.clear();
    artifacts.binding_ready = false;
    artifacts.authority_trigger_layout.binding_ready = false;

    if (execution_trajectory == nullptr || artifacts.authority_trigger_layout.trigger_points.empty()) {
        if (execution_trajectory == nullptr &&
            !artifacts.authority_trigger_layout.trigger_points.empty() &&
            artifacts.failure_reason.empty()) {
            artifacts.failure_reason = "execution trajectory unavailable for preview binding";
        }
        std::ostringstream oss;
        oss << "preview_binding_stage=skipped"
            << " layout_id=" << artifacts.authority_trigger_layout.layout_id
            << " trigger_points=" << artifacts.authority_trigger_layout.trigger_points.size()
            << " execution_points=" << (execution_trajectory ? execution_trajectory->size() : 0U);
        SILIGEN_LOG_INFO(oss.str());
        return;
    }

    const auto started_at = std::chrono::steady_clock::now();
    std::vector<std::size_t> enabled_trigger_indices;
    enabled_trigger_indices.reserve(artifacts.authority_trigger_layout.trigger_points.size());
    for (std::size_t interpolation_index = 0;
         interpolation_index < execution_trajectory->size();
         ++interpolation_index) {
        if ((*execution_trajectory)[interpolation_index].enable_position_trigger) {
            enabled_trigger_indices.push_back(interpolation_index);
        }
    }

    {
        std::ostringstream oss;
        oss << "preview_binding_stage=start"
            << " layout_id=" << artifacts.authority_trigger_layout.layout_id
            << " trigger_points=" << artifacts.authority_trigger_layout.trigger_points.size()
            << " execution_points=" << execution_trajectory->size()
            << " enabled_trigger_points=" << enabled_trigger_indices.size();
        SILIGEN_LOG_INFO(oss.str());
    }

    std::size_t last_interpolation_index = 0;
    std::size_t preferred_enabled_position = 0;
    std::vector<bool> consumed_enabled(enabled_trigger_indices.size(), false);
    std::vector<TriggerBindingTraceRow> binding_trace_rows;
    binding_trace_rows.reserve(artifacts.authority_trigger_layout.trigger_points.size());
    artifacts.authority_trigger_layout.bindings.reserve(
        artifacts.authority_trigger_layout.trigger_points.size());
    const float32 distance_tolerance_mm = Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm;
    const float32 match_tolerance_mm =
        std::max(kGluePointDedupEpsilonMm, distance_tolerance_mm);

    auto bind_candidate = [&](std::size_t trigger_index,
                              const auto& trigger,
                              const TriggerBindingCandidate& candidate,
                              TriggerBindingTraceRow&& trace_row) {
        const auto& trajectory_point = (*execution_trajectory)[candidate.interpolation_index];
        trace_row.matched = true;
        trace_row.interpolation_index = candidate.interpolation_index;
        trace_row.execution_sequence_id = trajectory_point.sequence_id;
        trace_row.execution_position = trajectory_point.position;
        trace_row.execution_trigger_position_mm = trajectory_point.trigger_position_mm;
        trace_row.distance_delta_mm = trajectory_point.trigger_position_mm - trigger.distance_mm_global;
        trace_row.position_delta_mm = trajectory_point.position.DistanceTo(trigger.position);
        trace_row.monotonic = candidate.monotonic;
        trace_row.execution_segment_type = trajectory_point.segment_type;
        binding_trace_rows.push_back(std::move(trace_row));

        InterpolationTriggerBinding binding;
        binding.binding_id =
            artifacts.authority_trigger_layout.layout_id + "-binding-" + std::to_string(trigger_index);
        binding.layout_ref = artifacts.authority_trigger_layout.layout_id;
        binding.trigger_ref = trigger.trigger_id;
        binding.interpolation_index = candidate.interpolation_index;
        binding.execution_position = trajectory_point.position;
        binding.match_error_mm = candidate.match_error_mm;
        binding.monotonic = candidate.monotonic;
        binding.bound = true;
        artifacts.authority_trigger_layout.bindings.push_back(std::move(binding));
        last_interpolation_index = candidate.interpolation_index;
    };

    for (std::size_t trigger_index = 0;
         trigger_index < artifacts.authority_trigger_layout.trigger_points.size();
         ++trigger_index) {
        const auto& trigger = artifacts.authority_trigger_layout.trigger_points[trigger_index];
        TriggerBindingTraceRow trace_row;
        trace_row.trigger_index = trigger_index;
        trace_row.trigger_id = trigger.trigger_id;
        trace_row.span_ref = trigger.span_ref;
        trace_row.source_segment_index = trigger.source_segment_index;
        trace_row.source_kind = trigger.source_kind;
        trace_row.trigger_position = trigger.position;
        trace_row.authority_distance_mm = trigger.distance_mm_global;
        TriggerBindingCandidate best_candidate;
        std::size_t nearest_distance_enabled_position = enabled_trigger_indices.size();
        std::size_t nearest_match_enabled_position = enabled_trigger_indices.size();
        float32 nearest_distance_error_mm = std::numeric_limits<float32>::max();
        float32 nearest_match_error_mm = std::numeric_limits<float32>::max();

        auto evaluate_candidate = [&](std::size_t enabled_position) {
            if (enabled_position >= enabled_trigger_indices.size() || consumed_enabled[enabled_position]) {
                return;
            }

            const std::size_t interpolation_index = enabled_trigger_indices[enabled_position];
            const auto& trajectory_point = (*execution_trajectory)[interpolation_index];
            const float32 distance_error_mm =
                std::fabs(trajectory_point.trigger_position_mm - trigger.distance_mm_global);
            const float32 match_error_mm = trajectory_point.position.DistanceTo(trigger.position);
            if (distance_error_mm < nearest_distance_error_mm) {
                nearest_distance_error_mm = distance_error_mm;
                nearest_distance_enabled_position = enabled_position;
            }
            if (match_error_mm < nearest_match_error_mm) {
                nearest_match_error_mm = match_error_mm;
                nearest_match_enabled_position = enabled_position;
            }
            if (distance_error_mm > distance_tolerance_mm ||
                match_error_mm > match_tolerance_mm) {
                return;
            }

            TriggerBindingCandidate candidate;
            candidate.found = true;
            candidate.enabled_position = enabled_position;
            candidate.interpolation_index = interpolation_index;
            candidate.distance_error_mm = distance_error_mm;
            candidate.match_error_mm = match_error_mm;
            candidate.monotonic = interpolation_index >= last_interpolation_index;
            if (IsBetterTriggerBindingCandidate(candidate, best_candidate)) {
                best_candidate = candidate;
            }
        };

        while (preferred_enabled_position < enabled_trigger_indices.size()) {
            if (consumed_enabled[preferred_enabled_position]) {
                ++preferred_enabled_position;
                continue;
            }
            const auto& trajectory_point =
                (*execution_trajectory)[enabled_trigger_indices[preferred_enabled_position]];
            if (trajectory_point.trigger_position_mm + distance_tolerance_mm <
                trigger.distance_mm_global) {
                ++preferred_enabled_position;
                continue;
            }
            break;
        }

        for (std::size_t enabled_position = preferred_enabled_position;
             enabled_position < enabled_trigger_indices.size();
             ++enabled_position) {
            if (consumed_enabled[enabled_position]) {
                continue;
            }

            const auto& trajectory_point =
                (*execution_trajectory)[enabled_trigger_indices[enabled_position]];
            evaluate_candidate(enabled_position);

            if (trajectory_point.trigger_position_mm >
                trigger.distance_mm_global + distance_tolerance_mm) {
                break;
            }
        }

        if (!best_candidate.found) {
            for (std::size_t enabled_position = 0;
                 enabled_position < enabled_trigger_indices.size();
                 ++enabled_position) {
                evaluate_candidate(enabled_position);
            }
        }

        if (!best_candidate.found) {
            std::ostringstream oss;
            oss << "preview_binding_stage=failed_first_trigger"
                << " layout_id=" << artifacts.authority_trigger_layout.layout_id
                << " trigger_index=" << trigger_index
                << " trigger_id=" << trigger.trigger_id
                << " trigger_distance_mm=" << trigger.distance_mm_global
                << " trigger_position=" << FormatPoint(trigger.position)
                << " execution_points=" << execution_trajectory->size()
                << " enabled_trigger_points=" << enabled_trigger_indices.size()
                << " last_interpolation_index=" << last_interpolation_index
                << " distance_tolerance_mm=" << distance_tolerance_mm
                << " match_tolerance_mm=" << match_tolerance_mm
                << " elapsed_ms=" << ElapsedMs(started_at);
            if (nearest_distance_enabled_position < enabled_trigger_indices.size()) {
                const std::size_t nearest_distance_index =
                    enabled_trigger_indices[nearest_distance_enabled_position];
                const auto& nearest_distance_point = (*execution_trajectory)[nearest_distance_index];
                oss << " nearest_distance={index=" << nearest_distance_index
                    << ",trigger_position_mm=" << nearest_distance_point.trigger_position_mm
                    << ",distance_error_mm=" << nearest_distance_error_mm
                    << ",match_error_mm=" << nearest_distance_point.position.DistanceTo(trigger.position)
                    << ",position=" << FormatPoint(nearest_distance_point.position)
                    << ",monotonic=" << (nearest_distance_index >= last_interpolation_index ? 1 : 0)
                    << '}';
            }
            if (nearest_match_enabled_position < enabled_trigger_indices.size()) {
                const std::size_t nearest_match_index =
                    enabled_trigger_indices[nearest_match_enabled_position];
                const auto& nearest_match_point = (*execution_trajectory)[nearest_match_index];
                oss << " nearest_match={index=" << nearest_match_index
                    << ",trigger_position_mm=" << nearest_match_point.trigger_position_mm
                    << ",distance_error_mm="
                    << std::fabs(nearest_match_point.trigger_position_mm - trigger.distance_mm_global)
                    << ",match_error_mm=" << nearest_match_error_mm
                    << ",position=" << FormatPoint(nearest_match_point.position)
                    << ",monotonic=" << (nearest_match_index >= last_interpolation_index ? 1 : 0)
                    << '}';
            }
            SILIGEN_LOG_WARNING(oss.str());
            binding_trace_rows.push_back(std::move(trace_row));
            LogBindingTraceWindow(
                artifacts.authority_trigger_layout,
                binding_trace_rows,
                trigger_index);
                artifacts.failure_reason = "authority trigger binding unavailable";
            return;
        }

        bind_candidate(trigger_index, trigger, best_candidate, std::move(trace_row));
        consumed_enabled[best_candidate.enabled_position] = true;
        preferred_enabled_position =
            std::max(preferred_enabled_position, best_candidate.enabled_position + 1U);
    }

    artifacts.binding_ready =
        artifacts.authority_trigger_layout.bindings.size() == artifacts.authority_trigger_layout.trigger_points.size();
    artifacts.authority_trigger_layout.binding_ready = artifacts.binding_ready;
    if (artifacts.binding_ready) {
        artifacts.authority_trigger_layout.state = AuthorityTriggerLayoutState::BindingReady;
        std::ostringstream oss;
        oss << "preview_binding_stage=complete"
            << " layout_id=" << artifacts.authority_trigger_layout.layout_id
            << " bindings=" << artifacts.authority_trigger_layout.bindings.size()
            << " trigger_points=" << artifacts.authority_trigger_layout.trigger_points.size()
            << " enabled_trigger_points=" << enabled_trigger_indices.size()
            << " elapsed_ms=" << ElapsedMs(started_at);
        SILIGEN_LOG_INFO(oss.str());
    } else if (artifacts.failure_reason.empty()) {
        artifacts.failure_reason = "authority trigger binding unavailable";
    }
}

}  // namespace

Result<AuthorityPreviewBuildResult> AuthorityPreviewAssemblyService::BuildAuthorityPreviewArtifacts(
    const AuthorityPreviewBuildInput& input) const {
    auto log_stage = [&](const char* stage, const std::string& detail = std::string()) {
        std::ostringstream oss;
        oss << "planning_artifacts_stage=" << stage
            << " dxf=" << input.dxf_filename
            << " process_segments=" << input.process_path.segments.size()
            << " preview_mode=authority_only";
        if (!detail.empty()) {
            oss << ' ' << detail;
        }
        SILIGEN_LOG_INFO(oss.str());
    };

    log_stage("start");

    if (input.process_path.segments.empty()) {
        return Result<AuthorityPreviewBuildResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "process path为空", "DispensePackagingAssembly"));
    }

    const auto& authority_process_path = ResolveAuthorityProcessPath(input);

    PlanningArtifactsAssemblyInput authority_input;
    authority_input.process_path = input.process_path;
    authority_input.authority_process_path = input.authority_process_path;
    authority_input.source_path = input.source_path;
    authority_input.dxf_filename = input.dxf_filename;
    authority_input.dispensing_velocity = input.dispensing_velocity;
    authority_input.acceleration = input.acceleration;
    authority_input.dispenser_interval_ms = input.dispenser_interval_ms;
    authority_input.dispenser_duration_ms = input.dispenser_duration_ms;
    authority_input.trigger_spatial_interval_mm = input.trigger_spatial_interval_mm;
    authority_input.valve_response_ms = input.valve_response_ms;
    authority_input.safety_margin_ms = input.safety_margin_ms;
    authority_input.min_interval_ms = input.min_interval_ms;
    authority_input.sample_dt = input.sample_dt;
    authority_input.sample_ds = input.sample_ds;
    authority_input.spline_max_step_mm = input.spline_max_step_mm;
    authority_input.spline_max_error_mm = input.spline_max_error_mm;
    authority_input.dispensing_strategy = input.dispensing_strategy;
    authority_input.subsegment_count = input.subsegment_count;
    authority_input.dispense_only_cruise = input.dispense_only_cruise;
    authority_input.downgrade_on_violation = input.downgrade_on_violation;
    authority_input.compensation_profile = input.compensation_profile;
    authority_input.spacing_tol_ratio = input.spacing_tol_ratio;
    authority_input.spacing_min_mm = input.spacing_min_mm;
    authority_input.spacing_max_mm = input.spacing_max_mm;

    auto trigger_artifacts_result = BuildTriggerArtifacts(authority_process_path, authority_input);
    if (trigger_artifacts_result.IsError()) {
        return Result<AuthorityPreviewBuildResult>::Failure(trigger_artifacts_result.GetError());
    }
    auto trigger_artifacts = trigger_artifacts_result.Value();
    {
        std::ostringstream oss;
        oss << "authority_points=" << trigger_artifacts.authority_trigger_points.size()
            << " layout_id=" << trigger_artifacts.authority_trigger_layout.layout_id
            << " validation=" << trigger_artifacts.validation_classification
            << " spacing_valid=" << (trigger_artifacts.spacing_valid ? 1 : 0)
            << " failure_reason=" << trigger_artifacts.failure_reason;
        log_stage("trigger_artifacts_ready", oss.str());
    }

    std::vector<TrajectoryPoint> preview_trajectory_points;
    auto preview_points_result =
        BuildInterpolationSeedPoints(
            input.process_path,
            input.spline_max_error_mm,
            ResolveInterpolationStep(authority_input));
    if (preview_points_result.IsError()) {
        if (trigger_artifacts.validation_classification == "fail") {
            std::ostringstream oss;
            oss << "preview_polyline_seed_fallback"
                << " layout_id=" << trigger_artifacts.authority_trigger_layout.layout_id
                << " reason=" << preview_points_result.GetError().GetMessage();
            log_stage("preview_polyline_failed_non_blocking", oss.str());
        } else {
            return Result<AuthorityPreviewBuildResult>::Failure(preview_points_result.GetError());
        }
    } else {
        preview_trajectory_points = ConvertPreviewPointsToTrajectory(preview_points_result.Value());
    }
    {
        std::ostringstream oss;
        oss << "preview_points=" << preview_trajectory_points.size();
        log_stage("preview_polyline_ready", oss.str());
    }

    const auto glue_points = CollectAuthorityPositions(trigger_artifacts.authority_trigger_layout);
    const auto total_length = ComputeProcessPathLength(input.process_path);

    AuthorityPreviewBuildResult result;
    result.segment_count = static_cast<int>(input.process_path.segments.size());
    result.total_length = total_length;
    result.estimated_time = EstimatePreviewTime(input, total_length);
    result.preview_trajectory_points = std::move(preview_trajectory_points);
    result.glue_points = glue_points;
    result.trigger_count = static_cast<int>(glue_points.size());
    result.dxf_filename = input.dxf_filename;
    result.timestamp = static_cast<int32>(std::time(nullptr));
    result.preview_authority_ready = !trigger_artifacts.authority_trigger_points.empty() && !glue_points.empty();
    result.preview_binding_ready = result.preview_authority_ready;
    result.preview_spacing_valid = trigger_artifacts.spacing_valid;
    result.preview_has_short_segment_exceptions = trigger_artifacts.has_short_segment_exceptions;
    result.preview_validation_classification = trigger_artifacts.validation_classification;
    result.preview_exception_reason = trigger_artifacts.exception_reason;
    result.preview_failure_reason = trigger_artifacts.failure_reason;
    if (!result.preview_authority_ready && result.preview_failure_reason.empty()) {
        result.preview_failure_reason = trigger_artifacts.authority_trigger_points.empty()
            ? "explicit trigger authority unavailable"
            : "authority preview unavailable";
    }
    result.authority_trigger_layout = trigger_artifacts.authority_trigger_layout;
    result.authority_trigger_points = trigger_artifacts.authority_trigger_points;
    result.spacing_validation_groups = trigger_artifacts.spacing_validation_groups;
    {
        std::ostringstream oss;
        oss << "preview_authority_ready=" << (result.preview_authority_ready ? 1 : 0)
            << " preview_binding_ready=" << (result.preview_binding_ready ? 1 : 0)
            << " preview_failure_reason=" << result.preview_failure_reason
            << " trigger_count=" << result.trigger_count;
        log_stage("complete", oss.str());
    }
    return Result<AuthorityPreviewBuildResult>::Success(std::move(result));
}

Result<ExecutionAssemblyBuildResult> ExecutionAssemblyService::BuildExecutionArtifactsFromAuthority(
    const ExecutionAssemblyBuildInput& input) const {
    auto log_stage = [&](const char* stage, const std::string& detail = std::string()) {
        std::ostringstream oss;
        oss << "planning_artifacts_stage=" << stage
            << " dxf=" << input.dxf_filename
            << " process_segments=" << input.process_path.segments.size()
            << " motion_points=" << input.motion_plan.points.size()
            << " preview_layout=" << input.authority_preview.authority_trigger_layout.layout_id;
        if (!detail.empty()) {
            oss << ' ' << detail;
        }
        SILIGEN_LOG_INFO(oss.str());
    };

    log_stage("execution_assembly_start");

    if (input.process_path.segments.empty()) {
        return Result<ExecutionAssemblyBuildResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "process path为空", "DispensePackagingAssembly"));
    }
    if (input.motion_plan.points.empty()) {
        return Result<ExecutionAssemblyBuildResult>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "motion plan为空", "DispensePackagingAssembly"));
    }

    auto trigger_artifacts = BuildTriggerArtifactsFromAuthorityPreview(input.authority_preview);
    {
        std::ostringstream oss;
        oss << "authority_points=" << trigger_artifacts.authority_trigger_points.size()
            << " spacing_valid=" << (trigger_artifacts.spacing_valid ? 1 : 0)
            << " failure_reason=" << trigger_artifacts.failure_reason;
        log_stage("authority_preview_loaded", oss.str());
    }

    PlanningArtifactsAssemblyInput execution_input;
    execution_input.process_path = input.process_path;
    execution_input.motion_plan = input.motion_plan;
    execution_input.source_path = input.source_path;
    execution_input.dxf_filename = input.dxf_filename;
    execution_input.dispensing_velocity = input.dispensing_velocity;
    execution_input.acceleration = input.acceleration;
    execution_input.dispenser_interval_ms = input.dispenser_interval_ms;
    execution_input.dispenser_duration_ms = input.dispenser_duration_ms;
    execution_input.trigger_spatial_interval_mm = input.trigger_spatial_interval_mm;
    execution_input.valve_response_ms = input.valve_response_ms;
    execution_input.safety_margin_ms = input.safety_margin_ms;
    execution_input.min_interval_ms = input.min_interval_ms;
    execution_input.max_jerk = input.max_jerk;
    execution_input.sample_dt = input.sample_dt;
    execution_input.sample_ds = input.sample_ds;
    execution_input.spline_max_step_mm = input.spline_max_step_mm;
    execution_input.spline_max_error_mm = input.spline_max_error_mm;
    execution_input.estimated_time_s = input.estimated_time_s;
    execution_input.use_interpolation_planner = input.use_interpolation_planner;
    execution_input.interpolation_algorithm = input.interpolation_algorithm;
    execution_input.compensation_profile = input.compensation_profile;

    auto interpolation_points_result =
        BuildInterpolationPoints(execution_input, input.process_path, trigger_artifacts);
    if (interpolation_points_result.IsError()) {
        return Result<ExecutionAssemblyBuildResult>::Failure(interpolation_points_result.GetError());
    }
    auto interpolation_points = interpolation_points_result.Value();
    {
        std::ostringstream oss;
        oss << "interpolation_points=" << interpolation_points.size();
        log_stage("execution_interpolation_ready", oss.str());
    }

    Siligen::Domain::Motion::DomainServices::InterpolationProgramPlanner program_planner;
    auto interpolation_program =
        program_planner.BuildProgram(input.process_path, input.motion_plan, input.acceleration);
    if (interpolation_program.IsError() && trigger_artifacts.validation_classification != "fail") {
        return Result<ExecutionAssemblyBuildResult>::Failure(interpolation_program.GetError());
    }
    {
        std::ostringstream oss;
        oss << "interpolation_program_ok=" << (interpolation_program.IsSuccess() ? 1 : 0)
            << " validation=" << trigger_artifacts.validation_classification;
        log_stage("execution_program_ready", oss.str());
    }

    ExecutionPackageBuilt built;
    if (interpolation_program.IsSuccess()) {
        built.execution_plan.interpolation_segments = std::move(interpolation_program.Value());
    }
    built.execution_plan.interpolation_points = std::move(interpolation_points);
    built.execution_plan.motion_trajectory = input.motion_plan;
    built.execution_plan.trigger_distances_mm = trigger_artifacts.distances;
    built.execution_plan.trigger_interval_ms = trigger_artifacts.interval_ms;
    built.execution_plan.trigger_interval_mm = trigger_artifacts.interval_mm;
    built.execution_plan.total_length_mm =
        input.motion_plan.total_length > kEpsilon ? input.motion_plan.total_length : ComputeProcessPathLength(input.process_path);
    built.total_length_mm = built.execution_plan.total_length_mm;
    built.estimated_time_s = input.estimated_time_s;
    built.source_path = input.source_path;
    built.source_fingerprint = input.authority_preview.authority_trigger_layout.layout_id;

    built.estimated_time_s = EstimateExecutionTime(execution_input, built);

    ExecutionPackageValidated execution_package(std::move(built));
    auto package_validation = execution_package.Validate();
    if (package_validation.IsError()) {
        return Result<ExecutionAssemblyBuildResult>::Failure(package_validation.GetError());
    }
    {
        std::ostringstream oss;
        oss << "trajectory_points=" << execution_package.execution_plan.motion_trajectory.points.size()
            << " interpolation_segments=" << execution_package.execution_plan.interpolation_segments.size()
            << " interpolation_points=" << execution_package.execution_plan.interpolation_points.size();
        log_stage("execution_package_ready", oss.str());
    }

    const auto selection = SelectExecutionTrajectory(execution_package);
    {
        std::ostringstream oss;
        oss << "authority_shared=" << (selection.authority_shared ? 1 : 0)
            << " execution_points="
            << (selection.execution_trajectory ? selection.execution_trajectory->size() : 0U);
        log_stage("execution_trajectory_selected", oss.str());
    }
    BindAuthorityLayoutToExecutionTrajectory(trigger_artifacts, selection.execution_trajectory);
    {
        std::ostringstream oss;
        oss << "binding_ready=" << (trigger_artifacts.binding_ready ? 1 : 0)
            << " bindings=" << trigger_artifacts.authority_trigger_layout.bindings.size()
            << " failure_reason=" << trigger_artifacts.failure_reason;
        log_stage("execution_binding_resolved", oss.str());
    }

    ExecutionAssemblyBuildResult result;
    result.execution_package = execution_package;
    if (selection.execution_trajectory) {
        result.execution_trajectory_points = *selection.execution_trajectory;
    }
    result.interpolation_trajectory_points = execution_package.execution_plan.interpolation_points;
    result.motion_trajectory_points = selection.motion_trajectory_points;
    result.preview_authority_shared_with_execution = trigger_artifacts.binding_ready;
    result.execution_binding_ready = trigger_artifacts.binding_ready;
    result.execution_failure_reason = trigger_artifacts.failure_reason;
    result.authority_trigger_layout = trigger_artifacts.authority_trigger_layout;
    {
        std::ostringstream oss;
        oss << "execution_binding_ready=" << (result.execution_binding_ready ? 1 : 0)
            << " execution_failure_reason=" << result.execution_failure_reason;
        log_stage("execution_assembly_complete", oss.str());
    }
    return Result<ExecutionAssemblyBuildResult>::Success(std::move(result));
}

}  // namespace Siligen::Application::Services::Dispensing





