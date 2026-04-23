#pragma once

#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "services/dispensing/SupplyStabilizationPolicy.h"
#include "runtime_execution/contracts/dispensing/IValvePort.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>
#include <string>

namespace Siligen::RuntimeExecution::Application::Services::Dispensing {

using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Dispensing::Ports::DispenserValveState;
using Siligen::Domain::Dispensing::Ports::DispenserValveStatus;
using Siligen::Domain::Dispensing::Ports::IValvePort;
using Siligen::Domain::Dispensing::Ports::SupplyValveState;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::uint32;

struct PurgeDispenserRequest {
    bool manage_supply = true;
    bool wait_for_completion = true;
    uint32 supply_stabilization_ms = 0;
    uint32 poll_interval_ms = 50;
    uint32 timeout_ms = 30000;

    bool Validate() const noexcept;
    std::string GetValidationError() const noexcept;
};

struct PurgeDispenserResponse {
    DispenserValveState dispenser_state;
    SupplyValveState supply_state = SupplyValveState::Closed;
    bool supply_managed = false;
    bool completed = false;
};

class PurgeDispenserProcess final {
   public:
    PurgeDispenserProcess(std::shared_ptr<IValvePort> valve_port,
                          std::shared_ptr<IConfigurationPort> config_port) noexcept;

    Result<PurgeDispenserResponse> Execute(const PurgeDispenserRequest& request) noexcept;

   private:
    std::shared_ptr<IValvePort> valve_port_;
    std::shared_ptr<IConfigurationPort> config_port_;

    Result<DispenserValveState> WaitForCompletion(uint32 timeout_ms, uint32 poll_interval_ms) noexcept;
};

}  // namespace Siligen::RuntimeExecution::Application::Services::Dispensing
