#pragma once

#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>
#include <optional>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::uint32;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;

/**
 * @brief 供胶阀稳压时间策略
 *
 * 规则:
 * - 若提供覆盖值，则在允许范围内直接使用
 * - 否则使用配置中的默认值
 * - 若配置不可用或无效，回退到默认值
 */
class SupplyStabilizationPolicy final {
   public:
    static Result<uint32> Resolve(const std::shared_ptr<IConfigurationPort>& config_port,
                                  std::optional<uint32> override_ms = std::nullopt) noexcept;

    static bool IsOverrideValid(uint32 override_ms) noexcept;

    static constexpr uint32 DefaultMs() noexcept { return kDefaultMs; }

   private:
    static constexpr uint32 kMaxMs = 5000;
    static constexpr uint32 kDefaultMs = 500;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices
