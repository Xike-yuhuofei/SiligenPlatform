#include "runtime/status/WorkflowRuntimeStatusExportPort.h"

#include "shared/types/Error.h"

#include <cstddef>
#include <utility>

namespace Siligen::Runtime::Service::Status {
namespace {

using MotionControlUseCase = Siligen::Application::UseCases::Motion::MotionControlUseCase;
using IRuntimeSupervisionPort = Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort;
using RuntimeAxisStatusExportSnapshot =
    Siligen::RuntimeExecution::Contracts::System::RuntimeAxisStatusExportSnapshot;
using RuntimeStatusExportSnapshot = Siligen::RuntimeExecution::Contracts::System::RuntimeStatusExportSnapshot;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

constexpr const char* kErrorSource = "WorkflowRuntimeStatusExportPort";

Result<std::shared_ptr<IRuntimeSupervisionPort>> EnsureSupervisionPort(
    const std::shared_ptr<IRuntimeSupervisionPort>& runtime_supervision_port) {
    if (!runtime_supervision_port) {
        return Result<std::shared_ptr<IRuntimeSupervisionPort>>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "runtime supervision port not initialized",
            kErrorSource));
    }
    return Result<std::shared_ptr<IRuntimeSupervisionPort>>::Success(runtime_supervision_port);
}

Result<std::shared_ptr<MotionControlUseCase>> EnsureMotionControlUseCase(
    const std::shared_ptr<MotionControlUseCase>& motion_control_use_case) {
    if (!motion_control_use_case) {
        return Result<std::shared_ptr<MotionControlUseCase>>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "motion control use case not initialized",
            kErrorSource));
    }
    return Result<std::shared_ptr<MotionControlUseCase>>::Success(motion_control_use_case);
}

RuntimeAxisStatusExportSnapshot BuildAxisStatusSnapshot(const Siligen::Domain::Motion::Ports::MotionStatus& status) {
    RuntimeAxisStatusExportSnapshot snapshot;
    snapshot.position = status.position.x;
    snapshot.velocity = status.velocity;
    snapshot.enabled = status.enabled;
    snapshot.homed = status.homing_state == "homed";
    snapshot.homing_state = status.homing_state;
    return snapshot;
}

}  // namespace

WorkflowRuntimeStatusExportPort::WorkflowRuntimeStatusExportPort(
    std::shared_ptr<IRuntimeSupervisionPort> runtime_supervision_port,
    std::shared_ptr<MotionControlUseCase> motion_control_use_case,
    std::shared_ptr<Siligen::Application::UseCases::Dispensing::Valve::ValveQueryUseCase> valve_query_use_case)
    : runtime_supervision_port_(std::move(runtime_supervision_port)),
      motion_control_use_case_(std::move(motion_control_use_case)),
      valve_query_use_case_(std::move(valve_query_use_case)) {}

Result<RuntimeStatusExportSnapshot> WorkflowRuntimeStatusExportPort::ReadSnapshot() const {
    auto supervision_port_result = EnsureSupervisionPort(runtime_supervision_port_);
    if (supervision_port_result.IsError()) {
        return Result<RuntimeStatusExportSnapshot>::Failure(supervision_port_result.GetError());
    }

    auto motion_use_case_result = EnsureMotionControlUseCase(motion_control_use_case_);
    if (motion_use_case_result.IsError()) {
        return Result<RuntimeStatusExportSnapshot>::Failure(motion_use_case_result.GetError());
    }

    auto supervision_result = runtime_supervision_port_->ReadSnapshot();
    if (supervision_result.IsError()) {
        return Result<RuntimeStatusExportSnapshot>::Failure(supervision_result.GetError());
    }

    RuntimeStatusExportSnapshot snapshot;
    const auto& supervision = supervision_result.Value();
    snapshot.connected = supervision.connected;
    snapshot.connection_state = supervision.connection_state;
    snapshot.machine_state = supervision.supervision.current_state;
    snapshot.machine_state_reason = supervision.supervision.state_reason;
    snapshot.interlock_latched = supervision.interlock_latched;
    snapshot.active_job_id = supervision.active_job_id;
    snapshot.active_job_state = supervision.active_job_state;
    snapshot.io = supervision.io;
    snapshot.effective_interlocks = supervision.effective_interlocks;
    snapshot.supervision = supervision.supervision;

    if (snapshot.connected) {
        auto all_status_result = motion_control_use_case_->GetAllAxesMotionStatus();
        if (all_status_result.IsSuccess()) {
            const auto& statuses = all_status_result.Value();
            const char* axis_names[] = {"X", "Y", "Z", "U"};
            for (size_t i = 0; i < statuses.size() && i < 4; ++i) {
                snapshot.axes[axis_names[i]] = BuildAxisStatusSnapshot(statuses[i]);
            }
        }

        auto position_result = motion_control_use_case_->GetCurrentPosition();
        if (position_result.IsSuccess()) {
            const auto position = position_result.Value();
            snapshot.has_position = true;
            snapshot.position.x = position.x;
            snapshot.position.y = position.y;
        }
    }

    if (valve_query_use_case_) {
        auto dispenser_result = valve_query_use_case_->GetDispenserStatus();
        if (dispenser_result.IsSuccess()) {
            snapshot.dispenser.valve_open = dispenser_result.Value().IsRunning();
        }

        auto supply_result = valve_query_use_case_->GetSupplyStatus();
        if (supply_result.IsSuccess()) {
            snapshot.dispenser.supply_open = supply_result.Value().IsOpen();
        }
    }

    return Result<RuntimeStatusExportSnapshot>::Success(std::move(snapshot));
}

}  // namespace Siligen::Runtime::Service::Status
