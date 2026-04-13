#pragma once

#include "../../services/projection/WorkflowProjectionService.h"

namespace Siligen::Workflow::Application::Queries {

class GetWorkflowRunHandler {
   public:
    [[nodiscard]] Contracts::WorkflowRunView Handle(const Domain::WorkflowRun& workflow_run) const {
        return projection_service_.BuildWorkflowRunView(workflow_run);
    }

   private:
    Services::Projection::WorkflowProjectionService projection_service_{};
};

}  // namespace Siligen::Workflow::Application::Queries
