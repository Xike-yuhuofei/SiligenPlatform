/**
 * @file CMPCoordinatedInterpolator.cpp
 * @brief CMP位置触发协调插补算法实现
 *
 * @author Claude Code
 * @date 2025-10-30
 */

#define NOMINMAX
#define MODULE_NAME "CMPCoordinatedInterpolator"
#include "CMPCoordinatedInterpolator.h"

#include "../dispensing/domain-services/PositionTriggerController.h"
#include "CMPCompensation.h"
#include "CMPValidator.h"
#include "domain-services/TrajectoryPlanner.h"  // 必须包含完整定义以使用 unique_ptr
#include "domain-services/TriggerCalculator.h"  // 必须包含完整定义以使用 unique_ptr
#include "domain-services/TimeTrajectoryPlanner.h"
#include "value-objects/SemanticPath.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace Siligen::Domain::Motion {

using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;

namespace {
constexpr float32 kEpsilon = 1e-6f;

MotionTrajectory BuildUnifiedTrajectory(const std::vector<Point2D>& points, const InterpolationConfig& config) {
    MotionTrajectory empty;
    if (points.size() < 2) {
        return empty;
    }

    ValueObjects::SemanticPath path;
    path.segments.reserve(points.size() - 1);
    for (size_t i = 1; i < points.size(); ++i) {
        ValueObjects::SemanticSegment seg;
        seg.geometry.type = ValueObjects::SegmentType::Line;
        seg.geometry.line.start = points[i - 1];
        seg.geometry.line.end = points[i];
        seg.geometry.length = points[i - 1].DistanceTo(points[i]);
        seg.dispense_on = true;
        seg.flow_rate = 1.0f;
        seg.tag = ValueObjects::SemanticTag::Normal;
        if (seg.geometry.length > 1e-6f) {
            path.segments.push_back(seg);
        }
    }

    if (path.segments.empty()) {
        return empty;
    }

    ValueObjects::TimePlanningConfig plan_cfg;
    plan_cfg.vmax = config.max_velocity;
    plan_cfg.amax = config.max_acceleration;
    plan_cfg.jmax = config.max_jerk;
    plan_cfg.sample_dt = config.time_step;
    plan_cfg.sample_ds = 0.0f;
    plan_cfg.curvature_speed_factor = config.curvature_speed_factor;
    plan_cfg.corner_speed_factor = 1.0f;
    plan_cfg.start_speed_factor = 1.0f;
    plan_cfg.end_speed_factor = 1.0f;

    DomainServices::TimeTrajectoryPlanner planner;
    return planner.Plan(path, plan_cfg);
}

std::vector<TrajectoryPoint> ConvertToTrajectoryPoints(const MotionTrajectory& motion) {
    std::vector<TrajectoryPoint> out;
    out.reserve(motion.points.size());
    for (size_t i = 0; i < motion.points.size(); ++i) {
        const auto& pt = motion.points[i];
        TrajectoryPoint p;
        p.position = Point2D(pt.position.x, pt.position.y);
        p.timestamp = pt.t;
        p.sequence_id = static_cast<uint32>(i);
        p.velocity = std::sqrt(pt.velocity.x * pt.velocity.x + pt.velocity.y * pt.velocity.y);
        out.push_back(p);
    }
    return out;
}

Point2D ResolvePositionAtTime(const MotionTrajectory& motion, float32 trigger_time) {
    if (motion.points.empty()) {
        return Point2D();
    }

    size_t trajectory_index = 0;
    for (size_t j = 0; j < motion.points.size(); ++j) {
        if (motion.points[j].t >= trigger_time) {
            trajectory_index = j;
            break;
        }
    }
    const auto& pt = motion.points[trajectory_index];
    return Point2D(pt.position.x, pt.position.y);
}

}  // namespace

CMPCoordinatedInterpolator::CMPCoordinatedInterpolator()
    : TrajectoryInterpolatorBase(),
      m_trigger_calculator(std::make_unique<TriggerCalculator>()),
      m_trajectory_generator(std::make_unique<TrajectoryPlanner>()),
      m_validator(std::make_unique<CMPValidator>()),
      m_compensation(std::make_unique<CMPCompensation>()) {
    m_timeline_cache.reserve(10);
    m_trigger_cache.reserve(1000);
}

CMPCoordinatedInterpolator::~CMPCoordinatedInterpolator() = default;

