#define MODULE_NAME "TrajectoryInterpolatorBase"
#include "TrajectoryInterpolatorBase.h"

#include "ArcInterpolator.h"
#include "LinearInterpolator.h"
#include "SplineInterpolator.h"
#include "domain/motion/CMPCoordinatedInterpolator.h"

#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Motion {

bool TrajectoryInterpolatorBase::ValidateParameters(const std::vector<Point2D>& points,
                                           const InterpolationConfig& config) const {
    if (points.empty()) {
        SILIGEN_LOG_ERROR("轨迹点为空，请提供至少2个点");
        return false;
    }
    if (points.size() < 2) {
        SILIGEN_LOG_ERROR_FMT_HELPER("插补需要至少2个点，当前仅有%zu个", points.size());
        return false;
    }
    if (config.max_velocity <= 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER("最大速度必须大于0，当前值:%.2f", config.max_velocity);
        return false;
    }
    if (config.max_acceleration <= 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER("最大加速度必须大于0，当前值:%.2f", config.max_acceleration);
        return false;
    }
    if (config.max_velocity > 2000.0f) {
        SILIGEN_LOG_WARNING_FMT_HELPER("速度超出建议范围(2000mm/s):%.2f", config.max_velocity);
    }
    if (config.max_acceleration > 10000.0f) {
        SILIGEN_LOG_WARNING_FMT_HELPER("加速度超出建议范围(10000mm/s²):%.2f", config.max_acceleration);
    }
    if (config.position_tolerance <= 0) {
        SILIGEN_LOG_ERROR("位置容差必须大于0");
        return false;
    }
    for (size_t i = 1; i < points.size(); ++i)
        if (CalculateDistance(points[i - 1], points[i]) < 1e-6f) {
            SILIGEN_LOG_ERROR_FMT_HELPER("检测到重复点(点%zu和点%zu)", i - 1, i);
            return false;
        }
    return true;
}

float32 TrajectoryInterpolatorBase::CalculateError(const std::vector<Point2D>& original,
                                          const std::vector<TrajectoryPoint>& interpolated) const {
    if (original.empty() || interpolated.empty()) return 0;
    float32 max_error = 0;
    for (const auto& orig : original) {
        float32 min_dist = std::numeric_limits<float32>::max();
        for (const auto& interp : interpolated) min_dist = std::min(min_dist, CalculateDistance(orig, interp.position));
        max_error = std::max(max_error, min_dist);
    }
    return max_error;
}

std::vector<TrajectoryPoint> TrajectoryInterpolatorBase::OptimizeTrajectoryDensity(const std::vector<TrajectoryPoint>& points,
                                                                          float32 max_step_size) const {
    if (points.empty() || max_step_size <= 0) return points;
    std::vector<TrajectoryPoint> optimized;
    optimized.reserve(points.size());
    optimized.push_back(points[0]);

    for (size_t i = 1; i < points.size(); ++i) {
        const auto& prev = optimized.back();
        const auto& curr = points[i];
        float32 dist = CalculateDistance(prev.position, curr.position);

        if (dist <= max_step_size) {
            optimized.push_back(curr);
        } else {
            int32 segs = static_cast<int32>(std::ceil(dist / max_step_size));
            for (int32 j = 1; j < segs; ++j) {
                float32 r = static_cast<float32>(j) / segs;
                TrajectoryPoint pt;
                pt.position.x = prev.position.x + r * (curr.position.x - prev.position.x);
                pt.position.y = prev.position.y + r * (curr.position.y - prev.position.y);
                pt.velocity = prev.velocity + r * (curr.velocity - prev.velocity);
                pt.acceleration = prev.acceleration + r * (curr.acceleration - prev.acceleration);
                pt.dispensing_time = prev.dispensing_time + r * (curr.dispensing_time - prev.dispensing_time);
                pt.timestamp = prev.timestamp + r * (curr.timestamp - prev.timestamp);
                pt.enable_position_trigger = false;
                pt.trigger_position_mm = 0.0f;
                pt.trigger_pulse_width_us = 0;
                pt.segment_type = (r >= 0.5f) ? curr.segment_type : prev.segment_type;
                pt.arc_center = (r >= 0.5f) ? curr.arc_center : prev.arc_center;
                pt.arc_radius = (r >= 0.5f) ? curr.arc_radius : prev.arc_radius;
                optimized.push_back(pt);
            }
            optimized.push_back(curr);
        }
    }
    return optimized;
}

