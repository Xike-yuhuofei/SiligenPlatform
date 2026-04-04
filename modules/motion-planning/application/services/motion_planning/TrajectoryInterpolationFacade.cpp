#include "application/services/motion_planning/TrajectoryInterpolationFacade.h"

#include "domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h"
#include "shared/types/Error.h"

namespace Siligen::Application::Services::MotionPlanning {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

Result<std::vector<Siligen::TrajectoryPoint>> TrajectoryInterpolationFacade::Interpolate(
    const std::vector<Siligen::Point2D>& points,
    Siligen::MotionPlanning::Contracts::InterpolationAlgorithm algorithm,
    const Siligen::InterpolationConfig& config) const noexcept {
    auto interpolator = Siligen::Domain::Motion::TrajectoryInterpolatorFactory::CreateInterpolator(algorithm);
    if (!interpolator) {
        return Result<std::vector<Siligen::TrajectoryPoint>>::Failure(
            Error(ErrorCode::NOT_IMPLEMENTED, "插补算法未实现", "TrajectoryInterpolationFacade"));
    }

    if (!interpolator->ValidateParameters(points, config)) {
        return Result<std::vector<Siligen::TrajectoryPoint>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "TrajectoryInterpolationFacade"));
    }

    return Result<std::vector<Siligen::TrajectoryPoint>>::Success(
        interpolator->CalculateInterpolation(points, config));
}

std::vector<Siligen::TrajectoryPoint> TrajectoryInterpolationFacade::OptimizeTrajectoryDensity(
    const std::vector<Siligen::TrajectoryPoint>& points,
    Siligen::MotionPlanning::Contracts::InterpolationAlgorithm algorithm,
    Siligen::Shared::Types::float32 max_step_size_mm) const {
    auto interpolator = Siligen::Domain::Motion::TrajectoryInterpolatorFactory::CreateInterpolator(algorithm);
    if (!interpolator) {
        return points;
    }
    return interpolator->OptimizeTrajectoryDensity(points, max_step_size_mm);
}

}  // namespace Siligen::Application::Services::MotionPlanning
