#pragma once

#include "domain/motion/ports/IAxisControlPort.h"
#include "domain/motion/ports/IHomingPort.h"
#include "domain/motion/ports/IJogControlPort.h"
#include "domain/motion/ports/IMotionConnectionPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "domain/motion/ports/IPositionControlPort.h"
#include "runtime_execution/contracts/motion/IIOControlPort.h"

namespace Siligen::RuntimeExecution::Contracts::Motion {

/**
 * @brief M9 owner 的统一 motion runtime 主入口。
 */
class IMotionRuntimePort : public Siligen::Domain::Motion::Ports::IMotionConnectionPort,
                           public Siligen::Domain::Motion::Ports::IAxisControlPort,
                           public Siligen::Domain::Motion::Ports::IPositionControlPort,
                           public Siligen::Domain::Motion::Ports::IMotionStatePort,
                           public Siligen::Domain::Motion::Ports::IJogControlPort,
                           public IIOControlPort,
                           public Siligen::Domain::Motion::Ports::IHomingPort {
   public:
    ~IMotionRuntimePort() override = default;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Motion

namespace Siligen::Domain::Motion::Ports {

using IMotionRuntimePort = Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort;

}  // namespace Siligen::Domain::Motion::Ports
