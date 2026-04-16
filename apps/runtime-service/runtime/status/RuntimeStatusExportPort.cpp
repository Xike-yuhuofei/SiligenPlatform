#include "runtime/status/RuntimeStatusExportPort.h"

#include "shared/types/Error.h"

#include <cstddef>
#include <string>
#include <utility>

namespace Siligen::Runtime::Service::Status {
namespace {

using IRuntimeSupervisionPort = Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort;
using RuntimeAxisStatusExportSnapshot =
    Siligen::RuntimeExecution::Contracts::System::RuntimeAxisStatusExportSnapshot;
using RuntimeJobExecutionExportSnapshot =
    Siligen::RuntimeExecution::Contracts::System::RuntimeJobExecutionExportSnapshot;
using RuntimeStatusExportSnapshot = Siligen::RuntimeExecution::Contracts::System::RuntimeStatusExportSnapshot;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

constexpr const char* kErrorSource = "RuntimeStatusExportPort";

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

bool IsTerminalJobState(const std::string& state) {
    return state == "completed" || state == "failed" || state == "cancelled";
}

RuntimeJobExecutionExportSnapshot BuildIdleJobExecutionSnapshot() {
    RuntimeJobExecutionExportSnapshot snapshot;
    snapshot.state = "idle";
    return snapshot;
}

RuntimeJobExecutionExportSnapshot BuildUnknownJobExecutionSnapshot(
    const std::string& job_id,
    const std::string& error_message) {
    RuntimeJobExecutionExportSnapshot snapshot;
    snapshot.job_id = job_id;
    snapshot.state = "unknown";
    snapshot.error_message = error_message;
    return snapshot;
}

}  // namespace

RuntimeStatusExportPort::RuntimeStatusExportPort(
    std::shared_ptr<IRuntimeSupervisionPort> runtime_supervision_port,
    RuntimeMotionStatusReader motion_status_reader,
    RuntimeDispenserStatusReader dispenser_status_reader,
    RuntimeJobExecutionStatusReader job_execution_status_reader)
    : runtime_supervision_port_(std::move(runtime_supervision_port)),
      motion_status_reader_(std::move(motion_status_reader)),
      dispenser_status_reader_(std::move(dispenser_status_reader)),
      job_execution_status_reader_(std::move(job_execution_status_reader)) {}

Result<RuntimeStatusExportSnapshot> RuntimeStatusExportPort::ReadSnapshot() const {
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
    snapshot.io = supervision.io;
    snapshot.effective_interlocks = supervision.effective_interlocks;
    snapshot.supervision = supervision.supervision;
    snapshot.job_execution = BuildIdleJobExecutionSnapshot();

    if (!supervision.active_job_id.empty()) {
        {
            std::lock_guard<std::mutex> lock(job_execution_mutex_);
            last_observed_job_id_ = supervision.active_job_id;
            cached_terminal_job_execution_.reset();
        }

        if (job_execution_status_reader_.read_job_status) {
            auto job_status_result = job_execution_status_reader_.read_job_status(supervision.active_job_id);
            if (job_status_result.IsSuccess()) {
                snapshot.job_execution = job_status_result.Value();
            } else {
                snapshot.job_execution = BuildUnknownJobExecutionSnapshot(
                    supervision.active_job_id,
                    job_status_result.GetError().GetMessage());
            }
        } else {
            snapshot.job_execution = BuildUnknownJobExecutionSnapshot(
                supervision.active_job_id,
                "job execution authority unavailable");
        }
    } else {
        std::optional<RuntimeJobExecutionExportSnapshot> cached_terminal_job_execution;
        std::string last_observed_job_id;
        {
            std::lock_guard<std::mutex> lock(job_execution_mutex_);
            cached_terminal_job_execution = cached_terminal_job_execution_;
            last_observed_job_id = last_observed_job_id_;
        }

        if (cached_terminal_job_execution.has_value()) {
            snapshot.job_execution = cached_terminal_job_execution.value();
        } else if (!last_observed_job_id.empty() && job_execution_status_reader_.read_job_status) {
            auto job_status_result = job_execution_status_reader_.read_job_status(last_observed_job_id);
            if (job_status_result.IsSuccess() && IsTerminalJobState(job_status_result.Value().state)) {
                snapshot.job_execution = job_status_result.Value();
                std::lock_guard<std::mutex> lock(job_execution_mutex_);
                cached_terminal_job_execution_ = snapshot.job_execution;
            } else {
                std::lock_guard<std::mutex> lock(job_execution_mutex_);
                last_observed_job_id_.clear();
            }
        }
    }

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
            const auto& dispenser = dispenser_result.Value();
            snapshot.dispenser.valve_open = dispenser.IsRunning();
            snapshot.dispenser.completedCount = dispenser.completedCount;
            snapshot.dispenser.totalCount = dispenser.totalCount;
            snapshot.dispenser.progress = dispenser.progress;
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
