#pragma once

#include "runtime_execution/contracts/motion/MotionValidationService.h"

namespace Siligen::RuntimeExecution::Application::Services::Motion {

class DefaultMotionValidationService final : public Siligen::Domain::Motion::DomainServices::MotionValidationService {
   public:
    Siligen::Shared::Types::Result<void> ValidatePosition(
        const Siligen::Shared::Types::Point2D& position) const override;
    Siligen::Shared::Types::Result<void> ValidateSpeed(float speed) const override;
    Siligen::Shared::Types::Result<bool> IsPositionErrorExceeded(
        const Siligen::Shared::Types::Point2D& target,
        const Siligen::Shared::Types::Point2D& actual) const override;
};

}  // namespace Siligen::RuntimeExecution::Application::Services::Motion
