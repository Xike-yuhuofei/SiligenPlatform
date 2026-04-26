#include "application/services/dispensing/PlanningAssemblyInternals.h"

#include "application/services/dispensing/PlanningArtifactExportAssemblyService.h"
#include "runtime_execution/contracts/dispensing/ProfileCompareExecutionCompiler.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <optional>
#include <sstream>
#include <utility>
#include <unordered_map>
#include <unordered_set>

namespace Siligen::Application::Services::Dispensing::Internal {

using Siligen::Application::Services::Dispensing::PlanningArtifactExportAssemblyInput;
using Siligen::Application::Services::Dispensing::PlanningArtifactExportAssemblyService;
using Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportGluePointMetadata;
using Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportExecutionTriggerMetadata;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan;
using Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerSourceKind;
using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareProgramSpan;
using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareTriggerPoint;
using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareTriggerProgram;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::uint32;

namespace {

constexpr float32 kDefaultCompletionPositionToleranceMm = 0.2f;

struct ExecutionTrajectorySelection {
    std::vector<TrajectoryPoint> motion_trajectory_points;
    const std::vector<TrajectoryPoint>* execution_trajectory = nullptr;
    bool authority_shared = false;
};

std::vector<TrajectoryPoint> BuildTrajectoryFromMotion(const MotionTrajectory& trajectory) {
    return ConvertMotionTrajectoryToTrajectoryPoints(trajectory);
}

Point2D ResolveExecutionFinalTargetPosition(const DispensingExecutionPlan& plan) {
    if (!plan.motion_trajectory.points.empty()) {
        return Point2D(plan.motion_trajectory.points.back().position);
    }
    if (!plan.interpolation_points.empty()) {
        return plan.interpolation_points.back().position;
    }
    if (!plan.interpolation_segments.empty() &&
        plan.interpolation_segments.back().positions.size() >= 2U) {
        const auto& positions = plan.interpolation_segments.back().positions;
        return Point2D(positions[0], positions[1]);
    }
    return Point2D{};
}

struct AuthorityFormalSpanWindow {
    std::string authority_span_ref;
    uint32 trigger_begin_index = 0U;
    uint32 trigger_end_index = 0U;
    float32 start_profile_position_mm = 0.0f;
    float32 end_profile_position_mm = 0.0f;
    Point2D start_position_mm{};
    Point2D end_position_mm{};
};

struct OwnerSpanFailureDetail {
    std::string message;
    Siligen::Domain::Dispensing::Contracts::FormalCompareGateDiagnostic formal_compare_gate;
};

struct WindowBoundary {
    float32 profile_position_mm = 0.0f;
    Point2D position_mm{};
};

struct WindowSearchFailure {
    uint32 trigger_begin_index = 0U;
    Point2D current_start_position_mm{};
    Point2D next_trigger_position_mm{};
    std::vector<std::string> rejected_candidates;
};

struct WindowSearchState {
    bool reachable = false;
    bool has_failure = false;
    uint32 total_span_count = 0U;
    ProfileCompareProgramSpan chosen_span;
    uint32 next_trigger_index = 0U;
    WindowSearchFailure failure;
};

Result<Point2D> ResolveBoundaryPositionFromInterpolation(
    const DispensingExecutionPlan& plan,
    float32 profile_position_mm) {
    constexpr float32 kToleranceMm = Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm;

    if (plan.interpolation_points.empty()) {
        return Result<Point2D>::Failure(Siligen::Shared::Types::Error(
            Siligen::Shared::Types::ErrorCode::TRAJECTORY_GENERATION_FAILED,
            "profile_compare owner span contract 缺少 interpolation_points，无法解析 span 边界位置",
            "ExecutionAssemblyService"));
    }

    if (plan.interpolation_points.size() == 1U) {
        if (std::fabs(plan.interpolation_points.front().trigger_position_mm - profile_position_mm) <= kToleranceMm ||
            std::fabs(profile_position_mm) <= kToleranceMm) {
            return Result<Point2D>::Success(plan.interpolation_points.front().position);
        }
        return Result<Point2D>::Failure(Siligen::Shared::Types::Error(
            Siligen::Shared::Types::ErrorCode::TRAJECTORY_GENERATION_FAILED,
            "profile_compare owner span contract 单点 interpolation 无法解析非零 span 边界",
            "ExecutionAssemblyService"));
    }

    auto interpolation_points = plan.interpolation_points;
    if (!Siligen::Shared::Types::InsertTriggerMarkerByDistance(
            interpolation_points,
            profile_position_mm,
            kToleranceMm,
            Siligen::Shared::Types::kDefaultTriggerPulseWidthUs)) {
        return Result<Point2D>::Failure(Siligen::Shared::Types::Error(
            Siligen::Shared::Types::ErrorCode::TRAJECTORY_GENERATION_FAILED,
            "profile_compare owner span contract 无法将 span 边界映射到 interpolation_points",
            "ExecutionAssemblyService"));
    }

    const auto marker_it = std::find_if(
        interpolation_points.begin(),
        interpolation_points.end(),
        [&](const auto& point) {
            return std::fabs(point.trigger_position_mm - profile_position_mm) <= kToleranceMm;
        });
    if (marker_it == interpolation_points.end()) {
        return Result<Point2D>::Failure(Siligen::Shared::Types::Error(
            Siligen::Shared::Types::ErrorCode::TRAJECTORY_GENERATION_FAILED,
            "profile_compare owner span contract 未找到已映射的 span 边界点",
            "ExecutionAssemblyService"));
    }
    return Result<Point2D>::Success(marker_it->position);
}

Result<Point2D> ResolveBoundaryPositionFromBindingOrInterpolation(
    const TriggerArtifacts& trigger_artifacts,
    const DispensingExecutionPlan& plan,
    uint32 trigger_index,
    float32 expected_profile_position_mm,
    float32 expected_span_distance_mm,
    const char* boundary_name) {
    constexpr float32 kToleranceMm = Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm;

    const auto& layout = trigger_artifacts.authority_trigger_layout;
    if (trigger_index < layout.trigger_points.size()) {
        const auto& trigger = layout.trigger_points[trigger_index];
        const bool boundary_matches_trigger =
            std::fabs(trigger.distance_mm_global - expected_profile_position_mm) <= kToleranceMm &&
            std::fabs(trigger.distance_mm_span - expected_span_distance_mm) <= kToleranceMm;
        if (boundary_matches_trigger) {
            if (trigger_index >= layout.bindings.size()) {
                return Result<Point2D>::Failure(Siligen::Shared::Types::Error(
                    Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
                    std::string("profile_compare owner span contract 缺少 ") + boundary_name +
                        " boundary trigger binding",
                    "ExecutionAssemblyService"));
            }

            const auto& binding = layout.bindings[trigger_index];
            if (!binding.bound || binding.trigger_ref != trigger.trigger_id) {
                return Result<Point2D>::Failure(Siligen::Shared::Types::Error(
                    Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
                    std::string("profile_compare owner span contract ") + boundary_name +
                        " boundary trigger 未绑定到 execution position",
                    "ExecutionAssemblyService"));
            }
            return Result<Point2D>::Success(binding.execution_position);
        }
    }

    return ResolveBoundaryPositionFromInterpolation(plan, expected_profile_position_mm);
}

bool CandidateIndicatesMonotonicCompareConstraint(const std::string& candidate) {
    return candidate.find("单调正向推进") != std::string::npos ||
        candidate.find("严格递增") != std::string::npos ||
        candidate.find("额外 boundary trigger") != std::string::npos ||
        candidate.find("归一化为 boundary") != std::string::npos;
}

Result<std::vector<AuthorityFormalSpanWindow>> BuildAuthorityFormalSpanWindows(
    const TriggerArtifacts& trigger_artifacts,
    const DispensingExecutionPlan& plan) {
    const auto& layout = trigger_artifacts.authority_trigger_layout;
    const auto& authority_triggers = layout.trigger_points;
    if (authority_triggers.empty()) {
        return Result<std::vector<AuthorityFormalSpanWindow>>::Success({});
    }
    if (layout.spans.empty()) {
        return Result<std::vector<AuthorityFormalSpanWindow>>::Failure(Siligen::Shared::Types::Error(
            Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
            "profile_compare owner span contract 缺少 authority spans",
            "ExecutionAssemblyService"));
    }

    std::unordered_map<std::string, std::size_t> span_index_by_id;
    span_index_by_id.reserve(layout.spans.size());
    for (std::size_t index = 0; index < layout.spans.size(); ++index) {
        span_index_by_id.emplace(layout.spans[index].span_id, index);
    }

    std::vector<std::optional<std::pair<uint32, uint32>>> trigger_ranges(layout.spans.size());
    std::vector<bool> seen_span_blocks(layout.spans.size(), false);
    std::optional<std::size_t> current_span_index;
    std::size_t expected_next_span_index = 0U;
    for (uint32 trigger_index = 0U; trigger_index < authority_triggers.size(); ++trigger_index) {
        const auto& trigger = authority_triggers[trigger_index];
        const auto span_it = span_index_by_id.find(trigger.span_ref);
        if (span_it == span_index_by_id.end()) {
            return Result<std::vector<AuthorityFormalSpanWindow>>::Failure(Siligen::Shared::Types::Error(
                Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
                "profile_compare owner span contract trigger 缺少合法 span_ref",
                "ExecutionAssemblyService"));
        }

        const auto span_index = span_it->second;
        if (!current_span_index.has_value() || current_span_index.value() != span_index) {
            if (seen_span_blocks[span_index]) {
                return Result<std::vector<AuthorityFormalSpanWindow>>::Failure(Siligen::Shared::Types::Error(
                    Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
                    "profile_compare owner span contract trigger 不能跨 authority span 非连续回返",
                    "ExecutionAssemblyService"));
            }
            if (span_index != expected_next_span_index) {
                return Result<std::vector<AuthorityFormalSpanWindow>>::Failure(Siligen::Shared::Types::Error(
                    Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
                    "profile_compare owner span contract trigger 未按 authority span 顺序分组",
                    "ExecutionAssemblyService"));
            }
            seen_span_blocks[span_index] = true;
            current_span_index = span_index;
            trigger_ranges[span_index] = std::make_pair(trigger_index, trigger_index);
            ++expected_next_span_index;
        } else {
            trigger_ranges[span_index]->second = trigger_index;
        }
    }

    std::vector<AuthorityFormalSpanWindow> windows;
    windows.reserve(layout.spans.size());
    for (std::size_t span_index = 0; span_index < layout.spans.size(); ++span_index) {
        if (!trigger_ranges[span_index].has_value()) {
            return Result<std::vector<AuthorityFormalSpanWindow>>::Failure(Siligen::Shared::Types::Error(
                Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
                "profile_compare owner span contract authority span 缺少 trigger",
                "ExecutionAssemblyService"));
        }

        const auto [trigger_begin_index, trigger_end_index] = trigger_ranges[span_index].value();
        const auto& authority_span = layout.spans[span_index];
        const auto& first_trigger = authority_triggers[trigger_begin_index];
        const auto start_profile_position_mm = first_trigger.distance_mm_global - first_trigger.distance_mm_span;
        const auto end_profile_position_mm = start_profile_position_mm + authority_span.total_length_mm;

        auto start_position_result = ResolveBoundaryPositionFromBindingOrInterpolation(
            trigger_artifacts,
            plan,
            trigger_begin_index,
            start_profile_position_mm,
            0.0f,
            "start");
        if (start_position_result.IsError()) {
            return Result<std::vector<AuthorityFormalSpanWindow>>::Failure(start_position_result.GetError());
        }
        auto end_position_result = ResolveBoundaryPositionFromBindingOrInterpolation(
            trigger_artifacts,
            plan,
            trigger_end_index,
            end_profile_position_mm,
            authority_span.total_length_mm,
            "end");
        if (end_position_result.IsError()) {
            return Result<std::vector<AuthorityFormalSpanWindow>>::Failure(end_position_result.GetError());
        }

        AuthorityFormalSpanWindow window;
        window.authority_span_ref = authority_span.span_id;
        window.trigger_begin_index = trigger_begin_index;
        window.trigger_end_index = trigger_end_index;
        window.start_profile_position_mm = start_profile_position_mm;
        window.end_profile_position_mm = end_profile_position_mm;
        window.start_position_mm = start_position_result.Value();
        window.end_position_mm = end_position_result.Value();
        windows.push_back(window);
    }

    return Result<std::vector<AuthorityFormalSpanWindow>>::Success(std::move(windows));
}

WindowBoundary ResolveSearchStartBoundary(
    const AuthorityFormalSpanWindow& authority_window,
    const ProfileCompareTriggerProgram& program,
    uint32 trigger_begin_index) {
    if (trigger_begin_index == authority_window.trigger_begin_index) {
        return WindowBoundary{
            authority_window.start_profile_position_mm,
            authority_window.start_position_mm,
        };
    }

    const auto& trigger = program.trigger_points[trigger_begin_index];
    return WindowBoundary{
        trigger.profile_position_mm,
        trigger.trigger_position_mm,
    };
}

WindowBoundary ResolveSearchEndBoundary(
    const AuthorityFormalSpanWindow& authority_window,
    const ProfileCompareTriggerProgram& program,
    uint32 trigger_end_index) {
    if (trigger_end_index == authority_window.trigger_end_index) {
        return WindowBoundary{
            authority_window.end_profile_position_mm,
            authority_window.end_position_mm,
        };
    }

    const auto& next_trigger = program.trigger_points[trigger_end_index + 1U];
    return WindowBoundary{
        next_trigger.profile_position_mm,
        next_trigger.trigger_position_mm,
    };
}

WindowSearchFailure BuildWindowSearchFailure(
    uint32 trigger_begin_index,
    const Point2D& current_start_position,
    const Point2D& next_trigger_position,
    std::vector<std::string> rejected_candidates) {
    WindowSearchFailure failure;
    failure.trigger_begin_index = trigger_begin_index;
    failure.current_start_position_mm = current_start_position;
    failure.next_trigger_position_mm = next_trigger_position;
    failure.rejected_candidates = std::move(rejected_candidates);
    return failure;
}

bool IsBetterReachableCandidate(
    const ProfileCompareProgramSpan& candidate_span,
    uint32 candidate_total_span_count,
    const WindowSearchState* best_state) {
    if (best_state == nullptr) {
        return true;
    }
    if (candidate_total_span_count != best_state->total_span_count) {
        return candidate_total_span_count < best_state->total_span_count;
    }

    const auto candidate_coverage = candidate_span.ExpectedTriggerCount();
    const auto best_coverage = best_state->chosen_span.ExpectedTriggerCount();
    if (candidate_coverage != best_coverage) {
        return candidate_coverage > best_coverage;
    }

    if (candidate_span.compare_source_axis != best_state->chosen_span.compare_source_axis) {
        return candidate_span.compare_source_axis < best_state->chosen_span.compare_source_axis;
    }
    return false;
}

bool IsBetterFailureCandidate(
    const WindowSearchFailure& candidate_failure,
    const ProfileCompareProgramSpan& candidate_span,
    const WindowSearchState* best_state) {
    if (best_state == nullptr || !best_state->has_failure) {
        return true;
    }
    if (candidate_failure.trigger_begin_index != best_state->failure.trigger_begin_index) {
        return candidate_failure.trigger_begin_index > best_state->failure.trigger_begin_index;
    }

    const auto candidate_coverage = candidate_span.ExpectedTriggerCount();
    const auto best_coverage = best_state->chosen_span.ExpectedTriggerCount();
    if (candidate_coverage != best_coverage) {
        return candidate_coverage > best_coverage;
    }

    if (candidate_span.compare_source_axis != best_state->chosen_span.compare_source_axis) {
        return candidate_span.compare_source_axis < best_state->chosen_span.compare_source_axis;
    }
    return false;
}

OwnerSpanFailureDetail BuildOwnerSpanFailureDetail(
    int32 compare_axis_mask,
    const std::string& authority_span_ref,
    uint32 trigger_begin_index,
    const Point2D& current_start_position,
    const Point2D& next_trigger_position,
    const std::vector<std::string>& rejected_candidates) {
    OwnerSpanFailureDetail detail;
    detail.message = "profile_compare owner 无法构建 formal span contract";
    if ((compare_axis_mask & 0x03) == 0) {
        detail.message += "; cmp_axis_mask 未包含 formal runtime 受支持的 X/Y compare 投影轴";
    }
    detail.message += "; trigger_begin_index=" + std::to_string(trigger_begin_index);
    {
        std::ostringstream oss;
        oss << "; current_start_position_mm=(" << current_start_position.x << "," << current_start_position.y << ")";
        oss << "; next_trigger_position_mm=(" << next_trigger_position.x << "," << next_trigger_position.y << ")";
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
    detail.formal_compare_gate.status = "production_blocked";
    detail.formal_compare_gate.reason_code = "unresolved_formal_compare_span";
    detail.formal_compare_gate.authority_span_ref = authority_span_ref;
    detail.formal_compare_gate.trigger_begin_index = trigger_begin_index;
    detail.formal_compare_gate.current_start_position_mm = current_start_position;
    detail.formal_compare_gate.next_trigger_position_mm = next_trigger_position;
    detail.formal_compare_gate.candidate_failures = rejected_candidates;
    if (!rejected_candidates.empty() &&
        std::all_of(
            rejected_candidates.begin(),
            rejected_candidates.end(),
            [](const std::string& candidate) {
                return CandidateIndicatesMonotonicCompareConstraint(candidate);
            })) {
        detail.message += "; owner formal span contract 当前仅支持递增 compare 位置，当前 span 需要下降或回返 compare 几何";
        detail.formal_compare_gate.reason_code = "descending_or_returning_compare_geometry";
    }
    return detail;
}

Result<ProfileCompareTriggerProgram> BuildProfileCompareProgram(
    const TriggerArtifacts& trigger_artifacts,
    const DispensingExecutionPlan& plan,
    const Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareRuntimeContract& runtime_contract,
    Siligen::Domain::Dispensing::Contracts::FormalCompareGateDiagnostic* formal_compare_gate) {
    if (formal_compare_gate != nullptr) {
        *formal_compare_gate = {};
    }
    const auto& layout = trigger_artifacts.authority_trigger_layout;
    ProfileCompareTriggerProgram program;
    if (layout.trigger_points.empty()) {
        return Result<ProfileCompareTriggerProgram>::Success(std::move(program));
    }
    if (layout.bindings.size() != layout.trigger_points.size()) {
        return Result<ProfileCompareTriggerProgram>::Failure(Siligen::Shared::Types::Error(
            Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
            "profile_compare owner span contract 缺少完整 execution bindings",
            "ExecutionAssemblyService"));
    }

    program.trigger_points.reserve(layout.trigger_points.size());
    for (std::size_t index = 0; index < layout.trigger_points.size(); ++index) {
        const auto& trigger = layout.trigger_points[index];
        const auto& binding = layout.bindings[index];
        if (binding.trigger_ref != trigger.trigger_id || !binding.bound) {
            return Result<ProfileCompareTriggerProgram>::Failure(Siligen::Shared::Types::Error(
                Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
                "profile_compare owner span contract 缺少已绑定 execution trigger",
                "ExecutionAssemblyService"));
        }
        if (!binding.monotonic) {
            return Result<ProfileCompareTriggerProgram>::Failure(Siligen::Shared::Types::Error(
                Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
                "profile_compare owner span contract 存在非单调 binding",
                "ExecutionAssemblyService"));
        }
        if (binding.interpolation_index >= plan.interpolation_points.size()) {
            return Result<ProfileCompareTriggerProgram>::Failure(Siligen::Shared::Types::Error(
                Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
                "profile_compare owner span contract binding 超出 interpolation_points 范围",
                "ExecutionAssemblyService"));
        }

        ProfileCompareTriggerPoint trigger_point;
        trigger_point.sequence_index = static_cast<uint32>(index);
        trigger_point.profile_position_mm = trigger.distance_mm_global;
        trigger_point.trigger_position_mm = binding.execution_position;
        const auto pulse_width_us = plan.interpolation_points[binding.interpolation_index].trigger_pulse_width_us;
        trigger_point.pulse_width_us = pulse_width_us > 0U
            ? pulse_width_us
            : Siligen::Shared::Types::kDefaultTriggerPulseWidthUs;
        program.trigger_points.push_back(trigger_point);
    }
    program.expected_trigger_count = static_cast<uint32>(program.trigger_points.size());

    auto authority_windows_result = BuildAuthorityFormalSpanWindows(trigger_artifacts, plan);
    if (authority_windows_result.IsError()) {
        return Result<ProfileCompareTriggerProgram>::Failure(authority_windows_result.GetError());
    }

    const auto authority_windows = authority_windows_result.Value();
    for (const auto& authority_window : authority_windows) {
        const auto terminal_trigger_index = authority_window.trigger_end_index + 1U;
        std::vector<WindowSearchState> states(
            terminal_trigger_index - authority_window.trigger_begin_index + 1U);

        auto state_at = [&](uint32 trigger_index) -> WindowSearchState& {
            return states[trigger_index - authority_window.trigger_begin_index];
        };
        auto const_state_at = [&](uint32 trigger_index) -> const WindowSearchState& {
            return states[trigger_index - authority_window.trigger_begin_index];
        };

        state_at(terminal_trigger_index).reachable = true;
        state_at(terminal_trigger_index).total_span_count = 0U;
        state_at(terminal_trigger_index).next_trigger_index = terminal_trigger_index;

        for (auto trigger_index = static_cast<int32>(authority_window.trigger_end_index);
             trigger_index >= static_cast<int32>(authority_window.trigger_begin_index);
             --trigger_index) {
            const auto current_trigger_index = static_cast<uint32>(trigger_index);
            auto& current_state = state_at(current_trigger_index);
            const auto start_boundary =
                ResolveSearchStartBoundary(authority_window, program, current_trigger_index);

            std::optional<WindowSearchFailure> local_failure;
            WindowSearchState best_reachable_state;
            bool has_best_reachable = false;
            WindowSearchState best_failure_state;
            bool has_best_failure = false;

            for (uint32 trigger_end_index = current_trigger_index;
                 trigger_end_index <= authority_window.trigger_end_index;
                 ++trigger_end_index) {
                const auto end_boundary =
                    ResolveSearchEndBoundary(authority_window, program, trigger_end_index);

                ProfileCompareProgramSpan candidate_span;
                candidate_span.trigger_begin_index = current_trigger_index;
                candidate_span.trigger_end_index = trigger_end_index;
                candidate_span.start_profile_position_mm = start_boundary.profile_position_mm;
                candidate_span.end_profile_position_mm = end_boundary.profile_position_mm;
                candidate_span.start_position_mm = start_boundary.position_mm;
                candidate_span.end_position_mm = end_boundary.position_mm;

                auto coverage_result =
                    Siligen::RuntimeExecution::Contracts::Dispensing::EvaluateProfileCompareOwnerSpanCoverage(
                        program,
                        candidate_span,
                        runtime_contract);
                if (coverage_result.IsError()) {
                    return Result<ProfileCompareTriggerProgram>::Failure(coverage_result.GetError());
                }

                auto coverage = coverage_result.Value();
                if (!coverage.compiled) {
                    if (trigger_end_index == current_trigger_index && !local_failure.has_value()) {
                        local_failure = BuildWindowSearchFailure(
                            current_trigger_index,
                            start_boundary.position_mm,
                            program.trigger_points[current_trigger_index].trigger_position_mm,
                            std::move(coverage.rejected_candidates));
                    }
                    continue;
                }

                candidate_span.compare_source_axis = coverage.execution_span.compare_source_axis;
                candidate_span.start_boundary_trigger_count =
                    coverage.execution_span.start_boundary_trigger_count;

                const auto next_trigger_index = trigger_end_index + 1U;
                const auto& next_state = const_state_at(next_trigger_index);
                if (next_state.reachable) {
                    const auto candidate_total_span_count = next_state.total_span_count + 1U;
                    if (!has_best_reachable ||
                        IsBetterReachableCandidate(
                            candidate_span,
                            candidate_total_span_count,
                            &best_reachable_state)) {
                        has_best_reachable = true;
                        best_reachable_state.reachable = true;
                        best_reachable_state.has_failure = false;
                        best_reachable_state.total_span_count = candidate_total_span_count;
                        best_reachable_state.chosen_span = candidate_span;
                        best_reachable_state.next_trigger_index = next_trigger_index;
                    }
                    continue;
                }

                if (next_state.has_failure &&
                    (!has_best_failure ||
                     IsBetterFailureCandidate(next_state.failure, candidate_span, &best_failure_state))) {
                    has_best_failure = true;
                    best_failure_state.reachable = false;
                    best_failure_state.has_failure = true;
                    best_failure_state.chosen_span = candidate_span;
                    best_failure_state.next_trigger_index = next_trigger_index;
                    best_failure_state.failure = next_state.failure;
                }
            }

            if (has_best_reachable) {
                current_state = best_reachable_state;
                continue;
            }

            current_state.reachable = false;
            current_state.has_failure = true;
            if (has_best_failure) {
                current_state = best_failure_state;
            } else if (local_failure.has_value()) {
                current_state.failure = std::move(local_failure.value());
            } else {
                current_state.failure = BuildWindowSearchFailure(
                    current_trigger_index,
                    start_boundary.position_mm,
                    program.trigger_points[current_trigger_index].trigger_position_mm,
                    {});
            }
        }

        const auto& root_state = const_state_at(authority_window.trigger_begin_index);
        if (!root_state.reachable) {
            const auto failure_detail = BuildOwnerSpanFailureDetail(
                runtime_contract.compare_axis_mask,
                authority_window.authority_span_ref,
                root_state.failure.trigger_begin_index,
                root_state.failure.current_start_position_mm,
                root_state.failure.next_trigger_position_mm,
                root_state.failure.rejected_candidates);
            if (formal_compare_gate != nullptr) {
                *formal_compare_gate = failure_detail.formal_compare_gate;
            }
            return Result<ProfileCompareTriggerProgram>::Failure(Siligen::Shared::Types::Error(
                Siligen::Shared::Types::ErrorCode::CMP_TRIGGER_SETUP_FAILED,
                failure_detail.message,
                "ExecutionAssemblyService"));
        }

        for (uint32 next_trigger_index = authority_window.trigger_begin_index;
             next_trigger_index < terminal_trigger_index;
             next_trigger_index = const_state_at(next_trigger_index).next_trigger_index) {
            const auto& state = const_state_at(next_trigger_index);
            program.spans.push_back(state.chosen_span);
        }
    }

    return Result<ProfileCompareTriggerProgram>::Success(std::move(program));
}

const char* ToString(LayoutTriggerSourceKind source_kind) {
    switch (source_kind) {
        case LayoutTriggerSourceKind::Anchor:
            return "anchor";
        case LayoutTriggerSourceKind::Generated:
            return "generated";
        case LayoutTriggerSourceKind::SharedVertex:
            return "shared_vertex";
    }
    return "generated";
}

Result<void> ValidateExecutionStrategyFeasibility(
    DispensingExecutionStrategy requested_strategy,
    DispensingExecutionGeometryKind geometry_kind) {
    if (geometry_kind != DispensingExecutionGeometryKind::POINT &&
        requested_strategy == DispensingExecutionStrategy::STATIONARY_SHOT) {
        return Result<void>::Failure(Siligen::Shared::Types::Error(
            Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
            "requested_execution_strategy=stationary_shot only supports POINT geometry",
            "ExecutionAssemblyService"));
    }

    return Result<void>::Success();
}

ExecutionTrajectorySelection SelectExecutionTrajectory(const ExecutionPackageValidated& execution_package) {
    ExecutionTrajectorySelection selection;
    selection.motion_trajectory_points = BuildTrajectoryFromMotion(execution_package.execution_plan.motion_trajectory);
    if (!execution_package.execution_plan.interpolation_points.empty()) {
        selection.execution_trajectory = &execution_package.execution_plan.interpolation_points;
        selection.authority_shared = true;
    }
    return selection;
}

PlanningArtifactsAssemblyInput BuildExecutionPlanningInput(
    const ExecutionAssemblyBuildInput& input,
    const ProcessPath& execution_process_path) {
    PlanningArtifactsAssemblyInput execution_input;
    execution_input.process_path = execution_process_path;
    execution_input.authority_process_path = input.authority_process_path;
    execution_input.motion_plan = input.motion_plan;
    execution_input.source_path = input.source_path;
    execution_input.dxf_filename = input.dxf_filename;
    execution_input.dispensing_velocity = input.dispensing_velocity;
    execution_input.acceleration = input.acceleration;
    execution_input.dispenser_interval_ms = input.dispenser_interval_ms;
    execution_input.dispenser_duration_ms = input.dispenser_duration_ms;
    execution_input.trigger_spatial_interval_mm = input.trigger_spatial_interval_mm;
    execution_input.valve_response_ms = input.valve_response_ms;
    execution_input.safety_margin_ms = input.safety_margin_ms;
    execution_input.min_interval_ms = input.min_interval_ms;
    execution_input.max_jerk = input.max_jerk;
    execution_input.sample_dt = input.sample_dt;
    execution_input.sample_ds = input.sample_ds;
    execution_input.curve_flatten_max_step_mm = input.curve_flatten_max_step_mm;
    execution_input.curve_flatten_max_error_mm = input.curve_flatten_max_error_mm;
    execution_input.execution_nominal_time_s = input.execution_nominal_time_s;
    execution_input.use_interpolation_planner = input.use_interpolation_planner;
    execution_input.interpolation_algorithm = input.interpolation_algorithm;
    execution_input.compensation_profile = input.compensation_profile;
    return execution_input;
}

}  // namespace

TriggerArtifacts BuildTriggerArtifactsFromAuthorityPreview(
    const AuthorityPreviewBuildResult& authority_preview) {
    TriggerArtifacts artifacts;
    artifacts.authority_trigger_layout = authority_preview.authority_trigger_layout;
    artifacts.authority_trigger_points = authority_preview.authority_trigger_points;
    artifacts.spacing_validation_groups = authority_preview.spacing_validation_groups;
    artifacts.positions = authority_preview.glue_points;
    artifacts.validation_classification = authority_preview.preview_validation_classification;
    artifacts.exception_reason = authority_preview.preview_exception_reason;
    artifacts.failure_reason = authority_preview.preview_failure_reason;
    artifacts.spacing_valid = authority_preview.preview_spacing_valid;
    artifacts.has_short_segment_exceptions = authority_preview.preview_has_short_segment_exceptions;
    artifacts.binding_ready = authority_preview.preview_binding_ready;
    artifacts.authority_trigger_layout.binding_ready = authority_preview.preview_binding_ready;
    for (const auto& trigger : authority_preview.authority_trigger_points) {
        artifacts.distances.push_back(trigger.trigger_distance_mm);
    }
    return artifacts;
}

Result<ExecutionPackageValidated> BuildValidatedExecutionPackage(
    const ExecutionAssemblyBuildInput& input,
    const ProcessPath& execution_process_path,
    const TriggerArtifacts& trigger_artifacts,
    ExecutionGenerationArtifacts generation_artifacts) {
    auto log_stage = [&](const char* stage, const std::string& detail = std::string()) {
        std::ostringstream oss;
        oss << "planning_artifacts_stage=" << stage
            << " dxf=" << input.dxf_filename
            << " authority_segments=" << input.authority_process_path.segments.size()
            << " canonical_execution_segments=" << input.canonical_execution_process_path.segments.size()
            << " execution_segments=" << execution_process_path.segments.size()
            << " motion_points=" << input.motion_plan.points.size()
            << " preview_layout=" << input.authority_preview.authority_trigger_layout.layout_id;
        if (!detail.empty()) {
            oss << ' ' << detail;
        }
        SILIGEN_LOG_INFO(oss.str());
    };

    ExecutionPackageBuilt built;
    built.execution_plan.geometry_kind = ResolveExecutionGeometryKind(execution_process_path);
    auto strategy_validation = ValidateExecutionStrategyFeasibility(
        input.requested_execution_strategy,
        built.execution_plan.geometry_kind);
    if (strategy_validation.IsError()) {
        return Result<ExecutionPackageValidated>::Failure(strategy_validation.GetError());
    }
    built.execution_plan.execution_strategy = ResolveExecutionStrategy(
        input.requested_execution_strategy,
        built.execution_plan.geometry_kind,
        generation_artifacts,
        input.motion_plan);
    built.execution_plan.interpolation_segments = std::move(generation_artifacts.interpolation_segments);
    built.execution_plan.interpolation_points = std::move(generation_artifacts.interpolation_points);
    built.execution_plan.motion_trajectory = generation_artifacts.motion_trajectory.points.empty()
        ? input.motion_plan
        : std::move(generation_artifacts.motion_trajectory);
    built.execution_plan.trigger_distances_mm = trigger_artifacts.distances;
    built.execution_plan.trigger_interval_ms = trigger_artifacts.interval_ms;
    built.execution_plan.trigger_interval_mm = trigger_artifacts.interval_mm;
    built.execution_plan.total_length_mm = built.execution_plan.motion_trajectory.total_length > kEpsilon
        ? built.execution_plan.motion_trajectory.total_length
        : input.motion_plan.total_length > kEpsilon
            ? input.motion_plan.total_length
        : ComputeProcessPathLength(execution_process_path);
    built.total_length_mm = built.execution_plan.total_length_mm;
    built.execution_nominal_time_s = input.execution_nominal_time_s;
    built.source_path = input.source_path;
    built.source_fingerprint = input.authority_preview.authority_trigger_layout.layout_id;
    built.execution_nominal_time_s =
        EstimateExecutionTime(BuildExecutionPlanningInput(input, execution_process_path), built);

    ExecutionPackageValidated execution_package(std::move(built));
    auto package_validation = execution_package.Validate();
    if (package_validation.IsError()) {
        return Result<ExecutionPackageValidated>::Failure(package_validation.GetError());
    }
    {
        std::ostringstream oss;
        oss << "trajectory_points=" << execution_package.execution_plan.motion_trajectory.points.size()
            << " interpolation_segments=" << execution_package.execution_plan.interpolation_segments.size()
            << " interpolation_points=" << execution_package.execution_plan.interpolation_points.size()
            << " geometry_kind=" << Siligen::Shared::Types::ToString(execution_package.execution_plan.geometry_kind)
            << " execution_strategy="
            << Siligen::Shared::Types::ToString(execution_package.execution_plan.execution_strategy);
        log_stage("execution_package_ready", oss.str());
    }

    return Result<ExecutionPackageValidated>::Success(std::move(execution_package));
}

Result<ExecutionPackageValidated> BuildFormalProfileCompareExecutionPackage(
    const ExecutionPackageValidated& base_package,
    const TriggerArtifacts& trigger_artifacts,
    const Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareRuntimeContract& runtime_contract,
    Siligen::Domain::Dispensing::Contracts::FormalCompareGateDiagnostic* formal_compare_gate) {
    if (formal_compare_gate != nullptr) {
        *formal_compare_gate = {};
    }
    auto program_result = BuildProfileCompareProgram(
        trigger_artifacts,
        base_package.execution_plan,
        runtime_contract,
        formal_compare_gate);
    if (program_result.IsError()) {
        return Result<ExecutionPackageValidated>::Failure(program_result.GetError());
    }

    const auto program = program_result.Value();
    if (program.Empty()) {
        return Result<ExecutionPackageValidated>::Success(base_package);
    }

    auto execution_package = base_package;
    execution_package.execution_plan.production_trigger_mode =
        Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode::PROFILE_COMPARE;
    execution_package.execution_plan.profile_compare_program = program;
    execution_package.execution_plan.completion_contract.enabled = true;
    execution_package.execution_plan.completion_contract.final_target_position_mm =
        ResolveExecutionFinalTargetPosition(execution_package.execution_plan);
    execution_package.execution_plan.completion_contract.final_position_tolerance_mm =
        kDefaultCompletionPositionToleranceMm;
    execution_package.execution_plan.completion_contract.expected_trigger_count =
        program.expected_trigger_count;

    auto package_validation = execution_package.Validate();
    if (package_validation.IsError()) {
        return Result<ExecutionPackageValidated>::Failure(package_validation.GetError());
    }

    return Result<ExecutionPackageValidated>::Success(std::move(execution_package));
}

void PopulateExecutionTrajectorySelection(
    const ExecutionPackageValidated& execution_package,
    ExecutionAssemblyBuildResult& result,
    bool& authority_shared) {
    const auto selection = SelectExecutionTrajectory(execution_package);
    authority_shared = selection.authority_shared;
    if (selection.execution_trajectory) {
        result.execution_trajectory_points = *selection.execution_trajectory;
    }
    result.interpolation_trajectory_points = execution_package.execution_plan.interpolation_points;
    result.motion_trajectory_points = selection.motion_trajectory_points;
}

Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest BuildExecutionExportRequest(
    const ExecutionAssemblyBuildInput& input,
    const ProcessPath& execution_process_path,
    const ExecutionAssemblyBuildResult& result) {
    PlanningArtifactExportAssemblyService export_assembly_service;
    PlanningArtifactExportAssemblyInput export_input;
    export_input.source_path = input.source_path;
    export_input.dxf_filename = input.dxf_filename;
    export_input.process_path = execution_process_path;
    export_input.glue_points = CollectAuthorityPositions(result.authority_trigger_layout);
    export_input.glue_distances_mm.reserve(result.authority_trigger_layout.trigger_points.size());
    export_input.glue_point_metadata.reserve(result.authority_trigger_layout.trigger_points.size());
    export_input.execution_trigger_metadata.reserve(result.authority_trigger_layout.bindings.size());
    std::unordered_map<std::string, const Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerPoint*> triggers_by_id;
    triggers_by_id.reserve(result.authority_trigger_layout.trigger_points.size());
    std::unordered_map<std::string, const Siligen::Domain::Dispensing::ValueObjects::DispenseSpan*> spans_by_id;
    spans_by_id.reserve(result.authority_trigger_layout.spans.size());
    for (const auto& trigger : result.authority_trigger_layout.trigger_points) {
        triggers_by_id.emplace(trigger.trigger_id, &trigger);
    }
    for (const auto& span : result.authority_trigger_layout.spans) {
        spans_by_id.emplace(span.span_id, &span);
    }
    for (const auto& trigger : result.authority_trigger_layout.trigger_points) {
        export_input.glue_distances_mm.push_back(trigger.distance_mm_global);
        PlanningArtifactExportGluePointMetadata metadata;
        metadata.span_ref = trigger.span_ref;
        metadata.sequence_index_global = trigger.sequence_index_global;
        metadata.sequence_index_span = trigger.sequence_index_span;
        metadata.source_segment_index = trigger.source_segment_index;
        metadata.distance_mm_span = trigger.distance_mm_span;
        metadata.source_kind = ToString(trigger.source_kind);
        const auto span_it = spans_by_id.find(trigger.span_ref);
        if (span_it != spans_by_id.end() && span_it->second != nullptr) {
            const auto& span = *span_it->second;
            metadata.component_index = span.component_index;
            metadata.span_order_index = span.order_index;
            metadata.span_closed = span.closed;
            metadata.span_total_length_mm = span.total_length_mm;
            metadata.span_actual_spacing_mm = span.actual_spacing_mm;
            metadata.span_phase_mm = span.phase_mm;
            metadata.span_dispatch_type = Siligen::Domain::Dispensing::ValueObjects::ToString(span.dispatch_type);
            metadata.span_split_reason = Siligen::Domain::Dispensing::ValueObjects::ToString(span.split_reason);
        }
        export_input.glue_point_metadata.push_back(std::move(metadata));
    }
    for (const auto& binding : result.authority_trigger_layout.bindings) {
        if (!binding.bound) {
            continue;
        }

        PlanningArtifactExportExecutionTriggerMetadata metadata;
        metadata.trajectory_index = binding.interpolation_index;
        metadata.authority_trigger_ref = binding.trigger_ref;
        metadata.binding_match_error_mm = binding.match_error_mm;
        metadata.binding_monotonic = binding.monotonic;
        const auto trigger_it = triggers_by_id.find(binding.trigger_ref);
        if (trigger_it != triggers_by_id.end() && trigger_it->second != nullptr) {
            const auto& trigger = *trigger_it->second;
            metadata.authority_trigger_index = trigger.sequence_index_global;
            metadata.source_segment_index = trigger.source_segment_index;
            metadata.authority_distance_mm = trigger.distance_mm_global;
            metadata.span_ref = trigger.span_ref;
            const auto span_it = spans_by_id.find(trigger.span_ref);
            if (span_it != spans_by_id.end() && span_it->second != nullptr) {
                const auto& span = *span_it->second;
                metadata.component_index = span.component_index;
                metadata.span_order_index = span.order_index;
            }
        }
        export_input.execution_trigger_metadata.push_back(std::move(metadata));
    }
    export_input.execution_trajectory_points = result.execution_trajectory_points;
    export_input.interpolation_trajectory_points = result.interpolation_trajectory_points;
    export_input.motion_trajectory_points = result.motion_trajectory_points;
    return export_assembly_service.BuildRequest(export_input);
}

}  // namespace Siligen::Application::Services::Dispensing::Internal
