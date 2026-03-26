#define MODULE_NAME "MotionRuntimeConnectionAdapter"

#include "MotionRuntimeConnectionAdapter.h"

namespace Siligen::Infrastructure::Adapters::Motion {

MotionRuntimeConnectionAdapter::MotionRuntimeConnectionAdapter(std::shared_ptr<MotionRuntimeFacade> runtime)
    : runtime_(std::move(runtime)) {}

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
    const Domain::Machine::Ports::HeartbeatConfig& config) {
    return runtime_->StartRuntimeHeartbeat(config);
}

void MotionRuntimeConnectionAdapter::StopHeartbeat() {
    runtime_->StopRuntimeHeartbeat();
}

Domain::Machine::Ports::HeartbeatStatus MotionRuntimeConnectionAdapter::GetHeartbeatStatus() const {
    return runtime_->GetRuntimeHeartbeatStatus();
}

Shared::Types::Result<bool> MotionRuntimeConnectionAdapter::Ping() const {
    return runtime_->PingRuntime();
}

}  // namespace Siligen::Infrastructure::Adapters::Motion
