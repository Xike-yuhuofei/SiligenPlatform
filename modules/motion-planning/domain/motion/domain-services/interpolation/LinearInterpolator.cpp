#include "LinearInterpolator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Siligen::Domain::Motion {

LinearInterpolator::LinearInterpolator() : TrajectoryInterpolatorBase() {
    m_sshape_cache.reserve(100);
    m_velocity_cache.reserve(1000);
}

std::vector<TrajectoryPoint> LinearInterpolator::CalculateInterpolation(const std::vector<Point2D>& points,
                                                                        const InterpolationConfig& config) {
    if (!ValidateParameters(points, config)) {
        // 通过依赖注入或静态实例记录日志，而不是直接依赖基础设施层
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
    return GenerateSShapeTrajectory(
        start, end, CalculateSShapeParameters(dist, start_vel, end_vel, max_vel, max_acc, max_jerk), time_step);
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
                    pt.enable_position_trigger = true;
                    pt.trigger_position_mm = prog * tuned_segments[i].length;
                    pt.trigger_pulse_width_us = 2000;
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

LinearInterpolator::SShapeParameters LinearInterpolator::CalculateSShapeParameters(
    float32 dist, float32 sv, float32 ev, float32 mv, float32 ma, float32 mj) const {
    SShapeParameters p{};
    p.total_distance = dist;
    p.max_acc = ma;
    p.max_jerk = mj;
    float32 ta = ma / mj, s_acc = (mv * mv - sv * sv) / (2 * ma), s_dec = (mv * mv - ev * ev) / (2 * ma);

    if (dist > s_acc + s_dec) {
        p.reach_max_velocity = true;
        if (mv > sv) {
            p.t1 = ta;
            p.t2 = (mv - sv) / ma - ta;
            p.t3 = ta;
            p.v1 = sv + ma * ta;
            p.v2 = mv;
        } else {
            p.v1 = p.v2 = sv;
        }
        float32 sa = (sv + mv) * (p.t1 + p.t2 + p.t3) / 2, sd = (mv + ev) * ta;
        p.t4 = (dist - sa - sd) / mv;
        if (mv > ev) {
            p.t5 = ta;
            p.t6 = (mv - ev) / ma - ta;
            p.t7 = ta;
            p.v3 = ev + ma * ta;
        } else
            p.v3 = ev;
    } else {
        p.reach_max_velocity = false;
        float32 vp = std::min(mv, std::sqrt((ma * ma * dist + sv * sv + ev * ev) / 2));
        if (vp > sv) {
            p.t1 = (vp - sv) / mj;
            p.t3 = (vp - ev) / mj;
        }
        p.v1 = p.v2 = p.v3 = vp;
    }
    p.total_time = p.t1 + p.t2 + p.t3 + p.t4 + p.t5 + p.t6 + p.t7;
    return p;
}

std::vector<TrajectoryPoint> LinearInterpolator::GenerateSShapeTrajectory(const Point2D& start,
                                                                          const Point2D& end,
                                                                          const SShapeParameters& p,
                                                                          float32 ts) const {
    std::vector<TrajectoryPoint> traj;
    float32 dist = CalculateDistance(start, end);
    Point2D dir = (end - start) / dist;
    for (float32 t = 0; t <= p.total_time; t += ts) {
        auto [pos, vel] = CalculatePositionAndVelocity(t, p, dist);
        TrajectoryPoint pt;
        pt.position = start + dir * pos;
        pt.velocity = vel;
        pt.timestamp = t;
        pt.sequence_id = static_cast<uint32>(traj.size());
        traj.push_back(pt);
    }
    return traj;
}

std::pair<float32, float32> LinearInterpolator::CalculatePositionAndVelocity(float32 t,
                                                                             const SShapeParameters& p,
                                                                             float32 dist) const {
    float32 pos = 0, vel = 0;
    float32 T[] = {0,
                   p.t1,
                   p.t1 + p.t2,
                   p.t1 + p.t2 + p.t3,
                   p.t1 + p.t2 + p.t3 + p.t4,
                   p.t1 + p.t2 + p.t3 + p.t4 + p.t5,
                   p.t1 + p.t2 + p.t3 + p.t4 + p.t5 + p.t6,
                   p.total_time};

    if (t <= T[1]) {
        vel = 0.5f * p.max_jerk * t * t;
        pos = p.max_jerk * t * t * t / 6;
    } else if (t <= T[2]) {
        float32 tl = t - T[1];
        vel = p.v1 + p.max_acc * tl;
        pos = p.s1 + p.v1 * tl + 0.5f * p.max_acc * tl * tl;
    } else if (t <= T[3]) {
        float32 tl = t - T[2];
        vel = p.v2 + p.max_acc * tl - 0.5f * p.max_jerk * tl * tl;
        pos = p.s1 + p.s2 + p.v2 * tl + 0.5f * p.max_acc * tl * tl - p.max_jerk * tl * tl * tl / 6;
    } else if (t <= T[4]) {
        float32 tl = t - T[3];
        vel = p.v3;
        pos = p.s1 + p.s2 + p.s3 + p.v3 * tl;
    } else if (t <= T[5]) {
        float32 tl = t - T[4];
        vel = p.v3 - 0.5f * p.max_jerk * tl * tl;
        pos = p.s1 + p.s2 + p.s3 + p.s4 + p.v3 * tl - p.max_jerk * tl * tl * tl / 6;
    } else if (t <= T[6]) {
        float32 tl = t - T[5];
        vel = p.v3 - p.max_acc * tl;
        pos = p.s1 + p.s2 + p.s3 + p.s4 + p.s5 + p.v3 * tl - 0.5f * p.max_acc * tl * tl;
    } else {
        float32 tl = t - T[6];
        vel = p.v1 - p.max_acc * tl + 0.5f * p.max_jerk * tl * tl;
        pos = p.s1 + p.s2 + p.s3 + p.s4 + p.s5 + p.s6 + p.v1 * tl - 0.5f * p.max_acc * tl * tl +
              p.max_jerk * tl * tl * tl / 6;
    }

    return {std::clamp(pos, 0.0f, dist), std::max(0.0f, vel)};
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

