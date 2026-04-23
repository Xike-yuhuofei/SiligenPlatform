#pragma once

#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>
#include <optional>

namespace Siligen::RuntimeExecution::Application::Services::Dispensing {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::uint32;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;

/**
 * @brief 供胶阀稳压时间策略
 *
 * 规则:
 * - 若提供覆盖值，则在允许范围内直接使用
 * - 否则必须使用配置中的显式稳压时间
 * - 配置端口缺失、读取失败或值无效时直接失败
 */
class SupplyStabilizationPolicy final {
   public:
    static Result<uint32> Resolve(const std::shared_ptr<IConfigurationPort>& config_port,
                                  std::optional<uint32> override_ms = std::nullopt) noexcept;

    static bool IsOverrideValid(uint32 override_ms) noexcept;

   private:
    static constexpr uint32 kMaxMs = 5000;
};

}  // namespace Siligen::RuntimeExecution::Application::Services::Dispensing
