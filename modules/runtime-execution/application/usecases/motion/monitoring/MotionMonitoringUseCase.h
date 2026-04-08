#pragma once

#include "domain/motion/ports/IHomingPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "domain/diagnostics/ports/IDiagnosticsPort.h"
#include "domain/supervision/ports/IEventPublisherPort.h"
#include "runtime_execution/contracts/motion/IIOControlPort.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <functional>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Siligen::Application::UseCases::Motion::Monitoring {

/**
 * @brief 运动状态与IO监控用例
 */
class MotionMonitoringUseCase {
   public:
    using MotionStatusCallback =
        std::function<void(Siligen::Shared::Types::LogicalAxisId, const Domain::Motion::Ports::MotionStatus&)>;
    using IOStatusCallback =
        std::function<void(const Siligen::RuntimeExecution::Contracts::Motion::IOStatus&)>;

    MotionMonitoringUseCase(std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
                            std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort> io_port,
                            std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port,
                            std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort>
                                interpolation_port = nullptr,
                            std::shared_ptr<Siligen::Domain::Diagnostics::Ports::IDiagnosticsPort>
                                diagnostics_port = nullptr,
                            std::shared_ptr<Siligen::Domain::System::Ports::IEventPublisherPort>
                                event_publisher_port = nullptr);

    ~MotionMonitoringUseCase();

    Result<Domain::Motion::Ports::MotionStatus> GetAxisMotionStatus(Siligen::Shared::Types::LogicalAxisId axis) const;
    Result<std::vector<Domain::Motion::Ports::MotionStatus>> GetAllAxesMotionStatus() const;
    Result<Point2D> GetCurrentPosition() const;
    Result<Siligen::RuntimeExecution::Contracts::Motion::CoordinateSystemStatus>
        GetCoordinateSystemStatus(int16 coord_sys) const;
    Result<uint32> GetInterpolationBufferSpace(int16 coord_sys) const;
    Result<uint32> GetLookAheadBufferSpace(int16 coord_sys) const;

    Result<Siligen::RuntimeExecution::Contracts::Motion::IOStatus> ReadDigitalInputStatus(int16 channel) const;
    Result<std::vector<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>>
        ReadAllDigitalInputStatus() const;
    Result<Siligen::RuntimeExecution::Contracts::Motion::IOStatus> ReadDigitalOutputStatus(int16 channel) const;
    Result<std::vector<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>>
        ReadAllDigitalOutputStatus() const;
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
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort> io_port_;
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> interpolation_port_;
    std::shared_ptr<Siligen::Domain::Diagnostics::Ports::IDiagnosticsPort> diagnostics_port_;
    std::shared_ptr<Siligen::Domain::System::Ports::IEventPublisherPort> event_publisher_port_;

    MotionStatusCallback motion_status_callback_;
    IOStatusCallback io_status_callback_;
    mutable std::mutex callback_mutex_;

    std::atomic<bool> status_update_running_{false};
    std::atomic<bool> stop_status_update_requested_{false};
    std::atomic<int32> status_update_interval_ms_{100};
    std::atomic<std::uint32_t> motion_status_failure_count_{0};
    std::atomic<std::uint32_t> io_status_failure_count_{0};
    std::atomic<bool> motion_status_failure_logged_{false};
    std::atomic<bool> io_status_failure_logged_{false};
    mutable std::mutex status_update_lifecycle_mutex_;
    std::thread status_update_thread_;

    Result<void> ValidateAxisNumber(Siligen::Shared::Types::LogicalAxisId axis) const;
    Result<void> ValidateChannelNumber(int16 channel) const;
    void NotifyMotionStatusUpdate(Siligen::Shared::Types::LogicalAxisId axis,
                                  const Domain::Motion::Ports::MotionStatus& status);
    void NotifyIOStatusUpdate(const Siligen::RuntimeExecution::Contracts::Motion::IOStatus& signal);
    void StatusUpdateLoop();
    void StatusUpdateTimer();
    void RecordPollingTransition(const char* component,
                                 const Siligen::Shared::Types::Error* error,
                                 std::uint32_t failure_count,
                                 bool recovered) const;
};

}  // namespace Siligen::Application::UseCases::Motion::Monitoring







