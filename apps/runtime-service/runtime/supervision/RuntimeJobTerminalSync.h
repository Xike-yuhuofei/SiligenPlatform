#pragma once

#include "runtime/supervision/RuntimeExecutionSupervisionBackend.h"

#include <memory>

namespace Siligen::Application::UseCases::Dispensing {
class DispensingWorkflowUseCase;
}

namespace Siligen::Runtime::Service::Supervision {

class RuntimeJobTerminalSync final : public IRuntimeJobTerminalSync {
   public:
    explicit RuntimeJobTerminalSync(
        std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingWorkflowUseCase>
            dispensing_workflow_use_case);

    void OnTerminalJobObserved(const std::string& job_id) const override;

   private:
    std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingWorkflowUseCase>
        dispensing_workflow_use_case_;
};

}  // namespace Siligen::Runtime::Service::Supervision
