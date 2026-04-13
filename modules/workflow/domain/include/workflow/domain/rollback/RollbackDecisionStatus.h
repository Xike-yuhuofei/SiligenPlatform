#pragma once

#include <cstdint>

namespace Siligen::Workflow::Domain {

enum class RollbackDecisionStatus : std::uint8_t {
    Requested = 0,
    Evaluating,
    Executing,
    Rejected,
    Performed,
    Failed,
};

}  // namespace Siligen::Workflow::Domain
