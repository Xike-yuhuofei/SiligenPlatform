#include "DispensingExecutionCompatibilityService.h"

#include "shared/types/Error.h"

#include <utility>

namespace Siligen::Application::Services::Dispensing {

using Siligen::Application::UseCases::Dispensing::DispensingExecutionRequest;
using Siligen::Application::UseCases::Dispensing::DispensingMVPRequest;
using Siligen::Application::UseCases::Dispensing::PlanningRequest;
using Siligen::Application::UseCases::Dispensing::PlanningResponse;
using Siligen::Domain::Dispensing::ValueObjects::JobExecutionMode;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

namespace {

std::string ResolveSourcePath(const DispensingMVPRequest& request, const std::string& override_path) {
    return override_path.empty() ? request.dxf_filepath : override_path;
}

Result<DispensingMVPRequest> NormalizeLegacyRequest(
    const DispensingMVPRequest& request,
    const std::string& override_path) {
    auto normalized = request;
    normalized.dxf_filepath = ResolveSourcePath(request, override_path);

    auto validation = normalized.Validate();
    if (validation.IsError()) {
        return Result<DispensingMVPRequest>::Failure(validation.GetError());
    }

    return Result<DispensingMVPRequest>::Success(std::move(normalized));
}

Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated BuildExecutionPackageFromPlan(
    const Siligen::Domain::Dispensing::DomainServices::DispensingPlan& plan,
    const std::string& source_path) {
    using Siligen::Domain::Dispensing::Contracts::ExecutionPackageBuilt;
    using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;

    ExecutionPackageBuilt built;
    built.execution_plan.interpolation_segments = plan.interpolation_segments;
    built.execution_plan.interpolation_points = plan.interpolation_points;
    built.execution_plan.motion_trajectory = plan.motion_trajectory;
    built.execution_plan.trigger_distances_mm = plan.trigger_distances_mm;
    built.execution_plan.trigger_interval_ms = plan.trigger_interval_ms;
    built.execution_plan.trigger_interval_mm = plan.trigger_interval_mm;
    built.execution_plan.total_length_mm = plan.total_length_mm;
    built.total_length_mm = plan.total_length_mm > 0.0f ? plan.total_length_mm : plan.motion_trajectory.total_length;
    built.estimated_time_s = plan.estimated_time_s > 0.0f ? plan.estimated_time_s : plan.motion_trajectory.total_time;
    built.source_path = source_path;
    return ExecutionPackageValidated(built);
}

}  // namespace

Result<PlanningRequest> DispensingExecutionCompatibilityService::BuildPlanningRequest(
    const DispensingMVPRequest& request,
    const std::string& source_path) const {
    auto normalized_result = NormalizeLegacyRequest(request, source_path);
    if (normalized_result.IsError()) {
        return Result<PlanningRequest>::Failure(normalized_result.GetError());
    }

    const auto& normalized = normalized_result.Value();

    PlanningRequest planning_request;
    planning_request.dxf_filepath = normalized.dxf_filepath;
    planning_request.optimize_path = normalized.optimize_path;
    planning_request.start_x = normalized.start_x;
    planning_request.start_y = normalized.start_y;
    planning_request.approximate_splines = normalized.approximate_splines;
    planning_request.two_opt_iterations = normalized.two_opt_iterations;
    planning_request.spline_max_step_mm = normalized.spline_max_step_mm;
    planning_request.spline_max_error_mm = normalized.spline_max_error_mm;
    planning_request.continuity_tolerance_mm = normalized.continuity_tolerance_mm;
    planning_request.curve_chain_angle_deg = normalized.curve_chain_angle_deg;
    planning_request.curve_chain_max_segment_mm = normalized.curve_chain_max_segment_mm;
    planning_request.use_hardware_trigger = normalized.use_hardware_trigger;
    planning_request.use_interpolation_planner = normalized.use_interpolation_planner;
    planning_request.interpolation_algorithm = normalized.interpolation_algorithm;
    planning_request.trajectory_config.arc_tolerance = normalized.arc_tolerance_mm > 0.0f
                                                           ? normalized.arc_tolerance_mm
                                                           : planning_request.trajectory_config.arc_tolerance;
    planning_request.trajectory_config.max_jerk = normalized.max_jerk > 0.0f
                                                      ? normalized.max_jerk
                                                      : planning_request.trajectory_config.max_jerk;

    if (normalized.acceleration_mm_s2.has_value() && normalized.acceleration_mm_s2.value() > 0.0f) {
        planning_request.trajectory_config.max_acceleration = normalized.acceleration_mm_s2.value();
    }

    const auto execution_mode = normalized.ResolveExecutionMode();
    if (execution_mode == JobExecutionMode::ValidationDryCycle &&
        normalized.dry_run_speed_mm_s.has_value() &&
        normalized.dry_run_speed_mm_s.value() > 0.0f) {
        planning_request.trajectory_config.max_velocity = normalized.dry_run_speed_mm_s.value();
    } else if (normalized.dispensing_speed_mm_s.has_value() && normalized.dispensing_speed_mm_s.value() > 0.0f) {
        planning_request.trajectory_config.max_velocity = normalized.dispensing_speed_mm_s.value();
    } else if (normalized.rapid_speed_mm_s.has_value() && normalized.rapid_speed_mm_s.value() > 0.0f) {
        planning_request.trajectory_config.max_velocity = normalized.rapid_speed_mm_s.value();
    }

    return Result<PlanningRequest>::Success(std::move(planning_request));
}

Result<DispensingExecutionRequest> DispensingExecutionCompatibilityService::BuildExecutionRequest(
    const PlanningResponse& planning_response,
    const DispensingMVPRequest& request,
    const std::string& source_path) const {
    auto normalized_result = NormalizeLegacyRequest(request, source_path);
    if (normalized_result.IsError()) {
        return Result<DispensingExecutionRequest>::Failure(normalized_result.GetError());
    }
    if (!planning_response.execution_package && !planning_response.execution_plan) {
        return Result<DispensingExecutionRequest>::Failure(
            Error(
                ErrorCode::INVALID_STATE,
                "planning response missing execution package",
                "DispensingExecutionCompatibilityService"));
    }

    const auto& normalized = normalized_result.Value();

    DispensingExecutionRequest execution_request;
    if (planning_response.execution_package) {
        execution_request.execution_package = *planning_response.execution_package;
    } else {
        execution_request.execution_package =
            BuildExecutionPackageFromPlan(*planning_response.execution_plan, normalized.dxf_filepath);
    }
    execution_request.source_path = execution_request.execution_package.source_path;
    execution_request.use_hardware_trigger = normalized.use_hardware_trigger;
    execution_request.dry_run = normalized.dry_run;
    execution_request.machine_mode = normalized.machine_mode;
    execution_request.execution_mode = normalized.execution_mode;
    execution_request.output_policy = normalized.output_policy;
    execution_request.max_jerk = normalized.max_jerk;
    execution_request.arc_tolerance_mm = normalized.arc_tolerance_mm;
    execution_request.dispensing_speed_mm_s = normalized.dispensing_speed_mm_s;
    execution_request.dry_run_speed_mm_s = normalized.dry_run_speed_mm_s;
    execution_request.rapid_speed_mm_s = normalized.rapid_speed_mm_s;
    execution_request.acceleration_mm_s2 = normalized.acceleration_mm_s2;
    execution_request.velocity_trace_enabled = normalized.velocity_trace_enabled;
    execution_request.velocity_trace_interval_ms = normalized.velocity_trace_interval_ms;
    execution_request.velocity_trace_path = normalized.velocity_trace_path;
    execution_request.velocity_guard_enabled = normalized.velocity_guard_enabled;
    execution_request.velocity_guard_ratio = normalized.velocity_guard_ratio;
    execution_request.velocity_guard_abs_mm_s = normalized.velocity_guard_abs_mm_s;
    execution_request.velocity_guard_min_expected_mm_s = normalized.velocity_guard_min_expected_mm_s;
    execution_request.velocity_guard_grace_ms = normalized.velocity_guard_grace_ms;
    execution_request.velocity_guard_interval_ms = normalized.velocity_guard_interval_ms;
    execution_request.velocity_guard_max_consecutive = normalized.velocity_guard_max_consecutive;
    execution_request.velocity_guard_stop_on_violation = normalized.velocity_guard_stop_on_violation;

    auto validation = execution_request.Validate();
    if (validation.IsError()) {
        return Result<DispensingExecutionRequest>::Failure(validation.GetError());
    }

    return Result<DispensingExecutionRequest>::Success(std::move(execution_request));
}

}  // namespace Siligen::Application::Services::Dispensing
