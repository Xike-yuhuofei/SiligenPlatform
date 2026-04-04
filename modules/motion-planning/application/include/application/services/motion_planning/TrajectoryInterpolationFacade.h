#pragma once

#include "motion_planning/contracts/InterpolationTypes.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"

#include <vector>

namespace Siligen::Application::Services::MotionPlanning {

class TrajectoryInterpolationFacade {
   public:
    Siligen::Shared::Types::Result<std::vector<Siligen::TrajectoryPoint>> Interpolate(
        const std::vector<Siligen::Point2D>& points,
        Siligen::MotionPlanning::Contracts::InterpolationAlgorithm algorithm,
        const Siligen::InterpolationConfig& config) const noexcept;

    std::vector<Siligen::TrajectoryPoint> OptimizeTrajectoryDensity(
        const std::vector<Siligen::TrajectoryPoint>& points,
        Siligen::MotionPlanning::Contracts::InterpolationAlgorithm algorithm,
        Siligen::Shared::Types::float32 max_step_size_mm) const;
};

}  // namespace Siligen::Application::Services::MotionPlanning
