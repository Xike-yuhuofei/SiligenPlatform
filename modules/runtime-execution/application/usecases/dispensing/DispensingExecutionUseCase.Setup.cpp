#define MODULE_NAME "DispensingExecutionUseCase"

#include "DispensingExecutionUseCase.Internal.h"

#include "runtime_execution/contracts/safety/InterlockPolicy.h"
#include "runtime_execution/contracts/safety/SafetyOutputGuard.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <limits>
#include <utility>

using namespace Siligen::Shared::Types;

namespace Siligen::Application::UseCases::Dispensing {

using Domain::Motion::Ports::MotionState;
using Domain::Motion::Ports::MotionStatus;
using Domain::Safety::DomainServices::InterlockPolicy;
using Domain::Safety::ValueObjects::InterlockCause;
using Domain::Safety::ValueObjects::InterlockPolicyConfig;

Result<void> DispensingExecutionUseCase::Impl::ValidateHardwareConnection(bool allow_disconnected) noexcept {
    if (!process_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "点胶流程服务未初始化", "DispensingExecutionUseCase"));
    }
    if (allow_disconnected) {
        return Result<void>::Success();
    }
    return process_port_->ValidateHardwareConnection();
}

Result<void> DispensingExecutionUseCase::Impl::ValidateExecutionPreconditions(bool allow_disconnected) const noexcept {
    if (!connection_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "hardware connection port not available", "DispensingExecutionUseCase"));
    }
    if (!allow_disconnected && !connection_port_->IsConnected()) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "hardware not connected", "DispensingExecutionUseCase"));
    }
    if (!motion_state_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "motion state port not available", "DispensingExecutionUseCase"));
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
                Error(ErrorCode::EMERGENCY_STOP_ACTIVATED, "axis in emergency stop state", "DispensingExecutionUseCase"));
        }
        if (!status.enabled || status.state == MotionState::DISABLED) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "required axis not enabled", "DispensingExecutionUseCase"));
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
                Error(ErrorCode::AXIS_NOT_HOMED, "required axis not homed", "DispensingExecutionUseCase"));
        }
        if (status.has_error || status.servo_alarm || status.following_error) {
            return Result<void>::Failure(
                Error(ErrorCode::HARDWARE_ERROR, "required axis has active error", "DispensingExecutionUseCase"));
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
                        Error(ErrorCode::EMERGENCY_STOP_ACTIVATED, reason, "DispensingExecutionUseCase"));
                case InterlockCause::SERVO_ALARM:
                    return Result<void>::Failure(
                        Error(ErrorCode::HARDWARE_ERROR, reason, "DispensingExecutionUseCase"));
                case InterlockCause::SAFETY_DOOR_OPEN:
                case InterlockCause::PRESSURE_ABNORMAL:
                case InterlockCause::TEMPERATURE_ABNORMAL:
                case InterlockCause::VOLTAGE_ABNORMAL:
                    return Result<void>::Failure(
                        Error(ErrorCode::INVALID_STATE, reason, "DispensingExecutionUseCase"));
                case InterlockCause::NONE:
                default:
                    return Result<void>::Failure(
                        Error(ErrorCode::INVALID_STATE, "interlock triggered", "DispensingExecutionUseCase"));
            }
        }
    }

    return Result<void>::Success();
}

Result<void> DispensingExecutionUseCase::Impl::RefreshRuntimeParameters(
    const DispensingExecutionRequest& request) noexcept {
    if (!process_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "点胶流程服务未初始化", "DispensingExecutionUseCase"));
    }

    const auto resolved_machine_mode = request.ResolveMachineMode();
    const auto resolved_execution_mode = request.ResolveExecutionMode();
    const auto resolved_output_policy = request.ResolveOutputPolicy();
    auto guard_result = Domain::Safety::DomainServices::SafetyOutputGuard::Evaluate(
        resolved_machine_mode,
        resolved_execution_mode,
        resolved_output_policy);
    if (guard_result.IsError()) {
        return Result<void>::Failure(guard_result.GetError());
    }

    Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides overrides;
    overrides.dry_run = request.dry_run;
    overrides.machine_mode = resolved_machine_mode;
    overrides.execution_mode = resolved_execution_mode;
    overrides.output_policy = resolved_output_policy;
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

    auto runtime_result = process_port_->BuildRuntimeParams(overrides);
    if (runtime_result.IsError()) {
        return Result<void>::Failure(runtime_result.GetError());
    }

    runtime_params_ = runtime_result.Value();
    resolved_execution_.machine_mode = resolved_machine_mode;
    resolved_execution_.execution_mode = resolved_execution_mode;
    resolved_execution_.output_policy = resolved_output_policy;
    resolved_execution_.guard_decision = guard_result.Value();
    return Result<void>::Success();
}

}  // namespace Siligen::Application::UseCases::Dispensing

