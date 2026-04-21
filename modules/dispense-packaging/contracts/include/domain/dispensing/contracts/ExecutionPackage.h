#pragma once

#include "domain/dispensing/value-objects/DispensingExecutionPlan.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace Siligen::Domain::Dispensing::Contracts {

using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;

struct ExecutionPackageBuilt {
    DispensingExecutionPlan execution_plan;
    float32 total_length_mm = 0.0f;
    float32 execution_nominal_time_s = 0.0f;
    std::string source_path;
    std::string source_fingerprint;
};

struct ExecutionPackageValidated {
    DispensingExecutionPlan execution_plan;
    float32 total_length_mm = 0.0f;
    float32 execution_nominal_time_s = 0.0f;
    std::string source_path;
    std::string source_fingerprint;

    ExecutionPackageValidated() = default;
    explicit ExecutionPackageValidated(const ExecutionPackageBuilt& built)
        : execution_plan(built.execution_plan),
          total_length_mm(built.total_length_mm),
          execution_nominal_time_s(built.execution_nominal_time_s),
          source_path(built.source_path),
          source_fingerprint(built.source_fingerprint) {}

    explicit ExecutionPackageValidated(ExecutionPackageBuilt&& built) noexcept
        : execution_plan(std::move(built.execution_plan)),
          total_length_mm(built.total_length_mm),
          execution_nominal_time_s(built.execution_nominal_time_s),
          source_path(std::move(built.source_path)),
          source_fingerprint(std::move(built.source_fingerprint)) {}

