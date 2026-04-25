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

#include "CMPCompensation.h"
#include "CMPValidator.h"
#include "domain-services/TrajectoryPlanner.h"  // 必须包含完整定义以使用 unique_ptr
#include "domain-services/TriggerCalculator.h"  // 必须包含完整定义以使用 unique_ptr
#include "domain-services/TimeTrajectoryPlanner.h"
#include "domain-services/TriggerTimelineSort.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace Siligen::Domain::Motion {

using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;
using Siligen::Domain::Motion::ValueObjects::TimePlanningConfig;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::ProcessPath::Contracts::ProcessSegment;
using Siligen::ProcessPath::Contracts::ProcessTag;
using Siligen::ProcessPath::Contracts::SegmentType;

namespace {
constexpr float32 kEpsilon = 1e-6f;

TimePlanningConfig BuildTimePlanningConfig(const InterpolationConfig& config) {
    TimePlanningConfig motion_config;
    motion_config.vmax = config.max_velocity;
    motion_config.amax = config.max_acceleration;
    motion_config.jmax = config.max_jerk;
    motion_config.sample_dt = config.time_step;
    motion_config.sample_ds = 0.0f;
    motion_config.curvature_speed_factor = config.curvature_speed_factor;
    motion_config.corner_speed_factor = 1.0f;
    motion_config.start_speed_factor = 1.0f;
    motion_config.end_speed_factor = 1.0f;
    motion_config.rapid_speed_factor = 1.0f;
    motion_config.arc_tolerance_mm = std::max(config.position_tolerance, 0.0f);
    motion_config.enforce_jerk_limit = true;
    return motion_config;
}

CMPConfiguration BuildValidationConfig(const CMPConfiguration& cmp_config,
                                       const std::vector<DispensingTriggerPoint>& trigger_points) {
    CMPConfiguration effective_config = cmp_config;
    if (!effective_config.trigger_points.empty() || effective_config.trigger_mode == CMPTriggerMode::RANGE) {
        return effective_config;
    }

    effective_config.trigger_points.reserve(trigger_points.size());
    for (const auto& trigger_point : trigger_points) {
        CMPTriggerPoint cmp_trigger;
        const long long rounded_distance =
            std::llround(static_cast<double>(std::max(trigger_point.trigger_distance, 0.0f)));
        cmp_trigger.position = static_cast<int32>(std::clamp<long long>(rounded_distance, 0, 1000000));
        cmp_trigger.action = DispensingAction::PULSE;
        cmp_trigger.pulse_width_us = static_cast<int32>(trigger_point.pulse_width_us);
        cmp_trigger.delay_time_us =
            static_cast<int32>(std::max(trigger_point.pre_trigger_delay_ms, 0.0f) * 1000.0f);
        cmp_trigger.is_enabled = trigger_point.is_enabled;
        effective_config.trigger_points.push_back(cmp_trigger);
    }

    return effective_config;
}

ProcessPath BuildLinearProcessPath(const std::vector<Point2D>& points) {
    ProcessPath path;
    if (points.size() < 2) {
        return path;
    }

    path.segments.reserve(points.size() - 1);
    for (size_t i = 1; i < points.size(); ++i) {
        ProcessSegment seg;
        seg.dispense_on = true;
        seg.flow_rate = 1.0f;
        seg.tag = ProcessTag::Normal;
        seg.geometry.type = SegmentType::Line;
        seg.geometry.line.start = points[i - 1];
        seg.geometry.line.end = points[i];
        seg.geometry.length = points[i - 1].DistanceTo(points[i]);
        if (seg.geometry.length > kEpsilon) {
            path.segments.push_back(seg);
        }
    }

    return path;
}

MotionTrajectory BuildUnifiedTrajectory(const ProcessPath& path, const InterpolationConfig& config) {
    if (path.segments.empty()) {
        return {};
    }

    DomainServices::TimeTrajectoryPlanner planner;
    return planner.Plan(path, BuildTimePlanningConfig(config));
}

MotionTrajectory BuildUnifiedTrajectory(const std::vector<Point2D>& points, const InterpolationConfig& config) {
    return BuildUnifiedTrajectory(BuildLinearProcessPath(points), config);
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

    if (trigger_time <= motion.points.front().t + kEpsilon) {
        const auto& pt = motion.points.front();
        return Point2D(pt.position.x, pt.position.y);
    }
    if (trigger_time >= motion.points.back().t - kEpsilon) {
        const auto& pt = motion.points.back();
        return Point2D(pt.position.x, pt.position.y);
    }

    for (size_t j = 1; j < motion.points.size(); ++j) {
        const auto& prev = motion.points[j - 1];
        const auto& curr = motion.points[j];
        if (trigger_time <= curr.t + kEpsilon) {
            const float32 dt = curr.t - prev.t;
            if (std::abs(dt) <= kEpsilon) {
                return Point2D(curr.position.x, curr.position.y);
            }

            const float32 ratio = std::clamp((trigger_time - prev.t) / dt, 0.0f, 1.0f);
            return Point2D(static_cast<float32>(prev.position.x + (curr.position.x - prev.position.x) * ratio),
                           static_cast<float32>(prev.position.y + (curr.position.y - prev.position.y) * ratio));
        }
    }

    const auto& pt = motion.points.back();
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
    SILIGEN_LOG_ERROR("CMP插补必须显式提供离散 trigger 列表，禁止退化为默认等间距触发");
    return {};
}

std::vector<TrajectoryPoint> CMPCoordinatedInterpolator::PositionTriggeredDispensing(
    const std::vector<Point2D>& points,
    const std::vector<DispensingTriggerPoint>& trigger_points,
    const CMPConfiguration& cmp_config,
    const InterpolationConfig& config) {
    const CMPConfiguration effective_config = BuildValidationConfig(cmp_config, trigger_points);
    if (!ValidateCMPConfiguration(effective_config)) {
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
                                                                             effective_config);

    // 生成CMP协调轨迹
    return GenerateCMPCoordinatedTrajectory(base_trajectory, timeline, effective_config);
}

std::vector<TrajectoryPoint> CMPCoordinatedInterpolator::PositionTriggeredDispensing(
    const ProcessPath& path,
    const std::vector<DispensingTriggerPoint>& trigger_points,
    const CMPConfiguration& cmp_config,
    const InterpolationConfig& config) {
    const CMPConfiguration effective_config = BuildValidationConfig(cmp_config, trigger_points);
    if (!ValidateCMPConfiguration(effective_config)) {
        SILIGEN_LOG_ERROR("CMP配置验证失败");
        return {};
    }

    MotionTrajectory motion_trajectory = BuildUnifiedTrajectory(path, config);
    std::vector<TrajectoryPoint> base_trajectory = ConvertToTrajectoryPoints(motion_trajectory);

    if (base_trajectory.empty()) {
        SILIGEN_LOG_ERROR("基础轨迹生成失败");
        return {};
    }

    TriggerTimeline timeline = m_trigger_calculator->CalculateTriggerTimeline(motion_trajectory,
                                                                             trigger_points,
                                                                             effective_config);

    return GenerateCMPCoordinatedTrajectory(base_trajectory, timeline, effective_config);
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
    auto sorted_timeline = SortTimelineByTime(hybrid_timeline);

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

// Private helper functions removed - now use m_trigger_calculator

}  // namespace Siligen::Domain::Motion

