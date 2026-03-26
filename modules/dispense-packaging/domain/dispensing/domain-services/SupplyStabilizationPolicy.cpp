#include "SupplyStabilizationPolicy.h"

namespace Siligen::Domain::Dispensing::DomainServices {

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

    if (config_port) {
        auto config_result = config_port->GetDispensingConfig();
        if (config_result.IsSuccess()) {
            const auto& config = config_result.Value();
            if (config.supply_stabilization_ms > 0) {
                return Result<uint32>::Success(static_cast<uint32>(config.supply_stabilization_ms));
            }
        }
    }

    return Result<uint32>::Success(kDefaultMs);
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
