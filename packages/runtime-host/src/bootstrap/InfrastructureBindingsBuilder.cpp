#include "InfrastructureBindings.h"

#include "shared/logging/PrintfLogFormatter.h"
#include "shared/di/LoggingServiceLocator.h"
#include "shared/interfaces/ILoggingService.h"

#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/configuration/ports/ValveConfig.h"
#include "domain/dispensing/value-objects/DispenseCompensationProfile.h"
#include "domain/motion/domain-services/interpolation/ValidatedInterpolationPort.h"

#include "logging/spdlog/SpdlogLoggingAdapter.h"
#include "legacy/adapters/diagnostics/health/testing/HardwareTestAdapter.h"
#include "legacy/adapters/dispensing/dispenser/ValveAdapter.h"
#include "legacy/adapters/dispensing/dispenser/triggering/TriggerControllerAdapter.h"
#include "legacy/adapters/motion/controller/interpolation/InterpolationAdapter.h"
#include "legacy/adapters/motion/controller/control/MultiCardMotionAdapter.h"
#include "legacy/adapters/motion/controller/homing/HomingPortAdapter.h"
#include "legacy/adapters/motion/controller/runtime/MotionRuntimeConnectionAdapter.h"
#include "legacy/adapters/motion/controller/runtime/MotionRuntimeFacade.h"
#include "siligen/device/adapters/drivers/multicard/IMultiCardWrapper.h"
#include "runtime/configuration/ConfigFileAdapter.h"
#include "runtime/diagnostics/DiagnosticsPortAdapter.h"
#include "domain/motion/domain-services/SevenSegmentSCurveProfile.h"
#include "runtime/storage/files/LocalFileStorageAdapter.h"
#include "runtime/events/InMemoryEventPublisherAdapter.h"
#include "runtime/scheduling/TaskSchedulerAdapter.h"
#include "infrastructure/adapters/planning/dxf/AutoPathSourceAdapter.h"
#include "infrastructure/adapters/planning/dxf/DXFAdapterFactory.h"
#include "infrastructure/adapters/planning/dxf/PbPathSourceAdapter.h"
#include "runtime/recipes/RecipeFileRepository.h"
#include "runtime/recipes/TemplateFileRepository.h"
#include "runtime/recipes/AuditFileRepository.h"
#include "runtime/recipes/ParameterSchemaFileProvider.h"
#include "runtime/recipes/RecipeBundleSerializer.h"

#if SILIGEN_ENABLE_MOCK_MULTICARD
#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"
#endif

#if HAS_REAL_MULTICARD
#include "siligen/device/adapters/drivers/multicard/RealMultiCardWrapper.h"
#endif

#include <algorithm>
#include <array>
#include <filesystem>
#include <initializer_list>
#include <stdexcept>
#include <system_error>
#include <vector>

#ifdef GetMessage
#undef GetMessage
#endif

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
    constexpr int max_levels = 12;

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

std::string ResolveWorkspaceRelativePath(
    const std::string& canonical_relative_path,
    std::initializer_list<std::string> fallback_relative_paths = {}) noexcept {
    auto workspace_root = FindWorkspaceRoot();

    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back(workspace_root / canonical_relative_path);
    for (const auto& fallback_relative_path : fallback_relative_paths) {
        candidates.emplace_back(workspace_root / fallback_relative_path);
    }

    std::error_code ec;
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return CanonicalizeOrKeep(candidate).string();
        }
        ec.clear();
    }

    return (workspace_root / canonical_relative_path).string();
}

std::string ResolveUploadDirectory(const std::string& relative_path) noexcept {
    return ResolveWorkspaceRelativePath(relative_path);
}

