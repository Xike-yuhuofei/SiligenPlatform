#include "WorkflowMotionRuntimeServicesProvider.h"

#include "runtime_execution/application/services/motion/MotionControlServiceImpl.h"
#include "runtime_execution/application/services/motion/MotionStatusServiceImpl.h"

namespace Siligen::RuntimeExecution::Host::Motion {

Siligen::Application::Services::Motion::Runtime::MotionRuntimeServicesBundle
WorkflowMotionRuntimeServicesProvider::CreateServices(
    const std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort>& motion_runtime_port)
    const {
    Siligen::Application::Services::Motion::Runtime::MotionRuntimeServicesBundle bundle;
    if (!motion_runtime_port) {
        return bundle;
    }

    bundle.motion_control_service =
        std::make_shared<Siligen::RuntimeExecution::Application::Services::Motion::MotionControlServiceImpl>(
            motion_runtime_port);
    bundle.motion_status_service =
        std::make_shared<Siligen::RuntimeExecution::Application::Services::Motion::MotionStatusServiceImpl>(
            motion_runtime_port);
    return bundle;
}

}  // namespace Siligen::RuntimeExecution::Host::Motion
