#pragma once

#include "runtime_execution/application/services/dispensing/PlanningArtifactExportPort.h"

#include <memory>

namespace Siligen::RuntimeExecution::Host::Planning {

class PlanningArtifactExportPortAdapter final
    : public Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort {
   public:
    Siligen::Shared::Types::Result<Siligen::Application::Services::Dispensing::PlanningArtifactExportResult> Export(
        const Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest& request) override;
};

std::shared_ptr<Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort>
CreatePlanningArtifactExportPort();

}  // namespace Siligen::RuntimeExecution::Host::Planning
