#pragma once

#include "runtime_execution/contracts/system/CalibrationExecutionTypes.h"
#include "shared/types/Result.h"

namespace Siligen::RuntimeExecution::Contracts::System {

class ICalibrationExecutionPort {
   public:
    virtual ~ICalibrationExecutionPort() = default;

    virtual Siligen::Shared::Types::Result<void> Prepare(
        const CalibrationExecutionRequest& request) noexcept = 0;
    virtual Siligen::Shared::Types::Result<void> Execute(
        const CalibrationExecutionRequest& request) noexcept = 0;
    virtual Siligen::Shared::Types::Result<void> Verify(
        const CalibrationExecutionRequest& request) noexcept = 0;
    virtual Siligen::Shared::Types::Result<void> Stop() noexcept = 0;
    virtual Siligen::Shared::Types::Result<CalibrationExecutionStatus> GetStatus() const noexcept = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::System
