#pragma once

#include "siligen/motion/safety/interlock_types.h"
#include "siligen/shared/result.h"

namespace Siligen::Motion::Safety::Ports {

class InterlockSignalPort {
   public:
    virtual ~InterlockSignalPort() = default;

    virtual Siligen::SharedKernel::Result<InterlockSignals> ReadSignals() const noexcept = 0;
};

}  // namespace Siligen::Motion::Safety::Ports