std::vector<TrajectoryPoint> CMPCoordinatedInterpolator::CalculateInterpolation(const std::vector<Point2D>& points,
                                                                                const InterpolationConfig& config) {
    if (!ValidateParameters(points, config)) {
        return {};
    }

    // 使用默认的CMP配置进行位置同步插补
    CMPConfiguration default_config;
    default_config.trigger_mode = CMPTriggerMode::POSITION_SYNC;
    default_config.cmp_channel = 1;
    default_config.pulse_width_us = 2000;
    default_config.trigger_position_tolerance = 0.1f;
    default_config.time_tolerance_ms = 1.0f;
    default_config.enable_compensation = true;
    default_config.compensation_factor = 1.0f;
    default_config.enable_multi_channel = false;

    // 生成默认触发点（等间距分布）
    std::vector<DispensingTriggerPoint> trigger_points;
    float32 total_length = 0.0f;
    for (size_t i = 1; i < points.size(); ++i) {
        total_length += points[i - 1].DistanceTo(points[i]);
    }

    float32 trigger_spacing = (config.trigger_spacing_mm > kEpsilon)
                                  ? config.trigger_spacing_mm
                                  : 10.0f;  // 每10mm一个触发点
    int32 num_triggers = static_cast<int32>(std::floor(total_length / trigger_spacing));

    float32 current_distance = 0.0f;
    for (int32 i = 0; i < num_triggers; ++i) {
        current_distance += trigger_spacing;

        // 找到对应的路径点
        float32 accumulated_distance = 0.0f;
        Point2D trigger_position;

        for (size_t j = 1; j < points.size(); ++j) {
            float32 segment_length = points[j - 1].DistanceTo(points[j]);
            if (accumulated_distance + segment_length >= current_distance) {
                float32 ratio = (current_distance - accumulated_distance) / segment_length;
                trigger_position = points[j - 1] + (points[j] - points[j - 1]) * ratio;
                break;
            }
            accumulated_distance += segment_length;
        }

        DispensingTriggerPoint trigger_point;
        trigger_point.position = trigger_position;
        trigger_point.trigger_distance = current_distance;
        trigger_point.sequence_id = static_cast<uint32>(i);
        trigger_point.pulse_width_us = 2000;
        trigger_point.pre_trigger_delay_ms = 0.0f;
        trigger_point.post_trigger_delay_ms = 0.0f;
        trigger_point.is_enabled = true;

        trigger_points.push_back(trigger_point);
    }

    return PositionTriggeredDispensing(points, trigger_points, default_config, config);
}

std::vector<TrajectoryPoint> CMPCoordinatedInterpolator::PositionTriggeredDispensing(
    const std::vector<Point2D>& points,
    const std::vector<DispensingTriggerPoint>& trigger_points,
    const CMPConfiguration& cmp_config,
    const InterpolationConfig& config) {
    if (!ValidateCMPConfiguration(cmp_config)) {
        SILIGEN_LOG_ERROR("CMP配置验证失败");
        return {};
    }

    // 生成统一时间参数化轨迹
    MotionTrajectory motion_trajectory = BuildUnifiedTrajectory(points, config);
    std::vector<TrajectoryPoint> base_trajectory = ConvertToTrajectoryPoints(motion_trajectory);

    if (base_trajectory.empty()) {
        SILIGEN_LOG_ERROR("基础轨迹生成失败");
        return {};
    }

    // 计算触发时间线(统一时间参数化轨迹)
    TriggerTimeline timeline = m_trigger_calculator->CalculateTriggerTimeline(motion_trajectory,
                                                                             trigger_points,
                                                                             cmp_config);

    // 生成CMP协调轨迹
    return GenerateCMPCoordinatedTrajectory(base_trajectory, timeline, cmp_config);
}

std::vector<TrajectoryPoint> CMPCoordinatedInterpolator::TimeSynchronizedDispensing(const std::vector<Point2D>& points,
                                                                                    float32 dispensing_interval,
                                                                                    const CMPConfiguration& cmp_config,
                                                                                    const InterpolationConfig& config) {
    MotionTrajectory motion_trajectory = BuildUnifiedTrajectory(points, config);
    std::vector<TrajectoryPoint> base_trajectory = ConvertToTrajectoryPoints(motion_trajectory);

    if (base_trajectory.empty()) {
        return {};
    }

    // 计算时间触发分布
    std::vector<float32> trigger_times =
        m_trigger_calculator->CalculateTimeTriggerDistribution(motion_trajectory, dispensing_interval);

    // 构建触发时间线
    TriggerTimeline timeline;
    for (size_t i = 0; i < trigger_times.size(); ++i) {
        float32 trigger_time = trigger_times[i];

        timeline.trigger_times.push_back(trigger_time);
        timeline.trigger_positions.push_back(ResolvePositionAtTime(motion_trajectory, trigger_time));
        timeline.pulse_widths.push_back(cmp_config.pulse_width_us);
        timeline.cmp_channels.push_back(cmp_config.cmp_channel);
    }

    return GenerateCMPCoordinatedTrajectory(base_trajectory, timeline, cmp_config);
}

