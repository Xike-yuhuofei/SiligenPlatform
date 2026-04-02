#include "runtime/system/LegacyMachineExecutionStateAdapter.h"

#include "domain/machine/aggregates/DispenserModel.h"
#include "shared/types/Error.h"
#include "shared/types/Types.h"

#include <utility>

namespace Siligen::Runtime::Host::System {
namespace {

using Siligen::RuntimeExecution::Contracts::System::MachineExecutionPhase;
using Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot;
using Siligen::Domain::Machine::Aggregates::Legacy::DispenserModel;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

constexpr const char* kErrorSource = "LegacyMachineExecutionStateAdapter";

MachineExecutionPhase MapPhase(const Siligen::DispenserState state) {
    switch (state) {
        case Siligen::DispenserState::UNINITIALIZED:
            return MachineExecutionPhase::Uninitialized;
        case Siligen::DispenserState::INITIALIZING:
            return MachineExecutionPhase::Initializing;
        case Siligen::DispenserState::READY:
            return MachineExecutionPhase::Ready;
        case Siligen::DispenserState::DISPENSING:
            return MachineExecutionPhase::Running;
        case Siligen::DispenserState::PAUSED:
            return MachineExecutionPhase::Paused;
        case Siligen::DispenserState::ERROR_STATE:
            return MachineExecutionPhase::Fault;
        case Siligen::DispenserState::EMERGENCY_STOP:
            return MachineExecutionPhase::EmergencyStop;
        default:
            return MachineExecutionPhase::Unknown;
    }
}

Result<std::shared_ptr<DispenserModel>> EnsureModel(
    const std::shared_ptr<DispenserModel>& dispenser_model) {
    if (!dispenser_model) {
        return Result<std::shared_ptr<DispenserModel>>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "machine execution state model not initialized", kErrorSource));
    }
    return Result<std::shared_ptr<DispenserModel>>::Success(dispenser_model);
}

}  // namespace

LegacyMachineExecutionStateAdapter::LegacyMachineExecutionStateAdapter()
    : dispenser_model_(std::make_shared<DispenserModel>()) {}

LegacyMachineExecutionStateAdapter::LegacyMachineExecutionStateAdapter(
    std::shared_ptr<DispenserModel> dispenser_model)
    : dispenser_model_(std::move(dispenser_model)) {}

Result<MachineExecutionSnapshot> LegacyMachineExecutionStateAdapter::ReadSnapshot() const {
    auto model_result = EnsureModel(dispenser_model_);
    if (model_result.IsError()) {
        return Result<MachineExecutionSnapshot>::Failure(model_result.GetError());
    }

    MachineExecutionSnapshot snapshot;
    const auto state = dispenser_model_->GetState();
    snapshot.phase = MapPhase(state);
    snapshot.emergency_stopped = state == Siligen::DispenserState::EMERGENCY_STOP;
    snapshot.manual_motion_allowed =
        state != Siligen::DispenserState::EMERGENCY_STOP &&
        state != Siligen::DispenserState::ERROR_STATE;

    auto queue_size_result = dispenser_model_->GetTaskQueueSize();
    if (queue_size_result.IsError()) {
        return Result<MachineExecutionSnapshot>::Failure(queue_size_result.GetError());
    }
    snapshot.pending_task_count = queue_size_result.Value();
    snapshot.has_pending_tasks = snapshot.pending_task_count > 0;

    if (state == Siligen::DispenserState::ERROR_STATE) {
        snapshot.recent_error_summary = "machine_in_error_state";
    } else if (snapshot.emergency_stopped) {
        snapshot.recent_error_summary = "machine_in_emergency_stop";
    }

    return Result<MachineExecutionSnapshot>::Success(std::move(snapshot));
}

Result<void> LegacyMachineExecutionStateAdapter::ClearPendingTasks() {
    auto model_result = EnsureModel(dispenser_model_);
    if (model_result.IsError()) {
        return Result<void>::Failure(model_result.GetError());
    }
    return dispenser_model_->ClearAllTasks();
}

Result<void> LegacyMachineExecutionStateAdapter::TransitionToEmergencyStop() {
    auto model_result = EnsureModel(dispenser_model_);
    if (model_result.IsError()) {
        return Result<void>::Failure(model_result.GetError());
    }
    return dispenser_model_->SetState(Siligen::DispenserState::EMERGENCY_STOP);
}

Result<void> LegacyMachineExecutionStateAdapter::RecoverToUninitialized() {
    auto model_result = EnsureModel(dispenser_model_);
    if (model_result.IsError()) {
        return Result<void>::Failure(model_result.GetError());
    }
    return dispenser_model_->SetState(Siligen::DispenserState::UNINITIALIZED);
}

}  // namespace Siligen::Runtime::Host::System
