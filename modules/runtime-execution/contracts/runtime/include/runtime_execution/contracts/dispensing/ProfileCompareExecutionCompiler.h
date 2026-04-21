#pragma once

#include "runtime_execution/contracts/dispensing/ProfileCompareExecutionSchedule.h"
#include "runtime_execution/contracts/dispensing/ProfileCompareRuntimeContract.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace Siligen::RuntimeExecution::Contracts::Dispensing {

using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareProgramSpan;
using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareTriggerProgram;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::uint32;

struct ProfileCompareExecutionCompileInput {
    const DispensingExecutionPlan& execution_plan;
    ProfileCompareRuntimeContract runtime_contract;
};

struct ProfileCompareOwnerSpanCoverage {
    bool compiled = false;
    ProfileCompareExecutionSpan execution_span;
    std::vector<std::string> rejected_candidates;
};

namespace Detail {

constexpr float32 kCompileToleranceMm = 1e-4f;
constexpr int32 kCompareAxisMaskX = 0x01;
constexpr int32 kCompareAxisMaskY = 0x02;

inline const char* AxisName(short compare_source_axis) noexcept {
    return compare_source_axis == 1 ? "X" : "Y";
}

inline int32 CompareAxisMaskBit(short compare_source_axis) noexcept {
    switch (compare_source_axis) {
        case 1:
            return kCompareAxisMaskX;
        case 2:
            return kCompareAxisMaskY;
        default:
            return 0;
    }
}

struct AxisCompileAttempt {
    bool compiled = false;
    ProfileCompareExecutionSpan execution_span;
    std::string failure_reason;
};

struct CompileFailureDetail {
    std::string message;
};

inline bool CandidateIndicatesMonotonicCompareConstraint(const std::string& candidate) {
    return candidate.find("单调正向推进") != std::string::npos ||
        candidate.find("严格递增") != std::string::npos ||
        candidate.find("额外 boundary trigger") != std::string::npos ||
        candidate.find("归一化为 boundary") != std::string::npos;
}

inline Result<void> ValidateOwnerSpanInput(
    const ProfileCompareTriggerProgram& program,
    const ProfileCompareProgramSpan& owner_span) noexcept {
    if (program.trigger_points.empty()) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "PROFILE_COMPARE execution_plan 缺少 trigger_points"));
    }
    if (owner_span.ExpectedTriggerCount() == 0U) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "profile_compare owner formal span contract 缺少有效 trigger 范围"));
    }
    if (owner_span.trigger_begin_index >= program.trigger_points.size() ||
        owner_span.trigger_end_index >= program.trigger_points.size()) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "profile_compare owner formal span contract trigger 索引越界"));
    }
    if (owner_span.start_profile_position_mm > owner_span.end_profile_position_mm + kCompileToleranceMm) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "profile_compare owner formal span contract profile 边界非法"));
    }
    if (!std::isfinite(owner_span.start_position_mm.x) || !std::isfinite(owner_span.start_position_mm.y) ||
        !std::isfinite(owner_span.end_position_mm.x) || !std::isfinite(owner_span.end_position_mm.y)) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "profile_compare owner formal span contract 位置边界非法"));
    }
    return Result<void>::Success();
}