std::string ResolveRecipeDirectory(
    const std::string& canonical_relative_path,
    std::initializer_list<std::string> fallback_relative_paths = {}) noexcept {
    return ResolveWorkspaceRelativePath(canonical_relative_path, fallback_relative_paths);
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

std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> CreateMultiCardInstance(
    Siligen::Shared::Types::HardwareMode mode,
    const Siligen::Shared::Types::HardwareConfiguration& config) {
    using Siligen::Infrastructure::Hardware::IMultiCardWrapper;

    if (mode == Siligen::Shared::Types::HardwareMode::Mock) {
#if SILIGEN_ENABLE_MOCK_MULTICARD
        return std::make_shared<Siligen::Infrastructure::Hardware::MockMultiCardWrapper>();
#elif HAS_REAL_MULTICARD
        return std::make_shared<Siligen::Infrastructure::Hardware::RealMultiCardWrapper>();
#else
        return std::shared_ptr<IMultiCardWrapper>();
#endif
    }

#if HAS_REAL_MULTICARD
    return std::make_shared<Siligen::Infrastructure::Hardware::RealMultiCardWrapper>();
#else
#if SILIGEN_ENABLE_MOCK_MULTICARD
    return std::make_shared<Siligen::Infrastructure::Hardware::MockMultiCardWrapper>();
#else
    return std::shared_ptr<IMultiCardWrapper>();
#endif
#endif
}

void BuildMotionRuntimeBindings(
    Siligen::Bootstrap::InfrastructureBindings& bindings,
    const std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper>& multi_card,
    const Siligen::Shared::Types::HardwareConfiguration& hw_config,
    const Siligen::Shared::Types::DiagnosticsConfig& diagnostics_config,
    const std::array<Siligen::Domain::Configuration::Ports::HomingConfig, 4>& homing_configs) {
    auto motion_adapter_result =
        Siligen::Infrastructure::Adapters::MultiCardMotionAdapter::Create(multi_card, hw_config, diagnostics_config);
    if (motion_adapter_result.IsError()) {
        throw std::runtime_error(
            "创建MultiCardMotionAdapter失败: " + motion_adapter_result.GetError().GetMessage());
    }

    auto homing_port = std::make_shared<Siligen::Infrastructure::Adapters::Motion::HomingPortAdapter>(
        multi_card,
        hw_config,
        homing_configs);
    auto motion_runtime = std::make_shared<Siligen::Infrastructure::Adapters::Motion::MotionRuntimeFacade>(
        motion_adapter_result.Value(),
        homing_port);
    bindings.motion_runtime_port = motion_runtime;
    bindings.hardware_connection_port =
        std::make_shared<Siligen::Infrastructure::Adapters::Motion::MotionRuntimeConnectionAdapter>(motion_runtime);
}

}  // namespace