    [[nodiscard]] Result<void> Validate() const noexcept {
        const bool has_interpolation_points = execution_plan.interpolation_points.size() >= 2U;
        const bool has_motion_points = execution_plan.motion_trajectory.points.size() >= 2U;
        const bool has_interpolation_segments = !execution_plan.interpolation_segments.empty();
        const bool has_single_point_target = execution_plan.HasSinglePointTarget();

        if (execution_plan.IsPointGeometry()) {
            if (execution_plan.execution_strategy ==
                Siligen::Shared::Types::DispensingExecutionStrategy::STATIONARY_SHOT) {
                if (!has_single_point_target) {
                    return Result<void>::Failure(
                        Error(ErrorCode::INVALID_PARAMETER,
                              "point execution package missing stationary target"));
                }
            } else if (!has_interpolation_segments && !has_interpolation_points && !has_motion_points) {
                return Result<void>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER,
                          "point execution package missing executable flying trajectory"));
            }
        } else if (!has_interpolation_segments && !has_interpolation_points && !has_motion_points) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "execution package missing executable trajectory"));
        }

        if (!execution_plan.IsPointGeometry() && has_interpolation_points) {
            const auto terminal_timestamp = execution_plan.interpolation_points.back().timestamp;
            if (terminal_timestamp <= 0.0f) {
                return Result<void>::Failure(Error(
                    ErrorCode::INVALID_PARAMETER,
                    "path execution interpolation_points missing terminal timestamp"));
            }

            const bool monotonic = std::adjacent_find(
                                       execution_plan.interpolation_points.begin(),
                                       execution_plan.interpolation_points.end(),
                                       [](const auto& lhs, const auto& rhs) {
                                           return rhs.timestamp < lhs.timestamp;
                                       }) == execution_plan.interpolation_points.end();
            if (!monotonic) {
                return Result<void>::Failure(Error(
                    ErrorCode::INVALID_PARAMETER,
                    "path execution interpolation_points timestamp must be monotonic"));
            }
        }

        if (total_length_mm < 0.0f) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "execution package total_length_mm cannot be negative"));
        }

        if (execution_nominal_time_s < 0.0f) {
            return Result<void>::Failure(
                Error(
                    ErrorCode::INVALID_PARAMETER,
                    "execution package execution_nominal_time_s cannot be negative"));
        }

        if (execution_plan.production_trigger_mode ==
            Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode::PROFILE_COMPARE) {
            const auto& program = execution_plan.profile_compare_program;
            if (program.trigger_points.empty()) {
                return Result<void>::Failure(Error(
                    ErrorCode::INVALID_PARAMETER,
                    "profile_compare execution package missing trigger_points"));
            }
            if (program.expected_trigger_count != program.trigger_points.size()) {
                return Result<void>::Failure(Error(
                    ErrorCode::INVALID_PARAMETER,
                    "profile_compare expected_trigger_count does not match trigger_points"));
            }

            float32 previous_profile_position_mm = -1.0f;
            uint32 previous_sequence_index = 0;
            bool has_previous = false;
            for (const auto& point : program.trigger_points) {
                if (!std::isfinite(point.profile_position_mm) || point.profile_position_mm < 0.0f) {
                    return Result<void>::Failure(Error(
                        ErrorCode::INVALID_PARAMETER,
                        "profile_compare trigger profile_position_mm must be finite and non-negative"));
                }
                if (point.pulse_width_us == 0U) {
                    return Result<void>::Failure(Error(
                        ErrorCode::INVALID_PARAMETER,
                        "profile_compare trigger pulse_width_us must be greater than 0"));
                }
                if (has_previous) {
                    if (point.sequence_index <= previous_sequence_index) {
                        return Result<void>::Failure(Error(
                            ErrorCode::INVALID_PARAMETER,
                            "profile_compare trigger sequence_index must be strictly increasing"));
                    }
                    if (point.profile_position_mm + 1e-6f < previous_profile_position_mm) {
                        return Result<void>::Failure(Error(
                            ErrorCode::INVALID_PARAMETER,
                    "profile_compare trigger profile_position_mm must be monotonic"));
                    }
                }
                previous_sequence_index = point.sequence_index;
                previous_profile_position_mm = point.profile_position_mm;
                has_previous = true;
            }

            if (program.spans.empty()) {
                return Result<void>::Failure(Error(
                    ErrorCode::INVALID_PARAMETER,
                    "profile_compare execution package missing owner span contract"));
            }

            constexpr float32 kSpanToleranceMm = 1e-4f;
            uint32 next_trigger_index = 0U;
            float32 previous_span_end_profile_mm = -1.0f;
            for (const auto& span : program.spans) {
                const auto expected_trigger_count = span.ExpectedTriggerCount();
                if (expected_trigger_count == 0U) {
                    return Result<void>::Failure(Error(
                        ErrorCode::INVALID_PARAMETER,
                        "profile_compare owner span trigger 范围无效"));
                }
                if (span.trigger_begin_index != next_trigger_index) {
                    return Result<void>::Failure(Error(
                        ErrorCode::INVALID_PARAMETER,
                        "profile_compare owner span trigger_begin_index 不连续"));
                }
                if (span.trigger_end_index >= program.trigger_points.size()) {
                    return Result<void>::Failure(Error(
                        ErrorCode::INVALID_PARAMETER,
                        "profile_compare owner span trigger 索引越界"));
                }
                if (span.compare_source_axis < 1 || span.compare_source_axis > 2) {
                    return Result<void>::Failure(Error(
                        ErrorCode::INVALID_PARAMETER,
                        "profile_compare owner span compare_source_axis 仅允许 X/Y"));
                }
                if (span.start_boundary_trigger_count > 1U) {
                    return Result<void>::Failure(Error(
                        ErrorCode::INVALID_PARAMETER,
                        "profile_compare owner span start_boundary_trigger_count 只能为 0 或 1"));
                }
                if (span.start_boundary_trigger_count > expected_trigger_count) {
                    return Result<void>::Failure(Error(
                        ErrorCode::INVALID_PARAMETER,
                        "profile_compare owner span start_boundary_trigger_count 超出 trigger 范围"));
                }
                if (!std::isfinite(span.start_profile_position_mm) ||
                    !std::isfinite(span.end_profile_position_mm)) {
                    return Result<void>::Failure(Error(
                        ErrorCode::INVALID_PARAMETER,
                        "profile_compare owner span profile 边界必须为有限值"));
                }
                if (span.start_profile_position_mm < -kSpanToleranceMm) {
                    return Result<void>::Failure(Error(
                        ErrorCode::INVALID_PARAMETER,
                        "profile_compare owner span profile 起点不能为负"));
                }
                if (span.start_profile_position_mm + kSpanToleranceMm < previous_span_end_profile_mm) {
                    return Result<void>::Failure(Error(
                        ErrorCode::INVALID_PARAMETER,
                        "profile_compare owner span profile 起点必须单调不减"));
                }
                if (span.end_profile_position_mm + kSpanToleranceMm < span.start_profile_position_mm) {
                    return Result<void>::Failure(Error(
                        ErrorCode::INVALID_PARAMETER,
                        "profile_compare owner span profile 终点不能早于起点"));
                }

                const auto& first_trigger = program.trigger_points[span.trigger_begin_index];
                const auto& last_trigger = program.trigger_points[span.trigger_end_index];
                if (first_trigger.profile_position_mm + kSpanToleranceMm < span.start_profile_position_mm) {
                    return Result<void>::Failure(Error(
                        ErrorCode::INVALID_PARAMETER,
                        "profile_compare owner span 起点不能晚于首个 trigger"));
                }
                if (last_trigger.profile_position_mm > span.end_profile_position_mm + kSpanToleranceMm) {
                    return Result<void>::Failure(Error(
                        ErrorCode::INVALID_PARAMETER,
                        "profile_compare owner span 终点不能早于末个 trigger"));
                }

                next_trigger_index = span.trigger_end_index + 1U;
                previous_span_end_profile_mm = span.end_profile_position_mm;
            }
            if (next_trigger_index != program.trigger_points.size()) {
                return Result<void>::Failure(Error(
                    ErrorCode::INVALID_PARAMETER,
                    "profile_compare owner span 未覆盖全部 trigger"));
            }

            if (!execution_plan.completion_contract.enabled) {
                return Result<void>::Failure(Error(
                    ErrorCode::INVALID_PARAMETER,
                    "profile_compare execution package missing completion contract"));
            }
            if (execution_plan.completion_contract.expected_trigger_count !=
                program.expected_trigger_count) {
                return Result<void>::Failure(Error(
                    ErrorCode::INVALID_PARAMETER,
                    "profile_compare completion contract trigger count mismatch"));
            }
            if (execution_plan.completion_contract.final_position_tolerance_mm <= 0.0f) {
                return Result<void>::Failure(Error(
                    ErrorCode::INVALID_PARAMETER,
                    "profile_compare final_position_tolerance_mm must be greater than 0"));
            }
        }

        return Result<void>::Success();
    }
};

