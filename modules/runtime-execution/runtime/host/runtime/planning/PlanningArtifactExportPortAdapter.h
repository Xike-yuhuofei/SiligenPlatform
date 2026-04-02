#pragma once

#include "application/services/dispensing/PlanningArtifactExportPort.h"

namespace Siligen::RuntimeExecution::Host::Planning {

class PlanningArtifactExportPortAdapter final
    : public Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort {
public:
    Siligen::Shared::Types::Result<Siligen::Application::Services::Dispensing::PlanningArtifactExportResult> Export(
        const Siligen::Application::Services::Dispensing::PlanningArtifactExportRequest& request) override;
};

}  // namespace Siligen::RuntimeExecution::Host::Planning
