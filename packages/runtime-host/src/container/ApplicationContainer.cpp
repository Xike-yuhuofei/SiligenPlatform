#include "ApplicationContainer.h"

#include "application/usecases/system/IHardLimitMonitor.h"
#include "shared/di/LoggingServiceLocator.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "ApplicationContainer"

namespace {

std::string NormalizeRelativePath(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}

std::filesystem::path CanonicalizeOrKeep(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    auto canonical_path = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        return path;
    }
    return canonical_path;
}

std::filesystem::path FindWorkspaceRoot() noexcept {
    std::error_code ec;
    std::filesystem::path current = std::filesystem::current_path(ec);
    if (ec) {
        SILIGEN_LOG_ERROR("FindWorkspaceRoot: Failed to get current path: " + ec.message());
        return std::filesystem::path(".");
    }

    const std::vector<std::string> fallback_markers = {
        "CMakeLists.txt",
        "CLAUDE.md",
        ".git"
    };

    std::filesystem::path candidate = current;
    std::filesystem::path fallback = current;
    bool fallback_found = false;
    constexpr int max_levels = 10;

    for (int level = 0; level < max_levels; ++level) {
        if (std::filesystem::exists(candidate / "WORKSPACE.md", ec) && !ec) {
            SILIGEN_LOG_INFO("FindWorkspaceRoot: Found workspace root: " + candidate.string());
            return candidate;
        }
        ec.clear();

        for (const auto& marker : fallback_markers) {
            if (std::filesystem::exists(candidate / marker, ec) && !ec) {
                if (!fallback_found) {
                    fallback = candidate;
                    fallback_found = true;
                }
                break;
            }
            ec.clear();
        }

        auto parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }

    if (fallback_found) {
        SILIGEN_LOG_WARNING("FindWorkspaceRoot: WORKSPACE.md not found, using fallback root: " +
                            fallback.string());
        return fallback;
    }

    SILIGEN_LOG_WARNING("FindWorkspaceRoot: Could not find workspace root, using: " + current.string());
    return current;
}

std::vector<std::string> BuildConfigCandidatePaths(const std::string& config_path) {
    std::vector<std::string> candidates{config_path};
    const auto normalized = NormalizeRelativePath(config_path);

    if (normalized == "config/machine_config.ini") {
        candidates.emplace_back("config/machine/machine_config.ini");
    } else if (normalized == "config/machine/machine_config.ini") {
        candidates.emplace_back("config/machine_config.ini");
    }

    return candidates;
}

std::string ResolveConfigFilePath(const std::string& config_path) noexcept {
    std::filesystem::path path(config_path);
    if (path.is_absolute()) {
        return path.string();
    }

    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (!ec) {
        auto cwd_candidate = cwd / path;
        if (std::filesystem::exists(cwd_candidate, ec) && !ec) {
            return CanonicalizeOrKeep(cwd_candidate).string();
        }
        ec.clear();
    }

    auto workspace_root = FindWorkspaceRoot();
    for (const auto& candidate_relative_path : BuildConfigCandidatePaths(config_path)) {
        auto workspace_candidate = workspace_root / candidate_relative_path;
        if (std::filesystem::exists(workspace_candidate, ec) && !ec) {
            return CanonicalizeOrKeep(workspace_candidate).string();
        }
        ec.clear();
    }

    return config_path;
}

}  // namespace

namespace Siligen::Application::Container {

ApplicationContainer::ApplicationContainer(
    const std::string& config_file_path,
    LogMode log_mode,
    const std::string& log_file_path)
    : config_file_path_(config_file_path),
      log_mode_(log_mode),
      log_file_path_(log_file_path) {
    const auto resolved_config_path = ResolveConfigFilePath(config_file_path_);
    if (resolved_config_path != config_file_path_) {
        SILIGEN_LOG_INFO("Resolved config path: " + resolved_config_path);
        config_file_path_ = resolved_config_path;
    }
}

ApplicationContainer::~ApplicationContainer() {
    if (hard_limit_monitor_) {
        hard_limit_monitor_->Stop();
        hard_limit_monitor_.reset();
    }

    use_case_instances_.clear();
    singleton_instances_.clear();
    service_instances_.clear();
    port_instances_.clear();

    valve_controller_.reset();
    velocity_profile_service_.reset();
    jog_controller_.reset();

    recipe_bundle_serializer_port_.reset();
    parameter_schema_port_.reset();
    audit_repository_.reset();
    template_repository_.reset();
    recipe_repository_.reset();
    task_scheduler_port_.reset();
    velocity_profile_port_.reset();
    interpolation_port_.reset();
    io_control_port_.reset();
    file_storage_port_.reset();
    valve_port_.reset();
    hardware_connection_port_.reset();
    test_config_manager_.reset();
    test_record_repository_.reset();
    hardware_test_port_.reset();
    preset_port_.reset();
    event_port_.reset();
    diagnostics_port_.reset();
    trigger_port_.reset();
    homing_port_.reset();
    motion_jog_port_.reset();
    motion_state_port_.reset();
    position_control_port_.reset();
    axis_control_port_.reset();
    motion_connection_port_.reset();
    motion_runtime_port_.reset();
    config_port_.reset();
    multiCard_.reset();

    if (logging_service_) {
        logging_service_->Flush();
        logging_service_.reset();
    }

    Shared::DI::LoggingServiceLocator::GetInstance().ClearService();
}

void ApplicationContainer::Configure() {
    ConfigurePorts();
    ConfigureServices();
    ConfigureUseCases();
}

void ApplicationContainer::RestoreStdout() {
    // 保留接口以兼容旧调用链。当前日志方案不再重定向 stdout/stderr。
}

void ApplicationContainer::SilenceStdout() {
    // 保留接口以兼容旧调用链。当前日志方案不再重定向 stdout/stderr。
}

void ApplicationContainer::ConfigurePorts() {
    ValidateSystemPorts();
    ValidateDiagnosticsPorts();
    ValidateMotionPorts();
    ValidateDispensingPorts();
    ValidateRecipePorts();
}

void ApplicationContainer::ConfigureUseCases() {
    // Use case 保持懒加载，由 Resolve<T>() 首次调用时创建。
}

void ApplicationContainer::ConfigureServices() {
    ConfigureMotionServices();
    ConfigureDispensingServices();
    ConfigureRecipeServices();
    SILIGEN_LOG_INFO("Domain Services configuration complete");
}

void* ApplicationContainer::GetMultiCardInstance() {
    return multiCard_.get();
}

void ApplicationContainer::SetMultiCardInstance(std::shared_ptr<void> multicard_instance) {
    multiCard_ = std::move(multicard_instance);
}

void ApplicationContainer::SetUploadBaseDir(const std::string& upload_base_dir) {
    upload_base_dir_ = upload_base_dir;
}

std::shared_ptr<Shared::Interfaces::ILoggingService> ApplicationContainer::GetLoggingService() {
    return logging_service_;
}

void ApplicationContainer::SetLoggingService(std::shared_ptr<Shared::Interfaces::ILoggingService> logging_service) {
    logging_service_ = std::move(logging_service);
    if (logging_service_) {
        Shared::DI::LoggingServiceLocator::GetInstance().SetService(logging_service_);
    } else {
        Shared::DI::LoggingServiceLocator::GetInstance().ClearService();
    }
}

template<>
std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort>
ApplicationContainer::CreateInstance<Domain::Configuration::Ports::IConfigurationPort>() {
    return config_port_;
}

}  // namespace Siligen::Application::Container
