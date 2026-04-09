#pragma once

#include "runtime_execution/contracts/motion/IAxisControlPort.h"
#include "runtime_execution/contracts/motion/IHomingPort.h"
#include "runtime_execution/contracts/motion/IJogControlPort.h"
#include "runtime_execution/contracts/motion/IMotionConnectionPort.h"
#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "runtime_execution/contracts/motion/IPositionControlPort.h"
#include "runtime_execution/contracts/motion/IIOControlPort.h"

namespace Siligen::RuntimeExecution::Contracts::Motion {

/**
 * @brief M9 owner 的统一 motion runtime 主入口。
 */
class IMotionRuntimePort : public Siligen::Domain::Motion::Ports::IMotionConnectionPort,
                           public IAxisControlPort,
                           public IPositionControlPort,
                           public IMotionStatePort,
                           public IJogControlPort,
                           public IIOControlPort,
                           public IHomingPort {
   public:
    ~IMotionRuntimePort() override = default;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Motion

namespace Siligen::Domain::Motion::Ports {

using IMotionRuntimePort = Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort;

}  // namespace Siligen::Domain::Motion::Ports
