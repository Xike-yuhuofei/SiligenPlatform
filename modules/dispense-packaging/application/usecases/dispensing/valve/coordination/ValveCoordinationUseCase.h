#pragma once

#include "domain/dispensing/ports/IValvePort.h"
#include "domain/geometry/GeometryTypes.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>
#include <vector>

namespace Siligen::Application::UseCases::Dispensing::Valve::Coordination {

using Siligen::Domain::Dispensing::Ports::IValvePort;
using Siligen::Domain::Geometry::GeometrySegment;
using Siligen::Shared::Types::Result;

struct ValveTimingConfig {
    uint32 interval_ms;
    uint32 duration_ms;
    float32 velocity;

    bool Validate() const {
        return (interval_ms >= 10 && interval_ms <= 60000) &&
               (duration_ms >= 10 && duration_ms <= 5000) &&
               (velocity > 0.0f && velocity <= 1000.0f);
    }
};

struct TriggerPoint {
    float32 position_mm;
    float32 time_s;
    uint32 segment_index;
};

struct ValveTimingResult {
    float32 total_length_mm;
    float32 total_time_s;
    uint32 trigger_count;
    std::vector<TriggerPoint> trigger_points;
};

class ValveCoordinationUseCase {
public:
    explicit ValveCoordinationUseCase(std::shared_ptr<IValvePort> valve_port);

    ~ValveCoordinationUseCase() = default;

    ValveCoordinationUseCase(const ValveCoordinationUseCase&) = delete;
    ValveCoordinationUseCase& operator=(const ValveCoordinationUseCase&) = delete;

    Result<ValveTimingResult> CalculateTimingParameters(
        const std::vector<GeometrySegment>& path,
        const ValveTimingConfig& config);

private:
    std::shared_ptr<IValvePort> valve_port_;
    float32 current_velocity_;

    float32 CalculateTotalLength(const std::vector<GeometrySegment>& path);
    std::vector<TriggerPoint> GenerateTriggerPoints(
        const std::vector<GeometrySegment>& path,
        float32 interval_mm);
};

}  // namespace Siligen::Application::UseCases::Dispensing::Valve::Coordination
