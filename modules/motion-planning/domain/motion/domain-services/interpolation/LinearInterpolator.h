#pragma once

#include "shared/types/Point.h"
#include "TrajectoryInterpolatorBase.h"

#include <vector>

namespace Siligen::Domain::Motion {

using ::int32;
using ::uint32;
using Siligen::MotionSegment;

class LinearInterpolator : public TrajectoryInterpolatorBase {
   public:
    LinearInterpolator();
    ~LinearInterpolator() override = default;

    std::vector<TrajectoryPoint> CalculateInterpolation(const std::vector<Point2D>& points,
                                                        const InterpolationConfig& config) override;

    InterpolationAlgorithm GetType() const override {
        return InterpolationAlgorithm::LINEAR;
    }

    std::vector<TrajectoryPoint> SShapeLinearInterpolation(const Point2D& start,
                                                           const Point2D& end,
                                                           float32 start_vel,
                                                           float32 end_vel,
                                                           float32 max_vel,
                                                           float32 max_acc,
                                                           float32 max_jerk,
                                                           float32 time_step = 0.001f);

    std::vector<TrajectoryPoint> VariableSpeedLinearInterpolation(const std::vector<Point2D>& waypoints,
                                                                  const std::vector<float32>& speed_profile,
                                                                  const InterpolationConfig& config);

    std::vector<TrajectoryPoint> ContinuousLinearInterpolation(const std::vector<MotionSegment>& segments,
                                                               const InterpolationConfig& config);

    std::vector<float32> CalculateOptimalSpeedProfile(const std::vector<MotionSegment>& segments,
                                                      float32 max_corner_speed);

    bool ValidateLinearParameters(const Point2D& start,
                                  const Point2D& end,
                                  float32 start_vel,
                                  float32 end_vel,
                                  float32 max_vel,
                                  float32 max_acc,
                                  float32 max_jerk) const;

   protected:
    struct SShapeParameters {
        float32 t1 = 0, t2 = 0, t3 = 0, t4 = 0, t5 = 0, t6 = 0, t7 = 0;
        float32 v1 = 0, v2 = 0, v3 = 0;
        float32 s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0, s6 = 0, s7 = 0;
        float32 total_time = 0, total_distance = 0, max_acc = 0, max_jerk = 0;
        bool reach_max_velocity = false;
    };

    SShapeParameters CalculateSShapeParameters(
        float32 distance, float32 start_vel, float32 end_vel, float32 max_vel, float32 max_acc, float32 max_jerk) const;

    std::vector<TrajectoryPoint> GenerateSShapeTrajectory(const Point2D& start,
                                                          const Point2D& end,
                                                          const SShapeParameters& params,
                                                          float32 time_step) const;

    std::pair<float32, float32> CalculatePositionAndVelocity(float32 t,
                                                             const SShapeParameters& params,
                                                             float32 distance) const;

    std::vector<float32> LookAheadSpeedOptimization(const std::vector<MotionSegment>& segments,
                                                    int32 look_ahead_points,
                                                    float32 smoothing_radius) const;

    float32 CalculateCornerSpeed(float32 angle, float32 max_acc, float32 smoothing_radius) const;

    std::vector<TrajectoryPoint> SmoothTrajectory(const std::vector<TrajectoryPoint>& points,
                                                  float32 smoothing_factor) const;

   private:
    mutable std::vector<SShapeParameters> m_sshape_cache;
    mutable std::vector<float32> m_velocity_cache;
};

}  // namespace Siligen::Domain::Motion


