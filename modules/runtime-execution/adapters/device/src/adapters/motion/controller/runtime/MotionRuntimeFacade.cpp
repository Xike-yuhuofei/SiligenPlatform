#define MODULE_NAME "MotionRuntimeFacade"

#include "MotionRuntimeFacade.h"

#include "siligen/device/adapters/drivers/multicard/error_mapper.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <thread>

namespace {

constexpr auto kConnectionGracePeriod = std::chrono::milliseconds(7000);

bool IsGracePeriodActive(const std::chrono::steady_clock::time_point& last_connect_time) {
    if (last_connect_time == std::chrono::steady_clock::time_point{}) {
        return false;
    }
    return (std::chrono::steady_clock::now() - last_connect_time) < kConnectionGracePeriod;
}

void SanitizeTransientCommunicationFault(Siligen::Domain::Motion::Ports::MotionStatus& status, bool suppress_fault) {
    using MotionState = Siligen::Domain::Motion::Ports::MotionState;

    if (!suppress_fault || !status.has_error ||
        !Siligen::Hal::Drivers::MultiCard::ErrorMapper::IsCommunicationError(status.error_code) ||
        status.servo_alarm || status.following_error || status.home_failed) {
        return;
    }

    status.has_error = false;
    status.error_code = 0;
    if (status.state == MotionState::FAULT) {
        status.state = status.enabled ? MotionState::IDLE : MotionState::DISABLED;
    }
}

}  // namespace

namespace Siligen::Infrastructure::Adapters::Motion {

using Siligen::Domain::Machine::Ports::HardwareConnectionConfig;
using Siligen::Domain::Machine::Ports::HardwareConnectionInfo;
using Siligen::Domain::Machine::Ports::HardwareConnectionState;
using Siligen::Domain::Machine::Ports::HeartbeatConfig;
using Siligen::Domain::Machine::Ports::HeartbeatStatus;
using Siligen::Domain::Motion::Ports::HomingStatus;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

MotionRuntimeFacade::MotionRuntimeFacade(
    std::shared_ptr<Siligen::Infrastructure::Adapters::MultiCardMotionAdapter> motion_adapter,
    std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port)
    : motion_adapter_(std::move(motion_adapter)), homing_port_(std::move(homing_port)) {
    connection_info_.device_type = "MultiCard Motion Runtime";
}

MotionRuntimeFacade::~MotionRuntimeFacade() {
    StopRuntimeStatusMonitoring();
    StopRuntimeHeartbeat();
    (void)Disconnect();
}

Result<void> MotionRuntimeFacade::Connect(const std::string& card_ip, const std::string& pc_ip, int16 port) {
    if (port < 0) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "端口号不能为负数", "MotionRuntimeFacade"));
    }

    HardwareConnectionConfig config;
    config.card_ip = card_ip;
    config.local_ip = pc_ip;
    config.card_port = static_cast<decltype(config.card_port)>(port);
    config.local_port = static_cast<decltype(config.local_port)>(port);
    return ConnectRuntime(config);
}

Result<void> MotionRuntimeFacade::Disconnect() {
    StopRuntimeStatusMonitoring();
    StopRuntimeHeartbeat();

    std::lock_guard<std::mutex> lock(connection_mutex_);
    if (!motion_adapter_) {
        connection_state_ = HardwareConnectionState::Disconnected;
        return Result<void>::Success();
    }

    auto result = motion_adapter_->Disconnect();
    if (result.IsError()) {
        connection_state_ = HardwareConnectionState::Error;
        last_error_ = result.GetError().GetMessage();
        return result;
    }

    connection_state_ = HardwareConnectionState::Disconnected;
    last_error_.clear();
    last_connect_time_ = std::chrono::steady_clock::time_point{};
    connection_info_.state = connection_state_;
    connection_info_.error_message.clear();
    connection_info_.firmware_version.clear();
    connection_info_.serial_number.clear();
    return Result<void>::Success();
}

Result<bool> MotionRuntimeFacade::IsConnected() const {
    if (!motion_adapter_) {
        return Result<bool>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Motion runtime未初始化", "MotionRuntimeFacade"));
    }
    return Result<bool>::Success(IsRuntimeConnected());
}

