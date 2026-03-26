#pragma once

#include "siligen/device/contracts/ports/device_ports.h"

#include <mutex>

namespace Siligen::Device::Adapters::Fake {

class FakeDispenserDevice final : public Siligen::Device::Contracts::Ports::DispenserDevicePort {
   public:
    FakeDispenserDevice();

    Siligen::SharedKernel::VoidResult Execute(
        const Siligen::Device::Contracts::Commands::DispenserCommand& command) override;
    Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DispenserState> ReadState() const override;
    Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DispenserCapability> DescribeCapability()
        const override;

   private:
    mutable std::mutex mutex_;
    Siligen::Device::Contracts::State::DispenserState state_;
    Siligen::Device::Contracts::Capabilities::DispenserCapability capability_;
};

}  // namespace Siligen::Device::Adapters::Fake