struct ExecutionPlanSummary {
    std::string geometry_kind = "unknown";
    std::string execution_strategy = "unknown";
    std::string production_trigger_mode = "unknown";
    uint32 interpolation_segment_count = 0U;
    uint32 interpolation_point_count = 0U;
    uint32 motion_point_count = 0U;
    uint32 trigger_count = 0U;
    uint32 owner_span_count = 0U;
    uint32 immediate_only_span_count = 0U;
    uint32 future_compare_span_count = 0U;
};

struct ExecutionBudgetBreakdown {
    float32 execution_nominal_time_s = 0.0f;
    float32 motion_completion_grace_s = 0.0f;
    uint32 owner_span_count = 0U;
    float32 owner_span_overhead_s = 0.0f;
    float32 cycle_budget_s = 0.0f;
    uint32 target_count = 0U;
    float32 total_budget_s = 0.0f;
};

constexpr float32 kExecutionMotionCompletionGraceRatio = 0.15f;
constexpr uint32 kExecutionMotionCompletionGraceMinMs = 5000U;
constexpr uint32 kExecutionMotionCompletionGraceMaxMs = 30000U;
constexpr float32 kProfileCompareOwnerSpanBudgetOverheadS = 1.0f;

inline float32 ResolveExecutionNominalTimeS(const ExecutionPackageValidated& execution_package) noexcept {
    if (execution_package.execution_nominal_time_s > 0.0f) {
        return execution_package.execution_nominal_time_s;
    }
    if (execution_package.execution_plan.motion_trajectory.total_time > 0.0f) {
        return execution_package.execution_plan.motion_trajectory.total_time;
    }
    if (execution_package.execution_plan.interpolation_points.size() >= 2U) {
        return std::max(0.0f, execution_package.execution_plan.interpolation_points.back().timestamp);
    }
    return 0.0f;
}

