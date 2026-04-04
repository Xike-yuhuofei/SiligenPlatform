#include "application/services/motion_planning/CmpInterpolationFacade.h"

#include "domain/motion/CMPCoordinatedInterpolator.h"

namespace Siligen::Application::Services::MotionPlanning {

std::vector<Siligen::TrajectoryPoint> CmpInterpolationFacade::PositionTriggeredDispensing(
    const std::vector<Siligen::Point2D>& points,
    const std::vector<Siligen::DispensingTriggerPoint>& trigger_points,
    const Siligen::CMPConfiguration& cmp_config,
    const Siligen::InterpolationConfig& config) const {
    Siligen::Domain::Motion::CMPCoordinatedInterpolator interpolator;
    return interpolator.PositionTriggeredDispensing(points, trigger_points, cmp_config, config);
}

std::vector<Siligen::TrajectoryPoint> CmpInterpolationFacade::PositionTriggeredDispensing(
    const Siligen::ProcessPath::Contracts::ProcessPath& path,
    const std::vector<Siligen::DispensingTriggerPoint>& trigger_points,
    const Siligen::CMPConfiguration& cmp_config,
    const Siligen::InterpolationConfig& config) const {
    Siligen::Domain::Motion::CMPCoordinatedInterpolator interpolator;
    return interpolator.PositionTriggeredDispensing(path, trigger_points, cmp_config, config);
}

}  // namespace Siligen::Application::Services::MotionPlanning
