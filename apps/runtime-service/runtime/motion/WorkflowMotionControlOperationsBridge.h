#pragma once

#include "runtime_execution/application/usecases/motion/MotionControlUseCase.h"

#include <memory>

namespace Siligen::Application::UseCases::Motion::Homing {
class HomeAxesUseCase;
class EnsureAxesReadyZeroUseCase;
}

namespace Siligen::Application::UseCases::Motion::Manual {
class ManualMotionControlUseCase;
}

namespace Siligen::Application::UseCases::Motion::Monitoring {
class MotionMonitoringUseCase;
}

namespace Siligen::Runtime::Service::Motion {

struct WorkflowMotionOperationsBundle {
    std::shared_ptr<Siligen::Application::UseCases::Motion::IMotionHomingOperations> homing_operations;
    std::shared_ptr<Siligen::Application::UseCases::Motion::IMotionManualOperations> manual_operations;
    std::shared_ptr<Siligen::Application::UseCases::Motion::IMotionMonitoringOperations> monitoring_operations;
};

WorkflowMotionOperationsBundle CreateWorkflowMotionOperations(
    std::shared_ptr<Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase> home_use_case,
    std::shared_ptr<Siligen::Application::UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase>
        ensure_ready_zero_use_case,
    std::shared_ptr<Siligen::Application::UseCases::Motion::Manual::ManualMotionControlUseCase> manual_use_case,
    std::shared_ptr<Siligen::Application::UseCases::Motion::Monitoring::MotionMonitoringUseCase>
        monitoring_use_case);

}  // namespace Siligen::Runtime::Service::Motion
