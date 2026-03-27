#pragma once

#include "domain/motion/ports/IMotionRuntimePort.h"
#include "shared/types/LogTypes.h"

#include <memory>
#include <string>

namespace Siligen {
namespace Shared {
namespace Interfaces {
class ILoggingService;
}  // namespace Interfaces
}  // namespace Shared

namespace Device {
namespace Contracts {
namespace Ports {
class DeviceConnectionPort;
class MachineHealthPort;
}  // namespace Ports
}  // namespace Contracts
}  // namespace Device

namespace Domain {
namespace Safety {
namespace Ports {
class IInterlockSignalPort;
}  // namespace Ports
}  // namespace Safety

namespace Configuration {
namespace Ports {
class IConfigurationPort;
class IFileStoragePort;
}  // namespace Ports
}  // namespace Configuration

namespace Motion {
namespace Ports {
class IAxisControlPort;
class IHomingPort;
class IInterpolationPort;
class IJogControlPort;
class IMotionConnectionPort;
class IMotionStatePort;
class IPositionControlPort;
class IVelocityProfilePort;
}  // namespace Ports
}  // namespace Motion

namespace Dispensing {
namespace Ports {
class ITriggerControllerPort;
class IValvePort;
class ITaskSchedulerPort;
}  // namespace Ports
}  // namespace Dispensing

namespace Diagnostics {
namespace Ports {
class IDiagnosticsPort;
class ICMPTestPresetPort;
class ITestConfigurationPort;
class ITestRecordRepository;
}  // namespace Ports
}  // namespace Diagnostics

namespace Recipes {
namespace Ports {
class IRecipeRepositoryPort;
class ITemplateRepositoryPort;
class IAuditRepositoryPort;
class IParameterSchemaPort;
class IRecipeBundleSerializerPort;
}  // namespace Ports
}  // namespace Recipes

namespace System {
namespace Ports {
class IEventPublisherPort;
}  // namespace Ports
}  // namespace System

namespace Trajectory {
namespace Ports {
class IPathSourcePort;
class IDXFPathSourcePort;
}  // namespace Ports
}  // namespace Trajectory
}  // namespace Domain

namespace Bootstrap {

struct InfrastructureBootstrapConfig {
    std::string config_file_path;
    Shared::Types::LogConfiguration log_config;
    int task_scheduler_threads = 4;
};

struct InfrastructureBindings {
    std::shared_ptr<Shared::Interfaces::ILoggingService> logging_service;

    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port;
    std::shared_ptr<Device::Contracts::Ports::DeviceConnectionPort> device_connection_port;
    std::shared_ptr<Device::Contracts::Ports::MachineHealthPort> machine_health_port;
    std::shared_ptr<Domain::Diagnostics::Ports::IDiagnosticsPort> diagnostics_port;
    std::shared_ptr<Domain::Dispensing::Ports::ITriggerControllerPort> trigger_port;
    std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port;
    std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port;
    std::shared_ptr<Domain::Motion::Ports::IVelocityProfilePort> velocity_profile_port;
    std::shared_ptr<Domain::Motion::Ports::IMotionRuntimePort> motion_runtime_port;
    std::shared_ptr<Domain::Configuration::Ports::IFileStoragePort> file_storage_port;
    std::shared_ptr<Domain::Diagnostics::Ports::ITestRecordRepository> test_record_repository;
    std::shared_ptr<Domain::Diagnostics::Ports::ITestConfigurationPort> test_config_manager;
    std::shared_ptr<Domain::Diagnostics::Ports::ICMPTestPresetPort> preset_port;
    std::shared_ptr<Domain::Dispensing::Ports::ITaskSchedulerPort> task_scheduler_port;
    std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port;
    std::shared_ptr<Domain::Safety::Ports::IInterlockSignalPort> interlock_signal_port;
    std::shared_ptr<Domain::Trajectory::Ports::IPathSourcePort> path_source_port;
    std::shared_ptr<Domain::Trajectory::Ports::IDXFPathSourcePort> dxf_path_source_port;
    std::shared_ptr<Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository;
    std::shared_ptr<Domain::Recipes::Ports::ITemplateRepositoryPort> template_repository;
    std::shared_ptr<Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository;
    std::shared_ptr<Domain::Recipes::Ports::IParameterSchemaPort> parameter_schema_port;
    std::shared_ptr<Domain::Recipes::Ports::IRecipeBundleSerializerPort> recipe_bundle_serializer_port;

    std::string upload_base_dir;
    std::string recipe_base_dir;
    std::shared_ptr<void> multicard_instance;
};

InfrastructureBindings CreateInfrastructureBindings(const InfrastructureBootstrapConfig& config);

}  // namespace Bootstrap
}  // namespace Siligen