inline float32 ComputeExecutionMotionCompletionGraceS(float32 execution_nominal_time_s) noexcept {
    if (execution_nominal_time_s <= 0.0f) {
        return static_cast<float32>(kExecutionMotionCompletionGraceMinMs) / 1000.0f;
    }

    const auto estimated_motion_time_ms = execution_nominal_time_s * 1000.0f;
    const auto ratio_grace_ms = static_cast<int32_t>(std::ceil(
        estimated_motion_time_ms * kExecutionMotionCompletionGraceRatio));
    const auto grace_ms = std::clamp(
        ratio_grace_ms,
        static_cast<int32_t>(kExecutionMotionCompletionGraceMinMs),
        static_cast<int32_t>(kExecutionMotionCompletionGraceMaxMs));
    return static_cast<float32>(grace_ms) / 1000.0f;
}

inline ExecutionPlanSummary BuildExecutionPlanSummary(const ExecutionPackageValidated& execution_package) noexcept {
    ExecutionPlanSummary summary;
    const auto& plan = execution_package.execution_plan;
    summary.geometry_kind = Siligen::Shared::Types::ToString(plan.geometry_kind);
    summary.execution_strategy = Siligen::Shared::Types::ToString(plan.execution_strategy);
    summary.production_trigger_mode =
        Siligen::Domain::Dispensing::ValueObjects::ToString(plan.production_trigger_mode);
    summary.interpolation_segment_count = static_cast<uint32>(plan.interpolation_segments.size());
    summary.interpolation_point_count = static_cast<uint32>(plan.interpolation_points.size());
    summary.motion_point_count = static_cast<uint32>(plan.motion_trajectory.points.size());

    if (plan.production_trigger_mode ==
        Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode::PROFILE_COMPARE) {
        summary.trigger_count = static_cast<uint32>(plan.profile_compare_program.trigger_points.size());
        summary.owner_span_count = static_cast<uint32>(plan.profile_compare_program.spans.size());
        for (const auto& span : plan.profile_compare_program.spans) {
            if (span.ExpectedTriggerCount() <= span.start_boundary_trigger_count) {
                ++summary.immediate_only_span_count;
            } else {
                ++summary.future_compare_span_count;
            }
        }
    }

    return summary;
}

inline ExecutionBudgetBreakdown BuildExecutionBudgetBreakdown(
    const ExecutionPackageValidated& execution_package,
    uint32 target_count) noexcept {
    ExecutionBudgetBreakdown breakdown;
    breakdown.execution_nominal_time_s = ResolveExecutionNominalTimeS(execution_package);
    breakdown.motion_completion_grace_s =
        ComputeExecutionMotionCompletionGraceS(breakdown.execution_nominal_time_s);
    const auto summary = BuildExecutionPlanSummary(execution_package);
    breakdown.owner_span_count = summary.owner_span_count;
    breakdown.owner_span_overhead_s =
        static_cast<float32>(summary.owner_span_count) * kProfileCompareOwnerSpanBudgetOverheadS;
    breakdown.cycle_budget_s =
        breakdown.execution_nominal_time_s + breakdown.motion_completion_grace_s + breakdown.owner_span_overhead_s;
    breakdown.target_count = target_count;
    breakdown.total_budget_s = target_count > 0U
        ? breakdown.cycle_budget_s * static_cast<float32>(target_count)
        : 0.0f;
    return breakdown;
}

}  // namespace Siligen::Domain::Dispensing::Contracts
