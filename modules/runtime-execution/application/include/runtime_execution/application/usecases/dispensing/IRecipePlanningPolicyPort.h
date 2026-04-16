#pragma once

#include "shared/types/DispensingExecutionSemantics.h"
#include "shared/types/Result.h"

#include <string>

namespace Siligen::Application::Ports::Dispensing {

struct ResolvedRecipePlanningPolicy {
    std::string recipe_id;
    std::string version_id;
    Siligen::Shared::Types::PointFlyingCarrierPolicy point_flying_carrier_policy;
};

class IRecipePlanningPolicyPort {
   public:
    virtual ~IRecipePlanningPolicyPort() = default;

    virtual Siligen::Shared::Types::Result<ResolvedRecipePlanningPolicy> ResolvePlanningPolicy(
        const std::string& recipe_id,
        const std::string& version_id) const = 0;
};

}  // namespace Siligen::Application::Ports::Dispensing
