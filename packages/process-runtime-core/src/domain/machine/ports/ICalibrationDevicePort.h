#pragma once

#include "domain/machine/value-objects/CalibrationTypes.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <string>

namespace Siligen::Domain::Machine::Ports {

using Siligen::Domain::Machine::ValueObjects::CalibrationRequest;
using Siligen::Domain::Machine::ValueObjects::CalibrationState;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::Result;

/**
 * @brief Calibration device status snapshot.
 */
struct CalibrationDeviceStatus {
    CalibrationState state = CalibrationState::Idle;
    float32 progress = 0.0f;  // 0-100
    int32 error_code = 0;
    std::string error_message;

    bool HasError() const noexcept {
        return !error_message.empty() || error_code != 0;
    }
};

/**
 * @brief Calibration device port.
 *
 * Abstracts hardware execution of calibration flow.
 */
class ICalibrationDevicePort {
   public:
    virtual ~ICalibrationDevicePort() = default;

    virtual Result<void> Prepare(const CalibrationRequest& request) noexcept = 0;
    virtual Result<void> Execute(const CalibrationRequest& request) noexcept = 0;
    virtual Result<void> Verify(const CalibrationRequest& request) noexcept = 0;
    virtual Result<void> Stop() noexcept = 0;
    virtual Result<CalibrationDeviceStatus> GetStatus() const noexcept = 0;
};

}  // namespace Siligen::Domain::Machine::Ports
