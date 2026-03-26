#pragma once

#include "domain/motion/value-objects/MotionTrajectory.h"
#include "domain/motion/ports/IInterpolationPort.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::Domain::Motion::Ports::InterpolationType;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::TrajectoryPoint;

struct ControllerConfig {
    bool use_hardware_trigger = true;
    float32 spatial_interval_mm = 1.0f;
    float32 pulse_per_mm = 200.0f;
    LogicalAxisId default_trigger_axis = LogicalAxisId::X;
    float32 acceleration_mm_s2 = 500.0f;
    std::vector<float32> trigger_distances_mm;
};

struct ControllerOutput {
    std::vector<InterpolationData> interpolation;
    bool use_hardware_trigger = false;
    LogicalAxisId trigger_axis = LogicalAxisId::X;
    std::vector<long> trigger_positions;
    float32 estimated_motion_time_ms = 0.0f;
};

class DispensingController {
   public:
    DispensingController() = default;
    ~DispensingController() = default;

    Result<ControllerOutput> Build(const MotionTrajectory& trajectory, const ControllerConfig& config) const noexcept;
    Result<ControllerOutput> Build(const std::vector<TrajectoryPoint>& trajectory,
                                   const ControllerConfig& config) const noexcept;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices
