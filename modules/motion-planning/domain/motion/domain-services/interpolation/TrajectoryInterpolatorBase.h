#pragma once

#include <memory>
#include <vector>

#include "motion_planning/contracts/InterpolationTypes.h"
#include "shared/types/Point.h"
#include "shared/types/Types.h"

namespace Siligen::Domain::Motion {

using ::float32;
using ::float64;
using ::int32;
using Siligen::InterpolationConfig;
using Siligen::MotionPlanning::Contracts::InterpolationAlgorithm;
using Siligen::Point2D;
using Siligen::TrajectoryPoint;

class TrajectoryInterpolatorBase {
   public:
    TrajectoryInterpolatorBase() = default;
    virtual ~TrajectoryInterpolatorBase() = default;

    virtual std::vector<TrajectoryPoint> CalculateInterpolation(const std::vector<Point2D>& points,
                                                                const InterpolationConfig& config) = 0;
    virtual bool ValidateParameters(const std::vector<Point2D>& points, const InterpolationConfig& config) const;
    virtual InterpolationAlgorithm GetType() const = 0;
    virtual float32 CalculateError(const std::vector<Point2D>& original,
                                   const std::vector<TrajectoryPoint>& interpolated) const;
    virtual std::vector<TrajectoryPoint> OptimizeTrajectoryDensity(const std::vector<TrajectoryPoint>& points,
                                                                   float32 max_step_size) const;

   protected:
    inline float32 CalculateDistance(const Point2D& p1, const Point2D& p2) const {
        return p1.DistanceTo(p2);
    }

    inline float32 CalculateAngle(const Point2D& p1, const Point2D& p2, const Point2D& p3) const {
        Point2D v1(p2.x - p1.x, p2.y - p1.y), v2(p3.x - p2.x, p3.y - p2.y);
        float32 dot = v1.x * v2.x + v1.y * v2.y;
        float32 m1 = std::sqrt(v1.x * v1.x + v1.y * v1.y), m2 = std::sqrt(v2.x * v2.x + v2.y * v2.y);
        return std::acos(dot / (m1 * m2));
    }

    virtual float32 CalculateCurvature(const Point2D& p1, const Point2D& p2, const Point2D& p3) const;
    virtual std::vector<float32> GenerateSShapeVelocityProfile(
        float32 distance, float32 max_velocity, float32 max_acceleration, float32 max_jerk, float32 time_step) const;
};

class TrajectoryInterpolatorFactory {
   public:
    static std::unique_ptr<TrajectoryInterpolatorBase> CreateInterpolator(InterpolationAlgorithm type);
    static std::vector<InterpolationAlgorithm> GetSupportedAlgorithms();
};

}  // namespace Siligen::Domain::Motion


