#include "application/services/trace_diagnostics/LoggingServiceFactory.h"

#include "adapters/logging/spdlog/SpdlogLoggingAdapter.h"

#include <utility>

namespace Siligen::TraceDiagnostics::Application::Services {

LoggingServiceBootstrapResult CreateLoggingService(
    const Siligen::Shared::Types::LogConfiguration& log_config,
    const std::string& logger_name) {
    auto service = std::make_shared<Siligen::Infrastructure::Adapters::Logging::SpdlogLoggingAdapter>(logger_name);
    auto configuration_result = service->Configure(log_config);

    LoggingServiceBootstrapResult result;
    result.service = std::move(service);
    result.configuration_result = std::move(configuration_result);
    return result;
}

}  // namespace Siligen::TraceDiagnostics::Application::Services
