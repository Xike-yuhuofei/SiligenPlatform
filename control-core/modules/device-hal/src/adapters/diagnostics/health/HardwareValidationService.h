#pragma once

#include "domain/motion/ports/IInterpolationPort.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Infrastructure::Adapters::Diagnostics::Health {

using Shared::Types::float32;
using Shared::Types::int32;
using Shared::Types::Result;

struct ApiLatencyStats {
    float32 min_ms = 0.0f;
    float32 max_ms = 0.0f;
    float32 avg_ms = 0.0f;
    float32 std_dev_ms = 0.0f;
    int32 samples = 0;
};

class HardwareValidationService {
   public:
    explicit HardwareValidationService(
        std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port);

    /**
     * @brief Measure the latency of MC_CmpPluse (or equivalent timed trigger) API calls.
     * @param iterations Number of times to call the API.
     * @return Result containing latency statistics.
     */
    Result<ApiLatencyStats> MeasureTimedTriggerLatency(int iterations);

    /**
     * @brief Measure the latency of MC_SetDo (immediate IO) API calls.
     * @param iterations Number of times to call the API.
     * @return Result containing latency statistics.
     */
    Result<ApiLatencyStats> MeasureImmediateIOLatency(int iterations);

   private:
    std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port_;
};

}  // namespace Siligen::Infrastructure::Adapters::Diagnostics::Health
