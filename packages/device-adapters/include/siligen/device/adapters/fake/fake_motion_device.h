#pragma once

#include "siligen/device/contracts/ports/device_ports.h"

#include <mutex>
#include <unordered_map>
#include <vector>

namespace Siligen::Device::Adapters::Fake {

class FakeMotionDevice final : public Siligen::Device::Contracts::Ports::MotionDevicePort,
                               public Siligen::Device::Contracts::Ports::DigitalIoPort,
                               public Siligen::Device::Contracts::Ports::MachineHealthPort {
   public:
    FakeMotionDevice();

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
    Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MachineHealthSnapshot> ReadHealth()
        const override;

    Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState> ReadInput(
        Siligen::SharedKernel::int32 channel) const override;
    Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState> ReadOutput(
        Siligen::SharedKernel::int32 channel) const override;
    Siligen::SharedKernel::VoidResult WriteOutput(Siligen::SharedKernel::int32 channel, bool value) override;

   private:
    void EnsureDefaultAxes();
    Siligen::Device::Contracts::State::AxisState& FindAxis(Siligen::SharedKernel::LogicalAxisId axis);

    mutable std::mutex mutex_;
    Siligen::Device::Contracts::Capabilities::DeviceCapabilities capabilities_;
    Siligen::Device::Contracts::State::DeviceSession session_;
    Siligen::Device::Contracts::State::MotionState state_;
    std::vector<Siligen::Device::Contracts::Faults::DeviceFault> active_faults_;
    std::unordered_map<Siligen::SharedKernel::int32, bool> inputs_;
    std::unordered_map<Siligen::SharedKernel::int32, bool> outputs_;
};

}  // namespace Siligen::Device::Adapters::Fake