Result<void> MotionRuntimeFacade::Reset() {
    if (!motion_adapter_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Motion runtime未初始化", "MotionRuntimeFacade"));
    }
    return motion_adapter_->Reset();
}

Result<std::string> MotionRuntimeFacade::GetCardInfo() const {
    if (!motion_adapter_) {
        return Result<std::string>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Motion runtime未初始化", "MotionRuntimeFacade"));
    }
    return motion_adapter_->GetCardInfo();
}

Result<void> MotionRuntimeFacade::EnableAxis(LogicalAxisId axis) { return motion_adapter_->EnableAxis(axis); }
Result<void> MotionRuntimeFacade::DisableAxis(LogicalAxisId axis) { return motion_adapter_->DisableAxis(axis); }
Result<bool> MotionRuntimeFacade::IsAxisEnabled(LogicalAxisId axis) const { return motion_adapter_->IsAxisEnabled(axis); }
Result<void> MotionRuntimeFacade::ClearAxisStatus(LogicalAxisId axis) { return motion_adapter_->ClearAxisStatus(axis); }
Result<void> MotionRuntimeFacade::ClearPosition(LogicalAxisId axis) { return motion_adapter_->ClearPosition(axis); }
Result<void> MotionRuntimeFacade::SetAxisVelocity(LogicalAxisId axis, float32 velocity) {
    return motion_adapter_->SetAxisVelocity(axis, velocity);
}
Result<void> MotionRuntimeFacade::SetAxisAcceleration(LogicalAxisId axis, float32 acceleration) {
    return motion_adapter_->SetAxisAcceleration(axis, acceleration);
}
Result<void> MotionRuntimeFacade::SetSoftLimits(LogicalAxisId axis, float32 negative_limit, float32 positive_limit) {
    return motion_adapter_->SetSoftLimits(axis, negative_limit, positive_limit);
}
Result<void> MotionRuntimeFacade::ConfigureAxis(LogicalAxisId axis,
                                                const Domain::Motion::Ports::AxisConfiguration& config) {
    return motion_adapter_->ConfigureAxis(axis, config);
}
Result<void> MotionRuntimeFacade::SetHardLimits(LogicalAxisId axis,
                                                short positive_io_index,
                                                short negative_io_index,
                                                short card_index,
                                                short signal_type) {
    return motion_adapter_->SetHardLimits(axis, positive_io_index, negative_io_index, card_index, signal_type);
}
Result<void> MotionRuntimeFacade::EnableHardLimits(LogicalAxisId axis, bool enable, short limit_type) {
    return motion_adapter_->EnableHardLimits(axis, enable, limit_type);
}
Result<void> MotionRuntimeFacade::SetHardLimitPolarity(LogicalAxisId axis,
                                                       short positive_polarity,
                                                       short negative_polarity) {
    return motion_adapter_->SetHardLimitPolarity(axis, positive_polarity, negative_polarity);
}

Result<void> MotionRuntimeFacade::MoveToPosition(const Point2D& position, float32 velocity) {
    return motion_adapter_->MoveToPosition(position, velocity);
}
Result<void> MotionRuntimeFacade::MoveAxisToPosition(LogicalAxisId axis, float32 position, float32 velocity) {
    return motion_adapter_->MoveAxisToPosition(axis, position, velocity);
}
Result<void> MotionRuntimeFacade::RelativeMove(LogicalAxisId axis, float32 distance, float32 velocity) {
    return motion_adapter_->RelativeMove(axis, distance, velocity);
}
Result<void> MotionRuntimeFacade::SynchronizedMove(
    const std::vector<Domain::Motion::Ports::MotionCommand>& commands) {
    return motion_adapter_->SynchronizedMove(commands);
}
Result<void> MotionRuntimeFacade::StopAxis(LogicalAxisId axis, bool immediate) {
    return motion_adapter_->StopAxis(axis, immediate);
}
Result<void> MotionRuntimeFacade::StopAllAxes(bool immediate) { return motion_adapter_->StopAllAxes(immediate); }
Result<void> MotionRuntimeFacade::EmergencyStop() { return motion_adapter_->EmergencyStop(); }
Result<void> MotionRuntimeFacade::WaitForMotionComplete(LogicalAxisId axis, int32 timeout_ms) {
    return motion_adapter_->WaitForMotionComplete(axis, timeout_ms);
}

