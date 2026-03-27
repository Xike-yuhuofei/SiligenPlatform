#include "MotionPlanner.h"

#include "domain/trajectory/value-objects/GeometryUtils.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <ruckig/ruckig.hpp>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "MotionPlanner"

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Domain::Trajectory::ValueObjects::ProcessTag;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::Domain::Trajectory::ValueObjects::Segment;
using Siligen::Domain::Trajectory::ValueObjects::ArcPrimitive;
using Siligen::Domain::Trajectory::ValueObjects::LinePrimitive;
using Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint;
using Siligen::Domain::Trajectory::ValueObjects::ArcPoint;
using Siligen::Domain::Trajectory::ValueObjects::ArcTangent;
using Siligen::Domain::Trajectory::ValueObjects::Clamp;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcLength;
using Siligen::Domain::Trajectory::ValueObjects::LineDirection;
using Siligen::Domain::Trajectory::ValueObjects::NormalizeAngle;
using Siligen::Shared::Types::Point2D;
using Siligen::Point3D;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::kRadToDeg;

namespace {
constexpr float32 kEpsilon = 1e-6f;
constexpr float32 kMinIntegrationVelocity = 1e-3f;
constexpr float32 kFallbackJmax = 5000.0f;

float32 SegmentLength(const Segment& segment) {
    if (segment.length > kEpsilon) {
        return segment.length;
    }
    if (segment.type == SegmentType::Line) {
        return segment.line.start.DistanceTo(segment.line.end);
    }
    if (segment.type == SegmentType::Spline) {
        if (segment.spline.control_points.size() < 2) {
            return 0.0f;
        }
        float32 length = 0.0f;
        for (size_t i = 1; i < segment.spline.control_points.size(); ++i) {
            length += segment.spline.control_points[i - 1].DistanceTo(segment.spline.control_points[i]);
        }
        return length;
    }
    return ComputeArcLength(segment.arc);
}

float32 SegmentCurvature(const Segment& segment) {
    float32 radius = 0.0f;
    if (segment.type == SegmentType::Arc) {
        radius = segment.arc.radius;
    } else if (segment.curvature_radius > kEpsilon) {
        radius = segment.curvature_radius;
    } else {
        return 0.0f;
    }
    if (radius <= kEpsilon) {
        return 0.0f;
    }
    return std::abs(1.0f / radius);
}

float32 ApplyTagLimit(ProcessTag tag, float32 vmax, const MotionConfig& config) {
    switch (tag) {
        case ProcessTag::Corner:
            return vmax * config.corner_speed_factor;
        case ProcessTag::Start:
            return vmax * config.start_speed_factor;
        case ProcessTag::End:
            return vmax * config.end_speed_factor;
        case ProcessTag::Rapid:
            return vmax * config.rapid_speed_factor;
        case ProcessTag::Normal:
        default:
            return vmax;
    }
}

float32 ApplyConstraintLimit(const ProcessSegment& segment, float32 vmax) {
    if (!segment.constraint.has_velocity_factor) {
        return vmax;
    }
    float32 factor = segment.constraint.max_velocity_factor;
    if (factor < 0.0f) {
        factor = 0.0f;
    }
    return vmax * factor;
}

float32 ComputeCurvatureLimit(float32 curvature,
                              float32 amax,
                              float32 vmax,
                              float32 curvature_factor) {
    if (curvature <= kEpsilon) {
        return vmax;
    }
    float32 factor = (curvature_factor > kEpsilon) ? curvature_factor : 1.0f;
    float32 limit = std::sqrt(std::max(0.0f, amax / curvature));
    return std::min(vmax, limit * factor);
}

float32 ClampTargetAccelerationForRuckig(float32 target_velocity,
                                         float32 target_acceleration,
                                         float32 max_velocity,
                                         float32 max_jerk) {
    if (max_jerk <= kEpsilon) {
        return target_acceleration;
    }

    if (target_acceleration < -kEpsilon) {
        float32 limit = max_velocity - target_velocity;
        if (limit <= kEpsilon) {
            return 0.0f;
        }
        float32 max_abs = std::sqrt(std::max(0.0f, 2.0f * max_jerk * limit));
        max_abs = std::max(0.0f, max_abs - 1e-4f);
        if (-target_acceleration > max_abs) {
            return -max_abs;
        }
        return target_acceleration;
    }

    if (target_acceleration > kEpsilon) {
        float32 min_velocity = -max_velocity;
        float32 limit = target_velocity - min_velocity;
        if (limit <= kEpsilon) {
            return 0.0f;
        }
        float32 max_abs = std::sqrt(std::max(0.0f, 2.0f * max_jerk * limit));
        max_abs = std::max(0.0f, max_abs - 1e-4f);
        if (target_acceleration > max_abs) {
            return max_abs;
        }
        return target_acceleration;
    }

    return 0.0f;
}

float32 ResolveSampleDs(const MotionConfig& config) {
    if (config.sample_ds > kEpsilon) {
        return config.sample_ds;
    }
    float32 dt = (config.sample_dt > kEpsilon) ? config.sample_dt : 0.01f;
    float32 vmax = std::max(config.vmax, 0.0f);
    float32 ds = vmax * dt * 0.5f;
    ds = Clamp(ds, 0.01f, 1.0f);
    return ds;
}

float32 ComputeChordStep(float32 radius, float32 tolerance) {
    if (radius <= kEpsilon || tolerance <= kEpsilon) {
        return 0.0f;
    }
    float32 tol = std::min(tolerance, radius);
    float32 term = 2.0f * radius * tol - tol * tol;
    if (term <= kEpsilon) {
        return 0.0f;
    }
    return 2.0f * std::sqrt(term);
}

float32 ResolveSegmentSampleDs(const Segment& segment, float32 base_ds, const MotionConfig& config) {
    if (base_ds <= kEpsilon || config.arc_tolerance_mm <= kEpsilon) {
        return base_ds;
    }
    float32 curvature = SegmentCurvature(segment);
    if (curvature <= kEpsilon) {
        return base_ds;
    }
    float32 radius = 1.0f / curvature;
    float32 chord = ComputeChordStep(radius, config.arc_tolerance_mm);
    if (chord <= kEpsilon) {
        return base_ds;
    }
    float32 ds = std::min(base_ds, chord);
    return (ds > kEpsilon) ? ds : base_ds;
}

std::vector<float32> MapVelocityProfileToSamples(const std::vector<float32>& velocities,
                                                 float32 dt,
                                                 const std::vector<float32>& s_samples) {
    std::vector<float32> mapped;
    if (velocities.size() < 2 || dt <= kEpsilon || s_samples.empty()) {
        return mapped;
    }

    std::vector<float32> s_profile(velocities.size(), 0.0f);
    for (size_t i = 1; i < velocities.size(); ++i) {
        s_profile[i] = s_profile[i - 1] + 0.5f * (velocities[i - 1] + velocities[i]) * dt;
    }

    mapped.resize(s_samples.size(), 0.0f);
    size_t idx = 1;
    for (size_t i = 0; i < s_samples.size(); ++i) {
        float32 s = s_samples[i];
        while (idx < s_profile.size() && s_profile[idx] + kEpsilon < s) {
            ++idx;
        }
        if (idx >= s_profile.size()) {
            mapped[i] = velocities.back();
            continue;
        }
        float32 s0 = s_profile[idx - 1];
        float32 s1 = s_profile[idx];
        float32 ratio = (s1 > s0 + kEpsilon) ? (s - s0) / (s1 - s0) : 0.0f;
        float32 v0 = velocities[idx - 1];
        float32 v1 = velocities[idx];
        mapped[i] = v0 + ratio * (v1 - v0);
    }
    return mapped;
}

struct TimeIntegrationResult {
    float32 dt = 0.0f;
    float32 legacy_dt = 0.0f;
    bool used_fallback = false;
};

struct RuckigSegment {
    ruckig::Trajectory<1> trajectory;
    float32 start_time = 0.0f;
    float32 duration = 0.0f;
    float32 start_s = 0.0f;
};

struct PathLengthSummary {
    std::vector<float32> lengths;
    std::vector<float32> cumulative;
    float32 total_length = 0.0f;
};

PathLengthSummary ComputePathLengths(const ProcessPath& path) {
    PathLengthSummary summary;
    summary.lengths.reserve(path.segments.size());
    summary.cumulative.resize(path.segments.size() + 1, 0.0f);
    for (size_t i = 0; i < path.segments.size(); ++i) {
        float32 len = SegmentLength(path.segments[i].geometry);
        if (len <= kEpsilon) {
            len = 0.0f;
        } else {
            summary.total_length += len;
        }
        summary.lengths.push_back(len);
        summary.cumulative[i + 1] = summary.cumulative[i] + len;
    }
    return summary;
}

std::vector<float32> BuildDistanceSamples(const ProcessPath& path,
                                          const std::vector<float32>& lengths,
                                          float32 total_length,
                                          float32 ds,
                                          const MotionConfig& config) {
    std::vector<float32> s_samples;
    s_samples.reserve(static_cast<size_t>(total_length / ds) + path.segments.size() + 2);
    float32 s_accum = 0.0f;
    s_samples.push_back(0.0f);
    for (size_t i = 0; i < path.segments.size(); ++i) {
        float32 len = lengths[i];
        if (len <= kEpsilon) {
            continue;
        }
        float32 segment_ds = (config.sample_ds <= kEpsilon) ? ResolveSegmentSampleDs(path.segments[i].geometry, ds, config)
                                                            : ds;
        if (segment_ds <= kEpsilon) {
            segment_ds = ds;
        }
        float32 local = segment_ds;
        while (local + kEpsilon < len) {
            s_samples.push_back(s_accum + local);
            local += segment_ds;
        }
        s_accum += len;
        if (s_samples.back() + kEpsilon < s_accum) {
            s_samples.push_back(s_accum);
        }
    }
    if (s_samples.empty() || std::abs(s_samples.back() - total_length) > kEpsilon) {
        s_samples.push_back(total_length);
    }
    return s_samples;
}

std::vector<float32> BuildVelocityLimits(const ProcessPath& path,
                                         const std::vector<float32>& lengths,
                                         const std::vector<float32>& s_samples,
                                         const MotionConfig& config,
                                         float32 amax) {
    const size_t sample_count = s_samples.size();
    std::vector<float32> v_limits(sample_count, 0.0f);
    size_t seg_index = 0;
    float32 seg_end = lengths.empty() ? 0.0f : lengths[0];
    for (size_t i = 0; i < sample_count; ++i) {
        float32 s = s_samples[i];
        while (seg_index + 1 < path.segments.size() && s > seg_end + kEpsilon) {
            ++seg_index;
            seg_end += lengths[seg_index];
        }
        const auto& seg = path.segments[seg_index];
        float32 curvature = SegmentCurvature(seg.geometry);
        float32 vmax = std::max(config.vmax, 0.0f);
        vmax = seg.constraint.has_velocity_factor ? ApplyConstraintLimit(seg, vmax) : ApplyTagLimit(seg.tag, vmax, config);
        v_limits[i] = std::max(ComputeCurvatureLimit(curvature, amax, vmax, config.curvature_speed_factor), 0.0f);
    }
    return v_limits;
}

TimeIntegrationResult ComputeTimeIntegrationStep(float32 ds_step, float32 v0, float32 v1, float32 amax) {
    TimeIntegrationResult result;
    if (ds_step <= kEpsilon) {
        return result;
    }

    float32 v_sum = v0 + v1;
    result.legacy_dt = ds_step / std::max(0.5f * v_sum, kMinIntegrationVelocity);
    if (v_sum > kEpsilon) {
        result.dt = 2.0f * ds_step / v_sum;
        return result;
    }

    result.used_fallback = true;
    if (amax > kEpsilon) {
        result.dt = 2.0f * std::sqrt(ds_step / amax);
    } else {
        result.dt = result.legacy_dt;
    }
    return result;
}

void EvaluateSegmentAtDistance(const Segment& segment,
                               float32 local_s,
                               Point2D& position,
                               Point2D& tangent,
                               float32& curvature) {
    if (segment.type == SegmentType::Line) {
        Point2D dir = LineDirection(segment.line);
        float32 len = segment.line.start.DistanceTo(segment.line.end);
        float32 dist = Clamp(local_s, 0.0f, len);
        position = segment.line.start + dir * dist;
        tangent = dir;
        curvature = 0.0f;
        return;
    }
    if (segment.type == SegmentType::Spline) {
        if (segment.spline.control_points.size() < 2) {
            position = segment.spline.control_points.empty() ? Point2D(0.0f, 0.0f) : segment.spline.control_points[0];
            tangent = Point2D(0.0f, 0.0f);
            curvature = 0.0f;
            return;
        }

        float32 total = 0.0f;
        std::vector<float32> cumulative;
        cumulative.reserve(segment.spline.control_points.size());
        cumulative.push_back(0.0f);
        for (size_t i = 1; i < segment.spline.control_points.size(); ++i) {
            total += segment.spline.control_points[i - 1].DistanceTo(segment.spline.control_points[i]);
            cumulative.push_back(total);
        }
        if (total <= kEpsilon) {
            position = segment.spline.control_points.front();
            tangent = Point2D(0.0f, 0.0f);
            curvature = 0.0f;
            return;
        }
        float32 dist = Clamp(local_s, 0.0f, total);
        size_t idx = 1;
        while (idx < cumulative.size() && cumulative[idx] + kEpsilon < dist) {
            ++idx;
        }
        idx = std::min(idx, cumulative.size() - 1);
        const auto& p0 = segment.spline.control_points[idx - 1];
        const auto& p1 = segment.spline.control_points[idx];
        float32 seg_len = cumulative[idx] - cumulative[idx - 1];
        float32 ratio = (seg_len > kEpsilon) ? (dist - cumulative[idx - 1]) / seg_len : 0.0f;
        position = p0 + (p1 - p0) * ratio;
        Point2D dir = (p1 - p0);
        float32 dlen = dir.Length();
        tangent = (dlen > kEpsilon) ? (dir / dlen) : Point2D(0.0f, 0.0f);
        curvature = SegmentCurvature(segment);
        return;
    }

    float32 radius = segment.arc.radius;
    float32 length = SegmentLength(segment);
    float32 dist = Clamp(local_s, 0.0f, length);
    float32 angle_offset = (radius > kEpsilon) ? (dist / radius) * kRadToDeg : 0.0f;
    float32 angle = segment.arc.clockwise
                        ? segment.arc.start_angle_deg - angle_offset
                        : segment.arc.start_angle_deg + angle_offset;
    angle = NormalizeAngle(angle);
    position = ArcPoint(segment.arc, angle);
    tangent = ArcTangent(segment.arc, angle).Normalized();
    curvature = (radius > kEpsilon) ? std::abs(1.0f / radius) : 0.0f;
}

size_t FindSegmentIndex(const std::vector<float32>& cumulative, float32 s) {
    if (cumulative.size() <= 1) {
        return 0;
    }
    auto it = std::upper_bound(cumulative.begin() + 1, cumulative.end(), s + kEpsilon);
    if (it == cumulative.begin()) {
        return 0;
    }
    size_t idx = static_cast<size_t>(it - cumulative.begin() - 1);
    if (idx >= cumulative.size() - 1) {
        idx = cumulative.size() - 2;
    }
    return idx;
}

std::vector<float32> ApplyJerkLimitedScan(const std::vector<float32>& v_in,
                                          const std::vector<float32>& s_samples,
                                          const std::vector<float32>& v_limits,
                                          float32 amax,
                                          float32 jmax,
                                          bool forward,
                                          int32& correction_count) {
    const size_t n = v_in.size();
    std::vector<float32> v_out(n, 0.0f);
    if (n == 0) {
        return v_out;
    }

    if (forward) {
        v_out[0] = std::min(v_in[0], v_limits[0]);
        float32 prev_a = 0.0f;
        for (size_t i = 1; i < n; ++i) {
            float32 ds_step = s_samples[i] - s_samples[i - 1];
            float32 target = std::min(v_in[i], v_limits[i]);
            TimeIntegrationResult step = ComputeTimeIntegrationStep(ds_step, v_out[i - 1], target, amax);
            float32 dt = std::max(step.dt, kEpsilon);
            float32 a_target = (target - v_out[i - 1]) / dt;
            a_target = Clamp(a_target, -amax, amax);
            float32 a_min = prev_a - jmax * dt;
            float32 a_max = prev_a + jmax * dt;
            float32 a = Clamp(a_target, a_min, a_max);
            float32 v_new = v_out[i - 1] + a * dt;
            v_new = Clamp(v_new, 0.0f, target);
            if (v_new + kEpsilon < target) {
                ++correction_count;
            }
            v_out[i] = v_new;
            prev_a = (v_out[i] - v_out[i - 1]) / dt;
        }
        return v_out;
    }

    v_out[n - 1] = std::min(v_in[n - 1], v_limits[n - 1]);
    float32 prev_a = 0.0f;
    for (size_t i = n - 1; i-- > 0;) {
        float32 ds_step = s_samples[i + 1] - s_samples[i];
        float32 target = std::min(v_in[i], v_limits[i]);
        TimeIntegrationResult step = ComputeTimeIntegrationStep(ds_step, v_out[i + 1], target, amax);
        float32 dt = std::max(step.dt, kEpsilon);
        float32 a_target = (target - v_out[i + 1]) / dt;
        a_target = Clamp(a_target, -amax, amax);
        float32 a_min = prev_a - jmax * dt;
        float32 a_max = prev_a + jmax * dt;
        float32 a = Clamp(a_target, a_min, a_max);
        float32 v_new = v_out[i + 1] + a * dt;
        v_new = Clamp(v_new, 0.0f, target);
        if (v_new + kEpsilon < target) {
            ++correction_count;
        }
        v_out[i] = v_new;
        prev_a = (v_out[i] - v_out[i + 1]) / dt;
    }
    return v_out;
}

}  // namespace

