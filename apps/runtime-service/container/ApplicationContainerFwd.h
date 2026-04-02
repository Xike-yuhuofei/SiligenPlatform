#pragma once

#include "domain/motion/ports/IIOControlPort.h"
#include "domain/motion/ports/IMotionRuntimePort.h"

namespace Siligen::Shared::Interfaces {
class ILoggingService;
}

namespace Siligen::Domain::Configuration::Ports {
class IConfigurationPort;
struct ValveSupplyConfig;
struct HomingConfig;
}

namespace Siligen::JobIngest::Contracts::Storage {
class IFileStoragePort;
}

namespace Siligen::Device::Contracts::Ports {
class MachineHealthPort;
}

namespace Siligen::Domain::Motion::Ports {
class IAxisControlPort;
class IHomingPort;
class IInterpolationPort;
class IJogControlPort;
class IMotionConnectionPort;
class IMotionStatePort;
class IPositionControlPort;
class IVelocityProfilePort;
}

namespace Siligen::Domain::Dispensing::Ports {
class ITaskSchedulerPort;
class ITriggerControllerPort;
class IValvePort;
}

namespace Siligen::Domain::System::Ports {
class IEventPublisherPort;
}

namespace Siligen::Domain::Diagnostics::Ports {
class ICMPTestPresetPort;
class IDiagnosticsPort;
class ITestConfigurationPort;
class ITestRecordRepository;
}

namespace Siligen::Domain::Recipes::Ports {
class IAuditRepositoryPort;
class IParameterSchemaPort;
class IRecipeBundleSerializerPort;
class IRecipeRepositoryPort;
class ITemplateRepositoryPort;
}

namespace Siligen::Domain::Motion::DomainServices {
class JogController;
class MotionControlService;
class MotionStatusService;
class VelocityProfileService;
}

namespace Siligen::Domain::Dispensing::DomainServices {
class ValveCoordinationService;
}

namespace Siligen::Application::UseCases::System {
class IHardLimitMonitor;
class InitializeSystemUseCase;
class EmergencyStopUseCase;
}

namespace Siligen::Application::UseCases::Motion::Homing {
class HomeAxesUseCase;
class EnsureAxesReadyZeroUseCase;
}

namespace Siligen::Application::UseCases::Motion {
class MotionControlUseCase;
}

namespace Siligen::Application::UseCases::Motion::Trajectory {
class ExecuteTrajectoryUseCase;
}

namespace Siligen::Application::UseCases::Motion::Manual {
class ManualMotionControlUseCase;
}

namespace Siligen::Application::UseCases::Motion::Initialization {
class MotionInitializationUseCase;
}

namespace Siligen::Application::UseCases::Motion::Safety {
class MotionSafetyUseCase;
}

namespace Siligen::Application::UseCases::Motion::Monitoring {
class MotionMonitoringUseCase;
}

namespace Siligen::Application::UseCases::Motion::Coordination {
class MotionCoordinationUseCase;
}

namespace Siligen::Application::UseCases::Motion::Interpolation {
class InterpolationPlanningUseCase;
}

namespace Siligen::Application::UseCases::Dispensing::Valve {
class ValveCommandUseCase;
class ValveQueryUseCase;
}

namespace Siligen::Application::UseCases::Dispensing {
class CleanupFilesUseCase;
class DispensingExecutionUseCase;
class DispensingWorkflowUseCase;
class IUploadFilePort;
class PlanningUseCase;
class UploadFileUseCase;
}

namespace Siligen::Application::UseCases::Recipes {
class CompareRecipeVersionsUseCase;
class CreateDraftVersionUseCase;
class CreateRecipeUseCase;
class CreateVersionFromPublishedUseCase;
class ExportRecipeBundlePayloadUseCase;
class ImportRecipeBundlePayloadUseCase;
class RecipeCommandUseCase;
class RecipeQueryUseCase;
class UpdateDraftVersionUseCase;
class UpdateRecipeUseCase;
}

namespace Siligen::RuntimeExecution::Contracts::System {
class IMachineExecutionStatePort;
}

namespace Siligen::Application::Container {

enum class LogMode {
    Console,
    File,
    Silent
};

class ApplicationContainer;

}  // namespace Siligen::Application::Container

