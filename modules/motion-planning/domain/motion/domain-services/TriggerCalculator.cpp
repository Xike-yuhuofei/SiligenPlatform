/**
 * @file TriggerCalculator.cpp
 * @brief 触发计算器实现
 * @details 从CMPCoordinatedInterpolator中提取的触发计算逻辑
 *
 * @author Claude Code
 * @date 2025-11-23
 */

#define NOMINMAX
#include "TriggerCalculator.h"
#include "TriggerTimelineSort.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>

namespace Siligen::Domain::Motion {

std::vector<DispensingTriggerPoint> TriggerCalculator::CalculatePositionTriggerDistribution(
    const std::vector<Point2D>& points, float32 spacing) const {
    std::vector<DispensingTriggerPoint> trigger_points;

    if (points.empty() || spacing <= 0.0f) {
        return trigger_points;
    }

    float32 total_length = 0.0f;
    for (size_t i = 1; i < points.size(); ++i) {
        total_length += points[i - 1].DistanceTo(points[i]);
    }

    int32 num_triggers = static_cast<int32>(std::floor(total_length / spacing));
    if (num_triggers <= 0) {
        return trigger_points;
    }

    for (int32 i = 0; i < num_triggers; ++i) {
        float32 distance = (i + 1) * spacing;
        Point2D position = FindPathPosition(points, distance);

        DispensingTriggerPoint trigger;
        trigger.position = position;
        trigger.trigger_distance = distance;
        trigger.sequence_id = i + 1;
        trigger.pulse_width_us = 2000;  // 默认2ms
        trigger.pre_trigger_delay_ms = 0.0f;
        trigger.post_trigger_delay_ms = 0.0f;
        trigger.is_enabled = true;

        trigger_points.push_back(trigger);
    }

    return trigger_points;
}

std::vector<float32> TriggerCalculator::CalculateTimeTriggerDistribution(const std::vector<TrajectoryPoint>& trajectory,
                                                                         float32 interval) const {
    std::vector<float32> trigger_times;

    if (trajectory.empty() || interval <= 0.0f) {
        return trigger_times;
    }

    float32 total_time = trajectory.back().timestamp;
    float32 current_time = interval;

    while (current_time < total_time) {
        trigger_times.push_back(current_time);
        current_time += interval;
    }

    return trigger_times;
}

std::vector<float32> TriggerCalculator::CalculateTimeTriggerDistribution(
    const ValueObjects::MotionTrajectory& trajectory,
    float32 interval) const {
    std::vector<float32> trigger_times;
    if (trajectory.points.empty() || interval <= 0.0f) {
        return trigger_times;
    }

    float32 total_time = trajectory.points.back().t;
    float32 current_time = interval;
    while (current_time < total_time) {
        trigger_times.push_back(current_time);
        current_time += interval;
    }
    return trigger_times;
}

std::vector<DispensingTriggerPoint> TriggerCalculator::OptimizeTriggerDistribution(
    const std::vector<DispensingTriggerPoint>& trigger_points,
    const std::vector<TrajectoryPoint>& trajectory,
    float32 min_spacing) const {
    std::vector<DispensingTriggerPoint> optimized_triggers;

    if (trigger_points.empty()) {
        return optimized_triggers;
    }

    for (const auto& trigger : trigger_points) {
        if (!trigger.is_enabled) {
            continue;
        }

        if (optimized_triggers.empty()) {
            optimized_triggers.push_back(trigger);
            continue;
        }

        const auto& last_optimized = optimized_triggers.back();
        float32 distance = trigger.position.DistanceTo(last_optimized.position);

        if (distance >= min_spacing) {
            optimized_triggers.push_back(trigger);
        } else {
            // 合并触发点
            auto& merged = optimized_triggers.back();
            merged.trigger_distance = (last_optimized.trigger_distance + trigger.trigger_distance) * 0.5f;
            merged.pulse_width_us = std::max(last_optimized.pulse_width_us, trigger.pulse_width_us);
        }
    }

    return optimized_triggers;
}

TriggerTimeline TriggerCalculator::CalculateTriggerTimeline(const std::vector<TrajectoryPoint>& trajectory,
                                                            const std::vector<DispensingTriggerPoint>& trigger_points,
                                                            const CMPConfiguration& cmp_config) const {
    TriggerTimeline timeline;

    if (trajectory.empty() || trigger_points.empty()) {
        return timeline;
    }

    std::vector<float32> cumulative_distances = CalculateCumulativeDistance(trajectory);

    for (const auto& trigger_point : trigger_points) {
        if (!trigger_point.is_enabled) {
            continue;
        }

        float32 compensated_distance = trigger_point.trigger_distance;
        float32 compensated_time = InterpolateTriggerTime(trajectory, compensated_distance, cumulative_distances);

        timeline.trigger_times.push_back(compensated_time);
        timeline.trigger_positions.push_back(trigger_point.position);
        timeline.pulse_widths.push_back(trigger_point.pulse_width_us);
        timeline.cmp_channels.push_back(cmp_config.cmp_channel);
    }

    return SortTimelineByTime(timeline);
}

TriggerTimeline TriggerCalculator::CalculateTriggerTimeline(const ValueObjects::MotionTrajectory& trajectory,
                                                            const std::vector<DispensingTriggerPoint>& trigger_points,
                                                            const CMPConfiguration& cmp_config) const {
    TriggerTimeline timeline;

    if (trajectory.points.empty() || trigger_points.empty()) {
        return timeline;
    }

    std::vector<float32> cumulative_distances = CalculateCumulativeDistance(trajectory);

    for (const auto& trigger_point : trigger_points) {
        if (!trigger_point.is_enabled) {
            continue;
        }

        float32 compensated_distance = trigger_point.trigger_distance;
        float32 compensated_time = InterpolateTriggerTime(trajectory, compensated_distance, cumulative_distances);

        timeline.trigger_times.push_back(compensated_time);
        timeline.trigger_positions.push_back(trigger_point.position);
        timeline.pulse_widths.push_back(trigger_point.pulse_width_us);
        timeline.cmp_channels.push_back(cmp_config.cmp_channel);
    }

    return SortTimelineByTime(timeline);
}

std::optional<int32> TriggerCalculator::FindClosestTrajectoryPoint(const std::vector<TrajectoryPoint>& trajectory,
                                                                   const Point2D& target_position) const {
    if (trajectory.empty()) {
        return std::nullopt;
    }

    int32 closest_index = 0;
    float32 min_distance = std::numeric_limits<float32>::max();

    for (size_t i = 0; i < trajectory.size(); ++i) {
        float32 distance = trajectory[i].position.DistanceTo(target_position);
        if (distance < min_distance) {
            min_distance = distance;
            closest_index = static_cast<int32>(i);
        }
    }

    return closest_index;
}

std::optional<int32> TriggerCalculator::FindClosestTrajectoryPoint(const ValueObjects::MotionTrajectory& trajectory,
                                                                   const Point2D& target_position) const {
    if (trajectory.points.empty()) {
        return std::nullopt;
    }

    int32 closest_index = 0;
    float32 min_distance = std::numeric_limits<float32>::max();

    for (size_t i = 0; i < trajectory.points.size(); ++i) {
        float32 distance = Point2D(trajectory.points[i].position).DistanceTo(target_position);
        if (distance < min_distance) {
            min_distance = distance;
            closest_index = static_cast<int32>(i);
        }
    }

    return closest_index;
}

std::vector<float32> TriggerCalculator::CalculateCumulativeDistance(
    const std::vector<TrajectoryPoint>& trajectory) const {
    std::vector<float32> cumulative_distances;

    if (trajectory.empty()) {
        return cumulative_distances;
    }

    cumulative_distances.reserve(trajectory.size());
    cumulative_distances.push_back(0.0f);

    for (size_t i = 1; i < trajectory.size(); ++i) {
        float32 segment_distance = trajectory[i - 1].position.DistanceTo(trajectory[i].position);
        cumulative_distances.push_back(cumulative_distances.back() + segment_distance);
    }

    return cumulative_distances;
}

std::vector<float32> TriggerCalculator::CalculateCumulativeDistance(
    const ValueObjects::MotionTrajectory& trajectory) const {
    std::vector<float32> cumulative_distances;
    if (trajectory.points.empty()) {
        return cumulative_distances;
    }

    cumulative_distances.reserve(trajectory.points.size());
    cumulative_distances.push_back(0.0f);

    for (size_t i = 1; i < trajectory.points.size(); ++i) {
        float32 segment_distance = trajectory.points[i - 1].position.DistanceTo3D(trajectory.points[i].position);
        cumulative_distances.push_back(cumulative_distances.back() + segment_distance);
    }

    return cumulative_distances;
}

float32 TriggerCalculator::InterpolateTriggerTime(const std::vector<TrajectoryPoint>& trajectory,
                                                  float32 target_distance,
                                                  const std::vector<float32>& cumulative_distances) const {
    if (trajectory.empty() || cumulative_distances.empty()) {
        return 0.0f;
    }

    // 边界检查
    if (target_distance <= cumulative_distances.front()) {
        return trajectory.front().timestamp;
    }
    if (target_distance >= cumulative_distances.back()) {
        return trajectory.back().timestamp;
    }

    // 找到目标距离所在的线段
    for (size_t i = 1; i < cumulative_distances.size(); ++i) {
        if (target_distance <= cumulative_distances[i]) {
            float32 ratio = (target_distance - cumulative_distances[i - 1]) /
                            (cumulative_distances[i] - cumulative_distances[i - 1]);

            float32 time_diff = trajectory[i].timestamp - trajectory[i - 1].timestamp;
            return trajectory[i - 1].timestamp + ratio * time_diff;
        }
    }

    return trajectory.back().timestamp;
}

float32 TriggerCalculator::InterpolateTriggerTime(const ValueObjects::MotionTrajectory& trajectory,
                                                  float32 target_distance,
                                                  const std::vector<float32>& cumulative_distances) const {
    if (trajectory.points.empty() || cumulative_distances.empty()) {
        return 0.0f;
    }

    if (target_distance <= cumulative_distances.front()) {
        return trajectory.points.front().t;
    }
    if (target_distance >= cumulative_distances.back()) {
        return trajectory.points.back().t;
    }

    for (size_t i = 1; i < cumulative_distances.size(); ++i) {
        if (target_distance <= cumulative_distances[i]) {
            float32 ratio = (target_distance - cumulative_distances[i - 1]) /
                            (cumulative_distances[i] - cumulative_distances[i - 1]);
            float32 time_diff = trajectory.points[i].t - trajectory.points[i - 1].t;
            return trajectory.points[i - 1].t + ratio * time_diff;
        }
    }

    return trajectory.points.back().t;
}

Point2D TriggerCalculator::FindPathPosition(const std::vector<Point2D>& points, float32 target_distance) const {
    if (points.empty()) {
        return Point2D();
    }

    float32 accumulated = 0.0f;

    for (size_t j = 1; j < points.size(); ++j) {
        float32 segment_length = points[j - 1].DistanceTo(points[j]);

        if (accumulated + segment_length >= target_distance) {
            float32 ratio = (target_distance - accumulated) / segment_length;
            return Point2D(points[j - 1].x + ratio * (points[j].x - points[j - 1].x),
                           points[j - 1].y + ratio * (points[j].y - points[j - 1].y));
        }

        accumulated += segment_length;
    }

    return points.back();
}

}  // namespace Siligen::Domain::Motion