Result<Point2D> MotionRuntimeFacade::GetCurrentPosition() const { return motion_adapter_->GetCurrentPosition(); }
Result<float32> MotionRuntimeFacade::GetAxisPosition(LogicalAxisId axis) const {
    return motion_adapter_->GetAxisPosition(axis);
}
Result<float32> MotionRuntimeFacade::GetAxisVelocity(LogicalAxisId axis) const {
    return motion_adapter_->GetAxisVelocity(axis);
}
Result<Domain::Motion::Ports::MotionStatus> MotionRuntimeFacade::GetAxisStatus(LogicalAxisId axis) const {
    auto status_result = motion_adapter_->GetAxisStatus(axis);
    if (status_result.IsError()) {
        return status_result;
    }

    auto status = status_result.Value();
    SanitizeTransientCommunicationFault(status, ShouldSuppressTransientStatusCommunicationErrors());
    return Result<Domain::Motion::Ports::MotionStatus>::Success(status);
}
Result<bool> MotionRuntimeFacade::IsAxisMoving(LogicalAxisId axis) const { return motion_adapter_->IsAxisMoving(axis); }
Result<bool> MotionRuntimeFacade::IsAxisInPosition(LogicalAxisId axis) const {
    return motion_adapter_->IsAxisInPosition(axis);
}
Result<std::vector<Domain::Motion::Ports::MotionStatus>> MotionRuntimeFacade::GetAllAxesStatus() const {
    auto statuses_result = motion_adapter_->GetAllAxesStatus();
    if (statuses_result.IsError()) {
        return statuses_result;
    }

    auto statuses = statuses_result.Value();
    const bool suppress_fault = ShouldSuppressTransientStatusCommunicationErrors();
    for (auto& status : statuses) {
        SanitizeTransientCommunicationFault(status, suppress_fault);
    }
    return Result<std::vector<Domain::Motion::Ports::MotionStatus>>::Success(statuses);
}

Result<void> MotionRuntimeFacade::StartJog(LogicalAxisId axis, int16 direction, float32 velocity) {
    return motion_adapter_->StartJog(axis, direction, velocity);
}
Result<void> MotionRuntimeFacade::StartJogStep(LogicalAxisId axis, int16 direction, float32 distance, float32 velocity) {
    return motion_adapter_->StartJogStep(axis, direction, distance, velocity);
}
Result<void> MotionRuntimeFacade::StopJog(LogicalAxisId axis) { return motion_adapter_->StopJog(axis); }
Result<void> MotionRuntimeFacade::SetJogParameters(LogicalAxisId axis,
                                                   const Domain::Motion::Ports::JogParameters& params) {
    return motion_adapter_->SetJogParameters(axis, params);
}

Result<Domain::Motion::Ports::IOStatus> MotionRuntimeFacade::ReadDigitalInput(int16 channel) {
    return motion_adapter_->ReadDigitalInput(channel);
}
Result<Domain::Motion::Ports::IOStatus> MotionRuntimeFacade::ReadDigitalOutput(int16 channel) {
    return motion_adapter_->ReadDigitalOutput(channel);
}
Result<void> MotionRuntimeFacade::WriteDigitalOutput(int16 channel, bool value) {
    return motion_adapter_->WriteDigitalOutput(channel, value);
}
Result<bool> MotionRuntimeFacade::ReadLimitStatus(LogicalAxisId axis, bool positive) {
    return motion_adapter_->ReadLimitStatus(axis, positive);
}
Result<bool> MotionRuntimeFacade::ReadServoAlarm(LogicalAxisId axis) { return motion_adapter_->ReadServoAlarm(axis); }

Result<void> MotionRuntimeFacade::HomeAxis(LogicalAxisId axis) {
    if (!homing_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing runtime未初始化", "MotionRuntimeFacade"));
    }
    return homing_port_->HomeAxis(axis);
}

Result<void> MotionRuntimeFacade::StopHoming(LogicalAxisId axis) {
    if (!homing_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing runtime未初始化", "MotionRuntimeFacade"));
    }
    return homing_port_->StopHoming(axis);
}

Result<void> MotionRuntimeFacade::ResetHomingState(LogicalAxisId axis) {
    if (!homing_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing runtime未初始化", "MotionRuntimeFacade"));
    }
    return homing_port_->ResetHomingState(axis);
}

