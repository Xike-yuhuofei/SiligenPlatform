#include "ClosedLoopAnchorConstraintSolver.h"

#include "ClosedLoopPlanningUtils.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Siligen::Domain::Dispensing::DomainServices::Internal {

namespace {

constexpr std::size_t kClosedPhaseCandidateCount = 32U;

void AppendUniquePhaseCandidate(std::vector<float32>& candidates,
                                float32 candidate_phase_mm,
                                float32 actual_spacing_mm) {
    const float32 wrapped_phase_mm = WrapLoopDistance(candidate_phase_mm, actual_spacing_mm);
    for (float32 existing_phase_mm : candidates) {
        if (CircularDistance(existing_phase_mm, wrapped_phase_mm, actual_spacing_mm) <=
            kClosedLoopPlanningEpsilon) {
            return;
        }
    }
    candidates.push_back(wrapped_phase_mm);
}

std::vector<float32> BuildAnchorPhaseCandidates(const std::vector<StrongAnchor>& anchors,
                                                float32 actual_spacing_mm) {
    std::vector<float32> phase_candidates;
    if (actual_spacing_mm <= kClosedLoopPlanningEpsilon) {
        return phase_candidates;
    }

    std::vector<float32> residues_mm;
    residues_mm.reserve(anchors.size());
    for (const auto& anchor : anchors) {
        if (!anchor.required) {
            continue;
        }
        const float32 residue_mm = WrapLoopDistance(anchor.distance_mm_span, actual_spacing_mm);
        residues_mm.push_back(residue_mm);
        AppendUniquePhaseCandidate(phase_candidates, residue_mm, actual_spacing_mm);
    }

    if (residues_mm.empty()) {
        AppendUniquePhaseCandidate(phase_candidates, 0.0f, actual_spacing_mm);
        return phase_candidates;
    }

    for (float32 reference_mm : residues_mm) {
        float32 adjusted_sum_mm = 0.0f;
        for (float32 residue_mm : residues_mm) {
            float32 adjusted_mm = residue_mm;
            while (adjusted_mm - reference_mm > actual_spacing_mm * 0.5f) {
                adjusted_mm -= actual_spacing_mm;
            }
            while (reference_mm - adjusted_mm > actual_spacing_mm * 0.5f) {
                adjusted_mm += actual_spacing_mm;
            }
            adjusted_sum_mm += adjusted_mm;
        }
        AppendUniquePhaseCandidate(
            phase_candidates,
            adjusted_sum_mm / static_cast<float32>(residues_mm.size()),
            actual_spacing_mm);
    }

    return phase_candidates;
}

bool TryResolveAnchorPhaseForInterval(const std::vector<StrongAnchor>& anchors,
                                      float32 total_length_mm,
                                      float32 actual_spacing_mm,
                                      float32 anchor_tolerance_mm,
                                      float32& phase_mm_out) {
    const auto phase_candidates = BuildAnchorPhaseCandidates(anchors, actual_spacing_mm);
    if (phase_candidates.empty()) {
        return false;
    }

    bool found_valid_phase = false;
    float32 best_phase_mm = 0.0f;
    float32 best_max_error_mm = std::numeric_limits<float32>::infinity();
    for (float32 phase_candidate_mm : phase_candidates) {
        bool valid = true;
        float32 max_error_mm = 0.0f;
        for (const auto& anchor : anchors) {
            if (!anchor.required) {
                continue;
            }
            const float32 residue_mm = WrapLoopDistance(anchor.distance_mm_span, actual_spacing_mm);
            const float32 error_mm = CircularDistance(phase_candidate_mm, residue_mm, actual_spacing_mm);
            max_error_mm = std::max(max_error_mm, error_mm);
            if (error_mm > anchor_tolerance_mm + kClosedLoopPlanningEpsilon) {
                valid = false;
                break;
            }
        }

        if (!valid) {
            continue;
        }
        if (!found_valid_phase ||
            max_error_mm < best_max_error_mm - kClosedLoopPlanningEpsilon ||
            (std::fabs(max_error_mm - best_max_error_mm) <= kClosedLoopPlanningEpsilon &&
             phase_candidate_mm < best_phase_mm)) {
            found_valid_phase = true;
            best_phase_mm = phase_candidate_mm;
            best_max_error_mm = max_error_mm;
        }
    }

    if (!found_valid_phase) {
        return false;
    }

    phase_mm_out = WrapLoopDistance(best_phase_mm, total_length_mm);
    return true;
}

std::vector<std::size_t> BuildExpandedDivisionCounts(std::size_t target_count,
                                                     std::size_t min_count,
                                                     std::size_t max_count) {
    const std::size_t expansion_limit = std::max<std::size_t>(
        8U,
        static_cast<std::size_t>(std::ceil(static_cast<float32>(target_count) * 0.1f)));
    std::vector<std::size_t> counts;
    counts.reserve(expansion_limit * 2U + 1U);
    for (std::size_t offset = 0; offset <= expansion_limit; ++offset) {
        if (offset == 0U) {
            if (target_count < min_count || target_count > max_count) {
                counts.push_back(target_count);
            }
            continue;
        }

        if (target_count > offset) {
            const std::size_t lower_count = target_count - offset;
            if (lower_count < min_count || lower_count > max_count) {
                counts.push_back(lower_count);
            }
        }

        const std::size_t upper_count = target_count + offset;
        if (upper_count < min_count || upper_count > max_count) {
            counts.push_back(upper_count);
        }
    }
    return counts;
}

float32 ScoreClosedLoopPhase(float32 phase_mm,
                             float32 total_length_mm,
                             float32 actual_spacing_mm,
                             std::size_t interval_count,
                             const std::vector<float32>& boundaries) {
    float32 min_clearance_mm = std::numeric_limits<float32>::infinity();
    for (std::size_t index = 0; index < interval_count; ++index) {
        const float32 trigger_distance_mm = WrapLoopDistance(
            phase_mm + actual_spacing_mm * static_cast<float32>(index),
            total_length_mm);
        min_clearance_mm = std::min(
            min_clearance_mm,
            CircularDistance(trigger_distance_mm, 0.0f, total_length_mm));
        for (float32 boundary_mm : boundaries) {
            min_clearance_mm = std::min(
                min_clearance_mm,
                CircularDistance(trigger_distance_mm, boundary_mm, total_length_mm));
        }
    }
    return std::isfinite(min_clearance_mm) ? min_clearance_mm : 0.0f;
}

}  // namespace

