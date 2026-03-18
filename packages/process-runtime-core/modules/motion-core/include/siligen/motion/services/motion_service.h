#pragma once

#include "siligen/motion/model/motion_types.h"
#include "siligen/shared/result_types.h"

namespace Siligen::Motion::Services {

class MotionService {
   public:
    virtual ~MotionService() = default;

    virtual Siligen::SharedKernel::VoidResult Execute(const Siligen::Motion::Model::MotionCommand& command) = 0;
    virtual Siligen::SharedKernel::Result<Siligen::Motion::Model::MotionState> QueryState() const = 0;
};

}  // namespace Siligen::Motion::Services
