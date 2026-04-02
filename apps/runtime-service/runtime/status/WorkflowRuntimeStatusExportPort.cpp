#include "runtime/status/WorkflowRuntimeStatusExportPort.h"

#include "shared/types/Error.h"

#include <cstddef>
#include <string>
#include <utility>

namespace Siligen::Runtime::Service::Status {
namespace {

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
    RuntimeMotionStatusReader motion_status_reader,
    RuntimeDispenserStatusReader dispenser_status_reader)
    : runtime_supervision_port_(std::move(runtime_supervision_port)),
      motion_status_reader_(std::move(motion_status_reader)),
      dispenser_status_reader_(std::move(dispenser_status_reader)) {}

Result<RuntimeStatusExportSnapshot> WorkflowRuntimeStatusExportPort::ReadSnapshot() const {
    auto supervision_port_result = EnsureSupervisionPort(runtime_supervision_port_);
    if (supervision_port_result.IsError()) {
        return Result<RuntimeStatusExportSnapshot>::Failure(supervision_port_result.GetError());
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

    if (snapshot.connected && motion_status_reader_.read_all_axes_motion_status) {
        auto all_status_result = motion_status_reader_.read_all_axes_motion_status();
        if (all_status_result.IsSuccess()) {
            const auto& statuses = all_status_result.Value();
            const char* axis_names[] = {"X", "Y", "Z", "U"};
            for (size_t i = 0; i < statuses.size() && i < 4; ++i) {
                snapshot.axes[axis_names[i]] = BuildAxisStatusSnapshot(statuses[i]);
            }
        }
    }

    if (snapshot.connected && motion_status_reader_.read_current_position) {
        auto position_result = motion_status_reader_.read_current_position();
        if (position_result.IsSuccess()) {
            const auto position = position_result.Value();
            snapshot.has_position = true;
            snapshot.position.x = position.x;
            snapshot.position.y = position.y;
        }
    }

    if (dispenser_status_reader_.read_dispenser_status) {
        auto dispenser_result = dispenser_status_reader_.read_dispenser_status();
        if (dispenser_result.IsSuccess()) {
            snapshot.dispenser.valve_open = dispenser_result.Value().IsRunning();
        }
    }

    if (dispenser_status_reader_.read_supply_status) {
        auto supply_result = dispenser_status_reader_.read_supply_status();
        if (supply_result.IsSuccess()) {
            snapshot.dispenser.supply_open = supply_result.Value().IsOpen();
        }
    }

    return Result<RuntimeStatusExportSnapshot>::Success(std::move(snapshot));
}

}  // namespace Siligen::Runtime::Service::Status
