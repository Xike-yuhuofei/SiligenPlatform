#pragma once

#include "domain/motion/ports/IAxisControlPort.h"
#include "domain/motion/ports/IHomingPort.h"
#include "domain/motion/ports/IIOControlPort.h"
#include "domain/motion/ports/IJogControlPort.h"
#include "domain/motion/ports/IMotionConnectionPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "domain/motion/ports/IPositionControlPort.h"

namespace Siligen::Domain::Motion::Ports {

/**
 * @brief 统一 motion runtime 主接口
 *
 * 该接口聚合运行时真实变化边界上的连接、回零、基础运动与状态能力。
 * 现有细粒度端口继续保留，用于渐进迁移与兼容。
 */
class IMotionRuntimePort : public IMotionConnectionPort,
                           public IAxisControlPort,
                           public IPositionControlPort,
                           public IMotionStatePort,
                           public IJogControlPort,
                           public IIOControlPort,
                           public IHomingPort {
   public:
    ~IMotionRuntimePort() override = default;
};

}  // namespace Siligen::Domain::Motion::Ports
