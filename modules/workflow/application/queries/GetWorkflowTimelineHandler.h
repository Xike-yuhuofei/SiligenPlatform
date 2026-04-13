#pragma once

#include "../../services/projection/WorkflowTimelineProjectionService.h"

#include <vector>

namespace Siligen::Workflow::Application::Queries {

class GetWorkflowTimelineHandler {
   public:
    [[nodiscard]] std::vector<Contracts::WorkflowTimelineEntry> Handle(
        const std::vector<Domain::StageTransitionRecord>& transitions,
        const std::vector<Domain::RollbackDecision>& decisions) const {
        std::vector<Contracts::WorkflowTimelineEntry> entries;
        entries.reserve(transitions.size() + decisions.size());
        for (const auto& transition : transitions) {
            entries.push_back(timeline_service_.BuildFromTransition(transition));
        }
        for (const auto& decision : decisions) {
            entries.push_back(timeline_service_.BuildFromRollbackDecision(decision));
        }
        return entries;
    }

   private:
    Services::Projection::WorkflowTimelineProjectionService timeline_service_{};
};

}  // namespace Siligen::Workflow::Application::Queries
