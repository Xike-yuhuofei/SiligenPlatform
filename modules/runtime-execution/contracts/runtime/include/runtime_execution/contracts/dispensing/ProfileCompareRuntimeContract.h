#pragma once

#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

namespace Siligen::RuntimeExecution::Contracts::Dispensing {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::uint32;

struct ProfileCompareRuntimeContract {
    float32 pulse_per_mm = 0.0f;
    int32 compare_axis_mask = 0;
    uint32 max_future_compare_count = 0U;

    [[nodiscard]] Result<void> Validate() const noexcept {
        if (pulse_per_mm <= 0.0f) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_CONFIG_VALUE,
                "pulse_per_mm 必须大于 0 以编译 profile_compare schedule"));
        }
        if (max_future_compare_count == 0U) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_CONFIG_VALUE,
                "ValveDispenser.max_count 必须大于 0 以编译 profile_compare schedule"));
        }
        return Result<void>::Success();
    }
};

}  // namespace Siligen::RuntimeExecution::Contracts::Dispensing
