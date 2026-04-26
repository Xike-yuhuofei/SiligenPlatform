#include "application/services/dispensing/PlanningAssemblyInternals.h"

#include "application/services/motion_planning/CmpInterpolationFacade.h"
#include "application/services/motion_planning/TrajectoryInterpolationFacade.h"
#include "modules/motion-planning/domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/types/CMPTypes.h"
#include "shared/types/TrajectoryTriggerUtils.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Siligen::Application::Services::Dispensing::Internal {
namespace {

using Siligen::Application::Services::MotionPlanning::CmpInterpolationFacade;
using Siligen::Application::Services::MotionPlanning::TrajectoryInterpolationFacade;
using Siligen::Domain::Motion::DomainServices::InterpolationProgramPlanner;
using Siligen::MotionPlanning::Contracts::MotionTrajectoryPoint;
using Siligen::RuntimeExecution::Contracts::Motion::InterpolationType;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::PointFlyingCarrierDirectionMode;
using Siligen::Shared::Types::PointFlyingCarrierPolicy;
using Siligen::Shared::Types::uint16;
using Siligen::Shared::Types::uint32;

bool HasFormalCarrierMotion(const MotionPlan& motion_plan) {
    return motion_plan.points.size() >= 2U && motion_plan.total_length > kEpsilon;
}

PlanningArtifactsAssemblyInput BuildExecutionPlanningInput(
    const ExecutionAssemblyBuildInput& input,
    const ProcessPath& execution_process_path) {
    PlanningArtifactsAssemblyInput execution_input;
    execution_input.process_path = execution_process_path;
    execution_input.authority_process_path = input.authority_process_path;
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
    execution_input.curve_flatten_max_step_mm = input.curve_flatten_max_step_mm;
    execution_input.curve_flatten_max_error_mm = input.curve_flatten_max_error_mm;
    execution_input.execution_nominal_time_s = input.execution_nominal_time_s;
    execution_input.use_interpolation_planner = input.use_interpolation_planner;
    execution_input.interpolation_algorithm = input.interpolation_algorithm;
    execution_input.compensation_profile = input.compensation_profile;
    return execution_input;
}

bool ApplyTriggerMarkersByPositionWithDiagnostics(
    std::vector<TrajectoryPoint>& points,
    const std::vector<Point2D>& trigger_positions,
    const std::vector<float32>& trigger_distances_mm,
    float32 dedup_tolerance_mm,
    float32 position_tolerance_mm,
    uint16 pulse_width_us = Siligen::Shared::Types::kDefaultTriggerPulseWidthUs) {
    if (!trigger_distances_mm.empty() && trigger_positions.size() != trigger_distances_mm.size()) {
        SILIGEN_LOG_WARNING(
            "build_interp_marker_stage=invalid_input trigger_positions and trigger_distances size mismatch");
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

TrajectoryPoint BuildPointExecutionMarker(
    const ExecutionAssemblyBuildInput& input,
    const TriggerArtifacts& trigger_artifacts) {
    TrajectoryPoint point;

    if (!input.motion_plan.points.empty()) {
        const auto& motion_point = input.motion_plan.points.front();
        point.position = Point2D(motion_point.position.x, motion_point.position.y);
        point.timestamp = motion_point.t;
        point.velocity = std::sqrt(
            motion_point.velocity.x * motion_point.velocity.x +
            motion_point.velocity.y * motion_point.velocity.y);
    } else if (!trigger_artifacts.positions.empty()) {
        point.position = trigger_artifacts.positions.front();
    }

    point.sequence_id = 0U;
    point.enable_position_trigger = true;
    point.trigger_position_mm = trigger_artifacts.distances.empty() ? 0.0f : trigger_artifacts.distances.front();
    point.trigger_pulse_width_us = Siligen::Shared::Types::kDefaultTriggerPulseWidthUs;
    return point;
}

Point2D ResolvePointExecutionPosition(
    const ExecutionAssemblyBuildInput& input,
    const ProcessPath& execution_process_path,
    const TriggerArtifacts& trigger_artifacts) {
    if (!trigger_artifacts.positions.empty()) {
        return trigger_artifacts.positions.front();
    }
    if (!input.motion_plan.points.empty()) {
        const auto& motion_point = input.motion_plan.points.front();
        return Point2D(motion_point.position.x, motion_point.position.y);
    }
    if (!execution_process_path.segments.empty()) {
        return execution_process_path.segments.front().geometry.line.start;
    }
    return Point2D{};
}

Result<PointFlyingCarrierPolicy> ResolvePointFlyingCarrierPolicy(const ExecutionAssemblyBuildInput& input) {
    if (!input.point_flying_carrier_policy.has_value()) {
        return Result<PointFlyingCarrierPolicy>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "POINT/flying_shot 缺少 point_flying_carrier_policy",
            "DispensePackagingAssembly"));
    }
    const auto& policy = input.point_flying_carrier_policy.value();
    if (!Siligen::Shared::Types::IsValid(policy)) {
        return Result<PointFlyingCarrierPolicy>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "POINT/flying_shot point_flying_carrier_policy 无效",
            "DispensePackagingAssembly"));
    }
    return Result<PointFlyingCarrierPolicy>::Success(policy);
}

