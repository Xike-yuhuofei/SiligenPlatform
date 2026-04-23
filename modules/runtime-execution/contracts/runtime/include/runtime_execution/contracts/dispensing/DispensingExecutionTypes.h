#pragma once

#include "runtime_execution/contracts/dispensing/ProfileCompareExecutionSchedule.h"
#include "runtime_execution/contracts/dispensing/DispenseCompensationProfile.h"
#include "runtime_execution/contracts/dispensing/GuardDecision.h"
#include "runtime_execution/contracts/dispensing/JobExecutionMode.h"
#include "runtime_execution/contracts/dispensing/ProcessOutputPolicy.h"
#include "runtime_execution/contracts/machine/MachineMode.h"
#include "shared/types/Point.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Siligen::Domain::Dispensing::ValueObjects {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::uint32;

struct ProfileCompareExpectedTraceItem {
    uint32 cycle_index = 1U;
    uint32 trigger_sequence_id = 0U;
    uint32 authority_trigger_index = 0U;
    uint32 span_index = 0U;
    uint32 component_index = 0U;
    uint32 span_order_index = 0U;
    uint32 source_segment_index = 0U;
    uint32 execution_interpolation_index = 0U;
    float32 authority_distance_mm = 0.0f;
    float32 execution_profile_position_mm = 0.0f;
    Point2D execution_position_mm{};
    Point2D execution_trigger_position_mm{};
    short compare_source_axis = 0;
    long compare_position_pulse = 0L;
    uint32 pulse_width_us = 0U;
    std::string authority_trigger_ref;
    std::string authority_span_ref;
    std::string trigger_mode;
};

struct ProfileCompareActualTraceItem {
    uint32 cycle_index = 1U;
    uint32 trigger_sequence_id = 0U;
    uint32 completion_sequence = 0U;
    uint32 span_index = 0U;
    uint32 local_completed_trigger_count = 0U;
    uint32 observed_completed_trigger_count = 0U;
    short compare_source_axis = 0;
    long compare_position_pulse = 0L;
    std::string authority_trigger_ref;
    std::string trigger_mode;
};

struct ProfileCompareTraceabilityMismatch {
    uint32 cycle_index = 1U;
    uint32 trigger_sequence_id = 0U;
    std::string code;
    std::string message;
};

struct ProfileCompareExpectedTrace {
    std::vector<ProfileCompareExpectedTraceItem> items;

    [[nodiscard]] bool Empty() const noexcept {
        return items.empty();
    }
};

struct DispensingRuntimeOverrides {
    bool dry_run = false;
    std::optional<Siligen::Domain::Machine::ValueObjects::MachineMode> machine_mode;
    std::optional<JobExecutionMode> execution_mode;
    std::optional<ProcessOutputPolicy> output_policy;
    std::optional<float32> dispensing_speed_mm_s;
    std::optional<float32> dry_run_speed_mm_s;
    std::optional<float32> rapid_speed_mm_s;
    std::optional<float32> acceleration_mm_s2;

    bool velocity_guard_enabled = true;
    float32 velocity_guard_ratio = 0.3f;
    float32 velocity_guard_abs_mm_s = 5.0f;
    float32 velocity_guard_min_expected_mm_s = 5.0f;
    int32 velocity_guard_grace_ms = 800;
    int32 velocity_guard_interval_ms = 200;
    int32 velocity_guard_max_consecutive = 3;
    bool velocity_guard_stop_on_violation = false;

