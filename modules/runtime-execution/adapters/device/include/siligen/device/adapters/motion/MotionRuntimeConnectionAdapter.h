#pragma once

#include "MotionRuntimeFacade.h"
#include "siligen/device/contracts/ports/device_ports.h"

#include <memory>

namespace Siligen::Infrastructure::Adapters::Motion {

/**
 * @brief Motion runtime 的稳定 device connection 适配器
 */
class MotionRuntimeConnectionAdapter final : public Siligen::Device::Contracts::Ports::DeviceConnectionPort {
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
    Shared::Types::Result<void> Disconnect() override;
    bool IsConnected() const override;
    Shared::Types::Result<void> Reconnect() override;
    Shared::Types::Result<void> StartStatusMonitoring(Shared::Types::uint32 interval_ms = 1000) override;
    void StopStatusMonitoring() override;
    std::string GetLastError() const override;
    void ClearError() override;
    void StopHeartbeat() override;
    Shared::Types::Result<bool> Ping() const override;

   private:
    std::shared_ptr<MotionRuntimeFacade> runtime_;
};

}  // namespace Siligen::Infrastructure::Adapters::Motion
