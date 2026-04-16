#include "ContainerBootstrap.h"

#include "../container/ApplicationContainer.h"
#include "InfrastructureBindings.h"

#include "process_planning/contracts/ConfigurationContracts.h"
#include "runtime_process_bootstrap/storage/ports/IFileStoragePort.h"
#include "runtime_process_bootstrap/diagnostics/ports/ICMPTestPresetPort.h"
#include "trace_diagnostics/contracts/IDiagnosticsPort.h"
#include "runtime_process_bootstrap/diagnostics/ports/ITestConfigurationPort.h"
#include "runtime_process_bootstrap/diagnostics/ports/ITestRecordRepository.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "dispense_packaging/contracts/ITriggerControllerPort.h"
#include "dispense_packaging/contracts/IValvePort.h"
#include "runtime_execution/contracts/motion/IAxisControlPort.h"
#include "runtime_execution/contracts/motion/IHomingPort.h"
#include "runtime_execution/contracts/motion/IJogControlPort.h"
#include "runtime_execution/contracts/motion/IMotionConnectionPort.h"
#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "runtime_execution/contracts/motion/IPositionControlPort.h"
#include "motion_planning/contracts/IVelocityProfilePort.h"
#include "runtime_execution/contracts/dispensing/ITaskSchedulerPort.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "runtime_execution/contracts/motion/IMotionRuntimePort.h"
#include "runtime/contracts/system/IEventPublisherPort.h"
#include "recipe_lifecycle/domain/recipes/ports/IParameterSchemaPort.h"
#include "recipe_lifecycle/domain/recipes/ports/IAuditRepositoryPort.h"
#include "recipe_lifecycle/domain/recipes/ports/IRecipeBundleSerializerPort.h"
#include "recipe_lifecycle/domain/recipes/ports/IRecipeRepositoryPort.h"
#include "recipe_lifecycle/domain/recipes/ports/ITemplateRepositoryPort.h"
#include "runtime_execution/contracts/safety/IInterlockSignalPort.h"
#include "process_path/contracts/IPathSourcePort.h"
#include "runtime/configuration/WorkspaceAssetPaths.h"

#include <iostream>
#include <stdexcept>

