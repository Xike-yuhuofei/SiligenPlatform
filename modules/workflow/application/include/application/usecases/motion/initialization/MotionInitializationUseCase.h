#pragma once

#include "domain/motion/ports/IAxisControlPort.h"
#include "domain/motion/ports/IIOControlPort.h"
#include "domain/motion/ports/IMotionConnectionPort.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>
#include <string>

namespace Siligen::Application::UseCases::Motion::Initialization {

/**
 * @brief 运动初始化与连接管理用例
 */
class MotionInitializationUseCase {
   public:
    MotionInitializationUseCase(std::shared_ptr<Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port,
                                std::shared_ptr<Domain::Motion::Ports::IAxisControlPort> axis_control_port,
                                std::shared_ptr<Domain::Motion::Ports::IIOControlPort> io_port);

    ~MotionInitializationUseCase() = default;

    Result<void> ConnectToController(const std::string& card_ip, const std::string& pc_ip, int16 port = 0);
    Result<void> DisconnectFromController();
    Result<void> ResetController();
    Result<bool> IsControllerConnected() const;

    Result<void> EnableAxis(Siligen::Shared::Types::LogicalAxisId axis);
    Result<void> DisableAxis(Siligen::Shared::Types::LogicalAxisId axis);
    Result<void> ClearAxisPosition(Siligen::Shared::Types::LogicalAxisId axis);
    Result<void> ClearAxisStatus(Siligen::Shared::Types::LogicalAxisId axis);

   private:
    std::shared_ptr<Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port_;
    std::shared_ptr<Domain::Motion::Ports::IAxisControlPort> axis_control_port_;
    std::shared_ptr<Domain::Motion::Ports::IIOControlPort> io_port_;
};

}  // namespace Siligen::Application::UseCases::Motion::Initialization






