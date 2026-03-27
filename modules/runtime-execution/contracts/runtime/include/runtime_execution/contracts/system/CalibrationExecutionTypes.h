#pragma once

#include "shared/types/Types.h"

#include <string>

namespace Siligen::RuntimeExecution::Contracts::System {

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;

enum class CalibrationExecutionState {
    Idle,
    Validating,
    Executing,
    Verifying,
    Completed,
    Failed,
    Canceled
};

struct CalibrationExecutionRequest {
    std::string calibration_id;
    int32 timeout_ms = 60000;
    bool wait_for_completion = true;

    bool Validate() const noexcept {
        return timeout_ms > 0 && timeout_ms <= 300000;
    }
};

struct CalibrationExecutionStatus {
    CalibrationExecutionState state = CalibrationExecutionState::Idle;
    float32 progress = 0.0f;
    int32 error_code = 0;
    std::string error_message;

    bool HasError() const noexcept {
        return !error_message.empty() || error_code != 0;
    }
};

struct CalibrationExecutionResult {
    std::string calibration_id;
    CalibrationExecutionState state = CalibrationExecutionState::Idle;
    bool success = false;
    int32 duration_ms = 0;
    std::string message;
};

}  // namespace Siligen::RuntimeExecution::Contracts::System
