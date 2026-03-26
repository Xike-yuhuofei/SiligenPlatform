#include "DispensingController.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Point2D;

namespace {
constexpr float32 kEpsilon = 1e-6f;

float32 SpeedMagnitude(const Siligen::Point3D& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

std::vector<Point2D> BuildTriggerPointsFromDistances(const MotionTrajectory& trajectory,
                                                     const std::vector<float32>& distances_mm) {
    std::vector<Point2D> points;
    if (trajectory.points.size() < 2 || distances_mm.empty()) {
        return points;
    }

    std::vector<float32> cumulative;
    cumulative.resize(trajectory.points.size(), 0.0f);
    for (size_t i = 1; i < trajectory.points.size(); ++i) {
        const auto& prev = trajectory.points[i - 1];
        const auto& curr = trajectory.points[i];
        float32 dist = prev.position.DistanceTo3D(curr.position);
        cumulative[i] = cumulative[i - 1] + dist;
    }

    std::vector<float32> sorted = distances_mm;
    std::sort(sorted.begin(), sorted.end());

    size_t idx = 1;
    for (float32 target : sorted) {
        while (idx < cumulative.size() && cumulative[idx] + kEpsilon < target) {
            ++idx;
        }
        if (idx >= cumulative.size()) {
            break;
        }

        const auto& prev = trajectory.points[idx - 1];
        const auto& curr = trajectory.points[idx];
        if (!prev.dispense_on || !curr.dispense_on) {
            continue;
        }

        float32 seg_len = cumulative[idx] - cumulative[idx - 1];
        float32 ratio = (seg_len > kEpsilon) ? (target - cumulative[idx - 1]) / seg_len : 0.0f;
        Point2D start(prev.position);
        Point2D end(curr.position);
        points.push_back(start + (end - start) * ratio);
    }

    return points;
}

std::vector<Point2D> BuildTriggerPointsFromDistances(const std::vector<TrajectoryPoint>& trajectory,
                                                     const std::vector<float32>& distances_mm) {
    std::vector<Point2D> points;
    if (trajectory.size() < 2 || distances_mm.empty()) {
        return points;
    }

    std::vector<float32> cumulative;
    cumulative.resize(trajectory.size(), 0.0f);
    for (size_t i = 1; i < trajectory.size(); ++i) {
        const auto& prev = trajectory[i - 1];
        const auto& curr = trajectory[i];
        float32 dist = prev.position.DistanceTo(curr.position);
        cumulative[i] = cumulative[i - 1] + dist;
    }

    std::vector<float32> sorted = distances_mm;
    std::sort(sorted.begin(), sorted.end());

    size_t idx = 1;
    for (float32 target : sorted) {
        while (idx < cumulative.size() && cumulative[idx] + kEpsilon < target) {
            ++idx;
        }
        if (idx >= cumulative.size()) {
            break;
        }

        float32 seg_len = cumulative[idx] - cumulative[idx - 1];
        float32 ratio = (seg_len > kEpsilon) ? (target - cumulative[idx - 1]) / seg_len : 0.0f;
        Point2D start(trajectory[idx - 1].position);
        Point2D end(trajectory[idx].position);
        points.push_back(start + (end - start) * ratio);
    }

    return points;
}

float32 EstimateMotionTimeMs(const std::vector<TrajectoryPoint>& trajectory,
                             float32 fallback_velocity) {
    if (trajectory.empty()) {
        return 0.0f;
    }

    float32 max_timestamp = 0.0f;
    for (const auto& pt : trajectory) {
        max_timestamp = std::max(max_timestamp, pt.timestamp);
    }
    if (max_timestamp > kEpsilon) {
        return max_timestamp * 1000.0f;
    }

    float32 length = 0.0f;
    for (size_t i = 1; i < trajectory.size(); ++i) {
        length += trajectory[i - 1].position.DistanceTo(trajectory[i].position);
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
    for (size_t i = 1; i < trajectory.size(); ++i) {
        length += trajectory[i - 1].position.DistanceTo(trajectory[i].position);
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

    output.interpolation.reserve(trajectory.points.size() - 1);

    for (size_t i = 1; i < trajectory.points.size(); ++i) {
        const auto& prev = trajectory.points[i - 1];
        const auto& curr = trajectory.points[i];
        InterpolationData data;
        data.type = InterpolationType::LINEAR;
        data.positions = {curr.position.x, curr.position.y};
        data.velocity = SpeedMagnitude(curr.velocity);
        data.acceleration = config.acceleration_mm_s2;
        float32 next_speed = (i + 1 < trajectory.points.size())
                                 ? SpeedMagnitude(trajectory.points[i + 1].velocity)
                                 : 0.0f;
        data.end_velocity = next_speed;
        output.interpolation.push_back(data);
    }

    output.estimated_motion_time_ms = trajectory.total_time * 1000.0f;

    bool allow_hardware_trigger = config.use_hardware_trigger;

    std::vector<Point2D> trigger_points;
    float32 interval_mm = config.spatial_interval_mm;
    if (allow_hardware_trigger) {
        if (!config.trigger_distances_mm.empty()) {
            trigger_points = BuildTriggerPointsFromDistances(trajectory, config.trigger_distances_mm);
        } else if (interval_mm > kEpsilon) {
            float32 accumulated = 0.0f;
            float32 next_trigger = interval_mm;
            for (size_t i = 1; i < trajectory.points.size(); ++i) {
                const auto& prev = trajectory.points[i - 1];
                const auto& curr = trajectory.points[i];
                if (!prev.dispense_on || !curr.dispense_on) {
                    accumulated = 0.0f;
                    next_trigger = interval_mm;
                    continue;
                }
                float32 dist = prev.position.DistanceTo3D(curr.position);
                accumulated += dist;
                if (accumulated + kEpsilon >= next_trigger) {
                    trigger_points.push_back(Point2D(curr.position));
                    next_trigger += interval_mm;
                }
            }
        }
    }

    if (!trigger_points.empty() && allow_hardware_trigger) {
        float32 x_min = trigger_points.front().x;
        float32 x_max = trigger_points.front().x;
        float32 y_min = trigger_points.front().y;
        float32 y_max = trigger_points.front().y;
        for (const auto& pt : trigger_points) {
            x_min = std::min(x_min, pt.x);
            x_max = std::max(x_max, pt.x);
            y_min = std::min(y_min, pt.y);
            y_max = std::max(y_max, pt.y);
        }
        float32 x_range = std::abs(x_max - x_min);
        float32 y_range = std::abs(y_max - y_min);
        output.trigger_axis = (x_range >= y_range) ? LogicalAxisId::X : LogicalAxisId::Y;

        output.trigger_positions.reserve(trigger_points.size());
        long last_pulse = std::numeric_limits<long>::min();
        for (const auto& pt : trigger_points) {
            float32 coord = (output.trigger_axis == LogicalAxisId::X) ? pt.x : pt.y;
            long pulse = static_cast<long>(std::llround(coord * config.pulse_per_mm));
            if (pulse == last_pulse) {
                continue;
            }
            output.trigger_positions.push_back(pulse);
            last_pulse = pulse;
        }

    }

    if (!output.trigger_positions.empty()) {
        output.use_hardware_trigger = true;
        return Result<ControllerOutput>::Success(output);
    }
    return Result<ControllerOutput>::Success(output);
}

Result<ControllerOutput> DispensingController::Build(const std::vector<TrajectoryPoint>& trajectory,
                                                     const ControllerConfig& config) const noexcept {
    ControllerOutput output;
    if (trajectory.size() < 2) {
        return Result<ControllerOutput>::Success(output);
    }

    float32 total_length = CalculateTotalLength(trajectory);
    float32 total_time_ms = EstimateMotionTimeMs(trajectory, 0.0f);
    float32 fallback_velocity = 0.0f;
    if (total_time_ms > kEpsilon) {
        fallback_velocity = total_length / (total_time_ms / 1000.0f);
    }

    output.interpolation.reserve(trajectory.size() - 1);
    for (size_t i = 1; i < trajectory.size(); ++i) {
        const auto& prev = trajectory[i - 1];
        const auto& curr = trajectory[i];
        InterpolationData data;
        data.type = InterpolationType::LINEAR;
        data.positions = {curr.position.x, curr.position.y};
        float32 velocity = curr.velocity;
        if (velocity <= kEpsilon) {
            float32 dt = curr.timestamp - prev.timestamp;
            if (dt > kEpsilon) {
                velocity = prev.position.DistanceTo(curr.position) / dt;
            } else {
                velocity = fallback_velocity;
            }
        }
        if (velocity < 0.0f) {
            velocity = 0.0f;
        }
        data.velocity = velocity;
        data.acceleration = config.acceleration_mm_s2;
        float32 next_speed = 0.0f;
        if (i + 1 < trajectory.size()) {
            next_speed = trajectory[i + 1].velocity;
            if (next_speed <= kEpsilon) {
                float32 dt = trajectory[i + 1].timestamp - curr.timestamp;
                if (dt > kEpsilon) {
                    next_speed = curr.position.DistanceTo(trajectory[i + 1].position) / dt;
                } else {
                    next_speed = fallback_velocity;
                }
            }
        }
        if (next_speed < 0.0f) {
            next_speed = 0.0f;
        }
        data.end_velocity = next_speed;
        output.interpolation.push_back(data);
    }

    output.estimated_motion_time_ms = (total_time_ms > kEpsilon)
                                          ? total_time_ms
                                          : EstimateMotionTimeMs(trajectory, fallback_velocity);

    bool allow_hardware_trigger = config.use_hardware_trigger;

    std::vector<Point2D> trigger_points;
    if (allow_hardware_trigger) {
        bool has_explicit = false;
        for (const auto& pt : trajectory) {
            if (pt.enable_position_trigger) {
                trigger_points.emplace_back(pt.position);
                has_explicit = true;
            }
        }
        if (!has_explicit) {
            if (!config.trigger_distances_mm.empty()) {
                trigger_points = BuildTriggerPointsFromDistances(trajectory, config.trigger_distances_mm);
            } else if (config.spatial_interval_mm > kEpsilon) {
                float32 accumulated = 0.0f;
                float32 next_trigger = config.spatial_interval_mm;
                for (size_t i = 1; i < trajectory.size(); ++i) {
                    float32 dist = trajectory[i - 1].position.DistanceTo(trajectory[i].position);
                    accumulated += dist;
                    if (accumulated + kEpsilon >= next_trigger) {
                        trigger_points.emplace_back(trajectory[i].position);
                        next_trigger += config.spatial_interval_mm;
                    }
                }
            }
        }
    }

    if (!trigger_points.empty() && allow_hardware_trigger) {
        float32 x_min = trigger_points.front().x;
        float32 x_max = trigger_points.front().x;
        float32 y_min = trigger_points.front().y;
        float32 y_max = trigger_points.front().y;
        for (const auto& pt : trigger_points) {
            x_min = std::min(x_min, pt.x);
            x_max = std::max(x_max, pt.x);
            y_min = std::min(y_min, pt.y);
            y_max = std::max(y_max, pt.y);
        }
        float32 x_range = std::abs(x_max - x_min);
        float32 y_range = std::abs(y_max - y_min);
        output.trigger_axis = (x_range >= y_range) ? LogicalAxisId::X : LogicalAxisId::Y;

        output.trigger_positions.reserve(trigger_points.size());
        long last_pulse = std::numeric_limits<long>::min();
        for (const auto& pt : trigger_points) {
            float32 coord = (output.trigger_axis == LogicalAxisId::X) ? pt.x : pt.y;
            long pulse = static_cast<long>(std::llround(coord * config.pulse_per_mm));
            if (pulse == last_pulse) {
                continue;
            }
            output.trigger_positions.push_back(pulse);
            last_pulse = pulse;
        }

    }

    if (!output.trigger_positions.empty()) {
        output.use_hardware_trigger = true;
        return Result<ControllerOutput>::Success(output);
    }
    return Result<ControllerOutput>::Success(output);
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
