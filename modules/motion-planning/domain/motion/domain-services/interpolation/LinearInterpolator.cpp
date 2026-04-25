#include "LinearInterpolator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Siligen::Domain::Motion {

namespace SCurveMath = Siligen::Domain::Motion::DomainServices::SCurveMath;

LinearInterpolator::LinearInterpolator() : TrajectoryInterpolatorBase() {
    m_velocity_cache.reserve(1000);
}

std::vector<TrajectoryPoint> LinearInterpolator::CalculateInterpolation(const std::vector<Point2D>& points,
                                                                        const InterpolationConfig& config) {
    if (!ValidateParameters(points, config)) {
        return {};
    }
    if (points.size() == 2) {
        return SShapeLinearInterpolation(points[0],
                                         points[1],
                                         0.0f,
                                         0.0f,
                                         config.max_velocity,
                                         config.max_acceleration,
                                         config.max_jerk,
                                         config.time_step);
    }
    std::vector<MotionSegment> segments;
    for (size_t i = 1; i < points.size(); ++i) {
        MotionSegment seg(points[i - 1], points[i]);
        seg.max_velocity = config.max_velocity;
        seg.max_acceleration = config.max_acceleration;
        segments.push_back(seg);
    }
    return ContinuousLinearInterpolation(segments, config);
}

std::vector<TrajectoryPoint> LinearInterpolator::SShapeLinearInterpolation(const Point2D& start,
                                                                           const Point2D& end,
                                                                           float32 start_vel,
                                                                           float32 end_vel,
                                                                           float32 max_vel,
                                                                           float32 max_acc,
                                                                           float32 max_jerk,
                                                                           float32 time_step) {
    if (!ValidateLinearParameters(start, end, start_vel, end_vel, max_vel, max_acc, max_jerk)) return {};
    float32 dist = CalculateDistance(start, end);
    if (dist < 1e-6f) return {TrajectoryPoint(start, start_vel)};

    SCurveMath::SCurveConfig sconfig;
    sconfig.start_velocity = start_vel;
    sconfig.max_velocity = max_vel;
    sconfig.end_velocity = end_vel;
    sconfig.max_acceleration = max_acc;
    sconfig.max_jerk = max_jerk;

    auto samples = SCurveMath::GenerateProfile(dist, sconfig, time_step);
    if (samples.empty()) return {};

    Point2D dir = (end - start) / dist;
    std::vector<TrajectoryPoint> trajectory;
    trajectory.reserve(samples.size());

    for (const auto& s : samples) {
        TrajectoryPoint pt;
        pt.position = start + dir * s.position;
        pt.velocity = s.velocity;
        pt.timestamp = s.t;
        pt.sequence_id = static_cast<uint32>(trajectory.size());
        trajectory.push_back(pt);
    }

    return trajectory;
}

std::vector<TrajectoryPoint> LinearInterpolator::VariableSpeedLinearInterpolation(
    const std::vector<Point2D>& waypoints,
    const std::vector<float32>& speed_profile,
    const InterpolationConfig& config) {
    if (waypoints.size() < 2 || speed_profile.size() != waypoints.size()) return {};
    std::vector<TrajectoryPoint> trajectory;
    for (size_t i = 1; i < waypoints.size(); ++i) {
        auto seg = SShapeLinearInterpolation(waypoints[i - 1],
                                             waypoints[i],
                                             speed_profile[i - 1],
                                             speed_profile[i],
                                             config.max_velocity,
                                             config.max_acceleration,
                                             config.max_jerk,
                                             config.time_step);
        if (!trajectory.empty() && !seg.empty()) seg.erase(seg.begin());
        trajectory.insert(trajectory.end(), seg.begin(), seg.end());
    }
    return trajectory;
}

