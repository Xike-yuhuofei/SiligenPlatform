#pragma once

#include "domain/dispensing/ports/IDispensingExecutionObserver.h"
#include "runtime_execution/contracts/dispensing/DispensingExecutionTypes.h"

#include <atomic>

namespace Siligen::RuntimeExecution::Contracts::Dispensing {

class IDispensingProcessPort {
   public:
    virtual ~IDispensingProcessPort() = default;

    virtual Siligen::Shared::Types::Result<void> ValidateHardwareConnection() noexcept = 0;
    virtual Siligen::Shared::Types::Result<Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams>
    BuildRuntimeParams(
        const Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides& overrides) noexcept = 0;

    virtual Siligen::Shared::Types::Result<Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionReport>
    ExecuteProcess(
        const Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan& plan,
        const Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams& params,
        const Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionOptions& options,
        std::atomic<bool>* stop_flag,
        std::atomic<bool>* pause_flag,
        std::atomic<bool>* pause_applied_flag,
        Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver* observer = nullptr) noexcept = 0;

    virtual void StopExecution(
        std::atomic<bool>* stop_flag,
        std::atomic<bool>* pause_flag = nullptr) noexcept = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Dispensing
