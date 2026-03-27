#include "runtime_execution/application/usecases/motion/MotionControlUseCase.h"

#include "application/usecases/motion/homing/EnsureAxesReadyZeroUseCase.h"
#include "application/usecases/motion/homing/HomeAxesUseCase.h"
#include "application/usecases/motion/manual/ManualMotionControlUseCase.h"
#include "application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
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
    std::shared_ptr<Homing::HomeAxesUseCase> home_use_case,
    std::shared_ptr<Homing::EnsureAxesReadyZeroUseCase> ensure_ready_zero_use_case,
    std::shared_ptr<Manual::ManualMotionControlUseCase> manual_use_case,
    std::shared_ptr<Monitoring::MotionMonitoringUseCase> monitoring_use_case)
    : home_use_case_(std::move(home_use_case)),
      ensure_ready_zero_use_case_(std::move(ensure_ready_zero_use_case)),
      manual_use_case_(std::move(manual_use_case)),
      monitoring_use_case_(std::move(monitoring_use_case)) {}

Result<Homing::HomeAxesResponse> MotionControlUseCase::Home(const Homing::HomeAxesRequest& request) {
    if (!home_use_case_) {
        return MissingDependency<Homing::HomeAxesResponse>("HomeAxesUseCase");
    }
    return home_use_case_->Execute(request);
}

Result<Homing::EnsureAxesReadyZeroResponse> MotionControlUseCase::EnsureAxesReadyZero(
    const Homing::EnsureAxesReadyZeroRequest& request) {
    if (!ensure_ready_zero_use_case_) {
        return MissingDependency<Homing::EnsureAxesReadyZeroResponse>("EnsureAxesReadyZeroUseCase");
    }
    return ensure_ready_zero_use_case_->Execute(request);
}

Result<bool> MotionControlUseCase::IsAxisHomed(Siligen::Shared::Types::LogicalAxisId axis) const {
    if (!home_use_case_) {
        return MissingDependency<bool>("HomeAxesUseCase");
    }
    return home_use_case_->IsAxisHomed(axis);
}

Result<void> MotionControlUseCase::ExecutePointToPointMotion(
    const Manual::ManualMotionCommand& command,
    bool invalidate_homing) {
    if (!manual_use_case_) {
        return MissingVoidDependency("ManualMotionControlUseCase");
    }
    return manual_use_case_->ExecutePointToPointMotion(command, invalidate_homing);
}

Result<void> MotionControlUseCase::StartJog(
    Siligen::Shared::Types::LogicalAxisId axis,
    int16 direction,
    Siligen::Shared::Types::float32 velocity) {
    if (!manual_use_case_) {
        return MissingVoidDependency("ManualMotionControlUseCase");
    }
    return manual_use_case_->StartJogMotion(axis, direction, velocity);
}

Result<void> MotionControlUseCase::StopJog(Siligen::Shared::Types::LogicalAxisId axis) {
    if (!manual_use_case_) {
        return MissingVoidDependency("ManualMotionControlUseCase");
    }
    return manual_use_case_->StopJogMotion(axis);
}

Result<Domain::Motion::Ports::MotionStatus> MotionControlUseCase::GetAxisMotionStatus(
    Siligen::Shared::Types::LogicalAxisId axis) const {
    if (!monitoring_use_case_) {
        return MissingDependency<Domain::Motion::Ports::MotionStatus>("MotionMonitoringUseCase");
    }
    return monitoring_use_case_->GetAxisMotionStatus(axis);
}

Result<std::vector<Domain::Motion::Ports::MotionStatus>> MotionControlUseCase::GetAllAxesMotionStatus() const {
    if (!monitoring_use_case_) {
        return MissingDependency<std::vector<Domain::Motion::Ports::MotionStatus>>("MotionMonitoringUseCase");
    }
    return monitoring_use_case_->GetAllAxesMotionStatus();
}

Result<Siligen::Shared::Types::Point2D> MotionControlUseCase::GetCurrentPosition() const {
    if (!monitoring_use_case_) {
        return MissingDependency<Siligen::Shared::Types::Point2D>("MotionMonitoringUseCase");
    }
    return monitoring_use_case_->GetCurrentPosition();
}

Result<Domain::Motion::Ports::CoordinateSystemStatus> MotionControlUseCase::GetCoordinateSystemStatus(
    int16 coord_sys) const {
    if (!monitoring_use_case_) {
        return MissingDependency<Domain::Motion::Ports::CoordinateSystemStatus>("MotionMonitoringUseCase");
    }
    return monitoring_use_case_->GetCoordinateSystemStatus(coord_sys);
}

Result<uint32> MotionControlUseCase::GetInterpolationBufferSpace(int16 coord_sys) const {
    if (!monitoring_use_case_) {
        return MissingDependency<uint32>("MotionMonitoringUseCase");
    }
    return monitoring_use_case_->GetInterpolationBufferSpace(coord_sys);
}

Result<uint32> MotionControlUseCase::GetLookAheadBufferSpace(int16 coord_sys) const {
    if (!monitoring_use_case_) {
        return MissingDependency<uint32>("MotionMonitoringUseCase");
    }
    return monitoring_use_case_->GetLookAheadBufferSpace(coord_sys);
}

Result<bool> MotionControlUseCase::ReadLimitStatus(
    Siligen::Shared::Types::LogicalAxisId axis,
    bool positive) const {
    if (!monitoring_use_case_) {
        return MissingDependency<bool>("MotionMonitoringUseCase");
    }
    return monitoring_use_case_->ReadLimitStatus(axis, positive);
}

Result<bool> MotionControlUseCase::ReadServoAlarmStatus(Siligen::Shared::Types::LogicalAxisId axis) const {
    if (!monitoring_use_case_) {
        return MissingDependency<bool>("MotionMonitoringUseCase");
    }
    return monitoring_use_case_->ReadServoAlarmStatus(axis);
}

}  // namespace Siligen::Application::UseCases::Motion