std::vector<TrajectoryPoint> LinearInterpolator::ContinuousLinearInterpolation(
    const std::vector<MotionSegment>& segments, const InterpolationConfig& config) {
    if (segments.empty()) return {};
    std::vector<MotionSegment> tuned_segments = segments;
    float32 curvature_factor = std::max(0.1f, config.curvature_speed_factor);
    for (auto& seg : tuned_segments) {
        if (seg.curvature_radius > config.corner_min_radius_mm) {
            float32 curvature_limit = std::sqrt(seg.max_acceleration * seg.curvature_radius) * curvature_factor;
            seg.max_velocity = std::min(seg.max_velocity, curvature_limit);
        }
    }

    auto speeds = CalculateOptimalSpeedProfile(tuned_segments, config.max_velocity);
    for (size_t i = 0; i < speeds.size(); ++i) {
        speeds[i] = std::min(speeds[i], tuned_segments[i].max_velocity);
    }

    if (config.enable_look_ahead) {
        float32 smoothing_radius = std::max(config.corner_smoothing_radius, config.corner_min_radius_mm);
        speeds = LookAheadSpeedOptimization(tuned_segments, config.look_ahead_points, smoothing_radius);
        for (size_t i = 0; i < speeds.size(); ++i) {
            speeds[i] = std::min(speeds[i], tuned_segments[i].max_velocity);
        }
    }

    std::vector<TrajectoryPoint> trajectory;
    for (size_t i = 0; i < tuned_segments.size(); ++i) {
        float32 sv = (i == 0) ? 0.0f : speeds[i - 1], ev = speeds[i];
        auto seg = SShapeLinearInterpolation(tuned_segments[i].start_point,
                                             tuned_segments[i].end_point,
                                             sv,
                                             ev,
                                             std::min(config.max_velocity, speeds[i]),
                                             tuned_segments[i].max_acceleration,
                                             config.max_jerk,
                                             config.time_step);

        if (tuned_segments[i].enable_dispensing) {
            for (auto& pt : seg) {
                float32 prog =
                    CalculateDistance(tuned_segments[i].start_point, pt.position) / tuned_segments[i].length;
                if (prog >= tuned_segments[i].dispensing_start_offset &&
                    prog <= (1.0f - tuned_segments[i].dispensing_end_offset)) {
                    pt.dispensing_time = config.time_step;
                }
            }
        }
        if (!trajectory.empty() && !seg.empty()) seg.erase(seg.begin());
        trajectory.insert(trajectory.end(), seg.begin(), seg.end());
    }
    return config.enable_smoothing ? SmoothTrajectory(trajectory, 0.1f) : trajectory;
}

std::vector<float32> LinearInterpolator::CalculateOptimalSpeedProfile(const std::vector<MotionSegment>& segments,
                                                                      float32 max_corner_speed) {
    std::vector<float32> speeds(segments.size(), max_corner_speed);
    for (size_t i = 0; i < segments.size(); ++i) {
        speeds[i] = std::min(speeds[i], std::sqrt(segments[i].max_acceleration * segments[i].length * 2.0f));
        if (i > 0)
            speeds[i] = std::min(
                speeds[i],
                std::sqrt(speeds[i - 1] * speeds[i - 1] + 2.0f * segments[i].max_acceleration * segments[i].length));
    }
    for (int32 i = static_cast<int32>(segments.size()) - 2; i >= 0; --i)
        speeds[i] = std::min(
            speeds[i],
            std::sqrt(speeds[i + 1] * speeds[i + 1] + 2.0f * segments[i].max_acceleration * segments[i].length));
    return speeds;
}

bool LinearInterpolator::ValidateLinearParameters(const Point2D& start,
                                                  const Point2D& end,
                                                  float32 start_vel,
                                                  float32 end_vel,
                                                  float32 max_vel,
                                                  float32 max_acc,
                                                  float32 max_jerk) const {
    if (max_vel <= 0 || max_acc <= 0 || max_jerk <= 0) return false;
    if (start_vel < 0 || end_vel < 0 || start_vel > max_vel || end_vel > max_vel) return false;
    return CalculateDistance(start, end) >= 1e-6f;
}

std::vector<float32> LinearInterpolator::LookAheadSpeedOptimization(const std::vector<MotionSegment>& segments,
                                                                    int32 look_ahead,
                                                                    float32 smoothing_radius) const {
    std::vector<float32> speeds(segments.size());
    for (size_t i = 0; i < segments.size(); ++i) {
        float32 min_speed = segments[i].max_velocity;
        int32 end = std::min(static_cast<int32>(segments.size()), static_cast<int32>(i) + look_ahead);
        for (int32 j = static_cast<int32>(i) + 1; j < end; ++j)
            min_speed = std::min(
                min_speed,
                CalculateCornerSpeed(
                    CalculateAngle(segments[j - 1].start_point, segments[j - 1].end_point, segments[j].end_point),
                    segments[j].max_acceleration,
                    smoothing_radius));
        speeds[i] = min_speed;
    }
    return speeds;
}

float32 LinearInterpolator::CalculateCornerSpeed(float32 angle, float32 max_acc, float32 radius) const {
    return angle < 1e-6f ? std::numeric_limits<float32>::max() : std::sqrt(max_acc * 0.5f * radius);
}

std::vector<TrajectoryPoint> LinearInterpolator::SmoothTrajectory(const std::vector<TrajectoryPoint>& pts,
                                                                  float32 f) const {
    if (pts.size() < 3) return pts;
    std::vector<TrajectoryPoint> out = pts;
    for (size_t i = 1; i < pts.size() - 1; ++i) {
        out[i].position.x = f * pts[i - 1].position.x + (1 - 2 * f) * pts[i].position.x + f * pts[i + 1].position.x;
        out[i].position.y = f * pts[i - 1].position.y + (1 - 2 * f) * pts[i].position.y + f * pts[i + 1].position.y;
        out[i].velocity = f * pts[i - 1].velocity + (1 - 2 * f) * pts[i].velocity + f * pts[i + 1].velocity;
    }
    return out;
}

}  // namespace Siligen::Domain::Motion
