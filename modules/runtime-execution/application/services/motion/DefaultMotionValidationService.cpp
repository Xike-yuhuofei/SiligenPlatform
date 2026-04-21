#include "runtime_execution/application/services/motion/DefaultMotionValidationService.h"

#include "shared/types/Error.h"

#include <cmath>

namespace Siligen::RuntimeExecution::Application::Services::Motion {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;

Result<void> DefaultMotionValidationService::ValidatePosition(const Point2D& position) const {
    if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "Target position is not finite",
            "DefaultMotionValidationService"));
    }
    return Result<void>::Success();
}

Result<void> DefaultMotionValidationService::ValidateSpeed(float speed) const {
    if (!std::isfinite(speed) || speed <= 0.0f) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "Speed must be positive",
            "DefaultMotionValidationService"));
    }
    return Result<void>::Success();
}

Result<bool> DefaultMotionValidationService::IsPositionErrorExceeded(
    const Point2D& target,
    const Point2D& actual) const {
    return Result<bool>::Success(target.DistanceTo(actual) > 0.01f);
}

}  // namespace Siligen::RuntimeExecution::Application::Services::Motion
