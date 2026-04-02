#include "runtime/supervision/RuntimeExecutionSupervisionBackend.h"

#include "shared/types/Types.h"

#include <utility>

namespace Siligen::Runtime::Service::Supervision {
namespace {

using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Runtime::Host::Supervision::RuntimeSupervisionInputs;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

void ReadLimitIfAvailable(
    const std::shared_ptr<Siligen::Application::UseCases::Motion::MotionControlUseCase>& motion_control_use_case,
    LogicalAxisId axis,
    bool positive,
    bool& target) {
    if (!motion_control_use_case) {
        return;
    }
    auto limit_result = motion_control_use_case->ReadLimitStatus(axis, positive);
    if (limit_result.IsSuccess()) {
        target = limit_result.Value();
    }
}

}  // namespace

RuntimeExecutionSupervisionBackend::RuntimeExecutionSupervisionBackend(
    std::shared_ptr<Siligen::Application::UseCases::Motion::MotionControlUseCase> motion_control_use_case,
    std::shared_ptr<Siligen::Application::UseCases::System::EmergencyStopUseCase> emergency_stop_use_case,
    std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase> dispensing_execution_use_case,
    std::shared_ptr<Siligen::Domain::Safety::Ports::IInterlockSignalPort> interlock_signal_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> device_connection_port)
    : motion_control_use_case_(std::move(motion_control_use_case)),
      emergency_stop_use_case_(std::move(emergency_stop_use_case)),
      dispensing_execution_use_case_(std::move(dispensing_execution_use_case)),
      interlock_signal_port_(std::move(interlock_signal_port)),
      device_connection_port_(std::move(device_connection_port)) {}

Result<RuntimeSupervisionInputs> RuntimeExecutionSupervisionBackend::ReadInputs() const {
    RuntimeSupervisionInputs inputs;
    inputs.has_hardware_connection_port = static_cast<bool>(device_connection_port_);

    if (device_connection_port_) {
        auto connection_result = device_connection_port_->ReadConnection();
        if (connection_result.IsSuccess()) {
            inputs.connection_info = connection_result.Value();
        }
        inputs.heartbeat_status = device_connection_port_->ReadHeartbeat();
    }

    const bool hardware_ready =
        !inputs.has_hardware_connection_port ||
        (inputs.connection_info.IsConnected() && !inputs.heartbeat_status.is_degraded);
    const bool can_query_motion_state = !inputs.has_hardware_connection_port || hardware_ready;

    if (can_query_motion_state && motion_control_use_case_) {
        auto all_status_result = motion_control_use_case_->GetAllAxesMotionStatus();
        if (all_status_result.IsSuccess()) {
            inputs.connected = true;
            const auto& statuses = all_status_result.Value();
            for (size_t i = 0; i < statuses.size() && i < 4; ++i) {
                const auto& status = statuses[i];
                inputs.estop_active = inputs.estop_active || (status.state == MotionState::ESTOP);
                inputs.any_axis_fault = inputs.any_axis_fault || status.has_error || status.servo_alarm ||
                                        status.following_error || status.state == MotionState::FAULT;
                inputs.any_axis_moving = inputs.any_axis_moving || status.state == MotionState::MOVING;
                if (i == 0) {
                    inputs.home_boundary_x_active = status.home_signal;
                } else if (i == 1) {
                    inputs.home_boundary_y_active = status.home_signal;
                }
            }
        }

        ReadLimitIfAvailable(motion_control_use_case_, LogicalAxisId::X, true, inputs.io.limit_x_pos);
        ReadLimitIfAvailable(motion_control_use_case_, LogicalAxisId::X, false, inputs.io.limit_x_neg);
        ReadLimitIfAvailable(motion_control_use_case_, LogicalAxisId::Y, true, inputs.io.limit_y_pos);
        ReadLimitIfAvailable(motion_control_use_case_, LogicalAxisId::Y, false, inputs.io.limit_y_neg);
    }

    if (inputs.has_hardware_connection_port) {
        inputs.connected = hardware_ready;
    }

    if (emergency_stop_use_case_) {
        auto estop_state_result = emergency_stop_use_case_->IsInEmergencyStop();
        if (estop_state_result.IsSuccess() && inputs.connected) {
            inputs.estop_state_known = true;
            inputs.estop_active = inputs.estop_active || estop_state_result.Value();
        }
    }

    inputs.io.estop = inputs.estop_active;
    inputs.io.estop_known = inputs.connected;

    if (interlock_signal_port_) {
        auto interlock_result = interlock_signal_port_->ReadSignals();
        if (interlock_result.IsSuccess()) {
            const auto& signals = interlock_result.Value();
            inputs.io.estop = inputs.estop_active || signals.emergency_stop_triggered;
            inputs.io.estop_known = true;
            inputs.io.door = signals.safety_door_open;
            inputs.io.door_known = true;
        }
        inputs.interlock_latched = interlock_signal_port_->IsLatched();
    }

    if (dispensing_execution_use_case_) {
        inputs.active_job_id = dispensing_execution_use_case_->GetActiveJobId();
        if (!inputs.active_job_id.empty()) {
            auto job_status_result = dispensing_execution_use_case_->GetJobStatus(inputs.active_job_id);
            if (job_status_result.IsSuccess()) {
                inputs.active_job_status_available = true;
                inputs.active_job_state = job_status_result.Value().state;
            } else {
                inputs.active_job_state = "unknown";
            }
        }
    }

    return Result<RuntimeSupervisionInputs>::Success(std::move(inputs));
}

}  // namespace Siligen::Runtime::Service::Supervision
