#define MODULE_NAME "DispensingExecutionUseCase"

#include "DispensingExecutionUseCase.h"

#include "domain/dispensing/domain-services/DispensingProcessService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <limits>

using namespace Siligen::Shared::Types;

namespace Siligen::Application::UseCases::Dispensing {

Result<void> DispensingExecutionUseCase::ValidateHardwareConnection() noexcept {
    if (!process_service_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "点胶流程服务未初始化", "DispensingExecutionUseCase"));
    }
    return process_service_->ValidateHardwareConnection();
}

Result<void> DispensingExecutionUseCase::RefreshRuntimeParameters(const DispensingMVPRequest& request) noexcept {
    if (!process_service_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "点胶流程服务未初始化", "DispensingExecutionUseCase"));
    }

    Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides overrides;
    overrides.dry_run = request.dry_run;
    overrides.dispensing_speed_mm_s = request.dispensing_speed_mm_s;
    overrides.dry_run_speed_mm_s = request.dry_run_speed_mm_s;
    overrides.rapid_speed_mm_s = request.rapid_speed_mm_s;
    overrides.acceleration_mm_s2 = request.acceleration_mm_s2;
    overrides.velocity_guard_enabled = request.velocity_guard_enabled;
    overrides.velocity_guard_ratio = request.velocity_guard_ratio;
    overrides.velocity_guard_abs_mm_s = request.velocity_guard_abs_mm_s;
    overrides.velocity_guard_min_expected_mm_s = request.velocity_guard_min_expected_mm_s;
    overrides.velocity_guard_grace_ms = request.velocity_guard_grace_ms;
    overrides.velocity_guard_interval_ms = request.velocity_guard_interval_ms;
    overrides.velocity_guard_max_consecutive = request.velocity_guard_max_consecutive;
    overrides.velocity_guard_stop_on_violation = request.velocity_guard_stop_on_violation;

    auto runtime_result = process_service_->BuildRuntimeParams(overrides);
    if (runtime_result.IsError()) {
        return Result<void>::Failure(runtime_result.GetError());
    }

    runtime_params_ = runtime_result.Value();
    return Result<void>::Success();
}

DispensingPlanRequest DispensingExecutionUseCase::BuildPlanRequest(
    const DispensingMVPRequest& request) const noexcept {
    DispensingPlanRequest plan_request;
    plan_request.dxf_filepath = request.dxf_filepath;
    plan_request.optimize_path = request.optimize_path;
    plan_request.start_x = request.start_x;
    plan_request.start_y = request.start_y;
    plan_request.approximate_splines = request.approximate_splines;
    plan_request.two_opt_iterations = request.two_opt_iterations;
    plan_request.spline_max_step_mm = request.spline_max_step_mm;
    plan_request.spline_max_error_mm = request.spline_max_error_mm;
    plan_request.continuity_tolerance_mm = request.continuity_tolerance_mm;
    plan_request.curve_chain_angle_deg = request.curve_chain_angle_deg;
    plan_request.curve_chain_max_segment_mm = request.curve_chain_max_segment_mm;
    plan_request.use_hardware_trigger = request.use_hardware_trigger;
    plan_request.dispensing_velocity = runtime_params_.dispensing_velocity;
    plan_request.acceleration = runtime_params_.acceleration;
    plan_request.dispenser_interval_ms = runtime_params_.dispenser_interval_ms;
    plan_request.dispenser_duration_ms = runtime_params_.dispenser_duration_ms;
    plan_request.trigger_spatial_interval_mm = runtime_params_.trigger_spatial_interval_mm;
    plan_request.pulse_per_mm = runtime_params_.pulse_per_mm;
    plan_request.valve_response_ms = runtime_params_.valve_response_ms;
    plan_request.safety_margin_ms = runtime_params_.safety_margin_ms;
    plan_request.min_interval_ms = runtime_params_.min_interval_ms;
    plan_request.compensation_profile = runtime_params_.compensation_profile;
    plan_request.sample_dt = runtime_params_.sample_dt;
    plan_request.sample_ds = runtime_params_.sample_ds;
    plan_request.arc_tolerance_mm = request.arc_tolerance_mm;
    plan_request.max_jerk = (request.max_jerk > 0.0f) ? request.max_jerk : DispensingMVPRequest::JERK;
    plan_request.use_interpolation_planner = request.use_interpolation_planner;
    plan_request.interpolation_algorithm = request.interpolation_algorithm;

    plan_request.dispensing_strategy = DispensingStrategy::BASELINE;
    plan_request.subsegment_count = 8;
    plan_request.dispense_only_cruise = false;
    if (config_port_) {
        auto system_result = config_port_->LoadConfiguration();
        if (system_result.IsSuccess()) {
            const auto& config = system_result.Value();
            plan_request.dxf_offset = Point2D(config.dxf.offset_x, config.dxf.offset_y);
            plan_request.bounds_check_enabled = true;
            plan_request.bounds_x_min = config.machine.soft_limits.x_min;
            plan_request.bounds_x_max = config.machine.soft_limits.x_max;
            plan_request.bounds_y_min = config.machine.soft_limits.y_min;
            plan_request.bounds_y_max = config.machine.soft_limits.y_max;
        } else {
            SILIGEN_LOG_WARNING("加载系统配置失败: " + system_result.GetError().GetMessage());
        }

        auto dispensing_result = config_port_->GetDispensingConfig();
        if (dispensing_result.IsSuccess()) {
            const auto& disp = dispensing_result.Value();
            plan_request.dispensing_strategy = disp.strategy;
            if (disp.subsegment_count > 0) {
                plan_request.subsegment_count = disp.subsegment_count;
            }
            plan_request.dispense_only_cruise = disp.dispense_only_cruise;
            plan_request.start_speed_factor = disp.start_speed_factor;
            plan_request.end_speed_factor = disp.end_speed_factor;
            plan_request.corner_speed_factor = disp.corner_speed_factor;
            plan_request.rapid_speed_factor = disp.rapid_speed_factor;
        }

        auto traj_result = config_port_->GetDxfTrajectoryConfig();
        if (traj_result.IsSuccess()) {
            const auto& traj = traj_result.Value();
            plan_request.python_ruckig_python = traj.python;
            plan_request.python_ruckig_script = traj.script;
        }
    }

    return plan_request;
}

}  // namespace Siligen::Application::UseCases::Dispensing

