#pragma once

#include "siligen/shared/result_types.h"

namespace Siligen::Motion::Ports {

class InterlockPort {
   public:
    virtual ~InterlockPort() = default;

    virtual Siligen::SharedKernel::Result<bool> IsMotionAllowed() const = 0;
};

}  // namespace Siligen::Motion::Ports
