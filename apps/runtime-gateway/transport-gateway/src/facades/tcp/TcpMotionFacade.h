#pragma once

#include "domain/machine/ports/IHardwareConnectionPort.h"
#include "application/usecases/motion/homing/EnsureAxesReadyZeroUseCase.h"
#include "application/usecases/motion/homing/HomeAxesUseCase.h"
#include "application/usecases/motion/manual/ManualMotionControlUseCase.h"
#include "application/usecases/motion/monitoring/MotionMonitoringUseCase.h"

#include <memory>

namespace Siligen::Application::Facades::Tcp {

class TcpMotionFacade {
   public:
    TcpMotionFacade(std::shared_ptr<UseCases::Motion::Homing::HomeAxesUseCase> home_use_case,
                    std::shared_ptr<UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase> ensure_ready_zero_use_case,
                    std::shared_ptr<UseCases::Motion::Manual::ManualMotionControlUseCase> manual_use_case,
                    std::shared_ptr<UseCases::Motion::Monitoring::MotionMonitoringUseCase> monitoring_use_case,
                    std::shared_ptr<Domain::Machine::Ports::IHardwareConnectionPort> hardware_connection_port);

    Shared::Types::Result<UseCases::Motion::Homing::HomeAxesResponse> Home(
        const UseCases::Motion::Homing::HomeAxesRequest& request);
    Shared::Types::Result<UseCases::Motion::Homing::EnsureAxesReadyZeroResponse> EnsureAxesReadyZero(
        const UseCases::Motion::Homing::EnsureAxesReadyZeroRequest& request);
    Shared::Types::Result<bool> IsAxisHomed(Shared::Types::LogicalAxisId axis) const;

    Shared::Types::Result<void> ExecutePointToPointMotion(
        const UseCases::Motion::Manual::ManualMotionCommand& command,
        bool invalidate_homing = false);
    Shared::Types::Result<void> StartJog(Shared::Types::LogicalAxisId axis, int16 direction, float32 velocity);
    Shared::Types::Result<void> StopJog(Shared::Types::LogicalAxisId axis);

    Shared::Types::Result<Domain::Motion::Ports::MotionStatus> GetAxisMotionStatus(
        Shared::Types::LogicalAxisId axis) const;
    Shared::Types::Result<std::vector<Domain::Motion::Ports::MotionStatus>> GetAllAxesMotionStatus() const;
    Shared::Types::Result<Point2D> GetCurrentPosition() const;
    Shared::Types::Result<Domain::Motion::Ports::CoordinateSystemStatus> GetCoordinateSystemStatus(int16 coord_sys) const;
    Shared::Types::Result<uint32> GetInterpolationBufferSpace(int16 coord_sys) const;
    Shared::Types::Result<uint32> GetLookAheadBufferSpace(int16 coord_sys) const;
    Shared::Types::Result<bool> ReadLimitStatus(Shared::Types::LogicalAxisId axis, bool positive) const;
    Shared::Types::Result<bool> ReadServoAlarmStatus(Shared::Types::LogicalAxisId axis) const;
    bool HasHardwareConnectionPort() const;
    Domain::Machine::Ports::HardwareConnectionInfo GetHardwareConnectionInfo() const;
    Domain::Machine::Ports::HeartbeatStatus GetHeartbeatStatus() const;
    bool IsHardwareReadyForMotion() const;

   private:
    std::shared_ptr<UseCases::Motion::Homing::HomeAxesUseCase> home_use_case_;
    std::shared_ptr<UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase> ensure_ready_zero_use_case_;
    std::shared_ptr<UseCases::Motion::Manual::ManualMotionControlUseCase> manual_use_case_;
    std::shared_ptr<UseCases::Motion::Monitoring::MotionMonitoringUseCase> monitoring_use_case_;
    std::shared_ptr<Domain::Machine::Ports::IHardwareConnectionPort> hardware_connection_port_;
};

}  // namespace Siligen::Application::Facades::Tcp
