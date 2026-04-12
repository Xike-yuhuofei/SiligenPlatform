#pragma once

#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "dispense_packaging/contracts/IValvePort.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>

namespace Siligen::Application::UseCases::Dispensing::Valve {

struct PurgeDispenserRequest {
    bool manage_supply = true;
    bool wait_for_completion = true;
    Siligen::Shared::Types::uint32 supply_stabilization_ms = 0;
    Siligen::Shared::Types::uint32 poll_interval_ms = 50;
    Siligen::Shared::Types::uint32 timeout_ms = 30000;

    bool Validate() const noexcept;
    std::string GetValidationError() const noexcept;
};

struct PurgeDispenserResponse {
    Domain::Dispensing::Ports::DispenserValveState dispenser_state;
    Domain::Dispensing::Ports::SupplyValveState supply_state =
        Domain::Dispensing::Ports::SupplyValveState::Closed;
    bool supply_managed = false;
    bool completed = false;
};

class ValveCommandUseCase {
   public:
    ValveCommandUseCase(
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

    std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port_;
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port_;
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port_;
};

}  // namespace Siligen::Application::UseCases::Dispensing::Valve
