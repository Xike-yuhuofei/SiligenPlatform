#include "application/services/dispensing/DispensePlanningFacade.h"

#include "domain/dispensing/domain-services/TriggerPlanner.h"
#include "domain/motion/CMPCoordinatedInterpolator.h"
#include "domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h"
#include "domain/trajectory/value-objects/GeometryUtils.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/types/CMPTypes.h"
#include "shared/types/TrajectoryTriggerUtils.h"
#include "shared/types/VisualizationTypes.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>

namespace Siligen::Application::Services::Dispensing {

using Siligen::Domain::Dispensing::Contracts::ExecutionPackageBuilt;
using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;
using Siligen::Domain::Dispensing::DomainServices::TriggerPlanner;
using Siligen::Domain::Dispensing::ValueObjects::TriggerPlan;
using Siligen::Domain::Motion::CMPCoordinatedInterpolator;
using Siligen::Domain::Motion::InterpolationAlgorithm;
using Siligen::Domain::Motion::TrajectoryInterpolatorFactory;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;
using Siligen::Domain::Trajectory::ValueObjects::ArcPoint;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcLength;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcSweep;
using Siligen::Domain::Trajectory::ValueObjects::Segment;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::MotionPlanning::Contracts::MotionPlan;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::VisualizationTriggerPoint;

namespace {

constexpr float32 kEpsilon = 1e-6f;
constexpr float32 kGluePointDedupEpsilonMm = 1e-4f;

struct GluePointSegment {
    std::vector<Siligen::Shared::Types::Point2D> points;
};

struct SpacingStats {
    size_t count = 0;
    size_t within_tol = 0;
    size_t below_min = 0;
    size_t above_max = 0;
    float32 min_dist = std::numeric_limits<float32>::max();
    float32 max_dist = 0.0f;
};

struct TriggerArtifacts {
    std::vector<float32> distances;
    uint32 interval_ms = 0;
    float32 interval_mm = 0.0f;
    bool downgraded = false;
};

struct ExecutionTrajectorySelection {
    std::vector<TrajectoryPoint> motion_trajectory_points;
    const std::vector<TrajectoryPoint>* execution_trajectory = nullptr;
};

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

SpacingStats EvaluateSpacing(
    const std::vector<GluePointSegment>& segments,
    float32 target_mm,
    float32 tol_ratio,
    float32 min_allowed,
    float32 max_allowed) {
    SpacingStats stats;
    if (target_mm <= kEpsilon) {
        return stats;
    }
    for (const auto& seg : segments) {
        if (seg.points.size() < 2) {
            continue;
        }
        for (size_t i = 1; i < seg.points.size(); ++i) {
            const float32 distance = seg.points[i - 1].DistanceTo(seg.points[i]);
            stats.count++;
            stats.min_dist = std::min(stats.min_dist, distance);
            stats.max_dist = std::max(stats.max_dist, distance);
            if (std::abs(distance - target_mm) <= target_mm * tol_ratio) {
                stats.within_tol++;
            }
            if (distance < min_allowed) {
                stats.below_min++;
            }
            if (distance > max_allowed) {
                stats.above_max++;
            }
        }
    }
    if (stats.count == 0) {
        stats.min_dist = 0.0f;
    }
    return stats;
}

std::vector<Siligen::Shared::Types::Point2D> BuildTriggerPointsFromDistances(
    const std::vector<Point2D>& path_points,
    const std::vector<float32>& distances_mm) {
    std::vector<Siligen::Shared::Types::Point2D> points;
    if (path_points.size() < 2 || distances_mm.empty()) {
        return points;
    }

    std::vector<float32> cumulative(path_points.size(), 0.0f);
    for (size_t i = 1; i < path_points.size(); ++i) {
        cumulative[i] = cumulative[i - 1] + path_points[i - 1].DistanceTo(path_points[i]);
    }

    auto sorted = distances_mm;
    std::sort(sorted.begin(), sorted.end());

    size_t idx = 1;
    for (const float32 target : sorted) {
        while (idx < cumulative.size() && cumulative[idx] + kEpsilon < target) {
            ++idx;
        }
        if (idx >= cumulative.size()) {
            break;
        }

        const float32 seg_len = cumulative[idx] - cumulative[idx - 1];
        const float32 ratio = (seg_len > kEpsilon) ? (target - cumulative[idx - 1]) / seg_len : 0.0f;
        const auto& start = path_points[idx - 1];
        const auto& end = path_points[idx];
        points.push_back(start + (end - start) * ratio);
    }

    return points;
}

std::vector<Siligen::Shared::Types::Point2D> RemoveConsecutiveNearDuplicatePoints(
    const std::vector<Siligen::Shared::Types::Point2D>& points,
    float32 epsilon_mm) {
    std::vector<Siligen::Shared::Types::Point2D> deduped;
    deduped.reserve(points.size());
    for (const auto& point : points) {
        if (deduped.empty() || deduped.back().DistanceTo(point) > epsilon_mm) {
            deduped.push_back(point);
        }
    }
    return deduped;
}

std::vector<VisualizationTriggerPoint> BuildPlannedTriggerPoints(
    const std::vector<Point2D>& preview_path_points,
    const std::vector<float32>& trigger_distances_mm) {
    std::vector<VisualizationTriggerPoint> triggers;
    if (preview_path_points.size() < 2 || trigger_distances_mm.empty()) {
        return triggers;
    }

    auto trigger_points = RemoveConsecutiveNearDuplicatePoints(
        BuildTriggerPointsFromDistances(preview_path_points, trigger_distances_mm),
        kGluePointDedupEpsilonMm);
    if (trigger_points.empty()) {
        return triggers;
    }

    triggers.reserve(trigger_points.size());
    const float32 nan_value = std::numeric_limits<float32>::quiet_NaN();
    for (const auto& pt : trigger_points) {
        triggers.emplace_back(pt, nan_value);
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
        return selection;
    }
    selection.execution_trajectory = &selection.motion_trajectory_points;
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

void ValidateGlueSpacing(
    const PlanningArtifactsBuildInput& input,
    const std::vector<GluePointSegment>& glue_segments,
    float32 target_spacing) {
    if (glue_segments.empty() || target_spacing <= kEpsilon) {
        return;
    }

    const float32 tol_ratio = input.spacing_tol_ratio > 0.0f ? input.spacing_tol_ratio : 0.1f;
    const float32 min_allowed = input.spacing_min_mm > 0.0f ? input.spacing_min_mm : 0.5f;
    const float32 max_allowed = input.spacing_max_mm > 0.0f ? input.spacing_max_mm : 10.0f;
    const auto stats = EvaluateSpacing(glue_segments, target_spacing, tol_ratio, min_allowed, max_allowed);
    if (stats.count == 0) {
        return;
    }

    const float32 within_ratio = static_cast<float32>(stats.within_tol) / static_cast<float32>(stats.count);
    if (within_ratio < 0.9f || stats.below_min > 0 || stats.above_max > 0) {
        SILIGEN_LOG_WARNING_FMT_HELPER(
            "Glue点距复核未达标: within=%.1f%% min=%.4f max=%.4f below=%zu above=%zu target=%.3f",
            within_ratio * 100.0f,
            stats.min_dist,
            stats.max_dist,
            stats.below_min,
            stats.above_max,
            target_spacing);
    }
}

float32 EstimateExecutionTime(
    const PlanningArtifactsBuildInput& input,
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

PlanningArtifactExportRequest BuildExportRequest(
    const PlanningArtifactsBuildInput& input,
    const ExecutionPackageValidated& execution_package,
    const ExecutionTrajectorySelection& selection,
    const std::vector<Siligen::Shared::Types::Point2D>& glue_points) {
    PlanningArtifactExportRequest export_request;
    export_request.source_path = input.source_path;
    export_request.dxf_filename = input.dxf_filename;
    export_request.process_path = input.process_path;
    export_request.glue_points = glue_points;
    if (selection.execution_trajectory) {
        export_request.execution_trajectory_points = *selection.execution_trajectory;
    }
    export_request.interpolation_trajectory_points = execution_package.execution_plan.interpolation_points;
    export_request.motion_trajectory_points = selection.motion_trajectory_points;
    return export_request;
}

float32 ResolveInterpolationStep(const PlanningArtifactsBuildInput& input) {
    if (input.sample_ds > kEpsilon) {
        return input.sample_ds;
    }
    if (input.spline_max_step_mm > kEpsilon) {
        return input.spline_max_step_mm;
    }
    return 1.0f;
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
    const Siligen::Domain::Trajectory::ValueObjects::ArcPrimitive& arc,
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

std::vector<Point2D> BuildInterpolationSeedPoints(const ProcessPath& path, float32 step) {
    std::vector<Point2D> points;
    for (const auto& segment : path.segments) {
        const auto& geometry = segment.geometry;
        if (geometry.is_point) {
            AppendUniquePoint(points, geometry.line.start);
        } else if (geometry.type == SegmentType::Line) {
            SampleLinePoints(geometry.line.start, geometry.line.end, step, points);
        } else if (geometry.type == SegmentType::Arc) {
            SampleArcPoints(geometry.arc, step, points);
        } else if (!geometry.spline.control_points.empty()) {
            for (size_t i = 1; i < geometry.spline.control_points.size(); ++i) {
                SampleLinePoints(
                    geometry.spline.control_points[i - 1],
                    geometry.spline.control_points[i],
                    step,
                    points);
            }
        }
    }
    return points;
}

Result<TriggerArtifacts> BuildTriggerArtifacts(
    const ProcessPath& path,
    const PlanningArtifactsBuildInput& input) {
    TriggerArtifacts artifacts;
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

    float32 residual_mm = 0.0f;
    float32 path_offset_mm = 0.0f;
    TriggerPlanner trigger_planner;

    for (const auto& segment : path.segments) {
        const float32 length_mm = SegmentLength(segment.geometry);
        if (length_mm <= kEpsilon) {
            if (segment.geometry.is_point && segment.dispense_on) {
                artifacts.distances.push_back(path_offset_mm);
            }
            continue;
        }

        if (segment.dispense_on) {
            auto trigger_plan_result = trigger_planner.Plan(
                length_mm,
                input.dispensing_velocity,
                input.acceleration,
                input.trigger_spatial_interval_mm,
                input.dispenser_interval_ms,
                residual_mm,
                trigger_plan,
                input.compensation_profile);
            if (trigger_plan_result.IsError()) {
                return Result<TriggerArtifacts>::Failure(trigger_plan_result.GetError());
            }

            const auto& trigger_result = trigger_plan_result.Value();
            for (float32 dist : trigger_result.spacing.distances_mm) {
                artifacts.distances.push_back(path_offset_mm + dist);
            }
            residual_mm = trigger_result.spacing.residual_mm;
            artifacts.interval_ms = std::max<uint32>(artifacts.interval_ms, trigger_result.interval_ms);
            artifacts.interval_mm = std::max(artifacts.interval_mm, trigger_result.plan.interval_mm);
            artifacts.downgraded = artifacts.downgraded || trigger_result.downgrade_applied;
        } else {
            residual_mm = 0.0f;
        }

        path_offset_mm += length_mm;
    }

    return Result<TriggerArtifacts>::Success(std::move(artifacts));
}

Result<std::vector<TrajectoryPoint>> BuildInterpolationPoints(
    const PlanningArtifactsBuildInput& input,
    const ProcessPath& path,
    const std::vector<float32>& trigger_distances) {
    if (!input.use_interpolation_planner) {
        return Result<std::vector<TrajectoryPoint>>::Success({});
    }

    const auto seed_points = BuildInterpolationSeedPoints(path, ResolveInterpolationStep(input));
    if (seed_points.size() < 2) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensePlanningFacade"));
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
            Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensePlanningFacade"));
    }

    if ((input.interpolation_algorithm == InterpolationAlgorithm::LINEAR ||
         input.interpolation_algorithm == InterpolationAlgorithm::CMP_COORDINATED) &&
        config.max_jerk <= 0.0f) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensePlanningFacade"));
    }

    std::vector<TrajectoryPoint> points;
    if (input.interpolation_algorithm == InterpolationAlgorithm::CMP_COORDINATED && !trigger_distances.empty()) {
        Siligen::CMPConfiguration cmp_config;
        cmp_config.trigger_mode = Siligen::CMPTriggerMode::POSITION_SYNC;
        cmp_config.cmp_channel = 1;
        cmp_config.pulse_width_us = 2000;
        cmp_config.trigger_position_tolerance = 0.1f;
        cmp_config.time_tolerance_ms = 1.0f;
        cmp_config.enable_compensation = true;
        cmp_config.compensation_factor = 1.0f;
        cmp_config.enable_multi_channel = false;

        std::vector<float32> cumulative(seed_points.size(), 0.0f);
        for (size_t i = 1; i < seed_points.size(); ++i) {
            cumulative[i] = cumulative[i - 1] + seed_points[i - 1].DistanceTo(seed_points[i]);
        }

        auto targets = trigger_distances;
        std::sort(targets.begin(), targets.end());

        std::vector<Siligen::DispensingTriggerPoint> triggers;
        triggers.reserve(targets.size());
        size_t idx = 1;
        uint32 seq = 0;
        for (float32 target : targets) {
            while (idx < cumulative.size() && cumulative[idx] + kEpsilon < target) {
                ++idx;
            }
            if (idx >= cumulative.size()) {
                break;
            }

            const float32 seg_len = cumulative[idx] - cumulative[idx - 1];
            const float32 ratio = (seg_len > kEpsilon) ? (target - cumulative[idx - 1]) / seg_len : 0.0f;
            const Point2D trigger_pos = seed_points[idx - 1] + (seed_points[idx] - seed_points[idx - 1]) * ratio;

            Siligen::DispensingTriggerPoint trigger;
            trigger.position = trigger_pos;
            trigger.trigger_distance = target;
            trigger.sequence_id = seq++;
            trigger.pulse_width_us = cmp_config.pulse_width_us;
            trigger.is_enabled = true;
            triggers.push_back(trigger);
        }

        CMPCoordinatedInterpolator cmp_interpolator;
        points = cmp_interpolator.PositionTriggeredDispensing(path, triggers, cmp_config, config);
    } else {
        if (input.interpolation_algorithm == InterpolationAlgorithm::CMP_COORDINATED) {
            return Result<std::vector<TrajectoryPoint>>::Failure(
                Error(ErrorCode::TRAJECTORY_GENERATION_FAILED,
                      "CMP插补缺少 trigger_distances_mm，不能退化为默认轨迹采样触发",
                      "DispensePlanningFacade"));
        }

        auto interpolator = TrajectoryInterpolatorFactory::CreateInterpolator(input.interpolation_algorithm);
        if (!interpolator) {
            return Result<std::vector<TrajectoryPoint>>::Failure(
                Error(ErrorCode::NOT_IMPLEMENTED, "插补算法未实现", "DispensePlanningFacade"));
        }
        if (!interpolator->ValidateParameters(seed_points, config)) {
            return Result<std::vector<TrajectoryPoint>>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensePlanningFacade"));
        }
        points = interpolator->CalculateInterpolation(seed_points, config);

        auto targets = trigger_distances;
        std::sort(targets.begin(), targets.end());
        const auto trigger_positions = BuildTriggerPointsFromDistances(seed_points, targets);
        if (!Siligen::Shared::Types::ApplyTriggerMarkersByPosition(points, trigger_positions, targets)) {
            return Result<std::vector<TrajectoryPoint>>::Failure(
                Error(ErrorCode::TRAJECTORY_GENERATION_FAILED,
                      "trigger_distances_mm 映射到插补轨迹失败",
                      "DispensePlanningFacade"));
        }
    }

    if (points.empty()) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "插补结果为空", "DispensePlanningFacade"));
    }

