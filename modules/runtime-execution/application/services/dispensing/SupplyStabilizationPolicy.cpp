#include "SupplyStabilizationPolicy.h"

namespace Siligen::RuntimeExecution::Application::Services::Dispensing {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

bool SupplyStabilizationPolicy::IsOverrideValid(uint32 override_ms) noexcept {
    return override_ms <= kMaxMs;
}

Result<uint32> SupplyStabilizationPolicy::Resolve(
    const std::shared_ptr<IConfigurationPort>& config_port,
    std::optional<uint32> override_ms) noexcept {
    if (override_ms.has_value()) {
        if (!IsOverrideValid(*override_ms)) {
            return Result<uint32>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "供胶阀稳定时间必须在0-5000ms范围内", "SupplyStabilizationPolicy"));
        }
        return Result<uint32>::Success(*override_ms);
    }

    if (!config_port) {
        return Result<uint32>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED,
                  "供胶阀稳压配置端口未初始化",
                  "SupplyStabilizationPolicy"));
    }

    auto config_result = config_port->GetDispensingConfig();
    if (config_result.IsError()) {
        return Result<uint32>::Failure(config_result.GetError());
    }

    const auto configured_ms = config_result.Value().supply_stabilization_ms;
    if (configured_ms <= 0 || configured_ms > static_cast<decltype(configured_ms)>(kMaxMs)) {
        return Result<uint32>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "供胶阀稳压时间必须在1-5000ms范围内且由配置显式提供",
                  "SupplyStabilizationPolicy"));
    }

    return Result<uint32>::Success(static_cast<uint32>(configured_ms));
}

}  // namespace Siligen::RuntimeExecution::Application::Services::Dispensing
