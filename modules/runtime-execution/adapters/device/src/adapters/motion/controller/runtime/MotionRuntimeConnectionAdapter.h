#pragma once

#include "domain/machine/ports/IHardwareConnectionPort.h"
#include "MotionRuntimeFacade.h"

#include <memory>

namespace Siligen::Infrastructure::Adapters::Motion {

/**
 * @brief IHardwareConnectionPort 兼容适配器
 *
 * 保留旧连接端口接口，但所有运行时逻辑都委托给统一 motion runtime。
 */
class MotionRuntimeConnectionAdapter final : public Domain::Machine::Ports::IHardwareConnectionPort {
   public:
    explicit MotionRuntimeConnectionAdapter(std::shared_ptr<MotionRuntimeFacade> runtime);
    ~MotionRuntimeConnectionAdapter() override = default;

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

