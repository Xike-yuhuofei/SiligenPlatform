#pragma once

#include "shared/types/AxisTypes.h"
#include "shared/types/Result.h"

#include <string>
#include <vector>

namespace Siligen::RuntimeExecution::Contracts::Motion {

struct HomeAxesExecutionRequest {
    std::vector<Siligen::Shared::Types::LogicalAxisId> axes;
    bool home_all_axes = false;
    bool force_rehome = false;
    bool wait_for_completion = true;
    Siligen::Shared::Types::int32 timeout_ms = 30000;

    bool Validate() const {
        if (home_all_axes && !axes.empty()) {
            return false;
        }
        return timeout_ms > 0 && timeout_ms <= 300000;
    }
};

struct HomeAxesExecutionResponse {
    struct AxisResult {
        Siligen::Shared::Types::LogicalAxisId axis = Siligen::Shared::Types::LogicalAxisId::INVALID;
        bool success = false;
        std::string message;
    };

    std::vector<Siligen::Shared::Types::LogicalAxisId> successfully_homed_axes;
    std::vector<Siligen::Shared::Types::LogicalAxisId> failed_axes;
    std::vector<std::string> error_messages;
    std::vector<AxisResult> axis_results;
    Siligen::Shared::Types::int32 total_time_ms = 0;
    bool all_completed = false;
    std::string status_message;
};

class IHomeAxesExecutionPort {
   public:
    virtual ~IHomeAxesExecutionPort() = default;

    virtual Siligen::Shared::Types::Result<HomeAxesExecutionResponse> Execute(
        const HomeAxesExecutionRequest& request) = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Motion
