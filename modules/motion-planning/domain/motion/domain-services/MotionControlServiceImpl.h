#pragma once

#include "MotionControlService.h"
#include "domain/motion/ports/IPositionControlPort.h"
#include "shared/types/Error.h"

#include <memory>
#include <utility>
#include <vector>

namespace Siligen::Domain::Motion::DomainServices {

class MotionControlServiceImpl final : public MotionControlService {
public:
    explicit MotionControlServiceImpl(
        std::shared_ptr<Siligen::Domain::Motion::Ports::IPositionControlPort> position_control_port)
        : position_control_port_(std::move(position_control_port)) {}

    Result<void> MoveToPosition(const Point2D& position, float speed) override;
    Result<void> MoveAxisToPosition(LogicalAxisId axis_id, float position, float speed) override;
    Result<void> MoveRelative(const Point2D& offset, float speed) override;

    Result<void> StopAllAxes() override;
    Result<void> EmergencyStop() override;

private:
    std::shared_ptr<Siligen::Domain::Motion::Ports::IPositionControlPort> position_control_port_;
};

}  // namespace Siligen::Domain::Motion::DomainServices


