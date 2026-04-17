#pragma once

#include "shared/types/DispensingExecutionSemantics.h"
#include "shared/types/Result.h"

#include <string>

namespace Siligen::Application::Ports::Dispensing {

struct ResolvedProductionBaseline {
    std::string baseline_id;
    std::string baseline_fingerprint;
    Siligen::Shared::Types::PointFlyingCarrierPolicy point_flying_carrier_policy;
};

class IProductionBaselinePort {
   public:
    virtual ~IProductionBaselinePort() = default;

    virtual Siligen::Shared::Types::Result<ResolvedProductionBaseline> ResolveCurrentBaseline() const = 0;
};

}  // namespace Siligen::Application::Ports::Dispensing
