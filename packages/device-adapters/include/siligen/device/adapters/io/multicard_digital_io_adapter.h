#pragma once

#include "siligen/device/adapters/drivers/multicard/IMultiCardWrapper.h"
#include "siligen/device/contracts/ports/device_ports.h"

#include <memory>

namespace Siligen::Device::Adapters::Io {

class MultiCardDigitalIoAdapter final : public Siligen::Device::Contracts::Ports::DigitalIoPort {
   public:
    explicit MultiCardDigitalIoAdapter(std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> driver);

    Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState> ReadInput(
        Siligen::SharedKernel::int32 channel) const override;
    Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState> ReadOutput(
        Siligen::SharedKernel::int32 channel) const override;
    Siligen::SharedKernel::VoidResult WriteOutput(Siligen::SharedKernel::int32 channel, bool value) override;

   private:
    Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DigitalIoState> ReadWordBit(
        unsigned long word,
        Siligen::SharedKernel::int32 channel,
        bool output) const;

    std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> driver_;
};

}  // namespace Siligen::Device::Adapters::Io