Result<Point2D> ResolvePointFlyingCarrierDirection(
    const ExecutionAssemblyBuildInput& input,
    const Point2D& point,
    PointFlyingCarrierDirectionMode direction_mode) {
    if (direction_mode != PointFlyingCarrierDirectionMode::APPROACH_DIRECTION) {
        return Result<Point2D>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "POINT/flying_shot point_flying_direction_mode 不受支持",
            "DispensePackagingAssembly"));
    }
    const auto delta = point - input.planning_start_position;
    const auto magnitude = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (magnitude <= kEpsilon) {
        return Result<Point2D>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "POINT/flying_shot planning_start_position 不能与目标点重合",
            "DispensePackagingAssembly"));
    }
    return Result<Point2D>::Success(Point2D(delta.x / magnitude, delta.y / magnitude));
}

Result<MotionTrajectory> BuildPointFlyingCarrierTrajectory(
    const ExecutionAssemblyBuildInput& input,
    const ProcessPath& execution_process_path,
    const TriggerArtifacts& trigger_artifacts) {
    const auto point = ResolvePointExecutionPosition(input, execution_process_path, trigger_artifacts);
    auto policy_result = ResolvePointFlyingCarrierPolicy(input);
    if (policy_result.IsError()) {
        return Result<MotionTrajectory>::Failure(policy_result.GetError());
    }
    const auto policy = policy_result.Value();
    auto direction_result = ResolvePointFlyingCarrierDirection(input, point, policy.direction_mode);
    if (direction_result.IsError()) {
        return Result<MotionTrajectory>::Failure(direction_result.GetError());
    }
    const auto carrier_length_mm = policy.trigger_spatial_interval_mm;
    if (carrier_length_mm <= kEpsilon) {
        return Result<MotionTrajectory>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "POINT/flying_shot trigger_spatial_interval_mm 必须大于 0",
            "DispensePackagingAssembly"));
    }
    MotionTrajectory trajectory;
    const auto direction = direction_result.Value();
    const auto end = point + direction * carrier_length_mm;
    const auto velocity_mm_s = std::max(input.dispensing_velocity, kEpsilon);
    const auto total_time_s = carrier_length_mm / velocity_mm_s;

    MotionTrajectoryPoint start_point;
    start_point.t = 0.0f;
    start_point.position = {point.x, point.y, 0.0f};
    start_point.velocity = {
        direction.x * velocity_mm_s,
        direction.y * velocity_mm_s,
        0.0f,
    };
    start_point.dispense_on = true;

    MotionTrajectoryPoint end_point;
    end_point.t = total_time_s;
    end_point.position = {end.x, end.y, 0.0f};
    end_point.velocity = {0.0f, 0.0f, 0.0f};
    end_point.dispense_on = true;

    trajectory.points = {start_point, end_point};
    trajectory.total_length = carrier_length_mm;
    trajectory.total_time = total_time_s;
    return Result<MotionTrajectory>::Success(std::move(trajectory));
}