Result<HomingStatus> MotionRuntimeFacade::GetHomingStatus(LogicalAxisId axis) const {
    if (!homing_port_) {
        return Result<HomingStatus>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing runtime未初始化", "MotionRuntimeFacade"));
    }
    return homing_port_->GetHomingStatus(axis);
}

Result<bool> MotionRuntimeFacade::IsAxisHomed(LogicalAxisId axis) const {
    if (!homing_port_) {
        return Result<bool>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing runtime未初始化", "MotionRuntimeFacade"));
    }
    return homing_port_->IsAxisHomed(axis);
}

Result<bool> MotionRuntimeFacade::IsHomingInProgress(LogicalAxisId axis) const {
    if (!homing_port_) {
        return Result<bool>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing runtime未初始化", "MotionRuntimeFacade"));
    }
    return homing_port_->IsHomingInProgress(axis);
}

Result<void> MotionRuntimeFacade::WaitForHomingComplete(LogicalAxisId axis, int32 timeout_ms) {
    if (!homing_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing runtime未初始化", "MotionRuntimeFacade"));
    }
    return homing_port_->WaitForHomingComplete(axis, timeout_ms);
}

Result<void> MotionRuntimeFacade::ConnectRuntime(const HardwareConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (connection_state_ == HardwareConnectionState::Connecting) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "正在连接中，请勿重复操作", "MotionRuntimeFacade"));
    }
    if (connection_state_ == HardwareConnectionState::Connected) {
        return Result<void>::Success();
    }
    if (!config.IsValid()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "连接配置无效: " + config.GetValidationError(), "MotionRuntimeFacade"));
    }
    if (!motion_adapter_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Motion runtime未初始化", "MotionRuntimeFacade"));
    }

    connection_state_ = HardwareConnectionState::Connecting;
    last_error_.clear();

    auto connect_result = motion_adapter_->Connect(
        config.card_ip,
        config.local_ip,
        static_cast<int16>(config.card_port));
    if (connect_result.IsError()) {
        connection_state_ = HardwareConnectionState::Error;
        last_error_ = connect_result.GetError().GetMessage();
        return Result<void>::Failure(Error(ErrorCode::CONNECTION_FAILED, last_error_, "MotionRuntimeFacade"));
    }

    connection_state_ = HardwareConnectionState::Connected;
    last_connection_config_ = config;
    has_last_connection_config_ = true;
    last_connect_time_ = std::chrono::steady_clock::now();
    UpdateDeviceInfo();
    return Result<void>::Success();
}

HardwareConnectionInfo MotionRuntimeFacade::GetRuntimeConnectionInfo() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    if (connection_state_ == HardwareConnectionState::Connected) {
        std::lock_guard<std::mutex> hb_lock(heartbeat_mutex_);
        const bool is_degraded = heartbeat_status_.is_degraded;
        const bool in_grace_period = IsGracePeriodActive(last_connect_time_);
        if (!is_degraded && !in_grace_period && motion_adapter_) {
            auto result = motion_adapter_->IsConnected();
            if (result.IsError() || !result.Value()) {
                const_cast<MotionRuntimeFacade*>(this)->connection_state_ = HardwareConnectionState::Disconnected;
                const_cast<MotionRuntimeFacade*>(this)->last_error_ =
                    result.IsError() ? result.GetError().GetMessage() : "Hardware not responding";
            }
        }
    }

    auto info = connection_info_;
    info.state = connection_state_;
    info.error_message = last_error_;
    return info;
}

bool MotionRuntimeFacade::ShouldSuppressTransientStatusCommunicationErrors() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    if (connection_state_ != HardwareConnectionState::Connected) {
        return false;
    }

    if (IsGracePeriodActive(last_connect_time_)) {
        return true;
    }

    std::lock_guard<std::mutex> hb_lock(heartbeat_mutex_);
    return heartbeat_status_.is_degraded;
}