ClosedLoopAnchorSolveResult ClosedLoopAnchorConstraintSolver::Solve(
    const ClosedLoopAnchorConstraintSolverInput& input) const {
    ClosedLoopAnchorSolveResult result;
    if (input.strong_anchors == nullptr ||
        input.strong_anchors->empty() ||
        input.total_length_mm <= kClosedLoopPlanningEpsilon ||
        input.target_spacing_mm <= kClosedLoopPlanningEpsilon) {
        return result;
    }

    const std::size_t min_count = std::max<std::size_t>(
        1U,
        static_cast<std::size_t>(
            std::ceil(input.total_length_mm / std::max(input.max_spacing_mm, kClosedLoopPlanningEpsilon) -
                      kClosedLoopPlanningEpsilon)));
    const std::size_t max_count = std::max<std::size_t>(
        min_count,
        static_cast<std::size_t>(
            std::floor(input.total_length_mm / std::max(input.min_spacing_mm, kClosedLoopPlanningEpsilon) +
                       kClosedLoopPlanningEpsilon)));

    bool found_window_solution = false;
    float32 best_spacing_error_mm = std::numeric_limits<float32>::infinity();
    for (std::size_t division_count = min_count; division_count <= max_count; ++division_count) {
        const float32 actual_spacing_mm = input.total_length_mm / static_cast<float32>(division_count);
        float32 phase_mm = 0.0f;
        if (!TryResolveAnchorPhaseForInterval(
                *input.strong_anchors,
                input.total_length_mm,
                actual_spacing_mm,
                input.anchor_tolerance_mm,
                phase_mm)) {
            continue;
        }

        const float32 spacing_error_mm = std::fabs(actual_spacing_mm - input.target_spacing_mm);
        if (!found_window_solution ||
            spacing_error_mm < best_spacing_error_mm - kClosedLoopPlanningEpsilon ||
            (std::fabs(spacing_error_mm - best_spacing_error_mm) <= kClosedLoopPlanningEpsilon &&
             phase_mm < result.phase_mm)) {
            found_window_solution = true;
            best_spacing_error_mm = spacing_error_mm;
            result.solved = true;
            result.within_window = true;
            result.interval_count = division_count;
            result.actual_spacing_mm = actual_spacing_mm;
            result.phase_mm = phase_mm;
            result.anchor_constraint_state = "satisfied";
        }
    }

    if (found_window_solution) {
        return result;
    }

    const std::size_t target_count = std::max<std::size_t>(
        1U,
        static_cast<std::size_t>(std::lround(input.total_length_mm / input.target_spacing_mm)));
    const auto expanded_counts = BuildExpandedDivisionCounts(target_count, min_count, max_count);
    for (std::size_t division_count : expanded_counts) {
        const float32 actual_spacing_mm = input.total_length_mm / static_cast<float32>(division_count);
        float32 phase_mm = 0.0f;
        if (!TryResolveAnchorPhaseForInterval(
                *input.strong_anchors,
                input.total_length_mm,
                actual_spacing_mm,
                input.anchor_tolerance_mm,
                phase_mm)) {
            continue;
        }

        result.solved = true;
        result.within_window = false;
        result.interval_count = division_count;
        result.actual_spacing_mm = actual_spacing_mm;
        result.phase_mm = phase_mm;
        result.anchor_constraint_state = "satisfied_with_exception";
        result.anchor_exception_reason = "anchor_constrained_spacing_outside_window";
        return result;
    }

    result.anchor_constraint_state = "unsatisfied";
    result.blocking_reason = "closed loop anchor constraints unsatisfied";
    return result;
}

