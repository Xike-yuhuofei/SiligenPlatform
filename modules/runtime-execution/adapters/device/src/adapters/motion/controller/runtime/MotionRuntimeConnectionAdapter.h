#pragma once

#include "MotionRuntimeFacade.h"
#include "runtime_execution/contracts/system/IHardwareConnectionPort.h"
#include "siligen/device/contracts/ports/device_ports.h"

#include <memory>

namespace Siligen::Infrastructure::Adapters::Motion {

/**
 * @brief 连接契约兼容适配器
 *
 * 稳定公开面实现 DeviceConnectionPort，旧 IHardwareConnectionPort 仅作兼容桥。
 */
class MotionRuntimeConnectionAdapter final : public Siligen::Device::Contracts::Ports::DeviceConnectionPort,
                                             public Domain::Machine::Ports::IHardwareConnectionPort {
   public:
    explicit MotionRuntimeConnectionAdapter(std::shared_ptr<MotionRuntimeFacade> runtime);
    ~MotionRuntimeConnectionAdapter() override = default;

    Shared::Types::Result<void> Connect(
        const Siligen::Device::Contracts::Commands::DeviceConnection& connection) override;
    Shared::Types::Result<Siligen::Device::Contracts::State::DeviceConnectionSnapshot> ReadConnection()
        const override;
    void SetConnectionStateCallback(
        std::function<void(const Siligen::Device::Contracts::State::DeviceConnectionSnapshot&)> callback) override;
    Shared::Types::Result<void> StartHeartbeat(
        const Siligen::Device::Contracts::State::HeartbeatSnapshot& config) override;
    Siligen::Device::Contracts::State::HeartbeatSnapshot ReadHeartbeat() const override;

    Shared::Types::Result<void> Connect(const Domain::Machine::Ports::HardwareConnectionConfig& config) override;
    Shared::Types::Result<void> Disconnect() override;
    Domain::Machine::Ports::HardwareConnectionInfo GetConnectionInfo() const override;
    bool IsConnected() const override;
    Shared::Types::Result<void> Reconnect() override;
    void SetConnectionStateCallback(
        std::function<void(const Domain::Machine::Ports::HardwareConnectionInfo&)> callback) override;
    Shared::Types::Result<void> StartStatusMonitoring(Shared::Types::uint32 interval_ms = 1000) override;
    void StopStatusMonitoring() override;
    std::string GetLastError() const override;
    void ClearError() override;
    Shared::Types::Result<void> StartHeartbeat(const Domain::Machine::Ports::HeartbeatConfig& config) override;
    void StopHeartbeat() override;
    Domain::Machine::Ports::HeartbeatStatus GetHeartbeatStatus() const override;
    Shared::Types::Result<bool> Ping() const override;

   private:
    std::shared_ptr<MotionRuntimeFacade> runtime_;
};

}  // namespace Siligen::Infrastructure::Adapters::Motion