bool MotionRuntimeFacade::IsRuntimeConnected() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    if (connection_state_ != HardwareConnectionState::Connected) {
        return false;
    }

    if (IsGracePeriodActive(last_connect_time_)) {
        return true;
    }

    {
        std::lock_guard<std::mutex> hb_lock(heartbeat_mutex_);
        if (heartbeat_status_.is_degraded) {
            return true;
        }
    }

    if (motion_adapter_) {
        auto result = motion_adapter_->IsConnected();
        if (result.IsError() || !result.Value()) {
            const_cast<MotionRuntimeFacade*>(this)->connection_state_ = HardwareConnectionState::Disconnected;
            const_cast<MotionRuntimeFacade*>(this)->last_error_ =
                result.IsError() ? result.GetError().GetMessage() : "Hardware not responding";
            return false;
        }
    }
    return true;
}

Result<void> MotionRuntimeFacade::ReconnectRuntime() {
    if (!has_last_connection_config_) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "没有可用的历史连接配置", "MotionRuntimeFacade"));
    }
    auto disconnect_result = Disconnect();
    if (disconnect_result.IsError()) {
        return disconnect_result;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return ConnectRuntime(last_connection_config_);
}

void MotionRuntimeFacade::SetRuntimeConnectionStateCallback(
    std::function<void(const HardwareConnectionInfo&)> callback) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    state_change_callback_ = std::move(callback);
}

Result<void> MotionRuntimeFacade::StartRuntimeStatusMonitoring(Shared::Types::uint32 interval_ms) {
    if (monitoring_active_) {
        return Result<void>::Success();
    }

    should_monitor_ = true;
    monitoring_active_ = true;
    monitoring_thread_ = std::thread([this, interval_ms]() { MonitoringLoop(interval_ms); });
    return Result<void>::Success();
}

void MotionRuntimeFacade::StopRuntimeStatusMonitoring() {
    should_monitor_ = false;
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    monitoring_active_ = false;
}

std::string MotionRuntimeFacade::GetRuntimeLastError() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    return last_error_;
}

void MotionRuntimeFacade::ClearRuntimeError() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    if (connection_state_ == HardwareConnectionState::Error) {
        connection_state_ = HardwareConnectionState::Disconnected;
    }
    last_error_.clear();
    connection_info_.error_message.clear();
}

Result<void> MotionRuntimeFacade::StartRuntimeHeartbeat(const HeartbeatConfig& config) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    if (!config.IsValid()) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "心跳配置无效", "MotionRuntimeFacade"));
    }

    if (heartbeat_active_) {
        StopRuntimeHeartbeat();
    }

    heartbeat_config_ = config;
    heartbeat_status_ = HeartbeatStatus{};
    heartbeat_status_.is_active = true;

    should_heartbeat_ = true;
    heartbeat_active_ = true;
    try {
        heartbeat_thread_ = std::thread([this]() { HeartbeatLoop(); });
    } catch (const std::exception& e) {
        heartbeat_active_ = false;
        should_heartbeat_ = false;
        return Result<void>::Failure(
            Error(ErrorCode::THREAD_START_FAILED, std::string("启动心跳线程失败: ") + e.what(), "MotionRuntimeFacade"));
    }
    return Result<void>::Success();
}

void MotionRuntimeFacade::StopRuntimeHeartbeat() {
    should_heartbeat_ = false;
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }

    std::lock_guard<std::mutex> lock(heartbeat_mutex_);
    heartbeat_active_ = false;
    heartbeat_status_.is_active = false;
}

HeartbeatStatus MotionRuntimeFacade::GetRuntimeHeartbeatStatus() const {
    std::lock_guard<std::mutex> lock(heartbeat_mutex_);
    auto status = heartbeat_status_;
    status.interval_ms = heartbeat_config_.interval_ms;
    status.failure_threshold = heartbeat_config_.failure_threshold;
    return status;
}

Result<bool> MotionRuntimeFacade::PingRuntime() const {
    return ExecuteHeartbeat();
}

