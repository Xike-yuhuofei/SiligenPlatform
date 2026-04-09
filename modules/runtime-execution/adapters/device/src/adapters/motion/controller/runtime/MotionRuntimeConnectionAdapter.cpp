#define MODULE_NAME "MotionRuntimeConnectionAdapter"

#include "MotionRuntimeConnectionAdapter.h"

namespace {

using DeviceConnection = Siligen::Device::Contracts::Commands::DeviceConnection;
using DeviceConnectionSnapshot = Siligen::Device::Contracts::State::DeviceConnectionSnapshot;
using HeartbeatSnapshot = Siligen::Device::Contracts::State::HeartbeatSnapshot;

}  // namespace

namespace Siligen::Infrastructure::Adapters::Motion {

MotionRuntimeConnectionAdapter::MotionRuntimeConnectionAdapter(std::shared_ptr<MotionRuntimeFacade> runtime)
    : runtime_(std::move(runtime)) {}

Shared::Types::Result<void> MotionRuntimeConnectionAdapter::Connect(const DeviceConnection& connection) {
    return runtime_->ConnectDevice(connection);
}

Shared::Types::Result<DeviceConnectionSnapshot> MotionRuntimeConnectionAdapter::ReadConnection() const {
    return runtime_->ReadConnection();
}

Shared::Types::Result<void> MotionRuntimeConnectionAdapter::Disconnect() {
    return runtime_->Disconnect();
}

bool MotionRuntimeConnectionAdapter::IsConnected() const {
    return runtime_->IsDeviceConnected();
}

Shared::Types::Result<void> MotionRuntimeConnectionAdapter::Reconnect() {
    return runtime_->ReconnectDevice();
}

void MotionRuntimeConnectionAdapter::SetConnectionStateCallback(
    std::function<void(const DeviceConnectionSnapshot&)> callback) {
    runtime_->SetConnectionStateCallback(std::move(callback));
}

Shared::Types::Result<void> MotionRuntimeConnectionAdapter::StartStatusMonitoring(Shared::Types::uint32 interval_ms) {
    return runtime_->StartStatusMonitoring(interval_ms);
}

void MotionRuntimeConnectionAdapter::StopStatusMonitoring() {
    runtime_->StopStatusMonitoring();
}

std::string MotionRuntimeConnectionAdapter::GetLastError() const {
    return runtime_->GetLastError();
}

void MotionRuntimeConnectionAdapter::ClearError() {
    runtime_->ClearError();
}

Shared::Types::Result<void> MotionRuntimeConnectionAdapter::StartHeartbeat(
    const HeartbeatSnapshot& config) {
    return runtime_->StartHeartbeat(config);
}

void MotionRuntimeConnectionAdapter::StopHeartbeat() {
    runtime_->StopHeartbeat();
}

HeartbeatSnapshot MotionRuntimeConnectionAdapter::ReadHeartbeat() const {
    return runtime_->ReadHeartbeat();
}

Shared::Types::Result<bool> MotionRuntimeConnectionAdapter::Ping() const {
    return runtime_->Ping();
}

}  // namespace Siligen::Infrastructure::Adapters::Motion
