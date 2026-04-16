/**
 * @file ArcTriggerPointCalculator.cpp
 * @brief 圆弧触发点计算器实现
 */

#include "ArcTriggerPointCalculator.h"
#include "domain/dispensing/ports/IValvePort.h"
#include "shared/types/Error.h"

#include <cmath>
#include <limits>

namespace Siligen {
namespace Domain {
namespace Dispensing {
namespace DomainServices {

using namespace Siligen::Shared::Types;

namespace {
constexpr float32 kEpsilon = 1e-6f;
constexpr float32 kDegToRad = 3.14159265359f / 180.0f;
constexpr uint32 kMaxTriggerCount = static_cast<uint32>(
    Siligen::Domain::Dispensing::Ports::PositionTriggeredDispenserParams::kMaxTriggerEvents);

float32 NormalizeAngleSweep(float32 diff) {
    float32 mod = std::fmod(diff, 360.0f);
    if (mod < 0.0f) {
        mod += 360.0f;
    }
    if (std::abs(mod) <= kEpsilon && std::abs(diff) > kEpsilon) {
        return 360.0f;
    }
    return mod;
}

float32 ComputeArcSweep(float32 start_angle, float32 end_angle, bool clockwise) {
    float32 diff = clockwise ? (start_angle - end_angle) : (end_angle - start_angle);
    return NormalizeAngleSweep(diff);
}

float32 StepArcAngle(float32 start_angle, float32 sweep, bool clockwise, float32 ratio) {
    float32 delta = sweep * ratio;
    return clockwise ? (start_angle - delta) : (start_angle + delta);
}

std::vector<long> BuildTriggerPositions(const DXFSegment& segment,
                                        uint32 trigger_count,
                                        LogicalAxisId axis,
                                        float32 pulse_per_mm,
                                        float32 arc_sweep) {
    std::vector<long> positions;
    if (trigger_count == 0 || pulse_per_mm <= kEpsilon) {
        return positions;
    }

    positions.reserve(trigger_count);
    long last_pulse = std::numeric_limits<long>::min();
    for (uint32 i = 0; i < trigger_count; ++i) {
        float32 t = (i + 1.0f) / (trigger_count + 1.0f);
        float32 angle = StepArcAngle(segment.start_angle, arc_sweep, segment.clockwise, t);
        float32 angle_rad = angle * kDegToRad;
        float32 x = segment.center_point.x + segment.radius * std::cos(angle_rad);
        float32 y = segment.center_point.y + segment.radius * std::sin(angle_rad);
        float32 coord = (axis == LogicalAxisId::X) ? x : y;  // 0-based: X=0, Y=1

        long pulse_position = static_cast<long>(coord * pulse_per_mm);
        if (pulse_position == last_pulse) {
            continue;
        }
        positions.push_back(pulse_position);
        last_pulse = pulse_position;
    }
    return positions;
}

enum class PulseDirection {
    UNKNOWN,
    INCREASING,
    DECREASING,
    MIXED
};

PulseDirection AnalyzePulseDirection(const std::vector<long>& pulses) {
    if (pulses.empty()) {
        return PulseDirection::UNKNOWN;
    }
    if (pulses.size() == 1) {
        return PulseDirection::INCREASING;
    }

    int direction = 0;
    long prev = pulses.front();
    for (size_t i = 1; i < pulses.size(); ++i) {
        long diff = pulses[i] - prev;
        if (diff == 0) {
            continue;
        }
        int current = (diff > 0) ? 1 : -1;
        if (direction == 0) {
            direction = current;
        } else if (direction != current) {
            return PulseDirection::MIXED;
        }
        prev = pulses[i];
    }
    if (direction == 0) {
        return PulseDirection::INCREASING;
    }
    return (direction > 0) ? PulseDirection::INCREASING : PulseDirection::DECREASING;
}

bool IsMonotonic(PulseDirection direction) {
    return direction == PulseDirection::INCREASING || direction == PulseDirection::DECREASING;
}

long PulseRange(const std::vector<long>& pulses) {
    if (pulses.empty()) {
        return 0;
    }
    long range = pulses.back() - pulses.front();
    return (range < 0) ? -range : range;
}
}

Result<ArcTriggerResult> ArcTriggerPointCalculator::Calculate(
    const DXFSegment& segment,
    float32 spatial_interval_mm,
    float32 pulse_per_mm) noexcept {

    if (spatial_interval_mm <= kEpsilon || pulse_per_mm <= kEpsilon) {
        return Result<ArcTriggerResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "圆弧触发参数无效", "ArcTriggerPointCalculator"));
    }
    if (segment.radius <= kEpsilon) {
        return Result<ArcTriggerResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "圆弧半径无效", "ArcTriggerPointCalculator"));
    }

