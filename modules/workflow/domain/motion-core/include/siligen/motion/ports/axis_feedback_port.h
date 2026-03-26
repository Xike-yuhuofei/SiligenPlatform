#pragma once

#include "siligen/motion/model/motion_types.h"
#include "siligen/shared/result_types.h"

namespace Siligen::Motion::Ports {

class AxisFeedbackPort {
   public:
    virtual ~AxisFeedbackPort() = default;

    virtual Siligen::SharedKernel::Result<Siligen::Motion::Model::MotionState> ReadState() const = 0;
};

}  // namespace Siligen::Motion::Ports
