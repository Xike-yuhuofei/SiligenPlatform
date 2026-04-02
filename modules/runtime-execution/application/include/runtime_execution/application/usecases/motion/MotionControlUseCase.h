#pragma once

#include "runtime_execution/contracts/motion/HomingProcess.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>
#include <vector>

namespace Siligen::Application::UseCases::Motion::Homing {
struct EnsureAxesReadyZeroRequest;
struct EnsureAxesReadyZeroResponse;
using HomeAxesRequest = Siligen::Domain::Motion::DomainServices::HomeAxesRequest;
using HomeAxesResponse = Siligen::Domain::Motion::DomainServices::HomeAxesResponse;
}  // namespace Siligen::Application::UseCases::Motion::Homing

namespace Siligen::Application::UseCases::Motion::Manual {
struct ManualMotionCommand;
}  // namespace Siligen::Application::UseCases::Motion::Manual

namespace Siligen::Application::UseCases::Motion {

class IMotionHomingOperations {
   public:
    virtual ~IMotionHomingOperations() = default;

    virtual Result<Homing::HomeAxesResponse> Home(const Homing::HomeAxesRequest& request) = 0;
    virtual Result<Homing::EnsureAxesReadyZeroResponse> EnsureAxesReadyZero(
        const Homing::EnsureAxesReadyZeroRequest& request) = 0;
    virtual Result<bool> IsAxisHomed(Siligen::Shared::Types::LogicalAxisId axis) const = 0;
};

class IMotionManualOperations {
   public:
    virtual ~IMotionManualOperations() = default;

    virtual Result<void> ExecutePointToPointMotion(
        const Manual::ManualMotionCommand& command,
        bool invalidate_homing) = 0;
    virtual Result<void> StartJog(
        Siligen::Shared::Types::LogicalAxisId axis,
        int16 direction,
        Siligen::Shared::Types::float32 velocity) = 0;
    virtual Result<void> StopJog(Siligen::Shared::Types::LogicalAxisId axis) = 0;
};

class IMotionMonitoringOperations {
   public:
    virtual ~IMotionMonitoringOperations() = default;

    virtual Result<Domain::Motion::Ports::MotionStatus> GetAxisMotionStatus(
        Siligen::Shared::Types::LogicalAxisId axis) const = 0;
    virtual Result<std::vector<Domain::Motion::Ports::MotionStatus>> GetAllAxesMotionStatus() const = 0;
    virtual Result<Siligen::Shared::Types::Point2D> GetCurrentPosition() const = 0;
    virtual Result<Domain::Motion::Ports::CoordinateSystemStatus> GetCoordinateSystemStatus(int16 coord_sys)
        const = 0;
    virtual Result<uint32> GetInterpolationBufferSpace(int16 coord_sys) const = 0;
    virtual Result<uint32> GetLookAheadBufferSpace(int16 coord_sys) const = 0;
    virtual Result<bool> ReadLimitStatus(Siligen::Shared::Types::LogicalAxisId axis, bool positive) const = 0;
    virtual Result<bool> ReadServoAlarmStatus(Siligen::Shared::Types::LogicalAxisId axis) const = 0;
};

class MotionControlUseCase {
   public:
    MotionControlUseCase(
        std::shared_ptr<IMotionHomingOperations> homing_operations,
        std::shared_ptr<IMotionManualOperations> manual_operations,
        std::shared_ptr<IMotionMonitoringOperations> monitoring_operations);

    Result<Homing::HomeAxesResponse> Home(const Homing::HomeAxesRequest& request);
    Result<Homing::EnsureAxesReadyZeroResponse> EnsureAxesReadyZero(
        const Homing::EnsureAxesReadyZeroRequest& request);
    Result<bool> IsAxisHomed(Siligen::Shared::Types::LogicalAxisId axis) const;

    Result<void> ExecutePointToPointMotion(
        const Manual::ManualMotionCommand& command,
        bool invalidate_homing = false);
    Result<void> StartJog(
        Siligen::Shared::Types::LogicalAxisId axis,
        int16 direction,
        Siligen::Shared::Types::float32 velocity);
    Result<void> StopJog(Siligen::Shared::Types::LogicalAxisId axis);

    Result<Domain::Motion::Ports::MotionStatus> GetAxisMotionStatus(
        Siligen::Shared::Types::LogicalAxisId axis) const;
    Result<std::vector<Domain::Motion::Ports::MotionStatus>> GetAllAxesMotionStatus() const;
    Result<Siligen::Shared::Types::Point2D> GetCurrentPosition() const;
    Result<Domain::Motion::Ports::CoordinateSystemStatus> GetCoordinateSystemStatus(int16 coord_sys) const;
    Result<uint32> GetInterpolationBufferSpace(int16 coord_sys) const;
    Result<uint32> GetLookAheadBufferSpace(int16 coord_sys) const;
    Result<bool> ReadLimitStatus(Siligen::Shared::Types::LogicalAxisId axis, bool positive) const;
    Result<bool> ReadServoAlarmStatus(Siligen::Shared::Types::LogicalAxisId axis) const;

   private:
    std::shared_ptr<IMotionHomingOperations> homing_operations_;
    std::shared_ptr<IMotionManualOperations> manual_operations_;
    std::shared_ptr<IMotionMonitoringOperations> monitoring_operations_;
};

}  // namespace Siligen::Application::UseCases::Motion
