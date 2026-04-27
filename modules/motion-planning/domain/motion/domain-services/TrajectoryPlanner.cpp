/**
 * @file TrajectoryPlanner.cpp
 * @brief 轨迹规划器实现
 * @details 从CMPCoordinatedInterpolator中提取的轨迹规划逻辑
 *
 * @author Claude Code
 * @date 2025-11-23
 * @update 2026-01-16 (重构阶段2: 重命名为TrajectoryPlanner)
 */

#define NOMINMAX
#include "TrajectoryPlanner.h"
#include "TriggerTimelineSort.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace Siligen::Domain::Motion {

namespace {
constexpr float32 kInterpolationEpsilon = 1e-6f;

TrajectoryPoint InterpolateTrajectoryPoint(const TrajectoryPoint& prev,
                                           const TrajectoryPoint& next,
                                           float32 target_time,
                                           const Point2D& target_position) {
    TrajectoryPoint point = prev;
    const float32 dt = next.timestamp - prev.timestamp;
    const float32 ratio = (std::abs(dt) <= kInterpolationEpsilon)
                              ? 0.0f
                              : std::clamp((target_time - prev.timestamp) / dt, 0.0f, 1.0f);

    point.position = target_position;
    point.timestamp = target_time;
    point.velocity = prev.velocity + (next.velocity - prev.velocity) * ratio;
    point.acceleration = prev.acceleration + (next.acceleration - prev.acceleration) * ratio;
    point.enable_position_trigger = false;
    point.trigger_pulse_width_us = 0;
    return point;
}

void ApplyTrigger(TrajectoryPoint& point, uint32 pulse_width, const Point2D* exact_position = nullptr) {
    if (exact_position != nullptr) {
        point.position = *exact_position;
    }
    point.enable_position_trigger = true;
    point.trigger_pulse_width_us = static_cast<uint16>(std::max<uint32>(point.trigger_pulse_width_us, pulse_width));
}
}  // namespace

std::vector<TrajectoryPoint> TrajectoryPlanner::GenerateCMPCoordinatedTrajectory(
    const std::vector<TrajectoryPoint>& base_trajectory,
    const TriggerTimeline& timeline,
    const CMPConfiguration& cmp_config) const {
    (void)cmp_config;
    std::vector<TrajectoryPoint> cmp_trajectory = base_trajectory;
    cmp_trajectory.reserve(base_trajectory.size() + timeline.trigger_times.size());

    if (base_trajectory.empty() || timeline.trigger_times.empty()) {
        // Domain层不应包含日志记录（违反架构规则DOMAIN_NO_IO_OR_THREADING）
        return cmp_trajectory;
    }

    // 在精确触发时间插入轨迹点，避免最终胶点被量化到最近采样点。
    for (size_t i = 0; i < timeline.trigger_times.size(); ++i) {
        float32 trigger_time = timeline.trigger_times[i];
        const Point2D& trigger_position = timeline.trigger_positions[i];
        const uint32 pulse_width = timeline.pulse_widths[i];

        if (trigger_time <= cmp_trajectory.front().timestamp + kInterpolationEpsilon) {
            ApplyTrigger(cmp_trajectory.front(), pulse_width, &trigger_position);
            continue;
        }
        if (trigger_time >= cmp_trajectory.back().timestamp - kInterpolationEpsilon) {
            ApplyTrigger(cmp_trajectory.back(), pulse_width, &trigger_position);
            continue;
        }

        auto upper = std::lower_bound(cmp_trajectory.begin(),
                                      cmp_trajectory.end(),
                                      trigger_time,
                                      [](const TrajectoryPoint& point, float32 time) {
                                          return point.timestamp < time;
                                      });

        if (upper == cmp_trajectory.end()) {
            ApplyTrigger(cmp_trajectory.back(), pulse_width, &trigger_position);
            continue;
        }

        if (std::abs(upper->timestamp - trigger_time) <= kInterpolationEpsilon) {
            ApplyTrigger(*upper, pulse_width, &trigger_position);
            continue;
        }

        if (upper == cmp_trajectory.begin()) {
            ApplyTrigger(cmp_trajectory.front(), pulse_width, &trigger_position);
            continue;
        }

        auto prev = upper - 1;
        if (std::abs(prev->timestamp - trigger_time) <= kInterpolationEpsilon) {
            ApplyTrigger(*prev, pulse_width, &trigger_position);
            continue;
        }

        TrajectoryPoint exact_trigger = InterpolateTrajectoryPoint(*prev, *upper, trigger_time, trigger_position);
        ApplyTrigger(exact_trigger, pulse_width);
        cmp_trajectory.insert(upper, exact_trigger);
    }

    for (size_t i = 0; i < cmp_trajectory.size(); ++i) {
        cmp_trajectory[i].sequence_id = static_cast<uint32>(i);
    }

    return cmp_trajectory;
}

