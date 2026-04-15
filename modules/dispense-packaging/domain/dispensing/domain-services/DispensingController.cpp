#include "DispensingController.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Point2D;

namespace {
constexpr float32 kEpsilon = 1e-6f;

struct TriggerSample {
    Point2D position;
    float32 path_position_mm = 0.0f;
};

void AppendTriggerSample(std::vector<TriggerSample>& samples,
                         const Point2D& position,
                         float32 path_position_mm) {
    if (path_position_mm < 0.0f) {
        return;
    }

    if (!samples.empty()) {
        const auto& previous = samples.back();
        if (std::fabs(previous.path_position_mm - path_position_mm) <= kEpsilon &&
            previous.position.DistanceTo(position) <= kEpsilon) {
            return;
        }
    }

    TriggerSample sample;
    sample.position = position;
    sample.path_position_mm = path_position_mm;
    samples.push_back(sample);
}

long MmToPulse(float32 value_mm, float32 pulse_per_mm) {
    return static_cast<long>(std::llround(value_mm * pulse_per_mm));
}

std::vector<PathTriggerEvent> BuildTriggerEvents(const std::vector<TriggerSample>& samples, float32 pulse_per_mm) {
    std::vector<PathTriggerEvent> events;
    events.reserve(samples.size());
    for (std::size_t index = 0; index < samples.size(); ++index) {
        PathTriggerEvent event;
        event.sequence_index = static_cast<uint32>(index);
        event.path_position_mm = samples[index].path_position_mm;
        event.x_position_pulse = MmToPulse(samples[index].position.x, pulse_per_mm);
        event.y_position_pulse = MmToPulse(samples[index].position.y, pulse_per_mm);
        events.push_back(event);
    }
    return events;
}

std::vector<TriggerSample> BuildTriggerSamplesFromDistances(const MotionTrajectory& trajectory,
                                                            const std::vector<float32>& distances_mm) {
    std::vector<TriggerSample> samples;
    if (trajectory.points.size() < 2 || distances_mm.empty()) {
        return samples;
    }

    std::vector<float32> cumulative(trajectory.points.size(), 0.0f);
    for (size_t index = 1; index < trajectory.points.size(); ++index) {
        const auto& previous = trajectory.points[index - 1];
        const auto& current = trajectory.points[index];
        cumulative[index] = cumulative[index - 1] + previous.position.DistanceTo3D(current.position);
    }

    std::vector<float32> sorted = distances_mm;
    std::sort(sorted.begin(), sorted.end());

    size_t index = 1;
    for (float32 target : sorted) {
        while (index < cumulative.size() && cumulative[index] + kEpsilon < target) {
            ++index;
        }
        if (index >= cumulative.size()) {
            break;
        }

        const auto& previous = trajectory.points[index - 1];
        const auto& current = trajectory.points[index];
        if (!previous.dispense_on || !current.dispense_on) {
            continue;
        }

        const float32 segment_length = cumulative[index] - cumulative[index - 1];
        const float32 ratio =
            segment_length > kEpsilon ? (target - cumulative[index - 1]) / segment_length : 0.0f;
        const Point2D start(previous.position);
        const Point2D end(current.position);
        AppendTriggerSample(samples, start + (end - start) * ratio, target);
    }

    return samples;
}

std::vector<TriggerSample> BuildTriggerSamplesFromDistances(const std::vector<TrajectoryPoint>& trajectory,
                                                            const std::vector<float32>& distances_mm) {
    std::vector<TriggerSample> samples;
    if (trajectory.size() < 2 || distances_mm.empty()) {
        return samples;
    }

    std::vector<float32> cumulative(trajectory.size(), 0.0f);
    for (size_t index = 1; index < trajectory.size(); ++index) {
        cumulative[index] = cumulative[index - 1] +
                            trajectory[index - 1].position.DistanceTo(trajectory[index].position);
    }

    std::vector<float32> sorted = distances_mm;
    std::sort(sorted.begin(), sorted.end());

    size_t index = 1;
    for (float32 target : sorted) {
        while (index < cumulative.size() && cumulative[index] + kEpsilon < target) {
            ++index;
        }
        if (index >= cumulative.size()) {
            break;
        }

        const float32 segment_length = cumulative[index] - cumulative[index - 1];
        const float32 ratio =
            segment_length > kEpsilon ? (target - cumulative[index - 1]) / segment_length : 0.0f;
        const Point2D start(trajectory[index - 1].position);
        const Point2D end(trajectory[index].position);
        AppendTriggerSample(samples, start + (end - start) * ratio, target);
    }

    return samples;
}

std::vector<TriggerSample> BuildTriggerSamplesFromInterval(const MotionTrajectory& trajectory, float32 interval_mm) {
    std::vector<TriggerSample> samples;
    if (trajectory.points.size() < 2 || interval_mm <= kEpsilon) {
        return samples;
    }

    float32 accumulated = 0.0f;
    float32 next_trigger = interval_mm;
    for (size_t index = 1; index < trajectory.points.size(); ++index) {
        const auto& previous = trajectory.points[index - 1];
        const auto& current = trajectory.points[index];
        const float32 segment_length = previous.position.DistanceTo3D(current.position);
        const float32 segment_end = accumulated + segment_length;
        if (!previous.dispense_on || !current.dispense_on) {
            while (next_trigger <= segment_end + kEpsilon) {
                next_trigger += interval_mm;
            }
            accumulated = segment_end;
            continue;
        }

        const Point2D start(previous.position);
        const Point2D end(current.position);
        while (segment_end + kEpsilon >= next_trigger) {
            const float32 offset_mm = next_trigger - accumulated;
            const float32 ratio = segment_length > kEpsilon ? offset_mm / segment_length : 0.0f;
            AppendTriggerSample(samples, start + (end - start) * ratio, next_trigger);
            next_trigger += interval_mm;
        }
        accumulated = segment_end;
    }

    return samples;
}

std::vector<TriggerSample> BuildTriggerSamplesFromInterval(const std::vector<TrajectoryPoint>& trajectory,
                                                           float32 interval_mm) {
    std::vector<TriggerSample> samples;
    if (trajectory.size() < 2 || interval_mm <= kEpsilon) {
        return samples;
    }

    float32 accumulated = 0.0f;
    float32 next_trigger = interval_mm;
    for (size_t index = 1; index < trajectory.size(); ++index) {
        const float32 segment_length = trajectory[index - 1].position.DistanceTo(trajectory[index].position);
        const Point2D start(trajectory[index - 1].position);
        const Point2D end(trajectory[index].position);
        while (accumulated + segment_length + kEpsilon >= next_trigger) {
            const float32 offset_mm = next_trigger - accumulated;
            const float32 ratio = segment_length > kEpsilon ? offset_mm / segment_length : 0.0f;
            AppendTriggerSample(samples, start + (end - start) * ratio, next_trigger);
            next_trigger += interval_mm;
        }
        accumulated += segment_length;
    }

    return samples;
}

float32 EstimateMotionTimeMs(const std::vector<TrajectoryPoint>& trajectory, float32 fallback_velocity) {
    if (trajectory.empty()) {
        return 0.0f;
    }

    float32 max_timestamp = 0.0f;
    for (const auto& point : trajectory) {
        max_timestamp = std::max(max_timestamp, point.timestamp);
    }
    if (max_timestamp > kEpsilon) {
        return max_timestamp * 1000.0f;
    }

    float32 length = 0.0f;
    for (size_t index = 1; index < trajectory.size(); ++index) {
        length += trajectory[index - 1].position.DistanceTo(trajectory[index].position);
    }
    if (fallback_velocity > kEpsilon) {
        return (length / fallback_velocity) * 1000.0f;
    }
    return 0.0f;
}

float32 CalculateTotalLength(const std::vector<TrajectoryPoint>& trajectory) {
    if (trajectory.size() < 2) {
        return 0.0f;
    }

    float32 length = 0.0f;
    for (size_t index = 1; index < trajectory.size(); ++index) {
        length += trajectory[index - 1].position.DistanceTo(trajectory[index].position);
    }
    return length;
}

}  // namespace

