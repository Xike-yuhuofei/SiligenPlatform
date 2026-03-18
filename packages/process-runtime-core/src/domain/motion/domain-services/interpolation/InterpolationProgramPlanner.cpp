#include "InterpolationProgramPlanner.h"

#include "domain/trajectory/value-objects/GeometryUtils.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint;
using Siligen::Domain::Motion::Ports::InterpolationType;
using Siligen::Domain::Trajectory::ValueObjects::Segment;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::Domain::Trajectory::ValueObjects::ArcPrimitive;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcLength;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcSweep;
using Siligen::Domain::Trajectory::ValueObjects::ArcPoint;
using Siligen::Domain::Trajectory::ValueObjects::SegmentEnd;

namespace {
constexpr float32 kEpsilon = 1e-6f;
constexpr const char* kErrorModule = "DXFDispensingPlanner";

struct SpeedSamples {
    std::vector<float32> s;
    std::vector<float32> v;
    float32 total_length = 0.0f;
};

float32 SpeedMagnitude(const MotionTrajectoryPoint& point) {
    float32 vx = point.velocity.x;
    float32 vy = point.velocity.y;
    return std::sqrt(vx * vx + vy * vy);
}

SpeedSamples BuildSpeedSamples(const MotionTrajectory& trajectory) {
    SpeedSamples samples;
    if (trajectory.points.size() < 2) {
        return samples;
    }

    samples.s.reserve(trajectory.points.size());
    samples.v.reserve(trajectory.points.size());

    samples.s.push_back(0.0f);
    samples.v.push_back(SpeedMagnitude(trajectory.points.front()));

    float32 cumulative = 0.0f;
    for (size_t i = 1; i < trajectory.points.size(); ++i) {
        const auto& prev = trajectory.points[i - 1];
        const auto& curr = trajectory.points[i];
        float32 ds = prev.position.DistanceTo3D(curr.position);
        cumulative += ds;
        samples.s.push_back(cumulative);

        float32 speed = SpeedMagnitude(curr);
        if (speed <= kEpsilon) {
            float32 dt = curr.t - prev.t;
            if (dt > kEpsilon && ds > kEpsilon) {
                speed = ds / dt;
            } else {
                speed = samples.v.back();
            }
        }
        samples.v.push_back(speed);
    }

    samples.total_length = cumulative;
    return samples;
}

float32 InterpolateSpeedAtS(const SpeedSamples& samples, float32 s) {
    if (samples.s.empty()) {
        return 0.0f;
    }
    if (s <= 0.0f) {
        return samples.v.front();
    }
    if (s >= samples.s.back()) {
        return samples.v.back();
    }

    auto it = std::lower_bound(samples.s.begin(), samples.s.end(), s);
    size_t idx = static_cast<size_t>(std::distance(samples.s.begin(), it));
    if (idx == 0) {
        return samples.v.front();
    }
    size_t prev = idx - 1;
    float32 s0 = samples.s[prev];
    float32 s1 = samples.s[idx];
    if (s1 - s0 <= kEpsilon) {
        return samples.v[idx];
    }
    float32 ratio = (s - s0) / (s1 - s0);
    return samples.v[prev] + ratio * (samples.v[idx] - samples.v[prev]);
}

float32 MaxSpeedInRange(const SpeedSamples& samples, float32 s_start, float32 s_end) {
    if (samples.s.empty()) {
        return 0.0f;
    }
    if (s_end < s_start) {
        std::swap(s_start, s_end);
    }
    float32 v_max = std::max(InterpolateSpeedAtS(samples, s_start), InterpolateSpeedAtS(samples, s_end));

    auto it_start = std::lower_bound(samples.s.begin(), samples.s.end(), s_start);
    auto it_end = std::lower_bound(samples.s.begin(), samples.s.end(), s_end);
    size_t idx_start = static_cast<size_t>(std::distance(samples.s.begin(), it_start));
    size_t idx_end = static_cast<size_t>(std::distance(samples.s.begin(), it_end));
    if (idx_end >= samples.v.size()) {
        idx_end = samples.v.size() - 1;
    }
    for (size_t i = idx_start; i <= idx_end && i < samples.v.size(); ++i) {
        v_max = std::max(v_max, samples.v[i]);
    }
    return v_max;
}

bool IsFiniteNumber(float32 value) {
    return std::isfinite(value) != 0;
}

float32 SegmentLength(const Segment& segment) {
    if (segment.length > kEpsilon) {
        return segment.length;
    }
    if (segment.type == SegmentType::Line) {
        return segment.line.start.DistanceTo(segment.line.end);
    }
    return ComputeArcLength(segment.arc);
}

}  // namespace

