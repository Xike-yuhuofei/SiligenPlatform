#pragma once

#include "runtime_execution/contracts/motion/IPositionControlPort.h"
#include "runtime_execution/contracts/motion/MotionControlService.h"

#include <memory>

namespace Siligen::RuntimeExecution::Application::Services::Motion {

class MotionControlServiceImpl final : public Siligen::Domain::Motion::DomainServices::MotionControlService {
   public:
    explicit MotionControlServiceImpl(
        std::shared_ptr<Siligen::Domain::Motion::Ports::IPositionControlPort> position_control_port);

    Result<void> MoveToPosition(const Point2D& position, float speed) override;
    Result<void> MoveAxisToPosition(LogicalAxisId axis_id, float position, float speed) override;
    Result<void> MoveRelative(const Point2D& offset, float speed) override;

    Result<void> StopAllAxes() override;
    Result<void> EmergencyStop() override;
    Result<void> RecoverFromEmergencyStop() override;

   private:
    std::shared_ptr<Siligen::Domain::Motion::Ports::IPositionControlPort> position_control_port_;
};

}  // namespace Siligen::RuntimeExecution::Application::Services::Motion