float32 ClosedLoopAnchorConstraintSolver::ResolvePhase(
    float32 total_length_mm,
    float32 actual_spacing_mm,
    std::size_t interval_count,
    const std::vector<float32>& boundaries) const {
    if (total_length_mm <= kClosedLoopPlanningEpsilon ||
        actual_spacing_mm <= kClosedLoopPlanningEpsilon ||
        interval_count == 0U) {
        return 0.0f;
    }

    float32 best_phase_mm = 0.0f;
    float32 best_score_mm = -1.0f;
    const float32 phase_step_mm = actual_spacing_mm / static_cast<float32>(kClosedPhaseCandidateCount);
    for (std::size_t candidate_index = 0; candidate_index < kClosedPhaseCandidateCount; ++candidate_index) {
        const float32 candidate_phase_mm = phase_step_mm * static_cast<float32>(candidate_index);
        const float32 score_mm = ScoreClosedLoopPhase(
            candidate_phase_mm,
            total_length_mm,
            actual_spacing_mm,
            interval_count,
            boundaries);
        if (score_mm > best_score_mm + kClosedLoopPlanningEpsilon ||
            (std::fabs(score_mm - best_score_mm) <= kClosedLoopPlanningEpsilon &&
             candidate_phase_mm < best_phase_mm)) {
            best_score_mm = score_mm;
            best_phase_mm = candidate_phase_mm;
        }
    }
    return best_phase_mm;
}

}  // namespace Siligen::Domain::Dispensing::DomainServices::Internal
