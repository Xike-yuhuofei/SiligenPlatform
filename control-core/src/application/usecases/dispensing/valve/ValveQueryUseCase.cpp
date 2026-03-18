#include "ValveQueryUseCase.h"

#include <utility>

namespace Siligen::Application::UseCases::Dispensing::Valve {

ValveQueryUseCase::ValveQueryUseCase(std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port)
    : valve_port_(std::move(valve_port)) {}

Shared::Types::Result<Domain::Dispensing::Ports::DispenserValveState> ValveQueryUseCase::GetDispenserStatus() {
    return valve_port_->GetDispenserStatus();
}

Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveStatusDetail> ValveQueryUseCase::GetSupplyStatus() {
    return valve_port_->GetSupplyStatus();
}

}  // namespace Siligen::Application::UseCases::Dispensing::Valve
