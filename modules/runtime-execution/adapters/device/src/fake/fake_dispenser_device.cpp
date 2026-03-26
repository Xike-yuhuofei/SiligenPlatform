#include "siligen/device/adapters/fake/fake_dispenser_device.h"

namespace Siligen::Device::Adapters::Fake {

FakeDispenserDevice::FakeDispenserDevice() {
    capability_.supports_prime = true;
    capability_.supports_pause = true;
    capability_.supports_resume = true;
    capability_.supports_continuous_mode = true;
}

Siligen::SharedKernel::VoidResult FakeDispenserDevice::Execute(
    const Siligen::Device::Contracts::Commands::DispenserCommand& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.execution_id = command.execution_id;
    switch (command.kind) {
        case Siligen::Device::Contracts::Commands::DispenserCommandKind::kPrime:
            state_.primed = true;
            break;
        case Siligen::Device::Contracts::Commands::DispenserCommandKind::kStart:
            state_.running = true;
            state_.paused = false;
            break;
        case Siligen::Device::Contracts::Commands::DispenserCommandKind::kPause:
            state_.paused = true;
            state_.running = false;
            break;
        case Siligen::Device::Contracts::Commands::DispenserCommandKind::kResume:
            state_.paused = false;
            state_.running = true;
            break;
        case Siligen::Device::Contracts::Commands::DispenserCommandKind::kStop:
            state_.running = false;
            state_.paused = false;
            break;
    }
    return Siligen::SharedKernel::VoidResult::Success();
}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DispenserState> FakeDispenserDevice::ReadState()
    const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DispenserState>::Success(state_);
}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DispenserCapability>
FakeDispenserDevice::DescribeCapability() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DispenserCapability>::Success(
        capability_);
}

}  // namespace Siligen::Device::Adapters::Fake
