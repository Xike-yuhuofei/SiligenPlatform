#include "TriggerPlanner.h"
#include "runtime_execution/contracts/dispensing/DispenseCompensationRules.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::RuntimeExecution::Contracts::Dispensing::AdjustPulseWidthMs;

namespace {
constexpr float32 kEpsilon = 1e-6f;
constexpr uint32 kMaxTimedTriggerCount = 1000;
constexpr uint32 kMaxTriggerPositions = 100000;
}  // namespace

Result<TriggerPlanResult> TriggerPlanner::Plan(float32 segment_length_mm,
                                               float32 velocity_mm_s,
                                               float32 acceleration_mm_s2,
                                               float32 spatial_interval_mm,
                                               uint32 dispenser_interval_ms,
                                               float32 residual_mm,
                                               const TriggerPlan& base_plan,
                                               const DispenseCompensationProfile& compensation) const noexcept {
    TriggerPlanResult result;
    result.plan = base_plan;

    if (segment_length_mm < kEpsilon || velocity_mm_s < kEpsilon) {
        result.spacing.residual_mm = residual_mm;
        return Result<TriggerPlanResult>::Success(result);
    }

    float32 interval_mm = ResolveSpatialInterval(spatial_interval_mm, velocity_mm_s, dispenser_interval_ms);
    uint32 interval_ms = dispenser_interval_ms;
    if (interval_ms == 0 && interval_mm > kEpsilon) {
        interval_ms = static_cast<uint32>(std::llround((interval_mm / velocity_mm_s) * 1000.0f));
    }
    if (interval_ms == 0) {
        interval_ms = 1;
    }

    bool downgraded = false;
    auto safety_result = EnforceSafetyBoundary(interval_ms,
                                               velocity_mm_s,
                                               interval_mm,
                                               base_plan.safety,
                                               downgraded);
    if (safety_result.IsError()) {
        return Result<TriggerPlanResult>::Failure(safety_result.GetError());
    }

    result.plan.interval_mm = interval_mm;
    result.interval_ms = interval_ms;
    result.downgrade_applied = downgraded;

    result.spacing = BuildSpacingPlan(segment_length_mm, interval_mm, residual_mm);

    if (base_plan.strategy == DispensingStrategy::SEGMENTED) {
        result.commands = BuildSegmentedCommands(segment_length_mm,
                                                 velocity_mm_s,
                                                 acceleration_mm_s2,
                                                 interval_mm,
                                                 static_cast<uint32>(base_plan.safety.min_interval_ms));
    } else if (base_plan.strategy == DispensingStrategy::SUBSEGMENTED) {
        result.commands = BuildSubSegmentedCommands(segment_length_mm,
                                                    velocity_mm_s,
                                                    acceleration_mm_s2,
                                                    interval_mm,
                                                    static_cast<uint32>(base_plan.safety.min_interval_ms),
                                                    base_plan.subsegment_count);
    }

    if (!result.commands.empty()) {
        for (auto& cmd : result.commands) {
            uint32 adjusted_pulse = AdjustPulseWidthMs(
                static_cast<uint32>(std::llround(cmd.estimated_duration_ms)),
                compensation,
                false);
            if (cmd.interval_ms < adjusted_pulse) {
                cmd.interval_ms = adjusted_pulse;
            }
        }
    }

    return Result<TriggerPlanResult>::Success(result);
}

TriggerSpacingPlan TriggerPlanner::BuildSpacingPlan(float32 length_mm,
                                                    float32 interval_mm,
                                                    float32 residual_mm) const {
    TriggerSpacingPlan plan;
    plan.residual_mm = residual_mm;
    if (length_mm <= kEpsilon || interval_mm <= kEpsilon) {
        return plan;
    }

    float32 carry = residual_mm;
    if (carry < 0.0f) {
        carry = 0.0f;
    }
    if (carry >= interval_mm) {
        carry = std::fmod(carry, interval_mm);
    }

    float32 first = interval_mm - carry;
    if (first < kEpsilon) {
        first = interval_mm;
    }

    const float32 end_epsilon = 1e-6f;
    for (float32 dist = first; dist <= length_mm + end_epsilon; dist += interval_mm) {
        if (dist > kEpsilon) {
            plan.distances_mm.push_back(dist);
            if (plan.distances_mm.size() >= kMaxTriggerPositions) {
                break;
            }
        }
    }

    float32 total = carry + length_mm;
    float32 remainder = std::fmod(total, interval_mm);
    if (remainder < kEpsilon || interval_mm - remainder < kEpsilon) {
        remainder = 0.0f;
    }
    plan.residual_mm = remainder;
    return plan;
}

