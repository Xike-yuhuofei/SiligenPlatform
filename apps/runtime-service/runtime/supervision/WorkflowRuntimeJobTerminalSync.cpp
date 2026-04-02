#include "runtime/supervision/WorkflowRuntimeJobTerminalSync.h"

#include "application/usecases/dispensing/DispensingWorkflowUseCase.h"

#include <utility>

namespace Siligen::Runtime::Service::Supervision {

WorkflowRuntimeJobTerminalSync::WorkflowRuntimeJobTerminalSync(
    std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingWorkflowUseCase>
        dispensing_workflow_use_case)
    : dispensing_workflow_use_case_(std::move(dispensing_workflow_use_case)) {}

void WorkflowRuntimeJobTerminalSync::OnTerminalJobObserved(const std::string& job_id) const {
    if (job_id.empty() || !dispensing_workflow_use_case_) {
        return;
    }

    (void)dispensing_workflow_use_case_->GetJobStatus(job_id);
}

}  // namespace Siligen::Runtime::Service::Supervision
