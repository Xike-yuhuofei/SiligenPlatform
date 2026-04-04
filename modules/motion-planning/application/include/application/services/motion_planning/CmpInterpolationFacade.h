#pragma once

#include "process_path/contracts/ProcessPath.h"
#include "shared/types/CMPTypes.h"
#include "shared/types/Point.h"

#include <vector>

namespace Siligen::Application::Services::MotionPlanning {

class CmpInterpolationFacade {
   public:
    std::vector<Siligen::TrajectoryPoint> PositionTriggeredDispensing(
        const std::vector<Siligen::Point2D>& points,
        const std::vector<Siligen::DispensingTriggerPoint>& trigger_points,
        const Siligen::CMPConfiguration& cmp_config,
        const Siligen::InterpolationConfig& config) const;

    std::vector<Siligen::TrajectoryPoint> PositionTriggeredDispensing(
        const Siligen::ProcessPath::Contracts::ProcessPath& path,
        const std::vector<Siligen::DispensingTriggerPoint>& trigger_points,
        const Siligen::CMPConfiguration& cmp_config,
        const Siligen::InterpolationConfig& config) const;
};

}  // namespace Siligen::Application::Services::MotionPlanning
