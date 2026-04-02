#pragma once

#include "ExecutionSessionStore.h"

#include "shared/types/Point.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>

namespace Siligen::Domain::Motion::Ports {
class IMotionStatePort;
}

namespace Siligen::Domain::Dispensing::Ports {
class IDispensingExecutionObserver;
}

namespace Siligen::Application::UseCases::Dispensing {

std::string ResolveVelocityTracePath(const std::string& dxf_path, const std::string& output_path);

std::unique_ptr<Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver> CreateExecutionObserver(
    const std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort>& motion_state_port,
    const std::string& trace_output_path,
    int32 trace_interval_ms,
    bool trace_enabled,
    bool dispense_enabled,
    const std::shared_ptr<TaskExecutionContext>& context);

}  // namespace Siligen::Application::UseCases::Dispensing
