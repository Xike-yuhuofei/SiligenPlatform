#include "runtime/system/LegacyMachineExecutionStateAdapter.h"

#include "shared/types/Error.h"

#include <utility>

namespace Siligen::Runtime::Host::System {
namespace {

using Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot;
using Siligen::Runtime::Host::System::ILegacyMachineExecutionStateBackend;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

constexpr const char* kErrorSource = "LegacyMachineExecutionStateAdapter";

Result<std::shared_ptr<ILegacyMachineExecutionStateBackend>> EnsureBackend(
    const std::shared_ptr<ILegacyMachineExecutionStateBackend>& backend) {
    if (!backend) {
        return Result<std::shared_ptr<ILegacyMachineExecutionStateBackend>>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "machine execution state backend not initialized",
            kErrorSource));
    }
    return Result<std::shared_ptr<ILegacyMachineExecutionStateBackend>>::Success(backend);
}

}  // namespace

LegacyMachineExecutionStateAdapter::LegacyMachineExecutionStateAdapter(
    std::shared_ptr<ILegacyMachineExecutionStateBackend> backend)
    : backend_(std::move(backend)) {}

Result<MachineExecutionSnapshot> LegacyMachineExecutionStateAdapter::ReadSnapshot() const {
    auto backend_result = EnsureBackend(backend_);
    if (backend_result.IsError()) {
        return Result<MachineExecutionSnapshot>::Failure(backend_result.GetError());
    }
    return backend_->ReadSnapshot();
}

Result<void> LegacyMachineExecutionStateAdapter::ClearPendingTasks() {
    auto backend_result = EnsureBackend(backend_);
    if (backend_result.IsError()) {
        return Result<void>::Failure(backend_result.GetError());
    }
    return backend_->ClearPendingTasks();
}

Result<void> LegacyMachineExecutionStateAdapter::TransitionToEmergencyStop() {
    auto backend_result = EnsureBackend(backend_);
    if (backend_result.IsError()) {
        return Result<void>::Failure(backend_result.GetError());
    }
    return backend_->TransitionToEmergencyStop();
}

Result<void> LegacyMachineExecutionStateAdapter::RecoverToUninitialized() {
    auto backend_result = EnsureBackend(backend_);
    if (backend_result.IsError()) {
        return Result<void>::Failure(backend_result.GetError());
    }
    return backend_->RecoverToUninitialized();
}

}  // namespace Siligen::Runtime::Host::System
