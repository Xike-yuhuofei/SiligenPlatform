#pragma once

#include "siligen/motion/model/motion_types.h"
#include "siligen/shared/result_types.h"

namespace Siligen::Motion::Ports {

class MotionControllerPort {
   public:
    virtual ~MotionControllerPort() = default;

    virtual Siligen::SharedKernel::VoidResult Execute(const Siligen::Motion::Model::MotionCommand& command) = 0;
    virtual Siligen::SharedKernel::VoidResult StopAll(bool immediate) = 0;
};

}  // namespace Siligen::Motion::Ports
