#pragma once

#include "runtime_execution/application/usecases/motion/homing/EnsureAxesReadyZeroUseCase.h"
#include "runtime_execution/application/usecases/motion/homing/HomeAxesUseCase.h"
#include "runtime_execution/application/usecases/motion/manual/ManualMotionControlUseCase.h"
#include "runtime_execution/application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>
#include <vector>

namespace Siligen::Application::UseCases::Motion {

class MotionControlUseCase {
   public:
    MotionControlUseCase(
        std::shared_ptr<Homing::HomeAxesUseCase> home_use_case,
        std::shared_ptr<Homing::EnsureAxesReadyZeroUseCase> ensure_ready_zero_use_case,
        std::shared_ptr<Manual::ManualMotionControlUseCase> manual_use_case,
        std::shared_ptr<Monitoring::MotionMonitoringUseCase> monitoring_use_case);

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
    std::shared_ptr<Homing::HomeAxesUseCase> home_use_case_;
    std::shared_ptr<Homing::EnsureAxesReadyZeroUseCase> ensure_ready_zero_use_case_;
    std::shared_ptr<Manual::ManualMotionControlUseCase> manual_use_case_;
    std::shared_ptr<Monitoring::MotionMonitoringUseCase> monitoring_use_case_;
};

}  // namespace Siligen::Application::UseCases::Motion
