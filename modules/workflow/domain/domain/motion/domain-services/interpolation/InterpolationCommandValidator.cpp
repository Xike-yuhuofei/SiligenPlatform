#include "InterpolationCommandValidator.h"

namespace Siligen::Domain::Motion::DomainServices {

Result<void> InterpolationCommandValidator::ValidateInterpolationData(const InterpolationData& data) const noexcept {
    if (data.positions.size() < 2) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "InterpolationData requires at least 2 axis positions"));
    }

    switch (data.type) {
        case InterpolationType::LINEAR:
        case InterpolationType::CIRCULAR_CW:
        case InterpolationType::CIRCULAR_CCW:
            return Result<void>::Success();
        case InterpolationType::SPIRAL:
            return Result<void>::Failure(
                Error(ErrorCode::NOT_IMPLEMENTED,
                      "Spiral interpolation not supported in MVP"));
        case InterpolationType::BUFFER_STOP:
            return Result<void>::Failure(
                Error(ErrorCode::NOT_IMPLEMENTED,
                      "Buffer stop not supported in MVP"));
        default:
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER,
                      "Unknown interpolation type"));
    }
}

Result<void> InterpolationCommandValidator::ValidateVelocityOverride(float32 override_percent) const noexcept {
    if (override_percent < 0.0f || override_percent > 200.0f) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "SetCoordinateSystemVelocityOverride: override_percent out of range"));
    }
    return Result<void>::Success();
}

Result<void> InterpolationCommandValidator::ValidateSCurveJerk(float32 jerk) const noexcept {
    if (jerk <= 0.0f) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "EnableCoordinateSystemSCurve: jerk must be > 0"));
    }
    return Result<void>::Success();
}

Result<void> InterpolationCommandValidator::ValidateCoordinateSystemMask(uint32 coord_sys_mask) const noexcept {
    if (coord_sys_mask == 0) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "Coordinate system mask cannot be 0"));
    }
    return Result<void>::Success();
}

}  // namespace Siligen::Domain::Motion::DomainServices
