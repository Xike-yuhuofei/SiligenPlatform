#pragma once

#include "dispense_packaging/contracts/IValvePort.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>

namespace Siligen::Application::UseCases::Dispensing::Valve {

class ValveQueryUseCase {
   public:
    explicit ValveQueryUseCase(std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port);

    Shared::Types::Result<Domain::Dispensing::Ports::DispenserValveState> GetDispenserStatus();
    Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveStatusDetail> GetSupplyStatus();

   private:
    std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port_;
};

}  // namespace Siligen::Application::UseCases::Dispensing::Valve
