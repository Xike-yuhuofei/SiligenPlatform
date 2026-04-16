#pragma once

#include "siligen/device/contracts/ports/device_ports.h"

#include <memory>
#include <mutex>
#include <string>

namespace Siligen::Infrastructure::Adapters::Motion {

class MotionRuntimeFacade;

class MotionRuntimeDevicePortAdapter final : public Siligen::Device::Contracts::Ports::MotionDevicePort {
   public:
    MotionRuntimeDevicePortAdapter(
        std::shared_ptr<MotionRuntimeFacade> runtime,
        Siligen::Device::Contracts::Capabilities::DeviceCapabilities capabilities);

    Siligen::SharedKernel::VoidResult Connect(
        const Siligen::Device::Contracts::Commands::DeviceConnection& connection) override;
    Siligen::SharedKernel::VoidResult Disconnect() override;
    Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DeviceSession> ReadSession() const override;
    Siligen::SharedKernel::VoidResult Execute(
        const Siligen::Device::Contracts::Commands::MotionCommand& command) override;
    Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MotionState> ReadState() const override;
    Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DeviceCapabilities> DescribeCapabilities()
        const override;
    Siligen::SharedKernel::Result<std::vector<Siligen::Device::Contracts::Faults::DeviceFault>> ReadFaults()
        const override;

   private:
    std::shared_ptr<MotionRuntimeFacade> runtime_;
    Siligen::Device::Contracts::Capabilities::DeviceCapabilities capabilities_;
    mutable std::mutex mutex_;
    std::string last_correlation_id_;
};

}  // namespace Siligen::Infrastructure::Adapters::Motion