std::vector<TriggerCommandPlan> TriggerPlanner::BuildSegmentedCommands(float32 length_mm,
                                                                       float32 velocity_mm_s,
                                                                       float32 acceleration_mm_s2,
                                                                       float32 spatial_interval_mm,
                                                                       uint32 min_interval_ms) const {
    std::vector<TriggerCommandPlan> commands;
    if (velocity_mm_s <= kEpsilon || length_mm <= kEpsilon) {
        return commands;
    }

    float32 cruise_vel = velocity_mm_s;
    float32 accel_dist = (cruise_vel * cruise_vel) / (2.0f * acceleration_mm_s2);
    bool is_triangular = (length_mm < 2.0f * accel_dist);

    if (is_triangular) {
        float32 peak_vel = std::sqrt(length_mm * acceleration_mm_s2);
        float32 avg_vel = peak_vel / 2.0f;

        float32 interval_mm = ResolveSpatialInterval(spatial_interval_mm, avg_vel, min_interval_ms);
        uint32 interval_ms = static_cast<uint32>((interval_mm / avg_vel) * 1000.0f);
        if (interval_ms == 0) interval_ms = 1;

        uint32 count = static_cast<uint32>((length_mm / 2.0f) / interval_mm);
        if (count > 0) {
            float32 ramp_time_s = peak_vel / acceleration_mm_s2;
            commands.push_back({count, interval_ms, ramp_time_s * 1000.0f});
            commands.push_back({count, interval_ms, ramp_time_s * 1000.0f});
        }
        return commands;
    }

    float32 cruise_dist = length_mm - 2.0f * accel_dist;
    float32 ramp_time_s = cruise_vel / acceleration_mm_s2;
    float32 cruise_time_s = cruise_dist / cruise_vel;
    float32 avg_ramp_vel = cruise_vel / 2.0f;

    float32 ramp_interval_mm = ResolveSpatialInterval(spatial_interval_mm, avg_ramp_vel, min_interval_ms);
    uint32 ramp_interval_ms = static_cast<uint32>((ramp_interval_mm / avg_ramp_vel) * 1000.0f);
    if (ramp_interval_ms == 0) ramp_interval_ms = 1;
    uint32 ramp_count = static_cast<uint32>(accel_dist / ramp_interval_mm);
    if (ramp_count > 0) {
        commands.push_back({ramp_count, ramp_interval_ms, ramp_time_s * 1000.0f});
    }

    float32 cruise_interval_mm = ResolveSpatialInterval(spatial_interval_mm, cruise_vel, min_interval_ms);
    uint32 cruise_interval_ms = static_cast<uint32>((cruise_interval_mm / cruise_vel) * 1000.0f);
    if (cruise_interval_ms == 0) cruise_interval_ms = 1;
    uint32 cruise_count = static_cast<uint32>(cruise_dist / cruise_interval_mm);
    if (cruise_count > 0) {
        commands.push_back({cruise_count, cruise_interval_ms, cruise_time_s * 1000.0f});
    }

    if (ramp_count > 0) {
        commands.push_back({ramp_count, ramp_interval_ms, ramp_time_s * 1000.0f});
    }

    return commands;
}