InterpolationData BuildPointFlyingCarrierSegment(
    const MotionTrajectory& motion_trajectory,
    const ExecutionAssemblyBuildInput& input) {
    InterpolationData segment;
    segment.type = InterpolationType::LINEAR;
    const auto& end = motion_trajectory.points.back().position;
    segment.positions = {end.x, end.y};
    segment.velocity = std::max(input.dispensing_velocity, kEpsilon);
    segment.acceleration = std::max(input.acceleration, kEpsilon);
    segment.end_velocity = 0.0f;
    return segment;
}

Result<ExecutionGenerationArtifacts> BuildPointFlyingExecutionArtifacts(
    const ExecutionAssemblyBuildInput& input,
    const ProcessPath& execution_process_path,
    const TriggerArtifacts& trigger_artifacts) {
    ExecutionGenerationArtifacts artifacts;
    auto motion_trajectory_result =
        BuildPointFlyingCarrierTrajectory(input, execution_process_path, trigger_artifacts);
    if (motion_trajectory_result.IsError()) {
        return Result<ExecutionGenerationArtifacts>::Failure(motion_trajectory_result.GetError());
    }
    artifacts.motion_trajectory = std::move(motion_trajectory_result.Value());
    artifacts.interpolation_points = ConvertMotionTrajectoryToTrajectoryPoints(artifacts.motion_trajectory);
    if (!trigger_artifacts.positions.empty() &&
        !ApplyTriggerMarkersByPositionWithDiagnostics(
            artifacts.interpolation_points,
            trigger_artifacts.positions,
            trigger_artifacts.distances,
            Siligen::Shared::Types::kTriggerMarkerDedupToleranceMm,
            Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm)) {
        return Result<ExecutionGenerationArtifacts>::Failure(Error(
            ErrorCode::TRAJECTORY_GENERATION_FAILED,
            "显式 trigger authority 映射到 point flying_shot carrier trajectory 失败",
            "DispensePackagingAssembly"));
    }
    artifacts.interpolation_segments.push_back(
        BuildPointFlyingCarrierSegment(artifacts.motion_trajectory, input));
    return Result<ExecutionGenerationArtifacts>::Success(std::move(artifacts));
}

}  // namespace

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

    Siligen::InterpolationConfig config{};
    config.max_velocity = input.dispensing_velocity;
    config.max_acceleration = input.acceleration;
    config.max_jerk = input.max_jerk;
    config.time_step = (input.sample_dt > kEpsilon) ? input.sample_dt : 0.001f;
    if (input.curve_flatten_max_error_mm > kEpsilon) {
        config.position_tolerance = input.curve_flatten_max_error_mm;
    }
    if (input.compensation_profile.curvature_speed_factor > kEpsilon) {
        config.curvature_speed_factor = input.compensation_profile.curvature_speed_factor;
    }
    if (input.trigger_spatial_interval_mm > kEpsilon) {
        config.trigger_spacing_mm = input.trigger_spatial_interval_mm;
    }

    if (config.max_velocity <= 0.0f || config.max_acceleration <= 0.0f || config.time_step <= 0.0f ||
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

    if (input.interpolation_algorithm == InterpolationAlgorithm::LINEAR) {
        const auto seed_points_result =
            BuildInterpolationSeedPoints(path, input.curve_flatten_max_error_mm, ResolveInterpolationStep(input));
        if (seed_points_result.IsError()) {
            return Result<std::vector<TrajectoryPoint>>::Failure(seed_points_result.GetError());
        }

        auto timed_points_result =
            BuildTimedExecutionTrajectoryFromMotionPlan(seed_points_result.Value(), input.motion_plan);
        if (timed_points_result.IsError()) {
            return Result<std::vector<TrajectoryPoint>>::Failure(timed_points_result.GetError());
        }

        auto points = std::move(timed_points_result.Value());
        if (points.size() < 2U) {
            return Result<std::vector<TrajectoryPoint>>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensePackagingAssembly"));
        }
        log_stage(
            "linear_canonical_path_consumed",
            "seed_points=" + std::to_string(points.size()) +
                " motion_points=" + std::to_string(input.motion_plan.points.size()));

        if (!trigger_artifacts.positions.empty() &&
            !ApplyTriggerMarkersByPositionWithDiagnostics(
                points,
                trigger_artifacts.positions,
                trigger_artifacts.distances,
                Siligen::Shared::Types::kTriggerMarkerDedupToleranceMm,
                std::max(config.position_tolerance, Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm))) {
            return Result<std::vector<TrajectoryPoint>>::Failure(Error(
                ErrorCode::TRAJECTORY_GENERATION_FAILED,
                "显式 trigger authority 映射到上游 motion plan 轨迹失败",
                "DispensePackagingAssembly"));
        }

        auto timing_validation = ValidatePathInterpolationTiming(
            points,
            input.motion_plan.total_time,
            "DispensePackagingAssembly");
        if (timing_validation.IsError()) {
            return Result<std::vector<TrajectoryPoint>>::Failure(timing_validation.GetError());
        }

        log_stage(
            "complete",
            "points=" + std::to_string(points.size()) +
                " elapsed_ms=" + std::to_string(ElapsedMs(started_at)));
        return Result<std::vector<TrajectoryPoint>>::Success(std::move(points));
    }

    const auto seed_started_at = std::chrono::steady_clock::now();
    log_stage("seed_start");
    const auto seed_points_result =
        BuildInterpolationSeedPoints(path, input.curve_flatten_max_error_mm, ResolveInterpolationStep(input));
    if (seed_points_result.IsError()) {
        log_stage(
            "seed_failed",
            "elapsed_ms=" + std::to_string(ElapsedMs(seed_started_at)) +
                " reason=" + seed_points_result.GetError().GetMessage());
        return Result<std::vector<TrajectoryPoint>>::Failure(seed_points_result.GetError());
    }
    const auto& seed_points = seed_points_result.Value();
    log_stage(
        "seed_done",
        "seed_points=" + std::to_string(seed_points.size()) +
            " elapsed_ms=" + std::to_string(ElapsedMs(seed_started_at)));
    if (seed_points.size() < 2U) {
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

        CmpInterpolationFacade cmp_interpolator;
        const auto cmp_started_at = std::chrono::steady_clock::now();
        log_stage("cmp_plan_start", "seed_points=" + std::to_string(seed_points.size()));
        points = cmp_interpolator.PositionTriggeredDispensing(path, triggers, cmp_config, config);
        log_stage(
            "cmp_plan_done",
            "points=" + std::to_string(points.size()) +
                " elapsed_ms=" + std::to_string(ElapsedMs(cmp_started_at)));
    } else {
        if (input.interpolation_algorithm == InterpolationAlgorithm::CMP_COORDINATED) {
            return Result<std::vector<TrajectoryPoint>>::Failure(Error(
                ErrorCode::TRAJECTORY_GENERATION_FAILED,
                "CMP插补缺少显式 trigger authority，不能退化为默认轨迹采样触发",
                "DispensePackagingAssembly"));
        }

        if (input.interpolation_algorithm != InterpolationAlgorithm::LINEAR) {
            TrajectoryInterpolationFacade interpolation_facade;
            auto interpolation_result =
                interpolation_facade.Interpolate(seed_points, input.interpolation_algorithm, config);
            if (interpolation_result.IsError()) {
                return Result<std::vector<TrajectoryPoint>>::Failure(interpolation_result.GetError());
            }
            points = interpolation_result.Value();
        }

        if (!trigger_artifacts.positions.empty() &&
            !ApplyTriggerMarkersByPositionWithDiagnostics(
                points,
                trigger_artifacts.positions,
                trigger_artifacts.distances,
                Siligen::Shared::Types::kTriggerMarkerDedupToleranceMm,
                std::max(config.position_tolerance, Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm))) {
            return Result<std::vector<TrajectoryPoint>>::Failure(Error(
                ErrorCode::TRAJECTORY_GENERATION_FAILED,
                "显式 trigger authority 映射到插补轨迹失败",
                "DispensePackagingAssembly"));
        }
    }

    if (points.empty()) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "插补结果为空", "DispensePackagingAssembly"));
    }

    log_stage(
        "complete",
        "points=" + std::to_string(points.size()) +
            " elapsed_ms=" + std::to_string(ElapsedMs(started_at)));
    return Result<std::vector<TrajectoryPoint>>::Success(std::move(points));
}

