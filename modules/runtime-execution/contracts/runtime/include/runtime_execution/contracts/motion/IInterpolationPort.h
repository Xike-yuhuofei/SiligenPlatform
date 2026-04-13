#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <cstdint>
#include <vector>

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::LogicalAxisId;

using int16 = std::int16_t;
using int32 = std::int32_t;
using uint32 = std::uint32_t;

namespace Siligen::RuntimeExecution::Contracts::Motion {

enum class InterpolationType {
    LINEAR,
    CIRCULAR_CW,
    CIRCULAR_CCW,
    SPIRAL,
    BUFFER_STOP
};

struct CoordinateSystemConfig {
    int16 dimension = 2;
    std::vector<LogicalAxisId> axis_map;
    float32 max_velocity = 0.0f;
    float32 max_acceleration = 0.0f;
    int16 look_ahead_segments = 100;
    bool look_ahead_enabled = true;
    // true: reuse the controller's current planned position as the coordinate-system origin.
    // false: pin the coordinate-system origin to machine zero so repeated runs stay anchored.
    bool use_current_planned_position_as_origin = true;
};

struct InterpolationData {
    InterpolationType type = InterpolationType::LINEAR;
    std::vector<float32> positions;
    float32 velocity = 0.0f;
    float32 acceleration = 0.0f;
    float32 end_velocity = 0.0f;
    union {
        struct {
            float32 center_x, center_y;
        };
        struct {
            float32 radius;
        };
        struct {
            float32 spiral_pitch;
        };
    };
    int16 circle_plane = 0;
};

enum class CoordinateSystemState {
    IDLE,
    MOVING,
    PAUSED,
    ERROR_STATE
};

struct CoordinateSystemStatus {
    CoordinateSystemState state = CoordinateSystemState::IDLE;
    bool is_moving = false;
    uint32 remaining_segments = 0;
    float32 current_velocity = 0.0f;
    int32 raw_status_word = 0;
    int32 raw_segment = 0;
    int32 mc_status_ret = 0;
};

class IInterpolationPort {
   public:
    virtual ~IInterpolationPort() = default;

    virtual Result<void> ConfigureCoordinateSystem(int16 coord_sys, const CoordinateSystemConfig& config) = 0;
    virtual Result<void> AddInterpolationData(int16 coord_sys, const InterpolationData& data) = 0;
    virtual Result<void> ClearInterpolationBuffer(int16 coord_sys) = 0;
    virtual Result<void> FlushInterpolationData(int16 coord_sys) = 0;
    virtual Result<void> StartCoordinateSystemMotion(uint32 coord_sys_mask) = 0;
    virtual Result<void> StopCoordinateSystemMotion(uint32 coord_sys_mask) = 0;
    virtual Result<void> SetCoordinateSystemVelocityOverride(int16 coord_sys, float32 override_percent) = 0;
    virtual Result<void> EnableCoordinateSystemSCurve(int16 coord_sys, float32 jerk) = 0;
    virtual Result<void> DisableCoordinateSystemSCurve(int16 coord_sys) = 0;
    virtual Result<void> SetConstLinearVelocityMode(int16 coord_sys, bool enabled, uint32 rotate_axis_mask) = 0;
    virtual Result<uint32> GetInterpolationBufferSpace(int16 coord_sys) const = 0;
    virtual Result<uint32> GetLookAheadBufferSpace(int16 coord_sys) const = 0;
    virtual Result<CoordinateSystemStatus> GetCoordinateSystemStatus(int16 coord_sys) const = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Motion

namespace Siligen::Domain::Motion::Ports {

using InterpolationType = Siligen::RuntimeExecution::Contracts::Motion::InterpolationType;
using CoordinateSystemConfig = Siligen::RuntimeExecution::Contracts::Motion::CoordinateSystemConfig;
using InterpolationData = Siligen::RuntimeExecution::Contracts::Motion::InterpolationData;
using CoordinateSystemState = Siligen::RuntimeExecution::Contracts::Motion::CoordinateSystemState;
using CoordinateSystemStatus = Siligen::RuntimeExecution::Contracts::Motion::CoordinateSystemStatus;
using IInterpolationPort = Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort;

}  // namespace Siligen::Domain::Motion::Ports