inline AxisCompileAttempt CompileOwnerSpanOnAxis(
    const ProfileCompareTriggerProgram& program,
    const ProfileCompareProgramSpan& owner_span,
    const ProfileCompareRuntimeContract& runtime_contract,
    short compare_source_axis,
    int32 compare_axis_mask) noexcept {
    AxisCompileAttempt attempt;
    const auto axis_mask_bit = CompareAxisMaskBit(compare_source_axis);
    if ((compare_axis_mask & axis_mask_bit) == 0) {
        attempt.failure_reason = std::string(AxisName(compare_source_axis)) +
            ":cmp_axis_mask 不允许当前 compare 轴";
        return attempt;
    }

    const auto start_axis_position_mm =
        compare_source_axis == 1 ? owner_span.start_position_mm.x : owner_span.start_position_mm.y;
    if (!std::isfinite(start_axis_position_mm)) {
        attempt.failure_reason = std::string(AxisName(compare_source_axis)) +
            ":profile_compare compare_source_axis formal runtime 仅支持 X/Y 单 compare 轴几何投影";
        return attempt;
    }

    auto& span = attempt.execution_span;
    span.trigger_begin_index = owner_span.trigger_begin_index;
    span.trigger_end_index = owner_span.trigger_end_index;
    span.expected_trigger_count = owner_span.ExpectedTriggerCount();
    span.compare_source_axis = compare_source_axis;
    span.start_profile_position_mm = owner_span.start_profile_position_mm;
    span.end_profile_position_mm = owner_span.end_profile_position_mm;
    span.start_position_mm = owner_span.start_position_mm;
    span.end_position_mm = owner_span.end_position_mm;

    long previous_compare_pulse = 0L;
    bool has_previous_compare = false;
    uint32 pulse_width_us = 0U;
    bool has_pulse_width = false;

    auto reject = [&](const std::string& reason) {
        attempt.compiled = false;
        attempt.failure_reason = std::string(AxisName(compare_source_axis)) + ":" + reason;
    };

    for (uint32 trigger_index = owner_span.trigger_begin_index;
         trigger_index <= owner_span.trigger_end_index;
         ++trigger_index) {
        const auto& trigger = program.trigger_points[trigger_index];
        if (trigger.pulse_width_us == 0U || trigger.pulse_width_us > 32767U) {
            reject("profile_compare pulse_width_us 必须在 1..32767 范围内");
            return attempt;
        }
        if (!has_pulse_width) {
            pulse_width_us = trigger.pulse_width_us;
            has_pulse_width = true;
        } else if (trigger.pulse_width_us != pulse_width_us) {
            reject("profile_compare span 内 pulse_width_us 必须保持一致");
            return attempt;
        }

        const auto axis_position_mm =
            compare_source_axis == 1 ? trigger.trigger_position_mm.x : trigger.trigger_position_mm.y;
        if (!std::isfinite(axis_position_mm)) {
            reject("profile_compare trigger_position_mm 缺少有效 compare 轴坐标");
            return attempt;
        }

        if (trigger.profile_position_mm + kCompileToleranceMm < owner_span.start_profile_position_mm ||
            trigger.profile_position_mm > owner_span.end_profile_position_mm + kCompileToleranceMm) {
            reject("profile_compare trigger 超出 owner span profile 边界");
            return attempt;
        }

        const auto relative_axis_position_mm = axis_position_mm - start_axis_position_mm;
        const auto relative_profile_position_mm =
            trigger.profile_position_mm - owner_span.start_profile_position_mm;
        const auto pulse_position = std::llround(
            static_cast<double>(relative_axis_position_mm) *
            static_cast<double>(runtime_contract.pulse_per_mm));
        if (relative_axis_position_mm < -kCompileToleranceMm || pulse_position < 0LL) {
            reject("profile_compare compare 轴不满足从当前 span 起点单调正向推进");
            return attempt;
        }
        if (pulse_position > static_cast<long long>(std::numeric_limits<long>::max())) {
            reject("profile_compare compare position 超出可表示 pulse 范围");
            return attempt;
        }

        if (pulse_position == 0LL) {
            const bool is_exact_span_start =
                std::fabs(relative_profile_position_mm) <= kCompileToleranceMm &&
                trigger.trigger_position_mm.DistanceTo(owner_span.start_position_mm) <= kCompileToleranceMm;
            if (!is_exact_span_start) {
                reject("profile_compare trigger 归一化为 boundary，但 trigger 不在当前 span 的真实起点");
                return attempt;
            }
            if (span.start_boundary_trigger_count > 0U || !span.compare_positions_pulse.empty()) {
                reject("profile_compare span 不能包含额外 boundary trigger");
                return attempt;
            }
            ++span.start_boundary_trigger_count;
            continue;
        }

        if (span.compare_positions_pulse.size() >= runtime_contract.max_future_compare_count) {
            reject("profile_compare future compare count 超过 formal runtime 上限");
            return attempt;
        }

        const auto normalized_pulse = static_cast<long>(pulse_position);
        if (has_previous_compare && normalized_pulse <= previous_compare_pulse) {
            reject("profile_compare future compare pulse 归一化后未保持严格递增");
            return attempt;
        }
        previous_compare_pulse = normalized_pulse;
        has_previous_compare = true;
        span.compare_positions_pulse.push_back(normalized_pulse);
    }

    if (!has_pulse_width) {
        reject("profile_compare 当前 span 无可编译 trigger");
        return attempt;
    }
    if (span.start_boundary_trigger_count + span.FutureCompareCount() != span.expected_trigger_count) {
        reject("profile_compare 当前 span 无法完整覆盖 owner trigger contract");
        return attempt;
    }

    span.pulse_width_us = pulse_width_us;
    attempt.compiled = true;
    return attempt;
}

