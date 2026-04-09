#pragma once

#include "runtime/contracts/system/IEventPublisherPort.h"
#include "runtime_execution/contracts/motion/IAxisControlPort.h"
#include "runtime_execution/contracts/motion/IHomingPort.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "runtime_execution/contracts/motion/IIOControlPort.h"
#include "runtime_execution/contracts/motion/IJogControlPort.h"
#include "runtime_execution/contracts/motion/IMotionConnectionPort.h"
#include "runtime_execution/contracts/motion/IMotionRuntimePort.h"
#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "runtime_execution/contracts/motion/IPositionControlPort.h"
#include "runtime_execution/contracts/motion/MotionControlService.h"
#include "runtime_execution/contracts/motion/MotionStatusService.h"

namespace Siligen::Shared::Interfaces {
class ILoggingService;
}

namespace Siligen::Domain::Configuration::Ports {
class IConfigurationPort;
class IFileStoragePort;
struct ValveSupplyConfig;
struct HomingConfig;
}

namespace Siligen::Device::Contracts::Ports {
class MachineHealthPort;
}

namespace Siligen::Domain::Motion::Ports {
class IVelocityProfilePort;
}

namespace Siligen::Domain::Dispensing::Ports {
class ITaskSchedulerPort;
class ITriggerControllerPort;
class IValvePort;
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
class DispensingExecutionUseCase;
class DispensingWorkflowUseCase;
class PlanningUseCase;
}

namespace Siligen::JobIngest::Contracts {
class IUploadFilePort;
}

namespace Siligen::JobIngest::Application::UseCases::Dispensing {
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

