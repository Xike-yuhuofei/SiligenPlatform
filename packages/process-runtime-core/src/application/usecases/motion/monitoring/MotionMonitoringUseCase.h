#pragma once

#include "domain/motion/ports/IIOControlPort.h"
#include "domain/motion/ports/IHomingPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <functional>
#include <memory>
#include <vector>

namespace Siligen::Application::UseCases::Motion::Monitoring {

/**
 * @brief 运动状态与IO监控用例
 */
class MotionMonitoringUseCase {
   public:
    using MotionStatusCallback =
        std::function<void(Siligen::Shared::Types::LogicalAxisId, const Domain::Motion::Ports::MotionStatus&)>;
    using IOStatusCallback = std::function<void(const Domain::Motion::Ports::IOStatus&)>;

    MotionMonitoringUseCase(std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
                            std::shared_ptr<Domain::Motion::Ports::IIOControlPort> io_port,
                            std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port);

    ~MotionMonitoringUseCase() = default;

    Result<Domain::Motion::Ports::MotionStatus> GetAxisMotionStatus(Siligen::Shared::Types::LogicalAxisId axis) const;
    Result<std::vector<Domain::Motion::Ports::MotionStatus>> GetAllAxesMotionStatus() const;
    Result<Point2D> GetCurrentPosition() const;

    Result<Domain::Motion::Ports::IOStatus> ReadDigitalInputStatus(int16 channel) const;
    Result<std::vector<Domain::Motion::Ports::IOStatus>> ReadAllDigitalInputStatus() const;
    Result<Domain::Motion::Ports::IOStatus> ReadDigitalOutputStatus(int16 channel) const;
    Result<std::vector<Domain::Motion::Ports::IOStatus>> ReadAllDigitalOutputStatus() const;
    Result<bool> ReadLimitStatus(Siligen::Shared::Types::LogicalAxisId axis, bool positive) const;
    Result<bool> ReadServoAlarmStatus(Siligen::Shared::Types::LogicalAxisId axis) const;

    void SetMotionStatusCallback(MotionStatusCallback callback);
    void SetIOStatusCallback(IOStatusCallback callback);

    Result<void> StartStatusUpdate(int32 interval_ms = 100);
    void StopStatusUpdate();
    bool IsStatusUpdateRunning() const;

   private:
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port_;
    std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port_;
    std::shared_ptr<Domain::Motion::Ports::IIOControlPort> io_port_;

    MotionStatusCallback motion_status_callback_;
    IOStatusCallback io_status_callback_;

    bool status_update_running_ = false;
    int32 status_update_interval_ms_ = 100;

    Result<void> ValidateAxisNumber(Siligen::Shared::Types::LogicalAxisId axis) const;
    Result<void> ValidateChannelNumber(int16 channel) const;
    void NotifyMotionStatusUpdate(Siligen::Shared::Types::LogicalAxisId axis,
                                  const Domain::Motion::Ports::MotionStatus& status);
    void NotifyIOStatusUpdate(const Domain::Motion::Ports::IOStatus& signal);
    void StatusUpdateTimer();
};

}  // namespace Siligen::Application::UseCases::Motion::Monitoring







