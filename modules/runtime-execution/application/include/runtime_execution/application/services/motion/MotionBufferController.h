#pragma once

#include "domain/motion/ports/IInterpolationPort.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>

namespace Siligen::RuntimeExecution::Application::Services::Motion {

using Siligen::Domain::Motion::Ports::IInterpolationPort;

class MotionBufferController {
   public:
    static Shared::Types::Result<std::shared_ptr<MotionBufferController>> Create(
        std::shared_ptr<IInterpolationPort> interpolation_port);

    int32 GetBufferSpace(short crd_num);
    bool WaitForBufferReady(short crd_num, int32 required_space, int32 timeout_ms = 5000);
    bool ClearBuffer(short crd_num);
    bool CheckBufferHealth(short crd_num, uint32& underflow_count);
    bool HandleBufferOverflow(short crd_num);
    bool HandleBufferUnderflow(short crd_num);

   private:
    explicit MotionBufferController(std::shared_ptr<IInterpolationPort> interpolation_port);

    std::shared_ptr<IInterpolationPort> interpolation_port_;
};

}  // namespace Siligen::RuntimeExecution::Application::Services::Motion
