#pragma once

#include "runtime_execution/contracts/motion/IPositionControlPort.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>

namespace Siligen::Application::UseCases::Motion::Safety {

/**
 * @brief 运动安全控制用例
 */
class MotionSafetyUseCase {
   public:
    explicit MotionSafetyUseCase(std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port);

    ~MotionSafetyUseCase() = default;

    Result<void> EmergencyStop();
    Result<void> StopAllAxes(bool immediate = false);

   private:
    std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port_;
};

}  // namespace Siligen::Application::UseCases::Motion::Safety
