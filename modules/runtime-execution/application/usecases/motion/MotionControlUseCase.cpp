#include "runtime_execution/application/usecases/motion/MotionControlUseCase.h"

#include "shared/types/Error.h"

#include <string>
#include <utility>

namespace Siligen::Application::UseCases::Motion {
namespace {

using Error = Siligen::Shared::Types::Error;
using ErrorCode = Siligen::Shared::Types::ErrorCode;

constexpr const char* kErrorSource = "MotionControlUseCase";

template <typename TResult>
Result<TResult> MissingDependency(const char* dependency_name) {
    return Result<TResult>::Failure(
        Error(ErrorCode::PORT_NOT_INITIALIZED, std::string(dependency_name) + " not available", kErrorSource));
}

Result<void> MissingVoidDependency(const char* dependency_name) {
    return Result<void>::Failure(
        Error(ErrorCode::PORT_NOT_INITIALIZED, std::string(dependency_name) + " not available", kErrorSource));
}

}  // namespace

MotionControlUseCase::MotionControlUseCase(
    std::shared_ptr<IMotionHomingOperations> homing_operations,
    std::shared_ptr<IMotionManualOperations> manual_operations,
    std::shared_ptr<IMotionMonitoringOperations> monitoring_operations)
    : homing_operations_(std::move(homing_operations)),
      manual_operations_(std::move(manual_operations)),
      monitoring_operations_(std::move(monitoring_operations)) {}

Result<Homing::HomeAxesResponse> MotionControlUseCase::Home(const Homing::HomeAxesRequest& request) {
    if (!homing_operations_) {
        return MissingDependency<Homing::HomeAxesResponse>("IMotionHomingOperations");
    }
    return homing_operations_->Home(request);
}

Result<Homing::EnsureAxesReadyZeroResponse> MotionControlUseCase::EnsureAxesReadyZero(
    const Homing::EnsureAxesReadyZeroRequest& request) {
    if (!homing_operations_) {
        return MissingDependency<Homing::EnsureAxesReadyZeroResponse>("IMotionHomingOperations");
    }
    return homing_operations_->EnsureAxesReadyZero(request);
}

Result<bool> MotionControlUseCase::IsAxisHomed(Siligen::Shared::Types::LogicalAxisId axis) const {
    if (!homing_operations_) {
        return MissingDependency<bool>("IMotionHomingOperations");
    }
    return homing_operations_->IsAxisHomed(axis);
}

Result<void> MotionControlUseCase::ExecutePointToPointMotion(
    const Manual::ManualMotionCommand& command,
    bool invalidate_homing) {
    if (!manual_operations_) {
        return MissingVoidDependency("IMotionManualOperations");
    }
    return manual_operations_->ExecutePointToPointMotion(command, invalidate_homing);
}

Result<void> MotionControlUseCase::StartJog(
    Siligen::Shared::Types::LogicalAxisId axis,
    int16 direction,
    Siligen::Shared::Types::float32 velocity) {
    if (!manual_operations_) {
        return MissingVoidDependency("IMotionManualOperations");
    }
    return manual_operations_->StartJog(axis, direction, velocity);
}

Result<void> MotionControlUseCase::StopJog(Siligen::Shared::Types::LogicalAxisId axis) {
    if (!manual_operations_) {
        return MissingVoidDependency("IMotionManualOperations");
    }
    return manual_operations_->StopJog(axis);
}

Result<Domain::Motion::Ports::MotionStatus> MotionControlUseCase::GetAxisMotionStatus(
    Siligen::Shared::Types::LogicalAxisId axis) const {
    if (!monitoring_operations_) {
        return MissingDependency<Domain::Motion::Ports::MotionStatus>("IMotionMonitoringOperations");
    }
    return monitoring_operations_->GetAxisMotionStatus(axis);
}

Result<std::vector<Domain::Motion::Ports::MotionStatus>> MotionControlUseCase::GetAllAxesMotionStatus() const {
    if (!monitoring_operations_) {
        return MissingDependency<std::vector<Domain::Motion::Ports::MotionStatus>>("IMotionMonitoringOperations");
    }
    return monitoring_operations_->GetAllAxesMotionStatus();
}

Result<Siligen::Shared::Types::Point2D> MotionControlUseCase::GetCurrentPosition() const {
    if (!monitoring_operations_) {
        return MissingDependency<Siligen::Shared::Types::Point2D>("IMotionMonitoringOperations");
    }
    return monitoring_operations_->GetCurrentPosition();
}

Result<Domain::Motion::Ports::CoordinateSystemStatus> MotionControlUseCase::GetCoordinateSystemStatus(
    int16 coord_sys) const {
    if (!monitoring_operations_) {
        return MissingDependency<Domain::Motion::Ports::CoordinateSystemStatus>("IMotionMonitoringOperations");
    }
    return monitoring_operations_->GetCoordinateSystemStatus(coord_sys);
}

Result<uint32> MotionControlUseCase::GetInterpolationBufferSpace(int16 coord_sys) const {
    if (!monitoring_operations_) {
        return MissingDependency<uint32>("IMotionMonitoringOperations");
    }
    return monitoring_operations_->GetInterpolationBufferSpace(coord_sys);
}

Result<uint32> MotionControlUseCase::GetLookAheadBufferSpace(int16 coord_sys) const {
    if (!monitoring_operations_) {
        return MissingDependency<uint32>("IMotionMonitoringOperations");
    }
    return monitoring_operations_->GetLookAheadBufferSpace(coord_sys);
}

Result<bool> MotionControlUseCase::ReadLimitStatus(
    Siligen::Shared::Types::LogicalAxisId axis,
    bool positive) const {
    if (!monitoring_operations_) {
        return MissingDependency<bool>("IMotionMonitoringOperations");
    }
    return monitoring_operations_->ReadLimitStatus(axis, positive);
}

Result<bool> MotionControlUseCase::ReadServoAlarmStatus(Siligen::Shared::Types::LogicalAxisId axis) const {
    if (!monitoring_operations_) {
        return MissingDependency<bool>("IMotionMonitoringOperations");
    }
    return monitoring_operations_->ReadServoAlarmStatus(axis);
}

}  // namespace Siligen::Application::UseCases::Motion
