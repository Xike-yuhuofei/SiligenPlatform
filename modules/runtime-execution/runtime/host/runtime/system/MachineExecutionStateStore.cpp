#include "runtime/system/MachineExecutionStateStore.h"

#include "shared/types/Error.h"

#include <string>

namespace Siligen::Runtime::Service::System {
namespace {

using Siligen::RuntimeExecution::Contracts::System::MachineExecutionPhase;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

constexpr const char* kErrorSource = "MachineExecutionStateStore";

bool AllowsManualMotion(const MachineExecutionPhase phase) {
    return phase != MachineExecutionPhase::Unknown &&
           phase != MachineExecutionPhase::Fault &&
           phase != MachineExecutionPhase::EmergencyStop;
}

std::string MakeRecentErrorSummary(const MachineExecutionPhase phase) {
    switch (phase) {
        case MachineExecutionPhase::Fault:
            return "machine_in_error_state";
        case MachineExecutionPhase::EmergencyStop:
            return "machine_in_emergency_stop";
        default:
            return {};
    }
}

}  // namespace

MachineExecutionStateStore::MachineExecutionStateStore() {
    ApplyPhase(MachineExecutionPhase::Uninitialized);
    snapshot_.pending_task_count = 0;
    snapshot_.has_pending_tasks = false;
}

const Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot&
MachineExecutionStateStore::ReadSnapshot() const {
    return snapshot_;
}

Result<void> MachineExecutionStateStore::SetPhase(const MachineExecutionPhase phase) {
    ApplyPhase(phase);
    return Result<void>::Success();
}

Result<void> MachineExecutionStateStore::SetPendingTaskCount(const std::int32_t pending_task_count) {
    if (pending_task_count < 0) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "pending task count must not be negative",
                  kErrorSource));
    }

    snapshot_.pending_task_count = pending_task_count;
    snapshot_.has_pending_tasks = pending_task_count > 0;
    return Result<void>::Success();
}

Result<void> MachineExecutionStateStore::ClearPendingTasks() {
    return SetPendingTaskCount(0);
}

Result<void> MachineExecutionStateStore::TransitionToEmergencyStop() {
    ApplyPhase(MachineExecutionPhase::EmergencyStop);
    return Result<void>::Success();
}

Result<void> MachineExecutionStateStore::RecoverToUninitialized() {
    if (snapshot_.phase != MachineExecutionPhase::EmergencyStop) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE,
                  "machine execution state can only recover from emergency stop",
                  kErrorSource));
    }

    ApplyPhase(MachineExecutionPhase::Uninitialized);
    return Result<void>::Success();
}

void MachineExecutionStateStore::ApplyPhase(const MachineExecutionPhase phase) {
    snapshot_.phase = phase;
    snapshot_.emergency_stopped = phase == MachineExecutionPhase::EmergencyStop;
    snapshot_.manual_motion_allowed = AllowsManualMotion(phase);
    snapshot_.recent_error_summary = MakeRecentErrorSummary(phase);
}

}  // namespace Siligen::Runtime::Service::System
