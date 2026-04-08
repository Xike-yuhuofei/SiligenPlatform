#pragma once

#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "domain/dispensing/domain-services/PurgeDispenserProcess.h"
#include "domain/dispensing/domain-services/ValveCoordinationService.h"
#include "domain/dispensing/ports/IValvePort.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "shared/types/Result.h"

#include <memory>

namespace Siligen::Application::UseCases::Dispensing::Valve {

using PurgeDispenserRequest = Domain::Dispensing::DomainServices::PurgeDispenserRequest;
using PurgeDispenserResponse = Domain::Dispensing::DomainServices::PurgeDispenserResponse;

class ValveCommandUseCase {
   public:
    ValveCommandUseCase(
        std::shared_ptr<Domain::Dispensing::DomainServices::ValveCoordinationService> valve_service,
        std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port,
        std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
        std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port);

    Shared::Types::Result<Domain::Dispensing::Ports::DispenserValveState> StartDispenser(
        const Domain::Dispensing::Ports::DispenserValveParams& params);
    Shared::Types::Result<PurgeDispenserResponse> PurgeDispenser(const PurgeDispenserRequest& request);
    Shared::Types::Result<void> StopDispenser();
    Shared::Types::Result<void> PauseDispenser();
    Shared::Types::Result<void> ResumeDispenser();
    Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveState> OpenSupplyValve();
    Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveState> CloseSupplyValve();

   private:
    Shared::Types::Result<void> EnsureHardwareConnected() const;

    std::shared_ptr<Domain::Dispensing::DomainServices::ValveCoordinationService> valve_service_;
    std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port_;
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port_;
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port_;
};

}  // namespace Siligen::Application::UseCases::Dispensing::Valve
