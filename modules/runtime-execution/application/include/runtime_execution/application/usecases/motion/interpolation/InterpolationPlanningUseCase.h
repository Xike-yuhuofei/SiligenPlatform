#pragma once

#include "motion_planning/contracts/InterpolationTypes.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"

#include <vector>

namespace Siligen::Application::UseCases::Motion::Interpolation {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Point2D;
using Siligen::TrajectoryPoint;
using Siligen::InterpolationConfig;
using Siligen::MotionPlanning::Contracts::InterpolationAlgorithm;

struct InterpolationPlanningRequest {
    std::vector<Point2D> points;
    InterpolationAlgorithm algorithm = InterpolationAlgorithm::LINEAR;
    InterpolationConfig config{};
    std::vector<float32> trigger_distances_mm;
    float32 max_step_size_mm = 0.0f;
    bool optimize_density = false;

    bool Validate() const noexcept;
};

struct InterpolationPlanningResult {
    std::vector<TrajectoryPoint> points;
    float32 total_length_mm = 0.0f;
    float32 total_time_s = 0.0f;
    int32 trigger_count = 0;
};

class InterpolationPlanningUseCase {
   public:
    InterpolationPlanningUseCase() = default;
    ~InterpolationPlanningUseCase() = default;

    Result<InterpolationPlanningResult> Execute(const InterpolationPlanningRequest& request) const noexcept;
};

}  // namespace Siligen::Application::UseCases::Motion::Interpolation
