#pragma once

#include "process_planning/contracts/configuration/ConfigTypes.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"

#include <cstddef>
#include <cstdint>

namespace Siligen::Domain::Safety::DomainServices {

using Shared::Types::float32;
using Shared::Types::Result;
using Siligen::TrajectoryPoint;
using Siligen::Domain::Configuration::ValueObjects::AxisConfiguration;
using Siligen::Domain::Configuration::ValueObjects::MotionConfiguration;

class SoftLimitValidator {
   public:
    static Result<void> ValidateTrajectory(const TrajectoryPoint* trajectory,
                                           size_t count,
                                           const MotionConfiguration& config) noexcept;

    static Result<void> ValidateTrajectory(const TrajectoryPoint* trajectory,
                                           size_t count,
                                           const MotionConfiguration& config,
                                           const AxisConfiguration* axis_configs,
                                           size_t axis_count) noexcept;

    static Result<void> ValidatePoint(const Shared::Types::Point2D& point,
                                      const MotionConfiguration& config) noexcept;

   private:
    static constexpr float32 EPSILON = 1e-6f;

    static bool FloatLessOrEqual(float32 a, float32 b) noexcept;
    static bool FloatGreaterOrEqual(float32 a, float32 b) noexcept;
    static bool FloatInRange(float32 val, float32 min, float32 max) noexcept;
    static bool IsFinite(float32 value) noexcept;
    static Result<void> ValidateConfig(const MotionConfiguration& config) noexcept;
    static Result<void> ValidateAxisLimit(Siligen::Shared::Types::LogicalAxisId axis_id,
                                          float32 min_val,
                                          float32 max_val,
                                          bool enabled) noexcept;
};

}  // namespace Siligen::Domain::Safety::DomainServices