Result<ExecutionGenerationArtifacts> BuildExecutionGenerationArtifacts(
    const ExecutionAssemblyBuildInput& input,
    const ProcessPath& execution_process_path,
    const TriggerArtifacts& trigger_artifacts) {
    const auto geometry_kind = ResolveExecutionGeometryKind(execution_process_path);
    const bool has_formal_carrier_motion = HasFormalCarrierMotion(input.motion_plan);
    if (geometry_kind == DispensingExecutionGeometryKind::POINT &&
        input.requested_execution_strategy == DispensingExecutionStrategy::STATIONARY_SHOT &&
        !has_formal_carrier_motion) {
        ExecutionGenerationArtifacts artifacts;
        artifacts.motion_trajectory = input.motion_plan;
        artifacts.interpolation_points.push_back(BuildPointExecutionMarker(input, trigger_artifacts));
        return Result<ExecutionGenerationArtifacts>::Success(std::move(artifacts));
    }

    if (geometry_kind == DispensingExecutionGeometryKind::POINT &&
        input.requested_execution_strategy == DispensingExecutionStrategy::FLYING_SHOT &&
        !has_formal_carrier_motion) {
        return BuildPointFlyingExecutionArtifacts(input, execution_process_path, trigger_artifacts);
    }

    const auto execution_input = BuildExecutionPlanningInput(input, execution_process_path);

    auto interpolation_points_result =
        BuildInterpolationPoints(execution_input, execution_process_path, trigger_artifacts);
    if (interpolation_points_result.IsError()) {
        return Result<ExecutionGenerationArtifacts>::Failure(interpolation_points_result.GetError());
    }

    InterpolationProgramPlanner program_planner;
    auto interpolation_program =
        program_planner.BuildProgram(execution_process_path, input.motion_plan, input.acceleration);
    if (interpolation_program.IsError()) {
        return Result<ExecutionGenerationArtifacts>::Failure(interpolation_program.GetError());
    }

    {
        std::ostringstream oss;
        oss << "planning_artifacts_stage=execution_generation_ready"
            << " dxf=" << input.dxf_filename
            << " execution_segments=" << execution_process_path.segments.size()
            << " motion_points=" << input.motion_plan.points.size()
            << " interpolation_segments=" << interpolation_program.Value().size()
            << " interpolation_points=" << interpolation_points_result.Value().size()
            << " validation=" << trigger_artifacts.validation_classification;
        SILIGEN_LOG_INFO(oss.str());
    }

    ExecutionGenerationArtifacts artifacts;
    artifacts.interpolation_points = std::move(interpolation_points_result.Value());
    artifacts.interpolation_segments = std::move(interpolation_program.Value());
    artifacts.motion_trajectory = input.motion_plan;
    return Result<ExecutionGenerationArtifacts>::Success(std::move(artifacts));
}

}  // namespace Siligen::Application::Services::Dispensing::Internal
