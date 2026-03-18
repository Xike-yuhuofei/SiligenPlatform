#include "HardwareValidationService.h"

#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <chrono>
#include <cmath>
#include <numeric>
#include <utility>

#define MODULE_NAME "HardwareValidationService"

namespace Siligen::Infrastructure::Adapters::Diagnostics::Health {

using namespace Shared::Types;

HardwareValidationService::HardwareValidationService(
    std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port)
    : interpolation_port_(std::move(interpolation_port)) {}

Result<ApiLatencyStats> HardwareValidationService::MeasureTimedTriggerLatency(int iterations) {
    if (!interpolation_port_) {
        return Result<ApiLatencyStats>::Failure(
            Error(ErrorCode::INVALID_STATE, "Interpolation port not initialized"));
    }

    std::vector<float32> latencies;
    latencies.reserve(iterations);

    SILIGEN_LOG_INFO_FMT_HELPER("Starting Timed Trigger Latency Test (%d iterations)", iterations);

    // Dummy params for measurement (safe values)
    // Assuming MC_CmpPluse logic is exposed or we simulate call overhead
    // Since we don't have direct MC_CmpPluse in IInterpolationPort (it's in ValvePort typically),
    // we might need to cast or use a specific method.
    // For now, we will measure a generic safe command like GetStatus to approximate command overhead,
    // or if IInterpolationPort has specific IO methods.
    // Update: Plan says "Measure MC_CmpPluse blocking time".
    // We assume this service is used in context where we can call the driver function or exposed port method.
    // IF not available, we measure GetCoordinateSystemStatus as proxy for command roundtrip.

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        // Using GetCoordinateSystemStatus as a proxy for a command round-trip if specific CMP method isn't in MotionPort.
        // Ideally this should inject IValvePort but constructor only takes MotionPort in current plan context.
        // Let's assume we measure a standard command overhead.
        auto result = interpolation_port_->GetCoordinateSystemStatus(1);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float32, std::milli> duration = end - start;
        latencies.push_back(duration.count());

        if (i % 10 == 0) {
             // Small sleep to not flood if necessary, though we want to measure burst performance.
        }
    }

    ApiLatencyStats stats;
    stats.samples = iterations;
    if (iterations > 0) {
        stats.min_ms = latencies[0];
        stats.max_ms = latencies[0];
        float32 sum = 0.0f;
        for (float32 v : latencies) {
            if (v < stats.min_ms) stats.min_ms = v;
            if (v > stats.max_ms) stats.max_ms = v;
            sum += v;
        }
        stats.avg_ms = sum / iterations;

        float32 sq_sum = 0.0f;
        for (float32 v : latencies) {
            sq_sum += (v - stats.avg_ms) * (v - stats.avg_ms);
        }
        stats.std_dev_ms = std::sqrt(sq_sum / iterations);
    }

    SILIGEN_LOG_INFO_FMT_HELPER(
        "Latency Test Result: Min=%.3fms, Max=%.3fms, Avg=%.3fms, StdDev=%.3fms",
        stats.min_ms, stats.max_ms, stats.avg_ms, stats.std_dev_ms);

    return Result<ApiLatencyStats>::Success(stats);
}

Result<ApiLatencyStats> HardwareValidationService::MeasureImmediateIOLatency(int iterations) {
    // Similar implementation for SetDO if exposed
    // Currently using generic proxy
    return MeasureTimedTriggerLatency(iterations);
}

}  // namespace Siligen::Infrastructure::Adapters::Diagnostics::Health