Result<std::vector<InterpolationData>> InterpolationProgramPlanner::BuildProgram(
    const ProcessPath& path,
    const MotionTrajectory& trajectory,
    float32 acceleration) const noexcept {
    if (path.segments.empty()) {
        return Result<std::vector<InterpolationData>>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "轨迹段为空，无法生成插补程序", kErrorModule));
    }

    auto samples = BuildSpeedSamples(trajectory);
    if (samples.s.empty()) {
        return Result<std::vector<InterpolationData>>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "速度轨迹为空，无法生成插补程序", kErrorModule));
    }

    std::vector<InterpolationData> program;
    program.reserve(path.segments.size());

    float32 s_cursor = 0.0f;
    for (size_t i = 0; i < path.segments.size(); ++i) {
        const auto& segment = path.segments[i].geometry;
        float32 seg_len = SegmentLength(segment);
        if (seg_len <= kEpsilon) {
            continue;
        }

        float32 s_start = s_cursor;
        float32 s_end = s_cursor + seg_len;
        s_cursor = s_end;

        float32 v_end = InterpolateSpeedAtS(samples, s_end);
        float32 v_max = MaxSpeedInRange(samples, s_start, s_end);
        float32 v_cmd = std::max(v_max, v_end);

        if (segment.type == SegmentType::Line) {
            InterpolationData data;
            data.type = InterpolationType::LINEAR;
            data.positions = {segment.line.end.x, segment.line.end.y};
            data.velocity = v_cmd;
            data.acceleration = acceleration;
            data.end_velocity = (i + 1 < path.segments.size()) ? v_end : 0.0f;
            program.push_back(std::move(data));
            continue;
        }

        if (segment.type != SegmentType::Arc) {
            return Result<std::vector<InterpolationData>>::Failure(
                Error(ErrorCode::INVALID_PARAMETER,
                      "不支持的轨迹段类型 (段 " + std::to_string(i + 1) + ")",
                      kErrorModule));
        }

        const auto& arc = segment.arc;
        if (!IsFiniteNumber(arc.center.x) || !IsFiniteNumber(arc.center.y) ||
            !IsFiniteNumber(arc.radius) || !IsFiniteNumber(arc.start_angle_deg) ||
            !IsFiniteNumber(arc.end_angle_deg)) {
            return Result<std::vector<InterpolationData>>::Failure(
                Error(ErrorCode::INVALID_PARAMETER,
                      "圆弧段参数无效(含NaN/Inf) (段 " + std::to_string(i + 1) + ")",
                      kErrorModule));
        }
        if (arc.radius <= kEpsilon) {
            return Result<std::vector<InterpolationData>>::Failure(
                Error(ErrorCode::INVALID_PARAMETER,
                      "圆弧段半径无效 (段 " + std::to_string(i + 1) + ", r=" + std::to_string(arc.radius) + ")",
                      kErrorModule));
        }

        float32 sweep = ComputeArcSweep(arc.start_angle_deg, arc.end_angle_deg, arc.clockwise);
        if (sweep <= kEpsilon) {
            return Result<std::vector<InterpolationData>>::Failure(
                Error(ErrorCode::INVALID_PARAMETER,
                      "圆弧段角度跨度无效 (段 " + std::to_string(i + 1) + ", sweep=" + std::to_string(sweep) + ")",
                      kErrorModule));
        }
        if (sweep > 360.0f + 1e-3f) {
            return Result<std::vector<InterpolationData>>::Failure(
                Error(ErrorCode::INVALID_PARAMETER,
                      "圆弧段角度跨度超限 (段 " + std::to_string(i + 1) + ", sweep=" + std::to_string(sweep) + ")",
                      kErrorModule));
        }

        const bool clockwise = arc.clockwise;
        const float32 direction = clockwise ? -1.0f : 1.0f;
        const bool full_circle = std::abs(sweep - 360.0f) <= 1e-3f;

        if (full_circle) {
            float32 half_sweep = sweep * 0.5f;
            float32 mid_angle = arc.start_angle_deg + direction * half_sweep;
            Point2D mid_point = ArcPoint(arc, mid_angle);
            Point2D end_point = SegmentEnd(segment);

            float32 s_mid = s_start + seg_len * 0.5f;
            float32 v_mid = InterpolateSpeedAtS(samples, s_mid);
            float32 v_max_first = MaxSpeedInRange(samples, s_start, s_mid);
            float32 v_max_second = MaxSpeedInRange(samples, s_mid, s_end);

            InterpolationData first;
            first.type = clockwise ? InterpolationType::CIRCULAR_CW : InterpolationType::CIRCULAR_CCW;
            first.positions = {mid_point.x, mid_point.y};
            first.center_x = arc.center.x;
            first.center_y = arc.center.y;
            first.circle_plane = 0;
            first.velocity = std::max(v_max_first, v_mid);
            first.acceleration = acceleration;
            first.end_velocity = v_mid;
            program.push_back(std::move(first));

            InterpolationData second;
            second.type = clockwise ? InterpolationType::CIRCULAR_CW : InterpolationType::CIRCULAR_CCW;
            second.positions = {end_point.x, end_point.y};
            second.center_x = arc.center.x;
            second.center_y = arc.center.y;
            second.circle_plane = 0;
            second.velocity = std::max(v_max_second, v_end);
            second.acceleration = acceleration;
            second.end_velocity = (i + 1 < path.segments.size()) ? v_end : 0.0f;
            program.push_back(std::move(second));
            continue;
        }

        Point2D end_point = SegmentEnd(segment);
        InterpolationData data;
        data.type = clockwise ? InterpolationType::CIRCULAR_CW : InterpolationType::CIRCULAR_CCW;
        data.positions = {end_point.x, end_point.y};
        data.center_x = arc.center.x;
        data.center_y = arc.center.y;
        data.circle_plane = 0;
        data.velocity = v_cmd;
        data.acceleration = acceleration;
        data.end_velocity = (i + 1 < path.segments.size()) ? v_end : 0.0f;
        program.push_back(std::move(data));
    }

    if (program.empty()) {
        return Result<std::vector<InterpolationData>>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "插补程序为空", kErrorModule));
    }

    return Result<std::vector<InterpolationData>>::Success(program);
}

}  // namespace Siligen::Domain::Motion::DomainServices
