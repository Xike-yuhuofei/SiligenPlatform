#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <cstdint>

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

using int16 = std::int16_t;

namespace Siligen::RuntimeExecution::Contracts::Motion {

struct JogParameters {
    float32 velocity = 0.0f;
    float32 acceleration = 0.0f;
    float32 deceleration = 0.0f;
    float32 smooth_time = 0.0f;
};

class IJogControlPort {
   public:
    virtual ~IJogControlPort() = default;

    virtual Result<void> StartJog(LogicalAxisId axis, int16 direction, float32 velocity) = 0;
    virtual Result<void> StartJogStep(LogicalAxisId axis, int16 direction, float32 distance, float32 velocity) = 0;
    virtual Result<void> StopJog(LogicalAxisId axis) = 0;
    virtual Result<void> SetJogParameters(LogicalAxisId axis, const JogParameters& params) = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Motion

namespace Siligen::Domain::Motion::Ports {

using IJogControlPort = Siligen::RuntimeExecution::Contracts::Motion::IJogControlPort;
using JogParameters = Siligen::RuntimeExecution::Contracts::Motion::JogParameters;

}  // namespace Siligen::Domain::Motion::Ports
