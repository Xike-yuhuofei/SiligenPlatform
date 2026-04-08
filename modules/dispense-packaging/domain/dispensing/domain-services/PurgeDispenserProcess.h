#pragma once

#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "domain/dispensing/ports/IValvePort.h"
#include "domain/dispensing/domain-services/SupplyStabilizationPolicy.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>
#include <string>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::uint32;
using Siligen::Domain::Dispensing::DomainServices::SupplyStabilizationPolicy;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Dispensing::Ports::DispenserValveState;
using Siligen::Domain::Dispensing::Ports::DispenserValveStatus;
using Siligen::Domain::Dispensing::Ports::IValvePort;
using Siligen::Domain::Dispensing::Ports::SupplyValveState;

/**
 * @brief 排胶请求参数（胶路建压/稳压由领域层负责）
 *
 * 说明:
 * - 供胶阀稳压时间默认参考 docs/plans/2026-01-19-purge-dispenser-usecase-plan.md
 * - supply_stabilization_ms 为 0 时使用配置默认值
 */
struct PurgeDispenserRequest {
    bool manage_supply = true;
    bool wait_for_completion = true;
    uint32 supply_stabilization_ms = 0;
    uint32 poll_interval_ms = 50;
    uint32 timeout_ms = 30000;

    bool Validate() const noexcept;
    std::string GetValidationError() const noexcept;
};

/**
 * @brief 排胶执行结果
 */
struct PurgeDispenserResponse {
    DispenserValveState dispenser_state;
    SupplyValveState supply_state = SupplyValveState::Closed;
    bool supply_managed = false;
    bool completed = false;
};

/**
 * @brief 排胶流程领域服务（建压/稳压+排胶时序）
 */
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

}  // namespace Siligen::Domain::Dispensing::DomainServices
