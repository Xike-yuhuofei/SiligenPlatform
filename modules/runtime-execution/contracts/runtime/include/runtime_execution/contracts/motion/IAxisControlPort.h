#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

namespace Siligen::RuntimeExecution::Contracts::Motion {

struct AxisConfiguration {
    float32 max_velocity = 0.0f;
    float32 max_acceleration = 0.0f;
    float32 jerk = 0.0f;
    float32 following_error_limit = 0.0f;
    float32 in_position_tolerance = 0.0f;
    float32 soft_limit_positive = 0.0f;
    float32 soft_limit_negative = 0.0f;
    bool encoder_enabled = true;
    bool soft_limits_enabled = true;
    bool hard_limits_enabled = true;
};

class IAxisControlPort {
   public:
    virtual ~IAxisControlPort() = default;

    virtual Result<void> EnableAxis(LogicalAxisId axis) = 0;
    virtual Result<void> DisableAxis(LogicalAxisId axis) = 0;
    virtual Result<bool> IsAxisEnabled(LogicalAxisId axis) const = 0;
    virtual Result<void> ClearAxisStatus(LogicalAxisId axis) = 0;
    virtual Result<void> ClearPosition(LogicalAxisId axis) = 0;

    virtual Result<void> SetAxisVelocity(LogicalAxisId axis, float32 velocity) = 0;
    virtual Result<void> SetAxisAcceleration(LogicalAxisId axis, float32 acceleration) = 0;
    virtual Result<void> SetSoftLimits(LogicalAxisId axis, float32 negative_limit, float32 positive_limit) = 0;
    virtual Result<void> ConfigureAxis(LogicalAxisId axis, const AxisConfiguration& config) = 0;

    virtual Result<void> SetHardLimits(LogicalAxisId axis,
                                       short positive_io_index,
                                       short negative_io_index,
                                       short card_index = 0,
                                       short signal_type = 0) = 0;
    virtual Result<void> EnableHardLimits(LogicalAxisId axis, bool enable, short limit_type = -1) = 0;
    virtual Result<void> SetHardLimitPolarity(LogicalAxisId axis, short positive_polarity, short negative_polarity) = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Motion

namespace Siligen::Domain::Motion::Ports {

using AxisConfiguration = Siligen::RuntimeExecution::Contracts::Motion::AxisConfiguration;
using IAxisControlPort = Siligen::RuntimeExecution::Contracts::Motion::IAxisControlPort;

}  // namespace Siligen::Domain::Motion::Ports
