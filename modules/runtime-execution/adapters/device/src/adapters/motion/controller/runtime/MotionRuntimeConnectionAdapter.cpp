#define MODULE_NAME "MotionRuntimeConnectionAdapter"

#include "MotionRuntimeConnectionAdapter.h"

namespace {

using DeviceConnection = Siligen::Device::Contracts::Commands::DeviceConnection;
using DeviceConnectionSnapshot = Siligen::Device::Contracts::State::DeviceConnectionSnapshot;
using DeviceConnectionState = Siligen::Device::Contracts::State::DeviceConnectionState;
using HeartbeatSnapshot = Siligen::Device::Contracts::State::HeartbeatSnapshot;
using HardwareConnectionConfig = Siligen::Domain::Machine::Ports::HardwareConnectionConfig;
using HardwareConnectionInfo = Siligen::Domain::Machine::Ports::HardwareConnectionInfo;
using HardwareConnectionState = Siligen::Domain::Machine::Ports::HardwareConnectionState;
using HeartbeatConfig = Siligen::Domain::Machine::Ports::HeartbeatConfig;
using HeartbeatStatus = Siligen::Domain::Machine::Ports::HeartbeatStatus;

HardwareConnectionConfig ToLegacyConnectionConfig(const DeviceConnection& connection) {
    HardwareConnectionConfig config;
    config.local_ip = connection.local_ip;
    config.local_port = connection.local_port;
    config.card_ip = connection.card_ip;
    config.card_port = connection.card_port;
    config.timeout_ms = connection.timeout_ms;
    return config;
}

DeviceConnectionSnapshot ToDeviceConnectionSnapshot(const HardwareConnectionInfo& info) {
    DeviceConnectionSnapshot snapshot;
    switch (info.state) {
        case HardwareConnectionState::Connected:
            snapshot.state = DeviceConnectionState::Connected;
            break;
        case HardwareConnectionState::Connecting:
            snapshot.state = DeviceConnectionState::Connecting;
            break;
        case HardwareConnectionState::Error:
            snapshot.state = DeviceConnectionState::Error;
            break;
        case HardwareConnectionState::Disconnected:
        default:
            snapshot.state = DeviceConnectionState::Disconnected;
            break;
    }
    snapshot.device_type = info.device_type;
    snapshot.firmware_version = info.firmware_version;
    snapshot.serial_number = info.serial_number;
    snapshot.error_message = info.error_message;
    return snapshot;
}

HeartbeatConfig ToLegacyHeartbeatConfig(const HeartbeatSnapshot& snapshot) {
    HeartbeatConfig config;
    config.enabled = snapshot.enabled;
    config.interval_ms = snapshot.interval_ms;
    config.timeout_ms = snapshot.timeout_ms;
    config.failure_threshold = snapshot.failure_threshold;
    return config;
}

HeartbeatSnapshot ToHeartbeatSnapshot(const HeartbeatStatus& status) {
    HeartbeatSnapshot snapshot;
    snapshot.enabled = true;
    snapshot.interval_ms = status.interval_ms;
    snapshot.failure_threshold = status.failure_threshold;
    snapshot.is_active = status.is_active;
    snapshot.total_sent = status.total_sent;
    snapshot.total_success = status.total_success;
    snapshot.total_failure = status.total_failure;
    snapshot.consecutive_failures = status.consecutive_failures;
    snapshot.last_error = status.last_error;
    snapshot.is_degraded = status.is_degraded;
    snapshot.degraded_reason = status.degraded_reason;
    return snapshot;
}

}  // namespace

namespace Siligen::Infrastructure::Adapters::Motion {

MotionRuntimeConnectionAdapter::MotionRuntimeConnectionAdapter(std::shared_ptr<MotionRuntimeFacade> runtime)
    : runtime_(std::move(runtime)) {}

Shared::Types::Result<void> MotionRuntimeConnectionAdapter::Connect(const DeviceConnection& connection) {
    return Connect(ToLegacyConnectionConfig(connection));
}

Shared::Types::Result<DeviceConnectionSnapshot> MotionRuntimeConnectionAdapter::ReadConnection() const {
    return Shared::Types::Result<DeviceConnectionSnapshot>::Success(ToDeviceConnectionSnapshot(GetConnectionInfo()));
}

Shared::Types::Result<void> MotionRuntimeConnectionAdapter::Connect(
    const Domain::Machine::Ports::HardwareConnectionConfig& config) {
    return runtime_->ConnectRuntime(config);
}

Shared::Types::Result<void> MotionRuntimeConnectionAdapter::Disconnect() {
    return runtime_->Disconnect();
}

Domain::Machine::Ports::HardwareConnectionInfo MotionRuntimeConnectionAdapter::GetConnectionInfo() const {
    return runtime_->GetRuntimeConnectionInfo();
}

bool MotionRuntimeConnectionAdapter::IsConnected() const {
    return runtime_->IsRuntimeConnected();
}

Shared::Types::Result<void> MotionRuntimeConnectionAdapter::Reconnect() {
    return runtime_->ReconnectRuntime();
}

void MotionRuntimeConnectionAdapter::SetConnectionStateCallback(
    std::function<void(const Domain::Machine::Ports::HardwareConnectionInfo&)> callback) {
    runtime_->SetRuntimeConnectionStateCallback(std::move(callback));
}

void MotionRuntimeConnectionAdapter::SetConnectionStateCallback(
    std::function<void(const DeviceConnectionSnapshot&)> callback) {
    runtime_->SetRuntimeConnectionStateCallback(
        [callback = std::move(callback)](const HardwareConnectionInfo& info) {
            if (callback) {
                callback(ToDeviceConnectionSnapshot(info));
            }
        });
}

Shared::Types::Result<void> MotionRuntimeConnectionAdapter::StartStatusMonitoring(Shared::Types::uint32 interval_ms) {
    return runtime_->StartRuntimeStatusMonitoring(interval_ms);
}

void MotionRuntimeConnectionAdapter::StopStatusMonitoring() {
    runtime_->StopRuntimeStatusMonitoring();
}

std::string MotionRuntimeConnectionAdapter::GetLastError() const {
    return runtime_->GetRuntimeLastError();
}

void MotionRuntimeConnectionAdapter::ClearError() {
    runtime_->ClearRuntimeError();
}

Shared::Types::Result<void> MotionRuntimeConnectionAdapter::StartHeartbeat(
    const HeartbeatSnapshot& config) {
    return StartHeartbeat(ToLegacyHeartbeatConfig(config));
}

Shared::Types::Result<void> MotionRuntimeConnectionAdapter::StartHeartbeat(
    const Domain::Machine::Ports::HeartbeatConfig& config) {
    return runtime_->StartRuntimeHeartbeat(config);
}

void MotionRuntimeConnectionAdapter::StopHeartbeat() {
    runtime_->StopRuntimeHeartbeat();
}

Domain::Machine::Ports::HeartbeatStatus MotionRuntimeConnectionAdapter::GetHeartbeatStatus() const {
    return runtime_->GetRuntimeHeartbeatStatus();
}

HeartbeatSnapshot MotionRuntimeConnectionAdapter::ReadHeartbeat() const {
    return ToHeartbeatSnapshot(GetHeartbeatStatus());
}

Shared::Types::Result<bool> MotionRuntimeConnectionAdapter::Ping() const {
    return runtime_->PingRuntime();
}

}  // namespace Siligen::Infrastructure::Adapters::Motion