Result<ControllerOutput> DispensingController::Build(const MotionTrajectory& trajectory,
                                                     const ControllerConfig& config) const noexcept {
    ControllerOutput output;
    if (trajectory.points.size() < 2) {
        return Result<ControllerOutput>::Success(output);
    }

    output.estimated_motion_time_ms = trajectory.total_time * 1000.0f;

    if (config.use_hardware_trigger) {
        std::vector<TriggerSample> trigger_samples;
        if (!config.trigger_distances_mm.empty()) {
            trigger_samples = BuildTriggerSamplesFromDistances(trajectory, config.trigger_distances_mm);
        } else if (config.spatial_interval_mm > kEpsilon) {
            trigger_samples = BuildTriggerSamplesFromInterval(trajectory, config.spatial_interval_mm);
        }
        output.trigger_events = BuildTriggerEvents(trigger_samples, config.pulse_per_mm);
    }

    output.use_hardware_trigger = config.use_hardware_trigger && !output.trigger_events.empty();
    return Result<ControllerOutput>::Success(output);
}

Result<ControllerOutput> DispensingController::Build(const std::vector<TrajectoryPoint>& trajectory,
                                                     const ControllerConfig& config) const noexcept {
    ControllerOutput output;
    if (trajectory.size() < 2) {
        return Result<ControllerOutput>::Success(output);
    }

    const float32 total_length = CalculateTotalLength(trajectory);
    const float32 total_time_ms = EstimateMotionTimeMs(trajectory, 0.0f);
    float32 fallback_velocity = 0.0f;
    if (total_time_ms > kEpsilon) {
        fallback_velocity = total_length / (total_time_ms / 1000.0f);
    }

    output.estimated_motion_time_ms =
        total_time_ms > kEpsilon ? total_time_ms : EstimateMotionTimeMs(trajectory, fallback_velocity);

    if (config.use_hardware_trigger) {
        std::vector<TriggerSample> trigger_samples;
        bool has_explicit = false;
        for (const auto& point : trajectory) {
            if (!point.enable_position_trigger) {
                continue;
            }
            AppendTriggerSample(trigger_samples, point.position, point.trigger_position_mm);
            has_explicit = true;
        }

        if (!has_explicit) {
            if (!config.trigger_distances_mm.empty()) {
                trigger_samples = BuildTriggerSamplesFromDistances(trajectory, config.trigger_distances_mm);
            } else if (config.spatial_interval_mm > kEpsilon) {
                trigger_samples = BuildTriggerSamplesFromInterval(trajectory, config.spatial_interval_mm);
            }
        }

        output.trigger_events = BuildTriggerEvents(trigger_samples, config.pulse_per_mm);
    }

    output.use_hardware_trigger = config.use_hardware_trigger && !output.trigger_events.empty();
    return Result<ControllerOutput>::Success(output);
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