std::vector<TriggerCommandPlan> TriggerPlanner::BuildSubSegmentedCommands(float32 length_mm,
                                                                          float32 velocity_mm_s,
                                                                          float32 acceleration_mm_s2,
                                                                          float32 spatial_interval_mm,
                                                                          uint32 min_interval_ms,
                                                                          int subsegment_count) const {
    std::vector<TriggerCommandPlan> commands;
    if (velocity_mm_s <= kEpsilon || length_mm <= kEpsilon) {
        return commands;
    }

    if (subsegment_count < 1) subsegment_count = 1;
    float32 cruise_vel = velocity_mm_s;
    float32 accel_dist = (cruise_vel * cruise_vel) / (2.0f * acceleration_mm_s2);
    bool is_triangular = (length_mm < 2.0f * accel_dist);
    float32 peak_vel = is_triangular ? std::sqrt(length_mm * acceleration_mm_s2) : cruise_vel;
    float32 ramp_time_s = peak_vel / acceleration_mm_s2;

    auto generate_ramp_cmds = [&](bool accel_phase) {
        float32 sub_time = ramp_time_s / static_cast<float32>(subsegment_count);
        float32 residual_dist = 0.0f;

        for (int i = 0; i < subsegment_count; ++i) {
            float32 t_start = static_cast<float32>(i) * sub_time;
            float32 t_end = static_cast<float32>(i + 1) * sub_time;
            float32 v_avg = acceleration_mm_s2 * (t_start + t_end) * 0.5f;
            if (!accel_phase) {
                v_avg = peak_vel - v_avg;
            }
            if (v_avg < 0.1f) v_avg = 0.1f;

            float32 dist = v_avg * sub_time;
            float32 interval_mm = ResolveSpatialInterval(spatial_interval_mm, v_avg, min_interval_ms);
            uint32 interval_ms = static_cast<uint32>((interval_mm / v_avg) * 1000.0f);
            if (interval_ms == 0) interval_ms = 1;

            float32 total_target_dist = dist + residual_dist;
            uint32 count = static_cast<uint32>(total_target_dist / interval_mm);
            residual_dist = total_target_dist - (count * interval_mm);

            if (count > 0) {
                commands.push_back({count, interval_ms, sub_time * 1000.0f});
            }
        }
    };

    generate_ramp_cmds(true);

    if (!is_triangular) {
        float32 cruise_dist = length_mm - 2.0f * accel_dist;
        float32 cruise_time_s = cruise_dist / cruise_vel;
        float32 cruise_interval_mm = ResolveSpatialInterval(spatial_interval_mm, cruise_vel, min_interval_ms);
        uint32 cruise_interval_ms = static_cast<uint32>((cruise_interval_mm / cruise_vel) * 1000.0f);
        if (cruise_interval_ms == 0) cruise_interval_ms = 1;
        uint32 cruise_count = static_cast<uint32>(cruise_dist / cruise_interval_mm);
        if (cruise_count > 0) {
            commands.push_back({cruise_count, cruise_interval_ms, cruise_time_s * 1000.0f});
        }
    }

    generate_ramp_cmds(false);
    return commands;
}

float32 TriggerPlanner::ResolveSpatialInterval(float32 spatial_interval_mm,
                                               float32 velocity_mm_s,
                                               uint32 fallback_interval_ms) const {
    if (spatial_interval_mm > kEpsilon) {
        return spatial_interval_mm;
    }
    if (velocity_mm_s <= kEpsilon || fallback_interval_ms == 0) {
        return 0.0f;
    }
    return velocity_mm_s * (static_cast<float32>(fallback_interval_ms) / 1000.0f);
}

Result<void> TriggerPlanner::EnforceSafetyBoundary(uint32& interval_ms,
                                                   float32 velocity_mm_s,
                                                   float32& interval_mm,
                                                   const SafetyBoundary& safety,
                                                   bool& downgrade_applied) const {
    uint32 min_interval = 0;
    const int32 computed = safety.duration_ms + safety.valve_response_ms + safety.margin_ms;
    if (computed > 0) {
        min_interval = static_cast<uint32>(computed);
    }
    if (safety.min_interval_ms > 0) {
        min_interval = std::max(min_interval, static_cast<uint32>(safety.min_interval_ms));
    }
    if (min_interval == 0 || interval_ms >= min_interval) {
        return Result<void>::Success();
    }

    if (!safety.downgrade_on_violation) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "触发间隔小于阀响应安全边界",
                  "TriggerPlanner"));
    }

    interval_ms = min_interval;
    if (velocity_mm_s > kEpsilon) {
        interval_mm = velocity_mm_s * (static_cast<float32>(interval_ms) / 1000.0f);
    }
    downgrade_applied = true;
    return Result<void>::Success();
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