    // 1. 计算圆弧长度
    float32 arc_sweep = ComputeArcSweep(segment.start_angle, segment.end_angle, segment.clockwise);
    float32 arc_length = segment.radius * arc_sweep * kDegToRad;
    if (arc_length <= kEpsilon) {
        return Result<ArcTriggerResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "圆弧长度无效", "ArcTriggerPointCalculator"));
    }

    // 2. 计算触发点数量
    uint32 trigger_count = static_cast<uint32>(arc_length / spatial_interval_mm);
    if (trigger_count == 0) {
        trigger_count = 1;  // 至少触发一次
    }
    if (trigger_count > kMaxTriggerCount) {
        trigger_count = kMaxTriggerCount;
    }

    // 3. 生成候选轴触发序列并选择触发轴 (0-based: X=0, Y=1)
    auto positions_x = BuildTriggerPositions(segment, trigger_count, LogicalAxisId::X, pulse_per_mm, arc_sweep);
    auto positions_y = BuildTriggerPositions(segment, trigger_count, LogicalAxisId::Y, pulse_per_mm, arc_sweep);

    auto dir_x = AnalyzePulseDirection(positions_x);
    auto dir_y = AnalyzePulseDirection(positions_y);
    bool mono_x = IsMonotonic(dir_x) && !positions_x.empty();
    bool mono_y = IsMonotonic(dir_y) && !positions_y.empty();

    // 选择触发轴 (0-based: X=0, Y=1)
    LogicalAxisId trigger_axis = LogicalAxisId::INVALID;
    if (mono_x && !mono_y) {
        trigger_axis = LogicalAxisId::X;
    } else if (mono_y && !mono_x) {
        trigger_axis = LogicalAxisId::Y;
    } else if (mono_x && mono_y) {
        long range_x = PulseRange(positions_x);
        long range_y = PulseRange(positions_y);
        if (range_x == range_y) {
            trigger_axis = SelectOptimalAxis(segment);
        } else {
            trigger_axis = (range_x >= range_y) ? LogicalAxisId::X : LogicalAxisId::Y;
        }
    } else {
        trigger_axis = SelectOptimalAxis(segment);
    }

    if (trigger_axis == LogicalAxisId::X && positions_x.empty() && !positions_y.empty()) {
        trigger_axis = LogicalAxisId::Y;
    } else if (trigger_axis == LogicalAxisId::Y && positions_y.empty() && !positions_x.empty()) {
        trigger_axis = LogicalAxisId::X;
    }

    std::vector<long> trigger_positions =
        (trigger_axis == LogicalAxisId::X) ? std::move(positions_x) : std::move(positions_y);

    if (trigger_positions.empty()) {
        return Result<ArcTriggerResult>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "圆弧触发点为空", "ArcTriggerPointCalculator"));
    }

    // 4. 构造结果
    ArcTriggerResult result;
    result.trigger_positions = std::move(trigger_positions);
    result.trigger_axis = trigger_axis;

    return Result<ArcTriggerResult>::Success(result);
}

LogicalAxisId ArcTriggerPointCalculator::SelectOptimalAxis(const DXFSegment& segment) noexcept {
    // 启发式：选择起点到终点变化范围更大的轴
    float32 x_range = std::abs(segment.end_point.x - segment.start_point.x);
    float32 y_range = std::abs(segment.end_point.y - segment.start_point.y);

    return (x_range >= y_range) ? LogicalAxisId::X : LogicalAxisId::Y;
}

}  // namespace DomainServices
}  // namespace Dispensing
}  // namespace Domain
}  // namespace Siligen
