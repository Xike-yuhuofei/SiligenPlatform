#include "InfrastructureBindings.h"

#include "shared/logging/PrintfLogFormatter.h"
#include "shared/di/LoggingServiceLocator.h"
#include "shared/interfaces/ILoggingService.h"

#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/configuration/ports/ValveConfig.h"
#include "domain/dispensing/value-objects/DispenseCompensationProfile.h"
#include "domain/motion/domain-services/interpolation/ValidatedInterpolationPort.h"

#include "application/services/trace_diagnostics/LoggingServiceFactory.h"
#include "siligen/device/adapters/hardware/HardwareTestAdapter.h"
#include "siligen/device/adapters/dispensing/ValveAdapter.h"
#include "siligen/device/adapters/dispensing/TriggerControllerAdapter.h"
#include "siligen/device/adapters/motion/InterpolationAdapter.h"
#include "siligen/device/adapters/motion/MultiCardMotionAdapter.h"
#include "siligen/device/adapters/motion/HomingPortAdapter.h"
#include "siligen/device/adapters/motion/MotionRuntimeConnectionAdapter.h"
#include "siligen/device/adapters/motion/MotionRuntimeFacade.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "siligen/device/adapters/drivers/multicard/IMultiCardWrapper.h"
#include "runtime/configuration/ConfigFileAdapter.h"
#include "runtime/configuration/InterlockConfigResolver.h"
#include "runtime/configuration/WorkspaceAssetPaths.h"
#include "runtime/diagnostics/DiagnosticsPortAdapter.h"
#include "domain/motion/domain-services/SevenSegmentSCurveProfile.h"
#include "runtime/storage/files/LocalFileStorageAdapter.h"
#include "runtime/events/InMemoryEventPublisherAdapter.h"
#include "runtime/scheduling/TaskSchedulerAdapter.h"
#include "security/AuditLogger.h"
#include "security/InterlockMonitor.h"
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
namespace RuntimeConfig = Siligen::Infrastructure::Configuration;
constexpr Siligen::Shared::Types::uint32 kInterlockStatusMonitorIntervalMs = 100;

class RuntimeInterlockSignalPort final : public Siligen::Domain::Safety::Ports::IInterlockSignalPort {
   public:
    RuntimeInterlockSignalPort(
        const std::string& audit_log_dir,
        std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> multicard)
        : audit_logger_(audit_log_dir),
          monitor_(audit_logger_, std::move(multicard)) {}

    ~RuntimeInterlockSignalPort() override {
        monitor_.Stop();
    }

    bool Initialize(const Siligen::InterlockConfig& config) {
        return monitor_.Initialize(config);
    }

    void StartMonitoring() {
        monitor_.Start();
    }

    void StopMonitoring() {
        monitor_.Stop();
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Safety::ValueObjects::InterlockSignals> ReadSignals()
        const noexcept override {
        return monitor_.ReadSignals();
    }

   private:
    Siligen::AuditLogger audit_logger_;
    Siligen::InterlockMonitor monitor_;
};

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
    auto connection_adapter =
        std::make_shared<Siligen::Infrastructure::Adapters::Motion::MotionRuntimeConnectionAdapter>(motion_runtime);
    bindings.motion_runtime_port = motion_runtime;
    bindings.device_connection_port = connection_adapter;
}

}  // namespace

namespace Siligen::Bootstrap {

InfrastructureBindings CreateInfrastructureBindings(const InfrastructureBootstrapConfig& config) {
    InfrastructureBindings bindings;
    const auto& resolved_config_path = config.config_file_path;

    auto log_bootstrap = TraceDiagnostics::Application::Services::CreateLoggingService(config.log_config, "siligen");
    if (log_bootstrap.configuration_result.IsError()) {
        SILIGEN_LOG_WARNING(
            "Failed to configure logging service: " +
            log_bootstrap.configuration_result.GetError().GetMessage());
    }
    bindings.logging_service = log_bootstrap.service;
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
    bindings.machine_health_port = hw_test_adapter;
    bindings.diagnostics_port =
        std::make_shared<Infrastructure::Adapters::Diagnostics::DiagnosticsPortAdapter>(bindings.machine_health_port);

    bindings.trigger_port = std::make_shared<Infrastructure::Adapters::Dispensing::TriggerControllerAdapter>(
        hw_test_adapter);

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

    bindings.upload_base_dir = RuntimeConfig::ResolveUploadDirectory();
    bindings.file_storage_port = std::make_shared<Infrastructure::Adapters::LocalFileStorageAdapter>(
        bindings.upload_base_dir);

    bindings.recipe_base_dir = RuntimeConfig::ResolveRecipeDirectory();
    bindings.recipe_repository = std::make_shared<Infrastructure::Adapters::Recipes::RecipeFileRepository>(
        bindings.recipe_base_dir);
    bindings.template_repository = std::make_shared<Infrastructure::Adapters::Recipes::TemplateFileRepository>(
        bindings.recipe_base_dir);
    bindings.audit_repository = std::make_shared<Infrastructure::Adapters::Recipes::AuditFileRepository>(
        bindings.recipe_base_dir);
    const auto schema_primary_dir = RuntimeConfig::ResolveRecipeSchemaDirectory();
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

    const auto interlock_resolution =
        Siligen::Infrastructure::Configuration::ResolveInterlockConfigFromIni(resolved_config_path);
    for (const auto& warning : interlock_resolution.warnings) {
        SILIGEN_LOG_WARNING("Interlock config: " + warning + " [config=" + resolved_config_path + "]");
    }
    auto interlock_port = std::make_shared<RuntimeInterlockSignalPort>(
        RuntimeConfig::ResolveWorkspaceRelativePath("logs/audit"),
        multi_card);
    if (!interlock_port->Initialize(interlock_resolution.config)) {
        SILIGEN_LOG_WARNING("Failed to initialize interlock monitor; runtime interlock checks disabled");
    } else {
        if (bindings.device_connection_port) {
            std::weak_ptr<RuntimeInterlockSignalPort> weak_interlock = interlock_port;
            bindings.device_connection_port->SetConnectionStateCallback(
                [weak_interlock](const Siligen::Device::Contracts::State::DeviceConnectionSnapshot& snapshot) {
                    auto interlock = weak_interlock.lock();
                    if (!interlock) {
                        return;
                    }

                    if (snapshot.IsConnected()) {
                        interlock->StartMonitoring();
                        return;
                    }
                    interlock->StopMonitoring();
                });

            const auto connection_snapshot = bindings.device_connection_port->ReadConnection();
            if (connection_snapshot.IsSuccess() && connection_snapshot.Value().IsConnected()) {
                interlock_port->StartMonitoring();
            }
            if (interlock_resolution.config.enabled) {
                const auto monitoring_result =
                    bindings.device_connection_port->StartStatusMonitoring(kInterlockStatusMonitorIntervalMs);
                if (monitoring_result.IsError()) {
                    SILIGEN_LOG_WARNING(
                        "Failed to start connection monitoring for interlock lifecycle: " +
                        monitoring_result.GetError().GetMessage());
                }
            }
        }
        bindings.interlock_signal_port = interlock_port;
    }

    bindings.path_source_port = std::make_shared<Infrastructure::Adapters::Parsing::PbPathSourceAdapter>();

    return bindings;
}

}  // namespace Siligen::Bootstrap
