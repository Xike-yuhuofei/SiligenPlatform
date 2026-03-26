#pragma once

#include "domain/machine/value-objects/CalibrationTypes.h"
#include "shared/types/Result.h"

namespace Siligen::Domain::Machine::Ports {

using Siligen::Domain::Machine::ValueObjects::CalibrationResult;
using Siligen::Shared::Types::Result;

/**
 * @brief Calibration result persistence port.
 */
class ICalibrationResultPort {
   public:
    virtual ~ICalibrationResultPort() = default;

    virtual Result<void> SaveResult(const CalibrationResult& result) noexcept = 0;
};

}  // namespace Siligen::Domain::Machine::Ports
