#pragma once

#include "runtime_execution/application/services/motion/runtime/IMotionRuntimeServicesProvider.h"

namespace Siligen::RuntimeExecution::Host::Motion {

class MotionRuntimeServicesProvider final
    : public Siligen::Application::Services::Motion::Runtime::IMotionRuntimeServicesProvider {
   public:
    Siligen::Application::Services::Motion::Runtime::MotionRuntimeServicesBundle CreateServices(
        const std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort>& motion_runtime_port)
        const override;
};

}  // namespace Siligen::RuntimeExecution::Host::Motion