namespace Siligen::Bootstrap {

InfrastructureBindings CreateInfrastructureBindings(const InfrastructureBootstrapConfig& config) {
    InfrastructureBindings bindings;

    auto resolved_config_path = ResolveConfigFilePath(config.config_file_path);
    if (resolved_config_path != config.config_file_path) {
        SILIGEN_LOG_INFO("Resolved config path: " + resolved_config_path);
    }

    auto log_adapter = std::make_shared<Infrastructure::Adapters::Logging::SpdlogLoggingAdapter>("siligen");
    auto log_config = config.log_config;
    auto log_result = log_adapter->Configure(log_config);
    if (log_result.IsError()) {
        SILIGEN_LOG_WARNING("Failed to configure logging service: " + log_result.GetError().GetMessage());
    }
    bindings.logging_service = log_adapter;
    Shared::DI::LoggingServiceLocator::GetInstance().SetService(bindings.logging_service);

    bindings.config_port = std::make_shared<Infrastructure::Adapters::ConfigFileAdapter>(resolved_config_path);

    auto hw_config_result = bindings.config_port->GetHardwareConfiguration();
    if (hw_config_result.IsError()) {
        throw std::runtime_error("加载硬件配置失败: " + hw_config_result.GetError().GetMessage());
    }
    const auto& hw_config = hw_config_result.Value();
    const int axis_count = static_cast<int>(hw_config.EffectiveAxisCount());

    auto mode_result = bindings.config_port->GetHardwareMode();
    if (mode_result.IsError()) {
        throw std::runtime_error("加载硬件模式失败: " + mode_result.GetError().GetMessage());
    }
    auto mode = mode_result.Value();

    auto multi_card = CreateMultiCardInstance(mode, hw_config);
    if (!multi_card) {
        throw std::runtime_error("无法创建MultiCard实例");
    }
    bindings.multicard_instance = multi_card;

    std::array<Domain::Configuration::Ports::HomingConfig, 4> homing_configs{};
    auto homing_configs_result = bindings.config_port->GetAllHomingConfigs();
    if (homing_configs_result.IsError()) {
        throw std::runtime_error("加载回零配置失败: " + homing_configs_result.GetError().GetMessage());
    }
    const auto& configs = homing_configs_result.Value();
    for (size_t i = 0; i < configs.size() && i < static_cast<size_t>(axis_count); ++i) {
        homing_configs[i] = configs[i];
    }

    auto hw_test_adapter = std::make_shared<Infrastructure::Adapters::Hardware::HardwareTestAdapter>(
        multi_card,
        hw_config,
        homing_configs);
    bindings.hardware_test_port = hw_test_adapter;
    bindings.diagnostics_port =
        std::make_shared<Infrastructure::Adapters::Diagnostics::DiagnosticsPortAdapter>(hw_test_adapter);

    bindings.trigger_port = std::make_shared<Infrastructure::Adapters::Dispensing::TriggerControllerAdapter>(
        bindings.hardware_test_port);

    auto valve_supply_result = bindings.config_port->GetValveSupplyConfig();
    if (valve_supply_result.IsError()) {
        throw std::runtime_error("读取供胶阀配置失败: " + valve_supply_result.GetError().GetMessage());
    }
    auto dispenser_config_result = bindings.config_port->GetDispenserValveConfig();
    if (dispenser_config_result.IsError()) {
        throw std::runtime_error("读取点胶阀配置失败: " + dispenser_config_result.GetError().GetMessage());
    }

    Domain::Dispensing::ValueObjects::DispenseCompensationProfile compensation_profile;
    auto dispensing_cfg_result = bindings.config_port->GetDispensingConfig();
    if (dispensing_cfg_result.IsSuccess()) {
        const auto& disp_cfg = dispensing_cfg_result.Value();
        compensation_profile.open_comp_ms = disp_cfg.open_comp_ms;
        compensation_profile.close_comp_ms = disp_cfg.close_comp_ms;
        compensation_profile.retract_enabled = disp_cfg.retract_enabled;
        compensation_profile.corner_pulse_scale = disp_cfg.corner_pulse_scale;
        compensation_profile.curvature_speed_factor = disp_cfg.curvature_speed_factor;
    }

    bindings.valve_port = std::make_shared<Infrastructure::Adapters::ValveAdapter>(
        multi_card,
        valve_supply_result.Value(),
        dispenser_config_result.Value(),
        compensation_profile);

    bindings.upload_base_dir = ResolveUploadDirectory("uploads/dxf/");
    bindings.file_storage_port = std::make_shared<Infrastructure::Adapters::LocalFileStorageAdapter>(
        bindings.upload_base_dir);

    bindings.recipe_base_dir = ResolveRecipeDirectory("data/recipes");
    bindings.recipe_repository = std::make_shared<Infrastructure::Adapters::Recipes::RecipeFileRepository>(
        bindings.recipe_base_dir);
    bindings.template_repository = std::make_shared<Infrastructure::Adapters::Recipes::TemplateFileRepository>(
        bindings.recipe_base_dir);
    bindings.audit_repository = std::make_shared<Infrastructure::Adapters::Recipes::AuditFileRepository>(
        bindings.recipe_base_dir);
    const auto schema_primary_dir = ResolveRecipeDirectory("data/schemas/recipes");
    std::error_code schema_ec;
    std::filesystem::create_directories(schema_primary_dir, schema_ec);
    bindings.parameter_schema_port = std::make_shared<Infrastructure::Adapters::Recipes::ParameterSchemaFileProvider>(
        schema_primary_dir);
    bindings.recipe_bundle_serializer_port =
        std::make_shared<Infrastructure::Adapters::Recipes::RecipeBundleSerializer>();

    auto interpolation_adapter = std::make_shared<Infrastructure::Adapters::InterpolationAdapter>(multi_card);
    bindings.interpolation_port =
        std::make_shared<Domain::Motion::DomainServices::ValidatedInterpolationPort>(interpolation_adapter);
    bindings.velocity_profile_port =
        std::make_shared<Domain::Motion::DomainServices::SevenSegmentSCurveProfile>();

    const size_t worker_threads = static_cast<size_t>(std::max(1, config.task_scheduler_threads));
    bindings.task_scheduler_port =
        std::make_shared<Infrastructure::Adapters::System::TaskScheduler::TaskSchedulerAdapter>(worker_threads);

    Shared::Types::DiagnosticsConfig diagnostics_config;
    auto diagnostics_result = bindings.config_port->GetDiagnosticsConfig();
    if (diagnostics_result.IsSuccess()) {
        diagnostics_config = diagnostics_result.Value();
    } else {
        SILIGEN_LOG_WARNING("加载诊断配置失败，使用默认值: " + diagnostics_result.GetError().GetMessage());
    }

    BuildMotionRuntimeBindings(bindings, multi_card, hw_config, diagnostics_config, homing_configs);

    bindings.event_port = std::make_shared<Infrastructure::Adapters::InMemoryEventPublisherAdapter>(100);

    bindings.path_source_port = std::make_shared<Infrastructure::Adapters::Parsing::PbPathSourceAdapter>();
    
    // 注册DXF专用路径源适配器
    bindings.dxf_path_source_port = Infrastructure::Adapters::Parsing::DXFAdapterFactory::CreateDXFPathSourceAdapter();

    return bindings;
}

}  // namespace Siligen::Bootstrap