std::vector<TrajectoryPoint> CMPCoordinatedInterpolator::HybridTriggerDispensing(
    const std::vector<Point2D>& points,
    const std::vector<DispensingTriggerPoint>& trigger_points,
    float32 time_interval,
    const CMPConfiguration& cmp_config,
    const InterpolationConfig& config) {
    MotionTrajectory motion_trajectory = BuildUnifiedTrajectory(points, config);
    std::vector<TrajectoryPoint> base_trajectory = ConvertToTrajectoryPoints(motion_trajectory);

    // 计算位置触发时间线
    TriggerTimeline position_timeline = m_trigger_calculator->CalculateTriggerTimeline(motion_trajectory,
                                                                                      trigger_points,
                                                                                      cmp_config);

    // 计算时间触发分布
    std::vector<float32> time_trigger_times =
        m_trigger_calculator->CalculateTimeTriggerDistribution(motion_trajectory, time_interval);

    // 合并两种触发模式
    TriggerTimeline hybrid_timeline;

    // 添加位置触发点
    for (size_t i = 0; i < position_timeline.trigger_times.size(); ++i) {
        hybrid_timeline.trigger_times.push_back(position_timeline.trigger_times[i]);
        hybrid_timeline.trigger_positions.push_back(position_timeline.trigger_positions[i]);
        hybrid_timeline.pulse_widths.push_back(position_timeline.pulse_widths[i]);
        hybrid_timeline.cmp_channels.push_back(position_timeline.cmp_channels[i]);
    }

    // 添加时间触发点（避免重复）
    for (float32 time_trigger : time_trigger_times) {
        bool is_duplicate = false;
        for (float32 existing_time : hybrid_timeline.trigger_times) {
            if (std::abs(existing_time - time_trigger) < 0.001f) {
                is_duplicate = true;
                break;
            }
        }

        if (!is_duplicate) {
            hybrid_timeline.trigger_times.push_back(time_trigger);
            hybrid_timeline.trigger_positions.push_back(ResolvePositionAtTime(motion_trajectory, time_trigger));
            hybrid_timeline.pulse_widths.push_back(cmp_config.pulse_width_us);
            hybrid_timeline.cmp_channels.push_back(cmp_config.cmp_channel);
        }
    }

    // 按时间排序
    std::vector<size_t> indices(hybrid_timeline.trigger_times.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&hybrid_timeline](size_t i, size_t j) {
        return hybrid_timeline.trigger_times[i] < hybrid_timeline.trigger_times[j];
    });

    TriggerTimeline sorted_timeline;
    for (size_t idx : indices) {
        sorted_timeline.trigger_times.push_back(hybrid_timeline.trigger_times[idx]);
        sorted_timeline.trigger_positions.push_back(hybrid_timeline.trigger_positions[idx]);
        sorted_timeline.pulse_widths.push_back(hybrid_timeline.pulse_widths[idx]);
        sorted_timeline.cmp_channels.push_back(hybrid_timeline.cmp_channels[idx]);
    }

    return GenerateCMPCoordinatedTrajectory(base_trajectory, sorted_timeline, cmp_config);
}

std::vector<TrajectoryPoint> CMPCoordinatedInterpolator::PatternBasedDispensing(
    const std::vector<Point2D>& points,
    const std::string& pattern_type,
    const std::vector<float32>& pattern_params,
    const CMPConfiguration& cmp_config,
    const InterpolationConfig& config) {
    MotionTrajectory motion_trajectory = BuildUnifiedTrajectory(points, config);
    std::vector<TrajectoryPoint> base_trajectory = ConvertToTrajectoryPoints(motion_trajectory);

    std::vector<DispensingTriggerPoint> trigger_points;

    auto resolve_trigger_pos = [&](float32 distance, Point2D& out_pos) -> bool {
        if (distance <= 0.0f) {
            return false;
        }
        float32 accumulated = 0.0f;
        for (size_t j = 1; j < points.size(); ++j) {
            float32 segment_length = points[j - 1].DistanceTo(points[j]);
            if (accumulated + segment_length >= distance) {
                float32 ratio = (distance - accumulated) / segment_length;
                out_pos = points[j - 1] + (points[j] - points[j - 1]) * ratio;
                return true;
            }
            accumulated += segment_length;
        }
        return false;
    };

    float32 total_length = 0.0f;
    for (size_t i = 1; i < points.size(); ++i) {
        total_length += points[i - 1].DistanceTo(points[i]);
    }

    if (pattern_type == "uniform") {
        // 均匀分布模式
        if (!pattern_params.empty() && pattern_params[0] > 0.0f) {
            float32 spacing = pattern_params[0];
            int32 num_triggers = static_cast<int32>(std::floor(total_length / spacing));
            for (int32 i = 0; i < num_triggers; ++i) {
                float32 distance = (i + 1) * spacing;
                Point2D trigger_pos;
                if (!resolve_trigger_pos(distance, trigger_pos)) {
                    break;
                }
                DispensingTriggerPoint trigger;
                trigger.position = trigger_pos;
                trigger.trigger_distance = distance;
                trigger.sequence_id = static_cast<uint32>(i);
                trigger.pulse_width_us = 2000;
                trigger.is_enabled = true;
                trigger_points.push_back(trigger);
            }
        }
    } else if (pattern_type == "variable") {
        // 变间距模式: pattern_params 表示相邻触发点的间距序列
        float32 distance = 0.0f;
        uint32 seq = 0;
        for (float32 spacing : pattern_params) {
            if (spacing <= 0.0f) {
                continue;
            }
            distance += spacing;
            if (distance > total_length) {
                break;
            }
            Point2D trigger_pos;
            if (!resolve_trigger_pos(distance, trigger_pos)) {
                break;
            }
            DispensingTriggerPoint trigger;
            trigger.position = trigger_pos;
            trigger.trigger_distance = distance;
            trigger.sequence_id = seq++;
            trigger.pulse_width_us = 2000;
            trigger.is_enabled = true;
            trigger_points.push_back(trigger);
        }
    }

    return PositionTriggeredDispensing(points, trigger_points, cmp_config, config);
}

