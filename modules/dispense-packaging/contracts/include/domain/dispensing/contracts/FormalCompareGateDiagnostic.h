#pragma once

#include "shared/types/Point2D.h"

#include <cstddef>
#include <string>
#include <vector>

namespace Siligen::Domain::Dispensing::Contracts {

struct FormalCompareGateDiagnostic {
    std::string status;
    std::string reason_code;
    std::string authority_span_ref;
    std::size_t trigger_begin_index = 0U;
    Siligen::Shared::Types::Point2D current_start_position_mm{};
    Siligen::Shared::Types::Point2D next_trigger_position_mm{};
    std::vector<std::string> candidate_failures;

    [[nodiscard]] bool HasValue() const noexcept { return !status.empty(); }
    [[nodiscard]] bool IsProductionBlocked() const noexcept { return status == "production_blocked"; }
};

}  // namespace Siligen::Domain::Dispensing::Contracts
