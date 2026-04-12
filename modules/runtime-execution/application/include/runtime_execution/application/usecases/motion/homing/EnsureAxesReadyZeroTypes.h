#pragma once

#include "shared/types/AxisTypes.h"
#include "shared/types/Result.h"

#include <string>
#include <vector>

namespace Siligen::Application::UseCases::Motion::Homing {

using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

struct EnsureAxesReadyZeroRequest {
    std::vector<LogicalAxisId> axes;
    bool home_all_axes = false;
    bool wait_for_completion = true;
    int32 timeout_ms = 80000;
    bool force_rehome = false;

    bool Validate() const {
        if (home_all_axes && !axes.empty()) {
            return false;
        }
        return timeout_ms > 0 && timeout_ms <= 300000;
    }
};

struct EnsureAxesReadyZeroResponse {
    struct AxisResult {
        LogicalAxisId axis = LogicalAxisId::INVALID;
        std::string supervisor_state;
        std::string planned_action;
        bool executed = false;
        bool success = false;
        std::string reason_code;
        std::string message;
    };

    bool accepted = false;
    std::string summary_state;
    std::string reason_code;
    std::string message;
    std::vector<AxisResult> axis_results;
    int32 total_time_ms = 0;
};

}  // namespace Siligen::Application::UseCases::Motion::Homing
