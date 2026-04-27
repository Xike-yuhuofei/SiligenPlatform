#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"

#include <memory>

namespace Siligen::Domain::Motion {

class MotionBufferController {
   public:
    static Shared::Types::Result<std::shared_ptr<MotionBufferController>> Create(
        std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> interpolation_port);

    int32 GetBufferSpace(short crd_num);
    bool WaitForBufferReady(short crd_num, int32 required_space, int32 timeout_ms = 5000);
    bool ClearBuffer(short crd_num);
    bool CheckBufferHealth(short crd_num, uint32& underflow_count);
    bool HandleBufferOverflow(short crd_num);
    static bool HandleBufferUnderflow(short crd_num);

   private:
    explicit MotionBufferController(
        std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> interpolation_port);

    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> interpolation_port_;
};

}  // namespace Siligen::Domain::Motion

