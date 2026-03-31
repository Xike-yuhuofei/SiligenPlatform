#pragma once

#include "domain/dispensing/value-objects/AuthorityTriggerLayout.h"
#include "shared/types/Types.h"

#include <cstddef>
#include <string>
#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices::Internal {

using Siligen::Shared::Types::float32;
using Siligen::Domain::Dispensing::ValueObjects::StrongAnchor;

struct ClosedLoopAnchorConstraintSolverInput {
    const std::vector<StrongAnchor>* strong_anchors = nullptr;
    float32 total_length_mm = 0.0f;
    float32 target_spacing_mm = 0.0f;
    float32 min_spacing_mm = 0.0f;
    float32 max_spacing_mm = 0.0f;
    float32 anchor_tolerance_mm = 0.0f;
};

struct ClosedLoopAnchorSolveResult {
    bool solved = false;
    bool within_window = false;
    std::size_t interval_count = 0;
    float32 actual_spacing_mm = 0.0f;
    float32 phase_mm = 0.0f;
    std::string anchor_constraint_state = "not_applicable";
    std::string anchor_exception_reason;
    std::string blocking_reason;
};

class ClosedLoopAnchorConstraintSolver {
   public:
    ClosedLoopAnchorSolveResult Solve(const ClosedLoopAnchorConstraintSolverInput& input) const;

    float32 ResolvePhase(
        float32 total_length_mm,
        float32 actual_spacing_mm,
        std::size_t interval_count,
        const std::vector<float32>& boundaries) const;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices::Internal
