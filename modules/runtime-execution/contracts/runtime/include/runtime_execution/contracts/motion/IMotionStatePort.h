#pragma once

#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <string>
#include <vector>

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;

namespace Siligen::RuntimeExecution::Contracts::Motion {

enum class MotionState {
    IDLE,
    MOVING,
    STOPPED,
    FAULT,
    HOMING,
    HOMED,
    ESTOP,
    DISABLED
};

struct MotionStatus {
    MotionState state = MotionState::IDLE;
    std::string homing_state = "unknown";
    Point2D position{0, 0};
    float32 velocity = 0.0f;
    float32 acceleration = 0.0f;
    bool in_position = false;
    bool has_error = false;
    int32 error_code = 0;
    bool enabled = false;
    bool following_error = false;
    bool home_failed = false;
    bool soft_limit_positive = false;
    bool soft_limit_negative = false;
    bool hard_limit_positive = false;
    bool hard_limit_negative = false;
    bool servo_alarm = false;
    bool home_signal = false;
    bool index_signal = false;
    float32 axis_position_mm = 0.0f;
    std::string selected_feedback_source = "encoder";
    float32 profile_position_mm = 0.0f;
    float32 encoder_position_mm = 0.0f;
    float32 profile_velocity_mm_s = 0.0f;
    float32 encoder_velocity_mm_s = 0.0f;
    int32 profile_position_ret = 0;
    int32 encoder_position_ret = 0;
    int32 profile_velocity_ret = 0;
    int32 encoder_velocity_ret = 0;
};

class IMotionStatePort {
   public:
    virtual ~IMotionStatePort() = default;

    virtual Result<Point2D> GetCurrentPosition() const = 0;
    virtual Result<float32> GetAxisPosition(LogicalAxisId axis) const = 0;
    virtual Result<float32> GetAxisVelocity(LogicalAxisId axis) const = 0;
    virtual Result<MotionStatus> GetAxisStatus(LogicalAxisId axis) const = 0;
    virtual Result<bool> IsAxisMoving(LogicalAxisId axis) const = 0;
    virtual Result<bool> IsAxisInPosition(LogicalAxisId axis) const = 0;
    virtual Result<std::vector<MotionStatus>> GetAllAxesStatus() const = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Motion

namespace Siligen::Domain::Motion::Ports {

using IMotionStatePort = Siligen::RuntimeExecution::Contracts::Motion::IMotionStatePort;
using MotionState = Siligen::RuntimeExecution::Contracts::Motion::MotionState;
using MotionStatus = Siligen::RuntimeExecution::Contracts::Motion::MotionStatus;

}  // namespace Siligen::Domain::Motion::Ports