inline CompileFailureDetail BuildCompileFailureDetail(
    int32 compare_axis_mask,
    const ProfileCompareProgramSpan& owner_span,
    const ProfileCompareTriggerProgram& program,
    const std::vector<std::string>& rejected_candidates) {
    CompileFailureDetail detail;
    detail.message = "profile_compare owner span 无法编译正式 execution schedule";
    if ((compare_axis_mask & (kCompareAxisMaskX | kCompareAxisMaskY)) == 0) {
        detail.message += "; cmp_axis_mask 未包含 formal runtime 受支持的 X/Y compare 投影轴";
    }
    detail.message += "; trigger_begin_index=" + std::to_string(owner_span.trigger_begin_index);
    {
        std::ostringstream oss;
        oss << "; current_start_position_mm=("
            << owner_span.start_position_mm.x
            << ","
            << owner_span.start_position_mm.y
            << ")";
        if (owner_span.trigger_begin_index < program.trigger_points.size()) {
            const auto& next_trigger = program.trigger_points[owner_span.trigger_begin_index].trigger_position_mm;
            oss << "; next_trigger_position_mm=(" << next_trigger.x << "," << next_trigger.y << ")";
        }
        detail.message += oss.str();
    }
    if (!rejected_candidates.empty()) {
        detail.message += "; candidates=";
        for (std::size_t index = 0; index < rejected_candidates.size(); ++index) {
            if (index > 0U) {
                detail.message += " | ";
            }
            detail.message += rejected_candidates[index];
        }
    }
    if (!rejected_candidates.empty() &&
        std::all_of(
            rejected_candidates.begin(),
            rejected_candidates.end(),
            [](const std::string& candidate) {
                return CandidateIndicatesMonotonicCompareConstraint(candidate);
            })) {
        detail.message += "; owner formal span contract 当前需要下降或回返递增 compare 位置";
    }
    return detail;
}

}  // namespace Detail

inline Result<ProfileCompareOwnerSpanCoverage> EvaluateProfileCompareOwnerSpanCoverage(
    const ProfileCompareTriggerProgram& program,
    const ProfileCompareProgramSpan& owner_span,
    const ProfileCompareRuntimeContract& runtime_contract) noexcept {
    auto runtime_validation = runtime_contract.Validate();
    if (runtime_validation.IsError()) {
        return Result<ProfileCompareOwnerSpanCoverage>::Failure(runtime_validation.GetError());
    }
    auto owner_span_validation = Detail::ValidateOwnerSpanInput(program, owner_span);
    if (owner_span_validation.IsError()) {
        return Result<ProfileCompareOwnerSpanCoverage>::Failure(owner_span_validation.GetError());
    }

    ProfileCompareOwnerSpanCoverage coverage;
    const bool force_owner_axis = owner_span.compare_source_axis == 1 || owner_span.compare_source_axis == 2;
    const int32 compare_axis_mask = runtime_contract.compare_axis_mask;

    auto try_axis = [&](short compare_source_axis) {
        const auto attempt = Detail::CompileOwnerSpanOnAxis(
            program,
            owner_span,
            runtime_contract,
            compare_source_axis,
            compare_axis_mask);
        if (attempt.compiled) {
            coverage.compiled = true;
            coverage.execution_span = attempt.execution_span;
            return true;
        }
        if (!attempt.failure_reason.empty()) {
            coverage.rejected_candidates.push_back(attempt.failure_reason);
        }
        return false;
    };

    if (force_owner_axis) {
        (void)try_axis(owner_span.compare_source_axis);
        return Result<ProfileCompareOwnerSpanCoverage>::Success(std::move(coverage));
    }

    if (try_axis(1)) {
        return Result<ProfileCompareOwnerSpanCoverage>::Success(std::move(coverage));
    }
    (void)try_axis(2);
    return Result<ProfileCompareOwnerSpanCoverage>::Success(std::move(coverage));
}

