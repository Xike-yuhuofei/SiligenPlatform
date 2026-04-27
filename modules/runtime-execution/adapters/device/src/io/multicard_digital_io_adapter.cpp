#include "siligen/device/adapters/io/multicard_digital_io_adapter.h"

#include "siligen/shared/error.h"

namespace Siligen::Device::Adapters::Io {
namespace {

Siligen::SharedKernel::Error MakeIoError(const std::string& message) {
    return Siligen::SharedKernel::Error(
        Siligen::SharedKernel::ErrorCode::HARDWARE_COMMAND_FAILED,
        message,
        "device-adapters.io");
}

}  // namespace

MultiCardDigitalIoAdapter::MultiCardDigitalIoAdapter(
    std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> driver)
    : driver_(std::move(driver)) {}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState>
MultiCardDigitalIoAdapter::ReadInput(Siligen::SharedKernel::int32 channel) const {
    long value = 0;
    const int result = driver_->MC_GetDiRaw(0, &value);
    if (result != 0) {
        return Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState>::Failure(
            MakeIoError("Failed to read digital input word"));
    }
    return ReadWordBit(static_cast<unsigned long>(value), channel, false);
}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState>
MultiCardDigitalIoAdapter::ReadOutput(Siligen::SharedKernel::int32 channel) const {
    unsigned long value = 0;
    const int result = driver_->MC_GetExtDoValue(0, &value);
    if (result != 0) {
        return Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState>::Failure(
            MakeIoError("Failed to read digital output word"));
    }
    return ReadWordBit(value, channel, true);
}

Siligen::SharedKernel::VoidResult MultiCardDigitalIoAdapter::WriteOutput(
    Siligen::SharedKernel::int32 channel,
    bool value) {
    const int result = driver_->MC_SetExtDoBit(0, static_cast<short>(channel), value ? 1U : 0U);
    if (result != 0) {
        return Siligen::SharedKernel::VoidResult::Failure(MakeIoError("Failed to write digital output"));
    }
    return Siligen::SharedKernel::VoidResult::Success();
}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState>
MultiCardDigitalIoAdapter::ReadWordBit(unsigned long word, Siligen::SharedKernel::int32 channel, bool output) {
    if (channel < 0 || channel >= 32) {
        return Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState>::Failure(
            Siligen::SharedKernel::Error(
                Siligen::SharedKernel::ErrorCode::INVALID_PARAMETER,
                "Digital IO channel out of range",
                "device-adapters.io"));
    }

    Siligen::Device::Contracts::State::DigitalIoState state;
    state.channel = channel;
    state.output = output;
    state.value = (word & (1UL << channel)) != 0;
    return Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState>::Success(state);
}

}  // namespace Siligen::Device::Adapters::Io
