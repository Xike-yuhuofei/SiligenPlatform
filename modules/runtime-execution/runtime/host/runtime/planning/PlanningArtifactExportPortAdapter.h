#pragma once

#include "application/services/dispensing/IPlanningArtifactExportPort.h"

namespace Siligen::RuntimeExecution::Host::Planning {

class PlanningArtifactExportPortAdapter final
    : public Application::Services::Dispensing::IPlanningArtifactExportPort {
public:
    Shared::Types::Result<Application::Services::Dispensing::PlanningArtifactExportResult> Export(
        const Application::Services::Dispensing::PlanningArtifactExportRequest& request) override;
};

}  // namespace Siligen::RuntimeExecution::Host::Planning
