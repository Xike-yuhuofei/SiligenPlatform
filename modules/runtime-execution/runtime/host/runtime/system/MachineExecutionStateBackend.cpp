#include "runtime/system/MachineExecutionStateBackend.h"

#include "shared/types/Error.h"

#include <utility>

namespace Siligen::Runtime::Service::System {
namespace {

using Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

constexpr const char* kErrorSource = "MachineExecutionStateBackend";

Result<std::shared_ptr<MachineExecutionStateStore>> EnsureStateStore(
    const std::shared_ptr<MachineExecutionStateStore>& state_store) {
    if (!state_store) {
        return Result<std::shared_ptr<MachineExecutionStateStore>>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "machine execution state store not initialized", kErrorSource));
    }
    return Result<std::shared_ptr<MachineExecutionStateStore>>::Success(state_store);
}

}  // namespace

MachineExecutionStateBackend::MachineExecutionStateBackend()
    : state_store_(std::make_shared<MachineExecutionStateStore>()) {}

MachineExecutionStateBackend::MachineExecutionStateBackend(std::shared_ptr<MachineExecutionStateStore> state_store)
    : state_store_(std::move(state_store)) {}

Result<MachineExecutionSnapshot> MachineExecutionStateBackend::ReadSnapshot() const {
    auto state_store_result = EnsureStateStore(state_store_);
    if (state_store_result.IsError()) {
        return Result<MachineExecutionSnapshot>::Failure(state_store_result.GetError());
    }

    return Result<MachineExecutionSnapshot>::Success(state_store_->ReadSnapshot());
}

Result<void> MachineExecutionStateBackend::ClearPendingTasks() {
    auto state_store_result = EnsureStateStore(state_store_);
    if (state_store_result.IsError()) {
        return Result<void>::Failure(state_store_result.GetError());
    }
    return state_store_->ClearPendingTasks();
}

Result<void> MachineExecutionStateBackend::TransitionToEmergencyStop() {
    auto state_store_result = EnsureStateStore(state_store_);
    if (state_store_result.IsError()) {
        return Result<void>::Failure(state_store_result.GetError());
    }
    return state_store_->TransitionToEmergencyStop();
}

Result<void> MachineExecutionStateBackend::RecoverToUninitialized() {
    auto state_store_result = EnsureStateStore(state_store_);
    if (state_store_result.IsError()) {
        return Result<void>::Failure(state_store_result.GetError());
    }
    return state_store_->RecoverToUninitialized();
}

}  // namespace Siligen::Runtime::Service::System
