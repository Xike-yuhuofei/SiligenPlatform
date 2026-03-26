#include "siligen/device/adapters/fake/fake_motion_device.h"

namespace Siligen::Device::Adapters::Fake {

FakeMotionDevice::FakeMotionDevice() {
    capabilities_.backend_name = "fake-device";
    capabilities_.mock_backend = true;
    capabilities_.axes = {
        {Siligen::SharedKernel::LogicalAxisId::X, true, true, true, true},
        {Siligen::SharedKernel::LogicalAxisId::Y, true, true, true, true},
    };
    capabilities_.io.input_channels = 16;
    capabilities_.io.output_channels = 16;
    capabilities_.dispenser.supports_prime = true;
    capabilities_.dispenser.supports_pause = true;
    capabilities_.dispenser.supports_resume = true;

    session_.backend_name = capabilities_.backend_name;
    session_.mock_backend = true;
    EnsureDefaultAxes();
}

Siligen::SharedKernel::VoidResult FakeMotionDevice::Connect(
    const Siligen::Device::Contracts::Commands::DeviceConnection& connection) {
    std::lock_guard<std::mutex> lock(mutex_);
    session_.session_id = connection.backend_name.empty() ? "fake-device#1" : connection.backend_name + "#1";
    session_.state = Siligen::Device::Contracts::State::DeviceSessionState::kReady;
    state_.connected = true;
    state_.lifecycle = Siligen::Device::Contracts::State::MotionLifecycleState::kIdle;
    state_.estop_active = false;
    return Siligen::SharedKernel::VoidResult::Success();
}

Siligen::SharedKernel::VoidResult FakeMotionDevice::Disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    session_.state = Siligen::Device::Contracts::State::DeviceSessionState::kDisconnected;
    state_.connected = false;
    state_.lifecycle = Siligen::Device::Contracts::State::MotionLifecycleState::kStopped;
    return Siligen::SharedKernel::VoidResult::Success();
}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DeviceSession> FakeMotionDevice::ReadSession() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DeviceSession>::Success(session_);
}

Siligen::SharedKernel::VoidResult FakeMotionDevice::Execute(
    const Siligen::Device::Contracts::Commands::MotionCommand& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.correlation_id = command.correlation_id;
    switch (command.kind) {
        case Siligen::Device::Contracts::Commands::MotionCommandKind::kMoveTo:
        case Siligen::Device::Contracts::Commands::MotionCommandKind::kJog:
            for (const auto& target : command.targets) {
                auto& axis = FindAxis(target.axis);
                if (command.kind == Siligen::Device::Contracts::Commands::MotionCommandKind::kMoveTo) {
                    axis.position_mm = target.position_mm;
                } else {
                    axis.position_mm += target.position_mm;
                }
                axis.velocity_mm_s = target.velocity_mm_s;
                axis.servo_enabled = true;
            }
            state_.lifecycle = Siligen::Device::Contracts::State::MotionLifecycleState::kRunning;
            session_.state = Siligen::Device::Contracts::State::DeviceSessionState::kRunning;
            break;
        case Siligen::Device::Contracts::Commands::MotionCommandKind::kHome:
            for (const auto& capability : capabilities_.axes) {
                auto& axis = FindAxis(capability.axis);
                axis.position_mm = 0.0;
                axis.homed = true;
            }
            state_.lifecycle = Siligen::Device::Contracts::State::MotionLifecycleState::kPrepared;
            break;
        case Siligen::Device::Contracts::Commands::MotionCommandKind::kStop:
            state_.lifecycle = Siligen::Device::Contracts::State::MotionLifecycleState::kStopped;
            session_.state = Siligen::Device::Contracts::State::DeviceSessionState::kReady;
            break;
        case Siligen::Device::Contracts::Commands::MotionCommandKind::kEmergencyStop:
            state_.estop_active = true;
            state_.lifecycle = Siligen::Device::Contracts::State::MotionLifecycleState::kFaulted;
            session_.state = Siligen::Device::Contracts::State::DeviceSessionState::kFaulted;
            break;
        case Siligen::Device::Contracts::Commands::MotionCommandKind::kExecuteTrajectory:
            state_.lifecycle = Siligen::Device::Contracts::State::MotionLifecycleState::kRunning;
            session_.state = Siligen::Device::Contracts::State::DeviceSessionState::kRunning;
            break;
    }
    return Siligen::SharedKernel::VoidResult::Success();
}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MotionState> FakeMotionDevice::ReadState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MotionState>::Success(state_);
}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DeviceCapabilities>
FakeMotionDevice::DescribeCapabilities() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DeviceCapabilities>::Success(
        capabilities_);
}

Siligen::SharedKernel::Result<std::vector<Siligen::Device::Contracts::Faults::DeviceFault>> FakeMotionDevice::ReadFaults()
    const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Siligen::SharedKernel::Result<std::vector<Siligen::Device::Contracts::Faults::DeviceFault>>::Success(
        active_faults_);
}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MachineHealthSnapshot> FakeMotionDevice::ReadHealth()
    const {
    std::lock_guard<std::mutex> lock(mutex_);
    Siligen::Device::Contracts::State::MachineHealthSnapshot snapshot;
    snapshot.connected = state_.connected;
    snapshot.estop_active = state_.estop_active;
    snapshot.active_faults = active_faults_;
    return Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MachineHealthSnapshot>::Success(snapshot);
}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState> FakeMotionDevice::ReadInput(
    Siligen::SharedKernel::int32 channel) const {
    std::lock_guard<std::mutex> lock(mutex_);
    Siligen::Device::Contracts::State::DigitalIoState state;
    state.channel = channel;
    state.output = false;
    const auto it = inputs_.find(channel);
    state.value = it != inputs_.end() ? it->second : false;
    return Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState>::Success(state);
}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState> FakeMotionDevice::ReadOutput(
    Siligen::SharedKernel::int32 channel) const {
    std::lock_guard<std::mutex> lock(mutex_);
    Siligen::Device::Contracts::State::DigitalIoState state;
    state.channel = channel;
    state.output = true;
    const auto it = outputs_.find(channel);
    state.value = it != outputs_.end() ? it->second : false;
    return Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState>::Success(state);
}

Siligen::SharedKernel::VoidResult FakeMotionDevice::WriteOutput(Siligen::SharedKernel::int32 channel, bool value) {
    std::lock_guard<std::mutex> lock(mutex_);
    outputs_[channel] = value;
    return Siligen::SharedKernel::VoidResult::Success();
}

void FakeMotionDevice::EnsureDefaultAxes() {
    if (!state_.axes.empty()) {
        return;
    }
    for (const auto& capability : capabilities_.axes) {
        Siligen::Device::Contracts::State::AxisState axis;
        axis.axis = capability.axis;
        state_.axes.push_back(axis);
    }
}

Siligen::Device::Contracts::State::AxisState& FakeMotionDevice::FindAxis(Siligen::SharedKernel::LogicalAxisId axis) {
    for (auto& axis_state : state_.axes) {
        if (axis_state.axis == axis) {
            return axis_state;
        }
    }

    state_.axes.push_back({});
    state_.axes.back().axis = axis;
    return state_.axes.back();
}

}  // namespace Siligen::Device::Adapters::Fake
