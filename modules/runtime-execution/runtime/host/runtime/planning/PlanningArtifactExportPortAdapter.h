#pragma once

#include <memory>

namespace Siligen::Application::Services::Dispensing {
class IPlanningArtifactExportPort;
}

namespace Siligen::RuntimeExecution::Host::Planning {

std::shared_ptr<Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort>
CreatePlanningArtifactExportPort();

}  // namespace Siligen::RuntimeExecution::Host::Planning