    [[nodiscard]] Result<void> Validate() const noexcept {
        const float32 dispensing_speed = dispensing_speed_mm_s.value_or(0.0f);
        const float32 dry_run_speed = dry_run_speed_mm_s.value_or(0.0f);
        const float32 rapid_speed = rapid_speed_mm_s.value_or(0.0f);
        const float32 acceleration = acceleration_mm_s2.value_or(0.0f);

        if (dispensing_speed_mm_s.has_value() && dispensing_speed < 0.0f) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "dispensing_speed_mm_s不能为负数"));
        }
        if (dry_run_speed_mm_s.has_value() && dry_run_speed < 0.0f) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "dry_run_speed_mm_s不能为负数"));
        }
        if (rapid_speed_mm_s.has_value() && rapid_speed < 0.0f) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "rapid_speed_mm_s不能为负数"));
        }
        if (dispensing_speed <= 0.0f && dry_run_speed <= 0.0f) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "点胶速度与空走速度至少提供一个"));
        }
        if (acceleration_mm_s2.has_value() && acceleration < 0.0f) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "acceleration_mm_s2不能为负数"));
        }
        if (velocity_guard_ratio < 0.0f || velocity_guard_ratio > 1.0f) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "velocity_guard_ratio超出范围(0-1)"));
        }
        if (velocity_guard_abs_mm_s < 0.0f || velocity_guard_min_expected_mm_s < 0.0f) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER,
                      "velocity_guard_abs_mm_s/velocity_guard_min_expected_mm_s不能为负数"));
        }
        if (velocity_guard_grace_ms < 0 ||
            velocity_guard_interval_ms < 0 ||
            velocity_guard_max_consecutive < 0) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER,
                      "velocity_guard_grace_ms/velocity_guard_interval_ms/velocity_guard_max_consecutive不能为负数"));
        }
        if (velocity_guard_enabled) {
            if (velocity_guard_ratio <= 0.0f && velocity_guard_abs_mm_s <= 0.0f) {
                return Result<void>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER, "速度保护阈值无效(比例/绝对阈值需至少一个>0)"));
            }
            if (velocity_guard_interval_ms == 0 || velocity_guard_max_consecutive == 0) {
                return Result<void>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER, "速度保护interval/max_consecutive必须大于0"));
            }
        }
        if (dry_run) {
            if (machine_mode.has_value() &&
                machine_mode.value() != Siligen::Domain::Machine::ValueObjects::MachineMode::Test) {
                return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "dry_run与machine_mode语义冲突"));
            }
            if (execution_mode.has_value() &&
                execution_mode.value() != JobExecutionMode::ValidationDryCycle) {
                return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "dry_run与execution_mode语义冲突"));
            }
            if (output_policy.has_value() &&
                output_policy.value() != ProcessOutputPolicy::Inhibited) {
                return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "dry_run与output_policy语义冲突"));
            }
        }
        return Result<void>::Success();
    }
};

struct DispensingRuntimeParams {
    float32 dispensing_velocity = 0.0f;
    float32 rapid_velocity = 0.0f;
    float32 acceleration = 0.0f;
    float32 pulse_per_mm = 0.0f;
    uint32 dispenser_interval_ms = 0;
    uint32 dispenser_duration_ms = 0;
    float32 trigger_spatial_interval_mm = 0.0f;
    float32 valve_response_ms = 0.0f;
    float32 safety_margin_ms = 0.0f;
    float32 min_interval_ms = 0.0f;
    float32 sample_dt = 0.01f;
    float32 sample_ds = 0.0f;
    DispenseCompensationProfile compensation_profile;
    bool velocity_guard_enabled = false;
    float32 velocity_guard_ratio = 0.0f;
    float32 velocity_guard_abs_mm_s = 0.0f;
    float32 velocity_guard_min_expected_mm_s = 0.0f;
    int32 velocity_guard_grace_ms = 0;
    int32 velocity_guard_interval_ms = 0;
    int32 velocity_guard_max_consecutive = 0;
    bool velocity_guard_stop_on_violation = false;
};

struct DispensingExecutionOptions {
    bool dispense_enabled = false;
    bool use_hardware_trigger = true;
    Siligen::Domain::Machine::ValueObjects::MachineMode machine_mode =
        Siligen::Domain::Machine::ValueObjects::MachineMode::Production;
    JobExecutionMode execution_mode = JobExecutionMode::Production;
    ProcessOutputPolicy output_policy = ProcessOutputPolicy::Enabled;
    GuardDecision guard_decision{};
    std::shared_ptr<const Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareExecutionSchedule>
        profile_compare_schedule;
    std::shared_ptr<const ProfileCompareExpectedTrace> expected_trace;
};

struct DispensingExecutionReport {
    uint32 executed_segments = 0;
    float32 total_distance = 0.0f;
    std::vector<ProfileCompareActualTraceItem> actual_trace;
    std::vector<ProfileCompareTraceabilityMismatch> traceability_mismatches;
    std::string traceability_verdict = "failed";
    std::string traceability_verdict_reason;
    bool strict_one_to_one_proven = false;
};

}  // namespace Siligen::Domain::Dispensing::ValueObjects
