#pragma once

#include "MotionStatusService.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "shared/types/Error.h"

#include <memory>
#include <utility>

namespace Siligen::Domain::Motion::DomainServices {

class MotionStatusServiceImpl final : public MotionStatusService {
public:
    explicit MotionStatusServiceImpl(
        std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port)
        : motion_state_port_(std::move(motion_state_port)) {}

    Result<Point2D> GetCurrentPosition() const override;
    Result<AxisStatus> GetAxisStatus(LogicalAxisId axis_id) const override;
    Result<std::vector<AxisStatus>> GetAllAxisStatus() const override;

    Result<bool> IsMoving() const override;
    Result<bool> HasError() const override;

private:
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port_;
};

}  // namespace Siligen::Domain::Motion::DomainServices


