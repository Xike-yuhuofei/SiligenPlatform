#pragma once

#include "runtime_execution/contracts/system/IRuntimeSupervisionPort.h"

#include <memory>
#include <mutex>
#include <string>

namespace Siligen::Application::UseCases::Dispensing {
class DispensingExecutionUseCase;
}

namespace Siligen::Runtime::Service::Supervision {

class IRuntimeJobTerminalSync;

class WorkflowRuntimeSupervisionPort final
    : public Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort {
   public:
    WorkflowRuntimeSupervisionPort(
        std::shared_ptr<Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort> inner_port,
        std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase>
            dispensing_execution_use_case,
        std::shared_ptr<IRuntimeJobTerminalSync> terminal_job_sync);

    Siligen::Shared::Types::Result<Siligen::RuntimeExecution::Contracts::System::RuntimeSupervisionSnapshot>
    ReadSnapshot() const override;

   private:
    void RememberObservedJob(const std::string& job_id) const;
    void ClearObservedJob() const;
    void SyncTerminalJobIfNeeded() const;

    std::shared_ptr<Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort> inner_port_;
    std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase>
        dispensing_execution_use_case_;
    std::shared_ptr<IRuntimeJobTerminalSync> terminal_job_sync_;
    mutable std::mutex observed_job_mutex_;
    mutable std::string last_observed_job_id_;
};

}  // namespace Siligen::Runtime::Service::Supervision