TriggerTimeline CMPCoordinatedInterpolator::CalculateTriggerTimeline(
    const std::vector<TrajectoryPoint>& trajectory,
    const std::vector<DispensingTriggerPoint>& trigger_points,
    const CMPConfiguration& cmp_config) const {
    return m_trigger_calculator->CalculateTriggerTimeline(trajectory, trigger_points, cmp_config);
}

Point2D CMPCoordinatedInterpolator::CompensateTriggerPosition(const Point2D& current_position,
                                                              const Point2D& target_position,
                                                              float32 current_velocity,
                                                              const CMPConfiguration& cmp_config) const {
    return m_compensation->CompensateTriggerPosition(current_position, target_position, current_velocity, cmp_config);
}

bool CMPCoordinatedInterpolator::ValidateCMPConfiguration(const CMPConfiguration& cmp_config) const {
    return m_validator->ValidateCMPConfiguration(cmp_config);
}

std::vector<DispensingTriggerPoint> CMPCoordinatedInterpolator::OptimizeTriggerDistribution(
    const std::vector<DispensingTriggerPoint>& trigger_points,
    const std::vector<TrajectoryPoint>& trajectory,
    float32 min_spacing) const {
    return m_trigger_calculator->OptimizeTriggerDistribution(trigger_points, trajectory, min_spacing);
}

std::vector<TrajectoryPoint> CMPCoordinatedInterpolator::GenerateCMPCoordinatedTrajectory(
    const std::vector<TrajectoryPoint>& base_trajectory,
    const TriggerTimeline& timeline,
    const CMPConfiguration& cmp_config) const {
    return m_trajectory_generator->GenerateCMPCoordinatedTrajectory(base_trajectory, timeline, cmp_config);
}

std::pair<int32, float32> CMPCoordinatedInterpolator::PredictTriggerPosition(
    const std::vector<TrajectoryPoint>& trajectory, float32 trigger_position, float32 prediction_time) const {
    return m_trajectory_generator->PredictTriggerPosition(trajectory, trigger_position, prediction_time);
}

std::vector<float32> CMPCoordinatedInterpolator::CalculateTimeTriggerDistribution(
    const std::vector<TrajectoryPoint>& trajectory, float32 interval) const {
    return m_trigger_calculator->CalculateTimeTriggerDistribution(trajectory, interval);
}

TriggerTimeline CMPCoordinatedInterpolator::CoordinateMultiChannelCMP(
    const TriggerTimeline& timeline, const std::vector<uint16>& channel_mapping) const {
    return m_trajectory_generator->CoordinateMultiChannelCMP(timeline, channel_mapping);
}

std::vector<float32> CMPCoordinatedInterpolator::AnalyzeTriggerAccuracy(const std::vector<TrajectoryPoint>& trajectory,
                                                                        const TriggerTimeline& timeline,
                                                                        const CMPConfiguration& cmp_config) const {
    return m_trajectory_generator->AnalyzeTriggerAccuracy(trajectory, timeline, cmp_config);
}

CMPConfiguration CMPCoordinatedInterpolator::AdjustTriggerParameters(float32 current_accuracy,
                                                                     float32 target_accuracy,
                                                                     const CMPConfiguration& cmp_config) const {
    return m_compensation->AdjustTriggerParameters(current_accuracy, target_accuracy, cmp_config);
}

// Private helper functions removed - now use m_trigger_calculator

}  // namespace Siligen::Domain::Motion

