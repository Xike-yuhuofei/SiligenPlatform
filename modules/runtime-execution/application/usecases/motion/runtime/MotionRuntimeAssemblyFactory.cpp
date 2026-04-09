#include "MotionRuntimeAssemblyFactory.h"

#include "shared/types/Error.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"

#include <cmath>
#include <utility>

namespace Siligen::Application::UseCases::Motion::Runtime {
namespace {

using MotionValidationService = Siligen::Domain::Motion::DomainServices::MotionValidationService;
using Point2D = Siligen::Shared::Types::Point2D;
using Error = Siligen::Shared::Types::Error;
using ErrorCode = Siligen::Shared::Types::ErrorCode;
template <typename T>
using Result = Siligen::Shared::Types::Result<T>;
using ResultVoid = Siligen::Shared::Types::Result<void>;

class DefaultMotionRuntimeValidationService final : public MotionValidationService {
public:
    ResultVoid ValidatePosition(const Point2D& position) const override {
        if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
            return ResultVoid::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "Target position is not finite",
                "MotionRuntimeAssemblyFactory"));
        }
        return ResultVoid::Success();
    }

    ResultVoid ValidateSpeed(float speed) const override {
        if (!std::isfinite(speed) || speed <= 0.0f) {
            return ResultVoid::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "Speed must be positive",
                "MotionRuntimeAssemblyFactory"));
        }
        return ResultVoid::Success();
    }

    Result<bool> IsPositionErrorExceeded(const Point2D& target, const Point2D& actual) const override {
        return Result<bool>::Success(target.DistanceTo(actual) > 0.01f);
    }
};

}  // namespace

Result<MotionRuntimeAssembly> MotionRuntimeAssemblyFactory::Create(MotionRuntimeAssemblyDependencies dependencies) {
    MotionRuntimeAssembly assembly;
    if (!dependencies.motion_runtime_port ||
        !dependencies.interpolation_port ||
        !dependencies.configuration_port ||
        !dependencies.event_publisher_port ||
        !dependencies.services_provider) {
        return Result<MotionRuntimeAssembly>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "Motion runtime assembly dependencies are incomplete",
            "MotionRuntimeAssemblyFactory"));
    }

    auto provided_services = dependencies.services_provider->CreateServices(dependencies.motion_runtime_port);
    assembly.motion_control_service = std::move(provided_services.motion_control_service);
    assembly.motion_status_service = std::move(provided_services.motion_status_service);
    assembly.motion_validation_service = std::move(provided_services.motion_validation_service);
    if (!assembly.motion_validation_service) {
        assembly.motion_validation_service = std::make_shared<DefaultMotionRuntimeValidationService>();
    }
    if (!assembly.motion_control_service || !assembly.motion_status_service) {
        return Result<MotionRuntimeAssembly>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "Motion runtime services provider returned incomplete services",
            "MotionRuntimeAssemblyFactory"));
    }

    assembly.home_use_case = std::make_unique<Homing::HomeAxesUseCase>(
        dependencies.motion_runtime_port,
        dependencies.configuration_port,
        dependencies.motion_runtime_port,
        dependencies.event_publisher_port,
        dependencies.motion_runtime_port);
    assembly.move_use_case = std::make_unique<PTP::MoveToPositionUseCase>(
        assembly.motion_control_service,
        assembly.motion_status_service,
        assembly.motion_validation_service,
        dependencies.machine_execution_state_port,
        nullptr);
    assembly.coordination_use_case = std::make_unique<Coordination::MotionCoordinationUseCase>(
        dependencies.interpolation_port,
        dependencies.motion_runtime_port,
        dependencies.motion_runtime_port,
        dependencies.trigger_controller_port);
    assembly.monitoring_use_case = std::make_unique<Monitoring::MotionMonitoringUseCase>(
        dependencies.motion_runtime_port,
        dependencies.motion_runtime_port,
        dependencies.motion_runtime_port);
    assembly.safety_use_case = std::make_unique<Safety::MotionSafetyUseCase>(dependencies.motion_runtime_port);
    assembly.path_execution_use_case = std::make_unique<Trajectory::DeterministicPathExecutionUseCase>(
        dependencies.interpolation_port,
        dependencies.motion_runtime_port);

    return Result<MotionRuntimeAssembly>::Success(std::move(assembly));
}

}  // namespace Siligen::Application::UseCases::Motion::Runtime
