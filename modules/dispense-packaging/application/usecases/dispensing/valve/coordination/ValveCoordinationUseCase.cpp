#include "ValveCoordinationUseCase.h"

#include "shared/types/Error.h"

#include <cmath>

namespace Siligen::Application::UseCases::Dispensing::Valve::Coordination {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

ValveCoordinationUseCase::ValveCoordinationUseCase(
    std::shared_ptr<IValvePort> valve_port)
    : valve_port_(std::move(valve_port)),
      current_velocity_(0.0f) {}

Result<ValveTimingResult> ValveCoordinationUseCase::CalculateTimingParameters(
    const std::vector<GeometrySegment>& path,
    const ValveTimingConfig& config) {
    if (!config.Validate()) {
        return Result<ValveTimingResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "invalid valve timing configuration"));
    }
    if (path.empty()) {
        return Result<ValveTimingResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "path is empty"));
    }

    current_velocity_ = config.velocity;
    ValveTimingResult result{};
    result.total_length_mm = CalculateTotalLength(path);
    result.total_time_s = result.total_length_mm / config.velocity;
    result.trigger_points = GenerateTriggerPoints(path, (config.interval_ms * config.velocity) / 1000.0f);
    result.trigger_count = static_cast<uint32>(result.trigger_points.size());
    return Result<ValveTimingResult>::Success(std::move(result));
}

float32 ValveCoordinationUseCase::CalculateTotalLength(const std::vector<GeometrySegment>& path) {
    float32 total_length = 0.0f;
    for (const auto& segment : path) {
        total_length += segment.Length();
    }
    return total_length;
}

std::vector<TriggerPoint> ValveCoordinationUseCase::GenerateTriggerPoints(
    const std::vector<GeometrySegment>& path,
    float32 interval_mm) {
    std::vector<TriggerPoint> trigger_points;
    if (interval_mm <= 0.0f) {
        return trigger_points;
    }

    float32 accumulated_length = 0.0f;
    float32 next_trigger_position = interval_mm;
    for (std::size_t segment_index = 0; segment_index < path.size(); ++segment_index) {
        const auto& segment = path[segment_index];
        const float32 segment_length = segment.Length();
        accumulated_length += segment_length;
        while (accumulated_length >= next_trigger_position) {
            TriggerPoint point{};
            point.position_mm = next_trigger_position;
            point.time_s = current_velocity_ > 0.0f ? (next_trigger_position / current_velocity_) : 0.0f;
            point.segment_index = static_cast<uint32>(segment_index);
            trigger_points.push_back(point);
            next_trigger_position += interval_mm;
        }
    }

    return trigger_points;
}

}  // namespace Siligen::Application::UseCases::Dispensing::Valve::Coordination
