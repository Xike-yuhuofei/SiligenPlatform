#pragma once

#include "../../services/projection/WorkflowProjectionService.h"

#include <vector>

namespace Siligen::Workflow::Application::Queries {

class GetWorkflowStageHistoryHandler {
   public:
    [[nodiscard]] std::vector<Contracts::StageTransitionRecordView> Handle(
        const std::vector<Domain::StageTransitionRecord>& transitions) const {
        std::vector<Contracts::StageTransitionRecordView> views;
        views.reserve(transitions.size());
        for (const auto& transition : transitions) {
            views.push_back(projection_service_.BuildStageTransitionView(transition));
        }
        return views;
    }

   private:
    Services::Projection::WorkflowProjectionService projection_service_{};
};

}  // namespace Siligen::Workflow::Application::Queries
