#pragma once

#include "container/ApplicationContainer.h"
#include "bootstrap/InfrastructureBindings.h"

#include <memory>
#include <string>

namespace Siligen::Apps::Runtime {

std::shared_ptr<Application::Container::ApplicationContainer> BuildContainer(
    const std::string& config_file_path,
    Application::Container::LogMode log_mode,
    const std::string& log_file_path,
    int task_scheduler_threads = 4);

}  // namespace Siligen::Apps::Runtime