    return Result<std::vector<TrajectoryPoint>>::Success(std::move(points));
}

}  // namespace

Result<PlanningArtifactsBuildResult> DispensePlanningFacade::AssemblePlanningArtifacts(
    const PlanningArtifactsBuildInput& input) const {
    if (input.process_path.segments.empty()) {
        return Result<PlanningArtifactsBuildResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "process path为空", "DispensePlanningFacade"));
    }
    if (input.motion_plan.points.empty()) {
        return Result<PlanningArtifactsBuildResult>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "motion plan为空", "DispensePlanningFacade"));
    }

    auto trigger_artifacts_result = BuildTriggerArtifacts(input.process_path, input);
    if (trigger_artifacts_result.IsError()) {
        return Result<PlanningArtifactsBuildResult>::Failure(trigger_artifacts_result.GetError());
    }
    auto trigger_artifacts = trigger_artifacts_result.Value();

    auto interpolation_points_result =
        BuildInterpolationPoints(input, input.process_path, trigger_artifacts.distances);
    if (interpolation_points_result.IsError()) {
        return Result<PlanningArtifactsBuildResult>::Failure(interpolation_points_result.GetError());
    }
    auto interpolation_points = interpolation_points_result.Value();

    Siligen::Domain::Motion::DomainServices::InterpolationProgramPlanner program_planner;
    auto interpolation_program =
        program_planner.BuildProgram(input.process_path, input.motion_plan, input.acceleration);
    if (interpolation_program.IsError()) {
        return Result<PlanningArtifactsBuildResult>::Failure(interpolation_program.GetError());
    }

    ExecutionPackageBuilt built;
    built.execution_plan.interpolation_segments = std::move(interpolation_program.Value());
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

    built.estimated_time_s = EstimateExecutionTime(input, built);

    ExecutionPackageValidated execution_package(std::move(built));
    auto package_validation = execution_package.Validate();
    if (package_validation.IsError()) {
        return Result<PlanningArtifactsBuildResult>::Failure(package_validation.GetError());
    }

    const auto selection = SelectExecutionTrajectory(execution_package);
    float32 preview_trigger_spacing_mm = execution_package.execution_plan.trigger_interval_mm;
    if (preview_trigger_spacing_mm <= kEpsilon) {
        preview_trigger_spacing_mm = input.trigger_spatial_interval_mm;
    }

    const auto preview_path_points = BuildInterpolationSeedPoints(input.process_path, ResolveInterpolationStep(input));
    const auto preview_triggers = BuildPlannedTriggerPoints(
        preview_path_points,
        execution_package.execution_plan.trigger_distances_mm);
    if (preview_triggers.empty()) {
        return Result<PlanningArtifactsBuildResult>::Failure(
            Error(
                ErrorCode::CMP_TRIGGER_SETUP_FAILED,
                "位置触发不可用，禁止回退为定时触发",
                "DispensePlanningFacade"));
    }

    const auto glue_points = CollectTriggerPositions(preview_triggers);
    const auto glue_segments = glue_points.empty()
        ? std::vector<GluePointSegment>{}
        : std::vector<GluePointSegment>{GluePointSegment{glue_points}};

    ValidateGlueSpacing(input, glue_segments, preview_trigger_spacing_mm);

    PlanningArtifactsBuildResult result;
    result.execution_package = execution_package;
    result.segment_count = static_cast<int>(input.process_path.segments.size());
    result.total_length = execution_package.total_length_mm;
    result.estimated_time = execution_package.estimated_time_s;
    if (selection.execution_trajectory) {
        result.trajectory_points = *selection.execution_trajectory;
    }
    result.glue_points = glue_points;
    result.trigger_count = static_cast<int>(glue_points.size());
    result.dxf_filename = input.dxf_filename;
    result.timestamp = static_cast<int32>(std::time(nullptr));
    result.planning_report = input.motion_plan.planning_report;
    result.export_request = BuildExportRequest(input, execution_package, selection, glue_points);
    return Result<PlanningArtifactsBuildResult>::Success(std::move(result));
}

}  // namespace Siligen::Application::Services::Dispensing