inline Result<ProfileCompareExecutionSchedule> CompileProfileCompareExecutionSchedule(
    const ProfileCompareExecutionCompileInput& input) noexcept {
    ProfileCompareExecutionSchedule schedule;
    schedule.path_total_length_mm = input.execution_plan.total_length_mm;

    if (input.execution_plan.production_trigger_mode != ProductionTriggerMode::PROFILE_COMPARE) {
        return Result<ProfileCompareExecutionSchedule>::Success(std::move(schedule));
    }

    auto runtime_validation = input.runtime_contract.Validate();
    if (runtime_validation.IsError()) {
        return Result<ProfileCompareExecutionSchedule>::Failure(runtime_validation.GetError());
    }

    const auto& program = input.execution_plan.profile_compare_program;
    if (program.trigger_points.empty()) {
        return Result<ProfileCompareExecutionSchedule>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "PROFILE_COMPARE execution_plan 缺少 trigger_points"));
    }
    if (program.spans.empty()) {
        return Result<ProfileCompareExecutionSchedule>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "profile_compare owner formal span contract 缺少 owner spans"));
    }
    if (program.expected_trigger_count != program.trigger_points.size()) {
        return Result<ProfileCompareExecutionSchedule>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "profile_compare owner formal span contract expected_trigger_count 与 trigger_points 不一致"));
    }

    schedule.expected_trigger_count = program.expected_trigger_count;
    schedule.spans.reserve(program.spans.size());

    uint32 next_trigger_index = 0U;
    for (std::size_t span_index = 0; span_index < program.spans.size(); ++span_index) {
        const auto& owner_span = program.spans[span_index];
        if (owner_span.compare_source_axis < 1 || owner_span.compare_source_axis > 2) {
            return Result<ProfileCompareExecutionSchedule>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare owner formal span contract compare_source_axis 仅允许 X/Y"));
        }
        if (owner_span.trigger_begin_index != next_trigger_index) {
            return Result<ProfileCompareExecutionSchedule>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "profile_compare owner formal span contract trigger_begin_index 不连续"));
        }

        auto coverage_result =
            EvaluateProfileCompareOwnerSpanCoverage(program, owner_span, input.runtime_contract);
        if (coverage_result.IsError()) {
            return Result<ProfileCompareExecutionSchedule>::Failure(coverage_result.GetError());
        }

        auto coverage = coverage_result.Value();
        if (!coverage.compiled) {
            const auto failure_detail = Detail::BuildCompileFailureDetail(
                input.runtime_contract.compare_axis_mask,
                owner_span,
                program,
                coverage.rejected_candidates);
            return Result<ProfileCompareExecutionSchedule>::Failure(Error(
                ErrorCode::CMP_TRIGGER_SETUP_FAILED,
                failure_detail.message,
                "ProfileCompareExecutionCompiler"));
        }

        auto compiled_span = std::move(coverage.execution_span);
        compiled_span.span_index = static_cast<uint32>(span_index);
        schedule.spans.push_back(std::move(compiled_span));
        next_trigger_index = owner_span.trigger_end_index + 1U;
    }

    auto validation = ValidateProfileCompareExecutionSchedule(input.execution_plan, schedule);
    if (validation.IsError()) {
        return Result<ProfileCompareExecutionSchedule>::Failure(validation.GetError());
    }

    return Result<ProfileCompareExecutionSchedule>::Success(std::move(schedule));
}

}  // namespace Siligen::RuntimeExecution::Contracts::Dispensing
