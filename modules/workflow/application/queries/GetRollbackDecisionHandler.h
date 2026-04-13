#pragma once

#include "../../services/projection/WorkflowProjectionService.h"

namespace Siligen::Workflow::Application::Queries {

class GetRollbackDecisionHandler {
   public:
    [[nodiscard]] Contracts::RollbackDecisionView Handle(const Domain::RollbackDecision& decision) const {
        return projection_service_.BuildRollbackDecisionView(decision);
    }

   private:
    Services::Projection::WorkflowProjectionService projection_service_{};
};

}  // namespace Siligen::Workflow::Application::Queries
