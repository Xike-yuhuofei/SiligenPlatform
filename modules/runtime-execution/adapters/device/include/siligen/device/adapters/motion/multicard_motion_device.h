#pragma once

#include "siligen/device/adapters/drivers/multicard/IMultiCardWrapper.h"
#include "siligen/device/contracts/ports/device_ports.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Siligen::Device::Adapters::Motion {

class MultiCardMotionDevice final : public Siligen::Device::Contracts::Ports::MotionDevicePort,
                                    public Siligen::Device::Contracts::Ports::MachineHealthPort {
   public:
    explicit MultiCardMotionDevice(
        std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> driver,
        Siligen::Device::Contracts::Capabilities::DeviceCapabilities capabilities = {});

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

   private:
    Siligen::SharedKernel::VoidResult ExecuteMove(const Siligen::Device::Contracts::Commands::MotionCommand& command);
    Siligen::SharedKernel::VoidResult ExecuteHome(const Siligen::Device::Contracts::Commands::MotionCommand& command);
    Siligen::SharedKernel::VoidResult ExecuteJog(const Siligen::Device::Contracts::Commands::MotionCommand& command);
    Siligen::SharedKernel::VoidResult Stop(bool immediate);
    void ReplaceActiveFault(int error_code, const std::string& operation);
    void ClearActiveFaults();

    std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> driver_;
    Siligen::Device::Contracts::Capabilities::DeviceCapabilities capabilities_;
    mutable std::mutex mutex_;
    Siligen::Device::Contracts::State::DeviceSession session_;
    Siligen::Device::Contracts::State::MotionState state_;
    std::vector<Siligen::Device::Contracts::Faults::DeviceFault> active_faults_;
};

}  // namespace Siligen::Device::Adapters::Motion
