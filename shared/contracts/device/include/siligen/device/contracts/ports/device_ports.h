#pragma once

#include "siligen/device/contracts/capabilities/device_capabilities.h"
#include "siligen/device/contracts/commands/device_commands.h"
#include "siligen/device/contracts/events/device_events.h"
#include "siligen/device/contracts/faults/device_faults.h"
#include "siligen/device/contracts/state/device_state.h"
#include "siligen/shared/result_types.h"
#include "shared/types/Result.h"

#include <functional>
#include <string>
#include <vector>

namespace Siligen::Device::Contracts::Ports {

class DeviceConnectionPort {
   public:
    virtual ~DeviceConnectionPort() = default;

    virtual Siligen::Shared::Types::Result<void> Connect(
        const Siligen::Device::Contracts::Commands::DeviceConnection& connection) = 0;
    virtual Siligen::Shared::Types::Result<void> Disconnect() = 0;
    virtual Siligen::Shared::Types::Result<Siligen::Device::Contracts::State::DeviceConnectionSnapshot>
    ReadConnection() const = 0;
    virtual bool IsConnected() const = 0;
    virtual Siligen::Shared::Types::Result<void> Reconnect() = 0;
    virtual void SetConnectionStateCallback(
        std::function<void(const Siligen::Device::Contracts::State::DeviceConnectionSnapshot&)> callback) = 0;
    virtual Siligen::Shared::Types::Result<void> StartStatusMonitoring(std::uint32_t interval_ms = 1000) = 0;
    virtual void StopStatusMonitoring() = 0;
    virtual std::string GetLastError() const = 0;
    virtual void ClearError() = 0;
    virtual Siligen::Shared::Types::Result<void> StartHeartbeat(
        const Siligen::Device::Contracts::State::HeartbeatSnapshot& config) = 0;
    virtual void StopHeartbeat() = 0;
    virtual Siligen::Device::Contracts::State::HeartbeatSnapshot ReadHeartbeat() const = 0;
    virtual Siligen::Shared::Types::Result<bool> Ping() const = 0;
};

class MotionDevicePort {
   public:
    virtual ~MotionDevicePort() = default;

    virtual Siligen::SharedKernel::VoidResult Connect(
        const Siligen::Device::Contracts::Commands::DeviceConnection& connection) = 0;
    virtual Siligen::SharedKernel::VoidResult Disconnect() = 0;
    virtual Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DeviceSession> ReadSession() const = 0;
    virtual Siligen::SharedKernel::VoidResult Execute(
        const Siligen::Device::Contracts::Commands::MotionCommand& command) = 0;
    virtual Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MotionState> ReadState() const = 0;
    virtual Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DeviceCapabilities>
    DescribeCapabilities() const = 0;
    virtual Siligen::SharedKernel::Result<std::vector<Siligen::Device::Contracts::Faults::DeviceFault>>
    ReadFaults() const = 0;
};

class DigitalIoPort {
   public:
    virtual ~DigitalIoPort() = default;

    virtual Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState> ReadInput(
        Siligen::SharedKernel::int32 channel) const = 0;
    virtual Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState> ReadOutput(
        Siligen::SharedKernel::int32 channel) const = 0;
    virtual Siligen::SharedKernel::VoidResult WriteOutput(Siligen::SharedKernel::int32 channel, bool value) = 0;
};

class DispenserDevicePort {
   public:
    virtual ~DispenserDevicePort() = default;

    virtual Siligen::SharedKernel::VoidResult Execute(
        const Siligen::Device::Contracts::Commands::DispenserCommand& command) = 0;
    virtual Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DispenserState> ReadState() const = 0;
    virtual Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DispenserCapability>
    DescribeCapability() const = 0;
};

class MachineHealthPort {
   public:
    virtual ~MachineHealthPort() = default;

    virtual Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MachineHealthSnapshot> ReadHealth()
        const = 0;
};

class DeviceEventSink {
   public:
    virtual ~DeviceEventSink() = default;

    virtual Siligen::SharedKernel::VoidResult Publish(
        const Siligen::Device::Contracts::Events::DeviceEvent& event) = 0;
};

}  // namespace Siligen::Device::Contracts::Ports
