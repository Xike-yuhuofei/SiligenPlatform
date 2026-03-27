#pragma once

#include "shared/interfaces/ILoggingService.h"
#include "shared/types/LogTypes.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>

namespace Siligen::TraceDiagnostics::Application::Services {

struct LoggingServiceBootstrapResult {
    std::shared_ptr<Siligen::Shared::Interfaces::ILoggingService> service;
    Siligen::Shared::Types::Result<void> configuration_result;
};

LoggingServiceBootstrapResult CreateLoggingService(
    const Siligen::Shared::Types::LogConfiguration& log_config,
    const std::string& logger_name);

}  // namespace Siligen::TraceDiagnostics::Application::Services
