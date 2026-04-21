#pragma once

#include "domain/dispensing/value-objects/DispensingExecutionPlan.h"
#include "shared/types/Error.h"
#include "shared/types/Point2D.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace Siligen::RuntimeExecution::Contracts::Dispensing {

using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan;
using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareProgramSpan;
using Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;

struct ProfileCompareExecutionSpan {
    uint32 span_index = 0;
    uint32 trigger_begin_index = 0;
    uint32 trigger_end_index = 0;
    uint32 expected_trigger_count = 0;
    uint32 start_boundary_trigger_count = 0;
    short compare_source_axis = 0;
    uint32 pulse_width_us = 0;
    float32 start_profile_position_mm = 0.0f;
    float32 end_profile_position_mm = 0.0f;
    Point2D start_position_mm{};
    Point2D end_position_mm{};
    std::vector<long> compare_positions_pulse;

    [[nodiscard]] uint32 FutureCompareCount() const noexcept {
        return static_cast<uint32>(compare_positions_pulse.size());
    }

    [[nodiscard]] bool Empty() const noexcept {
        return expected_trigger_count == 0U;
    }
};

struct ProfileCompareExecutionSchedule {
    uint32 expected_trigger_count = 0;
    float32 path_total_length_mm = 0.0f;
    std::vector<ProfileCompareExecutionSpan> spans;

    [[nodiscard]] bool Empty() const noexcept {
        return spans.empty();
    }
};

inline Result<void> ValidateProfileCompareExecutionSchedule(
    const DispensingExecutionPlan& plan,
    const ProfileCompareExecutionSchedule& schedule) noexcept {
    if (plan.production_trigger_mode != ProductionTriggerMode::PROFILE_COMPARE) {
        if (!schedule.Empty()) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule 仅允许用于 PROFILE_COMPARE execution_plan"));
        }
        return Result<void>::Success();
    }

    const auto& program = plan.profile_compare_program;
    if (program.trigger_points.empty()) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "PROFILE_COMPARE execution_plan 缺少 trigger_points"));
    }
    if (program.spans.empty()) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "PROFILE_COMPARE execution_plan 缺少 owner spans"));
    }
    if (schedule.Empty()) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "PROFILE_COMPARE execution request 缺少 profile compare schedule"));
    }
    if (schedule.expected_trigger_count != program.expected_trigger_count) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "profile_compare schedule expected_trigger_count 与 execution_plan 不一致"));
    }

    uint32 next_trigger_index = 0U;
    uint32 covered_trigger_count = 0U;
    float32 previous_end_profile_mm = -1.0f;
    constexpr float32 kToleranceMm = 1e-4f;
    if (schedule.spans.size() != program.spans.size()) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "profile_compare schedule span 数量必须与 owner spans 一致"));
    }

    for (std::size_t index = 0; index < schedule.spans.size(); ++index) {
        const auto& span = schedule.spans[index];
        const auto& owner_span = program.spans[index];
        if (span.expected_trigger_count == 0U) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span expected_trigger_count 必须大于 0"));
        }
        if (span.trigger_begin_index != next_trigger_index) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span trigger_begin_index 不连续"));
        }
        if (span.trigger_begin_index >= program.trigger_points.size() ||
            span.trigger_end_index >= program.trigger_points.size() ||
            span.trigger_end_index < span.trigger_begin_index) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span trigger 索引越界"));
        }
        const auto covered_by_index = span.trigger_end_index - span.trigger_begin_index + 1U;
        if (covered_by_index != span.expected_trigger_count) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span trigger 范围与 expected_trigger_count 不一致"));
        }
        if (span.expected_trigger_count != span.start_boundary_trigger_count + span.FutureCompareCount()) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span boundary/future compare 计数不一致"));
        }
        if (span.start_boundary_trigger_count > 1U) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span start_boundary_trigger_count 只能为 0 或 1"));
        }
        if (span.start_boundary_trigger_count != owner_span.start_boundary_trigger_count) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span start_boundary_trigger_count 必须与 owner span 一致"));
        }
        if (span.compare_source_axis < 1 || span.compare_source_axis > 2) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span compare_source_axis 仅允许 X/Y"));
        }
        if (span.compare_source_axis != owner_span.compare_source_axis) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span compare_source_axis 必须与 owner span 一致"));
        }
        if (span.pulse_width_us == 0U || span.pulse_width_us > 32767U) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span pulse_width_us 超出范围"));
        }
        if (span.start_profile_position_mm + kToleranceMm < previous_end_profile_mm) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span profile 起点必须单调不减"));
        }
        if (span.end_profile_position_mm + kToleranceMm < span.start_profile_position_mm) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span profile 终点必须不小于起点"));
        }
        if (span.trigger_begin_index != owner_span.trigger_begin_index ||
            span.trigger_end_index != owner_span.trigger_end_index) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span trigger 范围必须与 owner span 一致"));
        }
        if (std::fabs(span.start_profile_position_mm - owner_span.start_profile_position_mm) > kToleranceMm ||
            std::fabs(span.end_profile_position_mm - owner_span.end_profile_position_mm) > kToleranceMm) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span profile 边界必须与 owner span 一致"));
        }
        if (span.start_position_mm.DistanceTo(owner_span.start_position_mm) > kToleranceMm ||
            span.end_position_mm.DistanceTo(owner_span.end_position_mm) > kToleranceMm) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare schedule span 位置边界必须与 owner span 一致"));
        }

        long previous_compare_pulse = 0;
        bool has_previous_compare = false;
        for (const auto pulse : span.compare_positions_pulse) {
            if (pulse <= 0) {
                return Result<void>::Failure(Error(
                    ErrorCode::INVALID_PARAMETER,
                    "profile_compare schedule span future compare pulse 必须大于 0"));
            }
            if (has_previous_compare && pulse <= previous_compare_pulse) {
                return Result<void>::Failure(Error(
                    ErrorCode::INVALID_PARAMETER,
                    "profile_compare schedule span future compare pulse 必须严格递增"));
            }
            previous_compare_pulse = pulse;
            has_previous_compare = true;
        }

        next_trigger_index = span.trigger_end_index + 1U;
        covered_trigger_count += span.expected_trigger_count;
        previous_end_profile_mm = span.end_profile_position_mm;
    }

    if (covered_trigger_count != program.expected_trigger_count) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "profile_compare schedule 未覆盖全部 trigger"));
    }

    return Result<void>::Success();
}

}  // namespace Siligen::RuntimeExecution::Contracts::Dispensing
