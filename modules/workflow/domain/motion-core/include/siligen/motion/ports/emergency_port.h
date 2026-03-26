#pragma once

#include "siligen/shared/result_types.h"

namespace Siligen::Motion::Ports {

class EmergencyPort {
   public:
    virtual ~EmergencyPort() = default;

    virtual Siligen::SharedKernel::VoidResult TriggerEmergencyStop() = 0;
    virtual Siligen::SharedKernel::Result<bool> IsEmergencyStopActive() const = 0;
};

}  // namespace Siligen::Motion::Ports
