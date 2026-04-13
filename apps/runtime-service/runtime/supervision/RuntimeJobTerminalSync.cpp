#include "runtime/supervision/RuntimeJobTerminalSync.h"

#include "runtime_execution/application/usecases/dispensing/DispensingWorkflowUseCase.h"

#include <utility>

namespace Siligen::Runtime::Service::Supervision {

RuntimeJobTerminalSync::RuntimeJobTerminalSync(
    std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingWorkflowUseCase>
        dispensing_workflow_use_case)
    : dispensing_workflow_use_case_(std::move(dispensing_workflow_use_case)) {}

void RuntimeJobTerminalSync::OnTerminalJobObserved(const std::string& job_id) const {
    if (job_id.empty() || !dispensing_workflow_use_case_) {
        return;
    }

    dispensing_workflow_use_case_->OnRuntimeJobTerminal(job_id);
}

}  // namespace Siligen::Runtime::Service::Supervision
