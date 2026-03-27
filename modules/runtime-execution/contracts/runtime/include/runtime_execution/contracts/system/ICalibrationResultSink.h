#pragma once

#include "runtime_execution/contracts/system/CalibrationExecutionTypes.h"
#include "shared/types/Result.h"

namespace Siligen::RuntimeExecution::Contracts::System {

class ICalibrationResultSink {
   public:
    virtual ~ICalibrationResultSink() = default;

    virtual Siligen::Shared::Types::Result<void> SaveResult(
        const CalibrationExecutionResult& result) noexcept = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::System
