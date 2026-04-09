#pragma once

#include "siligen/device/contracts/commands/device_commands.h"
#include "siligen/device/contracts/state/device_state.h"
#include "runtime_execution/contracts/motion/IHomingPort.h"
#include "runtime_execution/contracts/motion/IMotionRuntimePort.h"
#include "siligen/device/adapters/motion/MultiCardMotionAdapter.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace Siligen::Infrastructure::Adapters::Motion {

/**
 * @brief 统一 motion runtime 门面
 *
 * 以单一运行时入口聚合连接、心跳、回零和基础运动控制。
 * 细粒度端口与运行时聚合都由同一 canonical concrete 提供。
 */
class MotionRuntimeFacade final : public RuntimeExecution::Contracts::Motion::IMotionRuntimePort {
   public:
    MotionRuntimeFacade(std::shared_ptr<Siligen::Infrastructure::Adapters::MultiCardMotionAdapter> motion_adapter,
                        std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port);
    ~MotionRuntimeFacade() override;

    Result<void> Connect(const std::string& card_ip, const std::string& pc_ip, int16 port = 0) override;
    Result<void> Disconnect() override;
    Result<bool> IsConnected() const override;
    Result<void> Reset() override;
    Result<std::string> GetCardInfo() const override;

    Result<void> EnableAxis(LogicalAxisId axis) override;
    Result<void> DisableAxis(LogicalAxisId axis) override;
    Result<bool> IsAxisEnabled(LogicalAxisId axis) const override;
    Result<void> ClearAxisStatus(LogicalAxisId axis) override;
    Result<void> ClearPosition(LogicalAxisId axis) override;
    Result<void> SetAxisVelocity(LogicalAxisId axis, float32 velocity) override;
    Result<void> SetAxisAcceleration(LogicalAxisId axis, float32 acceleration) override;
    Result<void> SetSoftLimits(LogicalAxisId axis, float32 negative_limit, float32 positive_limit) override;
    Result<void> ConfigureAxis(LogicalAxisId axis, const Domain::Motion::Ports::AxisConfiguration& config) override;
    Result<void> SetHardLimits(LogicalAxisId axis,
                               short positive_io_index,
                               short negative_io_index,
                               short card_index = 0,
                               short signal_type = 0) override;
    Result<void> EnableHardLimits(LogicalAxisId axis, bool enable, short limit_type = -1) override;
    Result<void> SetHardLimitPolarity(LogicalAxisId axis,
                                      short positive_polarity,
                                      short negative_polarity) override;

    Result<void> MoveToPosition(const Point2D& position, float32 velocity) override;
    Result<void> MoveAxisToPosition(LogicalAxisId axis, float32 position, float32 velocity) override;
    Result<void> RelativeMove(LogicalAxisId axis, float32 distance, float32 velocity) override;
    Result<void> SynchronizedMove(const std::vector<Domain::Motion::Ports::MotionCommand>& commands) override;
    Result<void> StopAxis(LogicalAxisId axis, bool immediate = false) override;
    Result<void> StopAllAxes(bool immediate = false) override;
    Result<void> EmergencyStop() override;
    Result<void> RecoverFromEmergencyStop() override;
    Result<void> WaitForMotionComplete(LogicalAxisId axis, int32 timeout_ms = 60000) override;

    Result<Point2D> GetCurrentPosition() const override;
    Result<float32> GetAxisPosition(LogicalAxisId axis) const override;
    Result<float32> GetAxisVelocity(LogicalAxisId axis) const override;
    Result<Domain::Motion::Ports::MotionStatus> GetAxisStatus(LogicalAxisId axis) const override;
    Result<bool> IsAxisMoving(LogicalAxisId axis) const override;
    Result<bool> IsAxisInPosition(LogicalAxisId axis) const override;
    Result<std::vector<Domain::Motion::Ports::MotionStatus>> GetAllAxesStatus() const override;