namespace {
namespace RuntimeConfig = Siligen::Infrastructure::Configuration;

Siligen::Shared::Types::LogConfiguration BuildLogConfig(
    Siligen::Application::Container::LogMode log_mode,
    const std::string& log_file_path) {
    Siligen::Shared::Types::LogConfiguration log_config;
    log_config.min_level = Siligen::Shared::Types::LogLevel::INFO;
    log_config.enable_console = (log_mode == Siligen::Application::Container::LogMode::Console);
    log_config.enable_file = (log_mode == Siligen::Application::Container::LogMode::File);
    log_config.log_file_path = log_file_path;
    log_config.enable_timestamp = true;
    log_config.enable_thread_id = false;
    return log_config;
}

void RegisterMotionRuntimePorts(
    Siligen::Application::Container::ApplicationContainer& container,
    const std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort>&
        motion_runtime_port) {
    container.RegisterPort<Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort>(
        motion_runtime_port);
    // 兼容旧调用方：细粒度 motion 端口由容器在注册 runtime 时自动回填，
    // bootstrap 不再为同一运行时对象重复注册多条并行入口。
}

void ApplyBindings(
    Siligen::Application::Container::ApplicationContainer& container,
    const Siligen::Bootstrap::InfrastructureBindings& bindings) {
    if (bindings.logging_service) {
        container.SetLoggingService(bindings.logging_service);
    }
    if (bindings.multicard_instance) {
        container.SetMultiCardInstance(bindings.multicard_instance);
    }

    if (bindings.config_port) {
        container.RegisterPort<Siligen::Domain::Configuration::Ports::IConfigurationPort>(bindings.config_port);
    }
    if (bindings.device_connection_port) {
        container.RegisterPort<Siligen::Device::Contracts::Ports::DeviceConnectionPort>(
            bindings.device_connection_port);
    }
    if (bindings.motion_device_port) {
        container.RegisterPort<Siligen::Device::Contracts::Ports::MotionDevicePort>(bindings.motion_device_port);
    }
    if (bindings.dispenser_device_port) {
        container.RegisterPort<Siligen::Device::Contracts::Ports::DispenserDevicePort>(
            bindings.dispenser_device_port);
    }
    if (bindings.trigger_port) {
        container.RegisterPort<Siligen::Domain::Dispensing::Ports::ITriggerControllerPort>(bindings.trigger_port);
    }
    if (bindings.machine_health_port) {
        container.RegisterPort<Siligen::Device::Contracts::Ports::MachineHealthPort>(bindings.machine_health_port);
    }
    if (bindings.diagnostics_port) {
        container.RegisterPort<Siligen::Domain::Diagnostics::Ports::IDiagnosticsPort>(bindings.diagnostics_port);
    }
    if (bindings.test_record_repository) {
        container.RegisterPort<Siligen::Domain::Diagnostics::Ports::ITestRecordRepository>(
            bindings.test_record_repository);
    }
    if (bindings.test_config_manager) {
        container.RegisterPort<Siligen::Domain::Diagnostics::Ports::ITestConfigurationPort>(
            bindings.test_config_manager);
    }
    if (bindings.preset_port) {
        container.RegisterPort<Siligen::Domain::Diagnostics::Ports::ICMPTestPresetPort>(bindings.preset_port);
    }
    if (bindings.valve_port) {
        container.RegisterPort<Siligen::Domain::Dispensing::Ports::IValvePort>(bindings.valve_port);
    }
    if (bindings.file_storage_port) {
        container.RegisterPort<Siligen::Domain::Configuration::Ports::IFileStoragePort>(bindings.file_storage_port);
    }
    if (bindings.interpolation_port) {
        container.RegisterPort<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort>(
            bindings.interpolation_port);
    }
    if (bindings.velocity_profile_port) {
        container.RegisterPort<Siligen::Domain::Motion::Ports::IVelocityProfilePort>(bindings.velocity_profile_port);
    }
    if (bindings.motion_runtime_port) {
        RegisterMotionRuntimePorts(container, bindings.motion_runtime_port);
    }
    if (bindings.task_scheduler_port) {
        container.RegisterPort<Siligen::Domain::Dispensing::Ports::ITaskSchedulerPort>(
            bindings.task_scheduler_port);
    }
    if (bindings.event_port) {
        container.RegisterPort<Siligen::Domain::System::Ports::IEventPublisherPort>(bindings.event_port);
    }
    if (bindings.interlock_signal_port) {
        container.RegisterPort<Siligen::Domain::Safety::Ports::IInterlockSignalPort>(bindings.interlock_signal_port);
    }
    if (bindings.path_source_port) {
        container.RegisterPort<Siligen::ProcessPath::Contracts::IPathSourcePort>(bindings.path_source_port);
    }
    if (bindings.recipe_repository) {
        container.RegisterPort<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort>(bindings.recipe_repository);
    }
    if (bindings.template_repository) {
        container.RegisterPort<Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort>(bindings.template_repository);
    }
    if (bindings.audit_repository) {
        container.RegisterPort<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort>(bindings.audit_repository);
    }
    if (bindings.parameter_schema_port) {
        container.RegisterPort<Siligen::Domain::Recipes::Ports::IParameterSchemaPort>(bindings.parameter_schema_port);
    }
    if (bindings.recipe_bundle_serializer_port) {
        container.RegisterPort<Siligen::Domain::Recipes::Ports::IRecipeBundleSerializerPort>(
            bindings.recipe_bundle_serializer_port);
    }
}

std::string ResolveContainerConfigPath(
    const std::string& requested_config_file_path,
    std::string* resolved_config_file_path) {
    const auto config_resolution = RuntimeConfig::ResolveConfigFilePathWithBridge(requested_config_file_path);
    if (config_resolution.fail_fast) {
        throw std::runtime_error(
            "配置路径解析失败: requested='" + requested_config_file_path +
            "', mode=" + RuntimeConfig::ConfigPathBridgeModeName(config_resolution.mode) +
            ", detail=" + config_resolution.detail);
    }
    if (resolved_config_file_path != nullptr) {
        *resolved_config_file_path = config_resolution.resolved_path;
    }
    return config_resolution.resolved_path;
}

}  // namespace

namespace Siligen::Apps::Runtime {

std::shared_ptr<Application::Container::ApplicationContainer> BuildContainer(
    const std::string& config_file_path,
    Application::Container::LogMode log_mode,
    const std::string& log_file_path,
    int task_scheduler_threads,
    std::string* resolved_config_file_path) {
    const auto resolved_config_path =
        ResolveContainerConfigPath(config_file_path, resolved_config_file_path);
    auto container = std::make_shared<Application::Container::ApplicationContainer>(
        resolved_config_path,
        log_mode,
        log_file_path);

    Siligen::Bootstrap::InfrastructureBootstrapConfig bootstrap_config;
    bootstrap_config.config_file_path = resolved_config_path;
    bootstrap_config.log_config = BuildLogConfig(log_mode, log_file_path);
    bootstrap_config.task_scheduler_threads = task_scheduler_threads;

    auto bindings = Siligen::Bootstrap::CreateInfrastructureBindings(bootstrap_config);
    ApplyBindings(*container, bindings);
    container->Configure();
    return container;
}

}  // namespace Siligen::Apps::Runtime