void MotionRuntimeFacade::MonitoringLoop(Shared::Types::uint32 interval_ms) {
    HardwareConnectionState last_state = HardwareConnectionState::Disconnected;
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        last_state = connection_state_;
    }

    while (should_monitor_) {
        HardwareConnectionState current_state = last_state;
        std::string current_error;
        std::function<void(const HardwareConnectionInfo&)> callback;
        HardwareConnectionInfo info;

        {
            std::lock_guard<std::mutex> lock(connection_mutex_);
            current_state = connection_state_;
            current_error = last_error_;
        }

        bool skip_check = false;
        {
            std::lock_guard<std::mutex> hb_lock(heartbeat_mutex_);
            skip_check = heartbeat_status_.is_degraded;
        }
        skip_check = skip_check || IsGracePeriodActive(last_connect_time_);

        if (current_state == HardwareConnectionState::Connected && !skip_check && motion_adapter_) {
            auto connected_result = motion_adapter_->IsConnected();
            if (connected_result.IsError() || !connected_result.Value()) {
                std::lock_guard<std::mutex> lock(connection_mutex_);
                connection_state_ = HardwareConnectionState::Disconnected;
                last_error_ =
                    connected_result.IsError() ? connected_result.GetError().GetMessage() : "连接意外断开";
                current_state = connection_state_;
                current_error = last_error_;
            }
        }

        if (last_state != current_state) {
            {
                std::lock_guard<std::mutex> lock(connection_mutex_);
                connection_info_.state = current_state;
                connection_info_.error_message = current_error;
                info = connection_info_;
                callback = state_change_callback_;
            }
            if (callback) {
                callback(info);
            }
        }

        last_state = current_state;
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}

void MotionRuntimeFacade::HeartbeatLoop() {
    SILIGEN_LOG_INFO("Motion runtime heartbeat thread started");

    while (should_heartbeat_) {
        auto result = ExecuteHeartbeat();

        bool notify_degraded = false;
        HardwareConnectionInfo callback_info;
        std::function<void(const HardwareConnectionInfo&)> callback;

        {
            std::lock_guard<std::mutex> lock(heartbeat_mutex_);
            heartbeat_status_.total_sent++;

            if (result.IsSuccess() && result.Value()) {
                heartbeat_status_.total_success++;
                heartbeat_status_.consecutive_failures = 0;
                heartbeat_status_.last_error.clear();
                if (heartbeat_status_.is_degraded) {
                    heartbeat_status_.is_degraded = false;
                    heartbeat_status_.degraded_reason.clear();
                    SILIGEN_LOG_INFO("Motion runtime heartbeat recovered");
                }
            } else {
                heartbeat_status_.total_failure++;
                heartbeat_status_.consecutive_failures++;
                heartbeat_status_.last_error =
                    result.IsError() ? result.GetError().GetMessage() : "硬件无响应";
            }

            if (!heartbeat_status_.is_degraded &&
                heartbeat_status_.consecutive_failures >= heartbeat_config_.failure_threshold) {
                heartbeat_status_.is_degraded = true;
                heartbeat_status_.degraded_reason = "MC_GetSts unavailable (error -999)";
                notify_degraded = true;
            }
        }

        if (notify_degraded) {
            std::lock_guard<std::mutex> lock(connection_mutex_);
            connection_info_.state = connection_state_;
            connection_info_.error_message = "降级模式: 心跳不可用，连接保持活跃";
            callback_info = connection_info_;
            callback = state_change_callback_;
        }
        if (notify_degraded && callback) {
            callback(callback_info);
        }

        if (notify_degraded) {
            SILIGEN_LOG_WARNING("Motion runtime entering degraded mode: heartbeat unavailable");
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            continue;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeat_config_.interval_ms));
    }

    SILIGEN_LOG_INFO("Motion runtime heartbeat thread stopped");
}

Result<bool> MotionRuntimeFacade::ExecuteHeartbeat() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    if (connection_state_ != HardwareConnectionState::Connected) {
        return Result<bool>::Success(false);
    }
    if (!motion_adapter_) {
        return Result<bool>::Failure(
            Error(ErrorCode::ADAPTER_NOT_INITIALIZED, "Motion adapter未初始化", "MotionRuntimeFacade"));
    }
    return motion_adapter_->IsConnected();
}

void MotionRuntimeFacade::UpdateDeviceInfo() {
    connection_info_.device_type = "MultiCard Motion Runtime";
    auto card_info_result = GetCardInfo();
    if (card_info_result.IsSuccess()) {
        connection_info_.firmware_version = card_info_result.Value();
        connection_info_.serial_number = "ManagedByMotionRuntime";
    } else {
        connection_info_.firmware_version = "Unknown";
        connection_info_.serial_number = "Unknown";
    }
}

}  // namespace Siligen::Infrastructure::Adapters::Motion