float32 TrajectoryInterpolatorBase::CalculateCurvature(const Point2D& p1, const Point2D& p2, const Point2D& p3) const {
    Point2D v1(p2.x - p1.x, p2.y - p1.y), v2(p3.x - p2.x, p3.y - p2.y);
    float64 cross = static_cast<float64>(v1.x * v2.y - v1.y * v2.x);
    float64 l1 = std::sqrt(static_cast<float64>(v1.x * v1.x + v1.y * v1.y));
    float64 l2 = std::sqrt(static_cast<float64>(v2.x * v2.x + v2.y * v2.y));
    if (l1 < 1e-10 || l2 < 1e-10) return 0;
    float64 sin_angle = cross / (l1 * l2);
    float64 chord = std::sqrt(static_cast<float64>((p3.x - p1.x) * (p3.x - p1.x) + (p3.y - p1.y) * (p3.y - p1.y)));
    return chord < 1e-10 ? 0 : static_cast<float32>(2.0 * std::abs(sin_angle) / chord);
}

std::vector<float32> TrajectoryInterpolatorBase::GenerateSShapeVelocityProfile(
    float32 distance, float32 max_vel, float32 max_acc, float32 max_jerk, float32 time_step) const {
    std::vector<float32> profile;
    if (distance <= 0 || max_vel <= 0 || max_acc <= 0 || max_jerk <= 0) return profile;

    float32 t_acc = max_acc / max_jerk;
    float32 v_lim = std::min(max_vel, max_acc * max_acc / max_jerk);
    float32 s_tri = v_lim * v_lim / max_acc;
    bool reach_max = distance > s_tri;

    float32 t1, t2, t3, t4, t5, t6, t7, v1, v2, v3;
    if (reach_max) {
        t1 = t3 = t5 = t7 = t_acc;
        t2 = t6 = v_lim / max_acc - t_acc;
        float32 s_acc = v_lim * (t_acc + t_acc / 2);
        t4 = (distance - 2 * s_acc) / v_lim;
        v1 = v3 = max_acc * t_acc;
        v2 = v_lim;
    } else {
        float32 vp = std::sqrt(distance * max_jerk);
        t1 = t3 = t5 = t7 = vp / max_jerk;
        t2 = t4 = t6 = 0;
        v1 = v2 = v3 = vp;
    }

    float32 total = t1 + t2 + t3 + t4 + t5 + t6 + t7;
    for (float32 t = 0; t <= total; t += time_step) {
        float32 vel = 0;
        float32 T[] = {0,
                       t1,
                       t1 + t2,
                       t1 + t2 + t3,
                       t1 + t2 + t3 + t4,
                       t1 + t2 + t3 + t4 + t5,
                       t1 + t2 + t3 + t4 + t5 + t6,
                       total};
        if (t <= T[1])
            vel = 0.5f * max_jerk * t * t;
        else if (t <= T[2])
            vel = v1 + max_acc * (t - T[1]);
        else if (t <= T[3]) {
            float32 tl = t - T[2];
            vel = v2 + max_acc * tl - 0.5f * max_jerk * tl * tl;
        } else if (t <= T[4])
            vel = v3;
        else if (t <= T[5]) {
            float32 tl = t - T[4];
            vel = v3 - 0.5f * max_jerk * tl * tl;
        } else if (t <= T[6])
            vel = v3 - max_acc * (t - T[5]);
        else {
            float32 tl = t - T[6];
            vel = v1 - max_acc * tl + 0.5f * max_jerk * tl * tl;
        }
        profile.push_back(std::clamp(vel, 0.0f, max_vel));
    }
    return profile;
}

std::unique_ptr<TrajectoryInterpolatorBase> TrajectoryInterpolatorFactory::CreateInterpolator(
    InterpolationAlgorithm type) {
    switch (type) {
        case InterpolationAlgorithm::LINEAR:
            return std::make_unique<LinearInterpolator>();
        case InterpolationAlgorithm::ARC:
            return std::make_unique<ArcInterpolator>();
        case InterpolationAlgorithm::SPLINE:
            return std::make_unique<SplineInterpolator>();
        case InterpolationAlgorithm::CMP_COORDINATED:
            return std::make_unique<CMPCoordinatedInterpolator>();
        default:
            return nullptr;
    }
}

std::vector<InterpolationAlgorithm> TrajectoryInterpolatorFactory::GetSupportedAlgorithms() {
    return {
        InterpolationAlgorithm::LINEAR,
        InterpolationAlgorithm::ARC,
        InterpolationAlgorithm::SPLINE,
        InterpolationAlgorithm::CMP_COORDINATED,
    };
}

}  // namespace Siligen::Domain::Motion


