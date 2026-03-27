#pragma once

#include "workflow/application/services/motion/runtime/IMotionRuntimeServicesProvider.h"

namespace Siligen::RuntimeExecution::Host::Motion {

class WorkflowMotionRuntimeServicesProvider final
    : public Siligen::Application::Services::Motion::Runtime::IMotionRuntimeServicesProvider {
   public:
    Siligen::Application::Services::Motion::Runtime::MotionRuntimeServicesBundle CreateServices(
        const std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionRuntimePort>& motion_runtime_port) const override;
};

}  // namespace Siligen::RuntimeExecution::Host::Motion