MotionPlanner::MotionPlanner(std::shared_ptr<VelocityProfileService> velocity_service)
    : velocity_service_(std::move(velocity_service)) {}

MotionTrajectory MotionPlanner::Plan(const ProcessPath& path, const MotionConfig& config) const {
    MotionTrajectory trajectory;
    Siligen::Domain::Trajectory::ValueObjects::PlanningReport report;
    if (path.segments.empty()) {
        return trajectory;
    }

    const float32 amax = std::max(config.amax, 1e-3f);
    const float32 dt = (config.sample_dt > kEpsilon) ? config.sample_dt : 0.01f;
    const float32 ds = ResolveSampleDs(config);
    const bool ruckig_enabled = config.enforce_jerk_limit;
    const bool jerk_enforced = ruckig_enabled && config.jmax > kEpsilon;
    const float32 effective_jmax = jerk_enforced ? config.jmax : kFallbackJmax;

    const auto path_lengths = ComputePathLengths(path);
    const auto& lengths = path_lengths.lengths;
    const auto& cumulative = path_lengths.cumulative;
    const float32 total_length = path_lengths.total_length;

    if (total_length <= kEpsilon) {
        float32 t = 0.0f;
        float32 dt_step = (config.sample_dt > kEpsilon) ? config.sample_dt : 0.0f;
        for (const auto& seg : path.segments) {
            if (!seg.geometry.is_point) {
                continue;
            }
            MotionTrajectoryPoint pt;
            Point2D pos2d = (seg.geometry.type == SegmentType::Line) ? seg.geometry.line.start
                                                                     : Point2D(0.0f, 0.0f);
            pt.t = t;
            pt.position = Point3D(pos2d.x, pos2d.y, 0.0f);
            pt.velocity = Point3D(0.0f, 0.0f, 0.0f);
            pt.process_tag = seg.tag;
            pt.dispense_on = seg.dispense_on;
            pt.flow_rate = seg.flow_rate;
            trajectory.points.push_back(pt);
            t += dt_step;
        }
        trajectory.total_length = 0.0f;
        trajectory.total_time = t;
        report.total_length_mm = 0.0f;
        report.total_time_s = t;
        trajectory.planning_report = report;
        return trajectory;
    }

    auto s_samples = BuildDistanceSamples(path, lengths, total_length, ds, config);

    const size_t sample_count = s_samples.size();
    auto v_limits = BuildVelocityLimits(path, lengths, s_samples, config, amax);

    std::vector<float32> v_profile;
    if (velocity_service_ && jerk_enforced) {
        Domain::Motion::Ports::VelocityProfileConfig vp_config;
        vp_config.max_velocity = std::max(config.vmax, 0.0f);
        vp_config.max_acceleration = amax;
        vp_config.max_jerk = effective_jmax;
        vp_config.time_step = dt;

        auto vp_result = velocity_service_->GenerateProfile(total_length, vp_config);
        if (vp_result.IsSuccess() && !vp_result.Value().velocities.empty()) {
            v_profile = MapVelocityProfileToSamples(vp_result.Value().velocities, vp_config.time_step, s_samples);
        }
    }

    if (v_profile.empty()) {
        v_profile = v_limits;
    }

    if (!v_profile.empty()) {
        v_profile.front() = 0.0f;
        v_profile.back() = 0.0f;
    }

    for (size_t i = 0; i < sample_count; ++i) {
        v_profile[i] = std::min(v_profile[i], v_limits[i]);
    }

    for (size_t i = 1; i < sample_count; ++i) {
        float32 ds_step = s_samples[i] - s_samples[i - 1];
        float32 reachable = std::sqrt(v_profile[i - 1] * v_profile[i - 1] + 2.0f * amax * ds_step);
        v_profile[i] = std::min(v_profile[i], reachable);
    }

    for (size_t i = sample_count; i-- > 1;) {
        float32 ds_step = s_samples[i] - s_samples[i - 1];
        float32 reachable = std::sqrt(v_profile[i] * v_profile[i] + 2.0f * amax * ds_step);
        v_profile[i - 1] = std::min(v_profile[i - 1], reachable);
    }

    if (ruckig_enabled && jerk_enforced) {
        int32 jerk_corrections = 0;
        auto v_forward = ApplyJerkLimitedScan(v_profile, s_samples, v_limits, amax, effective_jmax, true, jerk_corrections);
        auto v_backward = ApplyJerkLimitedScan(v_forward, s_samples, v_limits, amax, effective_jmax, false, jerk_corrections);
        for (size_t i = 0; i < sample_count; ++i) {
            v_profile[i] = std::min(v_forward[i], v_backward[i]);
        }
        if (!v_profile.empty()) {
            v_profile.front() = 0.0f;
            v_profile.back() = 0.0f;
        }
        for (size_t i = 0; i < sample_count; ++i) {
            v_profile[i] = std::min(v_profile[i], v_limits[i]);
        }
        static_cast<void>(jerk_corrections);
    }

    // Only route through per-segment Ruckig time parameterization when jerk limiting is
    // explicitly enabled with a real jerk bound; otherwise keep the scanned velocity profile.
    const bool use_ruckig = jerk_enforced;
    std::vector<float32> t_samples(sample_count, 0.0f);
    std::vector<float32> accel_targets;
    if (use_ruckig && sample_count > 1) {
        accel_targets.resize(sample_count - 1, 0.0f);
    }
    float32 integration_error = 0.0f;
    int32 integration_fallbacks = 0;
    float32 prev_accel = 0.0f;
    for (size_t i = 1; i < sample_count; ++i) {
        float32 ds_step = s_samples[i] - s_samples[i - 1];
        float32 v0 = v_profile[i - 1];
        float32 v1 = v_profile[i];
        TimeIntegrationResult step = ComputeTimeIntegrationStep(ds_step, v0, v1, amax);
        float32 dt_step = step.dt;
        t_samples[i] = t_samples[i - 1] + dt_step;
        integration_error += std::abs(dt_step - step.legacy_dt);
        if (step.used_fallback) {
            ++integration_fallbacks;
        }
        if (use_ruckig) {
            float32 dt_est = std::max(dt_step, kEpsilon);
            float32 a_est = (v1 - v0) / dt_est;
            a_est = Clamp(a_est, -amax, amax);
            float32 a_min = prev_accel - effective_jmax * dt_est;
            float32 a_max = prev_accel + effective_jmax * dt_est;
            float32 a_filtered = Clamp(a_est, a_min, a_max);
            float32 v_max = std::max(v0, v1);
            a_filtered = ClampTargetAccelerationForRuckig(v1, a_filtered, v_max, effective_jmax);
            accel_targets[i - 1] = a_filtered;
            prev_accel = a_filtered;
        }
    }

    report.total_length_mm = total_length;
    report.time_integration_error_s = integration_error;
    report.time_integration_fallbacks = integration_fallbacks;
    report.jerk_limit_enforced = jerk_enforced;

    if (use_ruckig) {
        std::vector<RuckigSegment> ruckig_segments;
        ruckig_segments.reserve(sample_count > 1 ? sample_count - 1 : 0);

        ruckig::Ruckig<1> otg;
        ruckig::InputParameter<1> input;
        std::array<double, 1> pos{};
        std::array<double, 1> vel{};
        std::array<double, 1> acc{};

        float32 current_time = 0.0f;
        float32 current_s = 0.0f;
        float32 current_acc = 0.0f;

        for (size_t i = 1; i < sample_count; ++i) {
            float32 ds_step = s_samples[i] - s_samples[i - 1];
            if (ds_step <= kEpsilon) {
                continue;
            }

            input.current_position[0] = 0.0;
            input.current_velocity[0] = v_profile[i - 1];
            input.current_acceleration[0] = current_acc;
            input.target_position[0] = ds_step;
            input.target_velocity[0] = v_profile[i];
            input.target_acceleration[0] = accel_targets.empty() ? 0.0f : accel_targets[i - 1];
            float32 vmax_input = std::max(v_profile[i - 1], v_profile[i]);
            if (i < v_limits.size()) {
                vmax_input = std::max(vmax_input, v_limits[i]);
            }
            if (vmax_input <= kEpsilon) {
                vmax_input = 1e-3f;  // Avoid invalid Ruckig input (max velocity must be > 0)
            }
            input.max_velocity[0] = vmax_input;
            input.max_acceleration[0] = amax;
            input.max_jerk[0] = effective_jmax;

            ruckig::Trajectory<1> traj;
            auto result = otg.calculate(input, traj);
            if (result != ruckig::Result::Working) {
                SILIGEN_LOG_WARNING_FMT_HELPER(
                    "Ruckig calculate failed: result=%d, index=%zu, ds=%.6f, s0=%.6f, s1=%.6f, "
                    "v0=%.6f, v1=%.6f, a0=%.6f, a1=%.6f, vmax=%.6f, amax=%.6f, jmax=%.6f, dt=%.6f, "
                    "sample_count=%zu, total_length=%.6f",
                    static_cast<int>(result),
                    i,
                    ds_step,
                    s_samples[i - 1],
                    s_samples[i],
                    v_profile[i - 1],
                    v_profile[i],
                    current_acc,
                    accel_targets.empty() ? 0.0f : accel_targets[i - 1],
                    input.max_velocity[0],
                    input.max_acceleration[0],
                    input.max_jerk[0],
                    dt,
                    sample_count,
                    total_length);
                report.jerk_plan_failed = true;
                trajectory.total_length = total_length;
                trajectory.total_time = 0.0f;
                trajectory.planning_report = report;
                return trajectory;
            }

            const double traj_duration = traj.get_duration();
            traj.at_time(traj_duration, pos, vel, acc);
            current_acc = static_cast<float32>(acc[0]);

            RuckigSegment seg;
            seg.start_time = current_time;
            seg.duration = static_cast<float32>(traj_duration);
            seg.start_s = current_s;
            seg.trajectory = std::move(traj);
            ruckig_segments.push_back(std::move(seg));

            current_time += ruckig_segments.back().duration;
            current_s += ds_step;
        }

        float32 total_time = ruckig_segments.empty()
                                 ? 0.0f
                                 : ruckig_segments.back().start_time + ruckig_segments.back().duration;
        report.total_time_s = total_time;
        if (total_time <= kEpsilon) {
            report.jerk_plan_failed = true;
            trajectory.total_length = total_length;
            trajectory.total_time = 0.0f;
            trajectory.planning_report = report;
            return trajectory;
        }

        int out_steps = static_cast<int>(std::ceil(total_time / dt));
        if (out_steps < 1) {
            out_steps = 1;
        }

        trajectory.points.reserve(static_cast<size_t>(out_steps) + 1);

        float32 max_speed = 0.0f;
        float32 max_acc = 0.0f;
        float32 max_jerk = 0.0f;
        int32 violation_count = 0;
        float32 prev_acc = 0.0f;
        float32 prev_t = 0.0f;
        bool has_prev = false;

        size_t seg_idx = 0;
        for (int step = 0; step <= out_steps; ++step) {
            float32 t = std::min(total_time, static_cast<float32>(step) * dt);
            while (seg_idx + 1 < ruckig_segments.size() &&
                   t > ruckig_segments[seg_idx].start_time + ruckig_segments[seg_idx].duration + kEpsilon) {
                ++seg_idx;
            }
            const auto& rseg = ruckig_segments[seg_idx];
            float32 local_t = t - rseg.start_time;

            std::array<double, 1> s_pos{};
            std::array<double, 1> s_vel{};
            std::array<double, 1> s_acc{};
            rseg.trajectory.at_time(local_t, s_pos, s_vel, s_acc);

            float32 s = rseg.start_s + static_cast<float32>(s_pos[0]);
            float32 speed = static_cast<float32>(s_vel[0]);
            float32 accel = static_cast<float32>(s_acc[0]);

            max_speed = std::max(max_speed, speed);
            max_acc = std::max(max_acc, std::abs(accel));

            float32 jerk = 0.0f;
            if (has_prev) {
                float32 dt_step = t - prev_t;
                if (dt_step > kEpsilon) {
                    jerk = (accel - prev_acc) / dt_step;
                    max_jerk = std::max(max_jerk, std::abs(jerk));
                }
            }

            size_t geom_idx = FindSegmentIndex(cumulative, s);
            float32 local_s = s - cumulative[geom_idx];
            const auto& geom_seg = path.segments[geom_idx];

            Point2D pos2d;
            Point2D tan;
            float32 curvature = 0.0f;
            EvaluateSegmentAtDistance(geom_seg.geometry, local_s, pos2d, tan, curvature);
            static_cast<void>(curvature);

            MotionTrajectoryPoint point;
            point.t = t;
            point.position = Point3D(pos2d, 0.0f);
            point.velocity = Point3D(tan.x * speed, tan.y * speed, 0.0f);
            point.process_tag = geom_seg.tag;
            point.dispense_on = geom_seg.dispense_on;
            if (geom_seg.dispense_on && config.vmax > kEpsilon) {
                point.flow_rate = geom_seg.flow_rate * (speed / config.vmax);
            } else {
                point.flow_rate = 0.0f;
            }
            trajectory.points.push_back(point);

            if (speed > config.vmax + 1e-3f) {
                ++violation_count;
            }
            if (std::abs(accel) > amax + 1e-3f) {
                ++violation_count;
            }
            if (jerk_enforced && std::abs(jerk) > effective_jmax + 1e-3f) {
                ++violation_count;
            }

            prev_acc = accel;
            prev_t = t;
            has_prev = true;
        }

        trajectory.total_time = total_time;
        trajectory.total_length = total_length;
        report.max_velocity_observed = max_speed;
        report.max_acceleration_observed = max_acc;
        report.max_jerk_observed = max_jerk;
        report.constraint_violations = violation_count;
        trajectory.planning_report = report;
        return trajectory;
    }

    float32 total_time = t_samples.empty() ? 0.0f : t_samples.back();
    if (total_time <= kEpsilon) {
        return trajectory;
    }

    int out_steps = static_cast<int>(std::ceil(total_time / dt));
    if (out_steps < 1) {
        out_steps = 1;
    }

    trajectory.points.reserve(static_cast<size_t>(out_steps) + 1);
    report.total_time_s = total_time;

    float32 max_speed = 0.0f;
    float32 max_acc = 0.0f;
    float32 max_jerk = 0.0f;
    int32 violation_count = 0;
    float32 prev_speed = 0.0f;
    float32 prev_acc = 0.0f;
    float32 prev_t = 0.0f;
    bool has_prev = false;

    size_t idx = 0;
    for (int step = 0; step <= out_steps; ++step) {
        float32 t = std::min(total_time, static_cast<float32>(step) * dt);
        while (idx + 1 < sample_count && t_samples[idx + 1] + kEpsilon < t) {
            ++idx;
        }
        size_t next = std::min(idx + 1, sample_count - 1);
        float32 t0 = t_samples[idx];
        float32 t1 = t_samples[next];
        float32 ratio = (t1 > t0 + kEpsilon) ? (t - t0) / (t1 - t0) : 0.0f;
        float32 s = s_samples[idx] + ratio * (s_samples[next] - s_samples[idx]);

        size_t seg_idx = FindSegmentIndex(cumulative, s);
        float32 local_s = s - cumulative[seg_idx];
        const auto& seg = path.segments[seg_idx];

        Point2D pos;
        Point2D tan;
        float32 curvature = 0.0f;
        EvaluateSegmentAtDistance(seg.geometry, local_s, pos, tan, curvature);
        static_cast<void>(curvature);

        float32 speed = v_profile[idx] + ratio * (v_profile[next] - v_profile[idx]);
        max_speed = std::max(max_speed, speed);

        float32 accel = 0.0f;
        float32 jerk = 0.0f;
        if (has_prev) {
            float32 dt_step = t - prev_t;
            if (dt_step > kEpsilon) {
                accel = (speed - prev_speed) / dt_step;
                max_acc = std::max(max_acc, std::abs(accel));
                if (config.jmax > kEpsilon) {
                    jerk = (accel - prev_acc) / dt_step;
                    max_jerk = std::max(max_jerk, std::abs(jerk));
                }
            }
        }

        MotionTrajectoryPoint point;
        point.t = t;
        point.position = Point3D(pos, 0.0f);
        point.velocity = Point3D(tan.x * speed, tan.y * speed, 0.0f);
        point.process_tag = seg.tag;
        point.dispense_on = seg.dispense_on;
        if (seg.dispense_on && config.vmax > kEpsilon) {
            point.flow_rate = seg.flow_rate * (speed / config.vmax);
        } else {
            point.flow_rate = 0.0f;
        }
        trajectory.points.push_back(point);

        if (speed > config.vmax + 1e-3f) {
            ++violation_count;
        }
        if (std::abs(accel) > amax + 1e-3f) {
            ++violation_count;
        }
        if (config.jmax > kEpsilon && std::abs(jerk) > config.jmax + 1e-3f) {
            ++violation_count;
        }

        prev_speed = speed;
        prev_acc = accel;
        prev_t = t;
        has_prev = true;
    }

    trajectory.total_time = total_time;
    trajectory.total_length = total_length;
    report.max_velocity_observed = max_speed;
    report.max_acceleration_observed = max_acc;
    report.max_jerk_observed = max_jerk;
    report.constraint_violations = violation_count;
    trajectory.planning_report = report;
    return trajectory;
}

}  // namespace Siligen::Domain::Motion::DomainServices

