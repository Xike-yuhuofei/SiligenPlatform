#pragma once

#include "shared/types/Types.h"

#include <string>

namespace Siligen::Domain::Machine::ValueObjects {

using Siligen::Shared::Types::int32;

/**
 * @brief Calibration workflow state.
 */
enum class CalibrationState {
    Idle,
    Validating,
    Executing,
    Verifying,
    Completed,
    Failed,
    Canceled
};

/**
 * @brief Calibration request payload.
 *
 * Keep this minimal so application/infrastructure can extend via ports.
 */
struct CalibrationRequest {
    std::string calibration_id;
    int32 timeout_ms = 60000;
    bool wait_for_completion = true;

    bool Validate() const noexcept {
        return timeout_ms > 0 && timeout_ms <= 300000;
    }
};

/**
 * @brief Calibration result summary.
 */
struct CalibrationResult {
    std::string calibration_id;
    CalibrationState state = CalibrationState::Idle;
    bool success = false;
    int32 duration_ms = 0;
    std::string message;
};

}  // namespace Siligen::Domain::Machine::ValueObjects
