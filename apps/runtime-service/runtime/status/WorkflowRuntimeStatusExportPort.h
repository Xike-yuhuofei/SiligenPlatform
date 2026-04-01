#pragma once

#include "application/usecases/dispensing/valve/ValveQueryUseCase.h"
#include "runtime_execution/application/usecases/motion/MotionControlUseCase.h"
#include "runtime_execution/contracts/system/IRuntimeStatusExportPort.h"
#include "runtime_execution/contracts/system/IRuntimeSupervisionPort.h"

#include <memory>

namespace Siligen::Runtime::Service::Status {

class WorkflowRuntimeStatusExportPort final
    : public Siligen::RuntimeExecution::Contracts::System::IRuntimeStatusExportPort {
   public:
    WorkflowRuntimeStatusExportPort(
        std::shared_ptr<Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort>
            runtime_supervision_port,
        std::shared_ptr<Siligen::Application::UseCases::Motion::MotionControlUseCase> motion_control_use_case,
        std::shared_ptr<Siligen::Application::UseCases::Dispensing::Valve::ValveQueryUseCase> valve_query_use_case);

    Siligen::Shared::Types::Result<Siligen::RuntimeExecution::Contracts::System::RuntimeStatusExportSnapshot>
    ReadSnapshot() const override;

   private:
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort> runtime_supervision_port_;
    std::shared_ptr<Siligen::Application::UseCases::Motion::MotionControlUseCase> motion_control_use_case_;
    std::shared_ptr<Siligen::Application::UseCases::Dispensing::Valve::ValveQueryUseCase> valve_query_use_case_;
};

}  // namespace Siligen::Runtime::Service::Status
