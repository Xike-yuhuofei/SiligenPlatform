#pragma once

#include "dispense_packaging/application/usecases/dispensing/PlanningPorts.h"

#include <memory>

namespace Siligen::Application::Services::ProcessPath {
class ProcessPathFacade;
}

namespace Siligen::Application::Services::MotionPlanning {
class MotionPlanningFacade;
}

namespace Siligen::Application::Services::DXF {
class DxfPbPreparationService;
}

namespace Siligen::Application::Ports::Dispensing {

std::shared_ptr<IProcessPathBuildPort> AdaptProcessPathFacade(
    std::shared_ptr<Siligen::Application::Services::ProcessPath::ProcessPathFacade> facade);
std::shared_ptr<IMotionPlanningPort> AdaptMotionPlanningFacade(
    std::shared_ptr<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade> facade);
std::shared_ptr<IPlanningInputPreparationPort> AdaptDxfPreparationService(
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> service);

}  // namespace Siligen::Application::Ports::Dispensing