TriggerTimeline TrajectoryPlanner::GenerateHybridTimeline(const TriggerTimeline& position_timeline,
                                                            const TriggerTimeline& time_timeline) const {
    TriggerTimeline hybrid_timeline;

    // 添加位置触发点
    for (size_t i = 0; i < position_timeline.trigger_times.size(); ++i) {
        hybrid_timeline.trigger_times.push_back(position_timeline.trigger_times[i]);
        hybrid_timeline.trigger_positions.push_back(position_timeline.trigger_positions[i]);
        hybrid_timeline.pulse_widths.push_back(position_timeline.pulse_widths[i]);
        hybrid_timeline.cmp_channels.push_back(position_timeline.cmp_channels[i]);
    }

    // 添加时间触发点（检查重复）
    for (size_t i = 0; i < time_timeline.trigger_times.size(); ++i) {
        float32 time_trigger = time_timeline.trigger_times[i];

        if (!IsDuplicateTrigger(hybrid_timeline.trigger_times, time_trigger, 0.001f)) {
            hybrid_timeline.trigger_times.push_back(time_trigger);
            hybrid_timeline.trigger_positions.push_back(time_timeline.trigger_positions[i]);
            hybrid_timeline.pulse_widths.push_back(time_timeline.pulse_widths[i]);
            hybrid_timeline.cmp_channels.push_back(time_timeline.cmp_channels[i]);
        }
    }

    // 按时间排序
    return SortTimelineByTime(hybrid_timeline);
}

TriggerTimeline TrajectoryPlanner::CoordinateMultiChannelCMP(const TriggerTimeline& timeline,
                                                               const std::vector<uint16>& channel_mapping) const {
    TriggerTimeline multi_timeline = timeline;

    if (channel_mapping.empty()) {
        return multi_timeline;
    }

    // 更新CMP通道映射
    for (size_t i = 0; i < multi_timeline.cmp_channels.size(); ++i) {
        uint16 original_channel = multi_timeline.cmp_channels[i];

        // 如果原通道在映射范围内，则使用映射后的通道
        if (original_channel > 0 && original_channel <= channel_mapping.size()) {
            multi_timeline.cmp_channels[i] = channel_mapping[original_channel - 1];
        }
    }

    return multi_timeline;
}

std::vector<float32> TrajectoryPlanner::AnalyzeTriggerAccuracy(const std::vector<TrajectoryPoint>& trajectory,
                                                                 const TriggerTimeline& timeline,
                                                                 const CMPConfiguration& cmp_config) const {
    std::vector<float32> accuracy_results;

    if (trajectory.empty() || timeline.trigger_times.empty()) {
        // Domain层不应包含日志记录
        return accuracy_results;
    }

    for (size_t i = 0; i < timeline.trigger_times.size(); ++i) {
        float32 target_time = timeline.trigger_times[i];
        Point2D target_position = timeline.trigger_positions[i];

        // 查找最接近的轨迹点
        int32 closest_index = 0;
        float32 min_distance = std::numeric_limits<float32>::max();

        for (size_t j = 0; j < trajectory.size(); ++j) {
            float32 distance = trajectory[j].position.DistanceTo(target_position);
            if (distance < min_distance) {
                min_distance = distance;
                closest_index = static_cast<int32>(j);
            }
        }

        // 计算精度
        if (closest_index >= 0 && closest_index < static_cast<int32>(trajectory.size())) {
            float32 actual_time = trajectory[closest_index].timestamp;
            float32 time_error = CalculateTimeError(actual_time, target_time);
            float32 position_error = trajectory[closest_index].position.DistanceTo(target_position);

            // 综合精度（时间和位置误差的加权平均）
            float32 accuracy = std::sqrt(time_error * time_error + position_error * position_error);
            accuracy_results.push_back(accuracy);
        }
    }

    return accuracy_results;
}

std::pair<int32, float32> TrajectoryPlanner::PredictTriggerPosition(const std::vector<TrajectoryPoint>& trajectory,
                                                                      float32 trigger_position,
                                                                      float32 prediction_time) const {
    if (trajectory.empty()) {
        return {-1, 0.0f};
    }

    // 简化的预测：查找最接近目标位置的点
    int32 closest_index = 0;
    float32 min_distance = std::numeric_limits<float32>::max();

    for (size_t i = 0; i < trajectory.size(); ++i) {
        float32 distance = std::abs(trajectory[i].position.x - trigger_position);
        if (distance < min_distance) {
            min_distance = distance;
            closest_index = static_cast<int32>(i);
        }
    }

    if (closest_index >= 0 && closest_index < static_cast<int32>(trajectory.size())) {
        float32 predicted_time = trajectory[closest_index].timestamp + prediction_time;
        return {closest_index, predicted_time};
    }

    return {-1, 0.0f};
}

bool TrajectoryPlanner::IsDuplicateTrigger(const std::vector<float32>& trigger_times,
                                             float32 new_time,
                                             float32 tolerance) const {
    for (float32 existing_time : trigger_times) {
        if (std::abs(existing_time - new_time) <= tolerance) {
            return true;
        }
    }
    return false;
}

float32 TrajectoryPlanner::CalculateTimeError(float32 actual_time, float32 target_time) const {
    return std::abs(actual_time - target_time) * 1000.0f;  // 转换为毫秒
}

}  // namespace Siligen::Domain::Motion