    Result<void> StartJog(LogicalAxisId axis, int16 direction, float32 velocity) override;
    Result<void> StartJogStep(LogicalAxisId axis, int16 direction, float32 distance, float32 velocity) override;
    Result<void> StopJog(LogicalAxisId axis) override;
    Result<void> SetJogParameters(LogicalAxisId axis,
                                  const Domain::Motion::Ports::JogParameters& params) override;

    Result<Domain::Motion::Ports::IOStatus> ReadDigitalInput(int16 channel) override;
    Result<Domain::Motion::Ports::IOStatus> ReadDigitalOutput(int16 channel) override;
    Result<void> WriteDigitalOutput(int16 channel, bool value) override;
    Result<bool> ReadLimitStatus(LogicalAxisId axis, bool positive) override;
    Result<bool> ReadServoAlarm(LogicalAxisId axis) override;

    Result<void> HomeAxis(LogicalAxisId axis) override;
    Result<void> StopHoming(LogicalAxisId axis) override;
    Result<void> ResetHomingState(LogicalAxisId axis) override;
    Result<Domain::Motion::Ports::HomingStatus> GetHomingStatus(LogicalAxisId axis) const override;
    Result<bool> IsAxisHomed(LogicalAxisId axis) const override;
    Result<bool> IsHomingInProgress(LogicalAxisId axis) const override;
    Result<void> WaitForHomingComplete(LogicalAxisId axis, int32 timeout_ms = 80000) override;

    Result<void> ConnectDevice(const Siligen::Device::Contracts::Commands::DeviceConnection& connection);
    Result<Siligen::Device::Contracts::State::DeviceConnectionSnapshot> ReadConnection() const;
    bool IsDeviceConnected() const;
    Result<void> ReconnectDevice();
    void SetConnectionStateCallback(
        std::function<void(const Siligen::Device::Contracts::State::DeviceConnectionSnapshot&)> callback);
    Result<void> StartStatusMonitoring(std::uint32_t interval_ms = 1000);
    void StopStatusMonitoring();
    std::string GetLastError() const;
    void ClearError();
    Result<void> StartHeartbeat(const Siligen::Device::Contracts::State::HeartbeatSnapshot& config);
    void StopHeartbeat();
    Siligen::Device::Contracts::State::HeartbeatSnapshot ReadHeartbeat() const;
    Result<bool> Ping() const;

   private:
    bool ShouldSuppressTransientStatusCommunicationErrors() const;
    void MonitoringLoop(std::uint32_t interval_ms);
    void HeartbeatLoop();
    Result<bool> ExecuteHeartbeat() const;
    void UpdateDeviceInfo();

    std::shared_ptr<Siligen::Infrastructure::Adapters::MultiCardMotionAdapter> motion_adapter_;
    std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port_;

    mutable std::mutex connection_mutex_;
    Siligen::Device::Contracts::State::DeviceConnectionState connection_state_ =
        Siligen::Device::Contracts::State::DeviceConnectionState::Disconnected;
    std::string last_error_;
    Siligen::Device::Contracts::State::DeviceConnectionSnapshot connection_snapshot_;
    Siligen::Device::Contracts::Commands::DeviceConnection last_connection_;
    bool has_last_connection_ = false;
    std::chrono::steady_clock::time_point last_connect_time_{};

    std::thread monitoring_thread_;
    std::atomic<bool> should_monitor_{false};
    std::atomic<bool> monitoring_active_{false};
    std::uint32_t monitoring_interval_ms_ = 0;

    std::thread heartbeat_thread_;
    std::atomic<bool> should_heartbeat_{false};
    std::atomic<bool> heartbeat_active_{false};
    mutable std::mutex heartbeat_mutex_;
    Siligen::Device::Contracts::State::HeartbeatSnapshot heartbeat_snapshot_;

    std::function<void(const Siligen::Device::Contracts::State::DeviceConnectionSnapshot&)> state_change_callback_;
};

}  // namespace Siligen::Infrastructure::Adapters::Motion
