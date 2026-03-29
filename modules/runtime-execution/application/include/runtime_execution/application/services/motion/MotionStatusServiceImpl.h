#pragma once

#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "runtime_execution/contracts/motion/MotionStatusService.h"

#include <memory>

namespace Siligen::RuntimeExecution::Application::Services::Motion {

class MotionStatusServiceImpl final : public Siligen::Domain::Motion::DomainServices::MotionStatusService {
   public:
    explicit MotionStatusServiceImpl(
        std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port);

    Result<Siligen::Shared::Types::Point2D> GetCurrentPosition() const override;
    Result<Siligen::Shared::Types::AxisStatus> GetAxisStatus(Siligen::Shared::Types::LogicalAxisId axis_id) const override;
    Result<std::vector<Siligen::Shared::Types::AxisStatus>> GetAllAxisStatus() const override;

    Result<bool> IsMoving() const override;
    Result<bool> HasError() const override;

   private:
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port_;
};

}  // namespace Siligen::RuntimeExecution::Application::Services::Motion
