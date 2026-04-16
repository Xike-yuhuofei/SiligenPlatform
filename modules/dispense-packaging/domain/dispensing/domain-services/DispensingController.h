#pragma once

#include "motion_planning/contracts/MotionTrajectory.h"
#include "runtime_execution/contracts/dispensing/IValvePort.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::MotionPlanning::Contracts::MotionTrajectory;
using Siligen::RuntimeExecution::Contracts::Motion::InterpolationData;
using Siligen::RuntimeExecution::Contracts::Motion::InterpolationType;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;
using Siligen::TrajectoryPoint;
using Siligen::Domain::Dispensing::Ports::PathTriggerEvent;

struct ControllerConfig {
    bool use_hardware_trigger = true;
    float32 spatial_interval_mm = 1.0f;
    float32 pulse_per_mm = 200.0f;
    float32 acceleration_mm_s2 = 500.0f;
    std::vector<float32> trigger_distances_mm;
};

struct ControllerOutput {
    std::vector<InterpolationData> interpolation;
    bool use_hardware_trigger = false;
    std::vector<PathTriggerEvent> trigger_events;
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
