#include "MotionPlanner.h"

#include "process_path/contracts/GeometryUtils.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <algorithm>
#include <cmath>
#include <vector>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "MotionPlanner"

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint;
using Siligen::ProcessPath::Contracts::ArcPrimitive;
using Siligen::ProcessPath::Contracts::LinePrimitive;
using Siligen::ProcessPath::Contracts::ProcessSegment;
using Siligen::ProcessPath::Contracts::ProcessTag;
using Siligen::ProcessPath::Contracts::Segment;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::ProcessPath::Contracts::ArcPoint;
using Siligen::ProcessPath::Contracts::ArcTangent;
using Siligen::ProcessPath::Contracts::Clamp;
using Siligen::ProcessPath::Contracts::ComputeArcLength;
using Siligen::ProcessPath::Contracts::LineDirection;
using Siligen::ProcessPath::Contracts::NormalizeAngle;
using Siligen::Shared::Types::Point2D;
using Siligen::Point3D;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::kRadToDeg;

namespace {
constexpr float32 kEpsilon = 1e-6f;
constexpr float32 kMinIntegrationVelocity = 1e-3f;

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

float32 ApplyTagLimit(ProcessTag tag, float32 vmax, const TimePlanningConfig& config) {
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

float32 ResolveSampleDs(const TimePlanningConfig& config) {
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

float32 ResolveSegmentSampleDs(const Segment& segment, float32 base_ds, const TimePlanningConfig& config) {
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
                                          const TimePlanningConfig& config) {
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
                                         const TimePlanningConfig& config,
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

MotionPlan MotionPlanner::Plan(const ProcessPath& path, const TimePlanningConfig& config) const {
    MotionPlan trajectory;
    Siligen::Domain::Motion::ValueObjects::MotionPlanningReport report;
    if (path.segments.empty()) {
        return trajectory;
    }

    const float32 amax = std::max(config.amax, 1e-3f);
    const float32 dt = (config.sample_dt > kEpsilon) ? config.sample_dt : 0.01f;
    const float32 ds = ResolveSampleDs(config);
    const bool jerk_enforced = config.enforce_jerk_limit && config.jmax > kEpsilon;
    const float32 effective_jmax = jerk_enforced ? config.jmax : 0.0f;

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

    if (jerk_enforced) {
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

    std::vector<float32> t_samples(sample_count, 0.0f);
    float32 integration_error = 0.0f;
    int32 integration_fallbacks = 0;
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
    }

    report.total_length_mm = total_length;
    report.time_integration_error_s = integration_error;
    report.time_integration_fallbacks = integration_fallbacks;
    report.jerk_limit_enforced = jerk_enforced;

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
                if (jerk_enforced) {
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
        if (jerk_enforced && std::abs(jerk) > config.jmax + 1e-3f) {
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

