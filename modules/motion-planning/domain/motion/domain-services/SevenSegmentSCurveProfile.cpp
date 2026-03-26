#include "SevenSegmentSCurveProfile.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Motion::DomainServices {

Result<VelocityProfile> SevenSegmentSCurveProfile::GenerateProfile(
    float32 distance,
    const VelocityProfileConfig& config) const noexcept {

    VelocityProfile profile;
    profile.type = VelocityProfileType::S_CURVE_7_SEG;

    float32 v_max = config.max_velocity;
    float32 a_max = config.max_acceleration;
    float32 j_max = config.max_jerk;
    float32 dt = config.time_step;

    SegmentTimes times = CalculateSegmentTimes(distance, v_max, a_max, j_max);
    profile.total_time = times.Total();

    int num_points = static_cast<int>(std::ceil(profile.total_time / dt)) + 1;
    profile.velocities.reserve(num_points);
    profile.accelerations.reserve(num_points);

    for (int i = 0; i < num_points; ++i) {
        float32 t = i * dt;
        if (t > profile.total_time) {
            t = profile.total_time;
        }

        profile.velocities.push_back(CalculateVelocityAt(t, times, v_max, a_max, j_max));
        profile.accelerations.push_back(CalculateAccelerationAt(t, times, a_max, j_max));
    }

    return Result<VelocityProfile>::Success(profile);
}

SevenSegmentSCurveProfile::SegmentTimes SevenSegmentSCurveProfile::CalculateSegmentTimes(
    float32 distance,
    float32 v_max,
    float32 a_max,
    float32 j_max) const noexcept {

    SegmentTimes times;

    float32 t_j = a_max / j_max;
    float32 t_a = v_max / a_max;

    if (t_a < 2 * t_j) {
        t_j = std::sqrt(v_max / j_max);
        times.t1 = t_j;
        times.t2 = 0;
        times.t3 = t_j;
    } else {
        times.t1 = t_j;
        times.t2 = t_a - 2 * t_j;
        times.t3 = t_j;
    }

    float32 s_acc = 0;
    if (times.t2 > 0) {
        s_acc = v_max * (times.t1 + times.t2 + times.t3) / 2.0f;
    } else {
        s_acc = j_max * times.t1 * times.t1 * times.t1 / 3.0f;
    }

    if (2 * s_acc > distance) {
        float32 v_actual = std::sqrt(distance * a_max);
        t_a = v_actual / a_max;

        if (t_a < 2 * t_j) {
            t_j = t_a / 2.0f;
        }

        times.t1 = t_j;
        times.t2 = std::max(0.0f, t_a - 2 * t_j);
        times.t3 = t_j;
        times.t4 = 0;
        times.t5 = t_j;
        times.t6 = times.t2;
        times.t7 = t_j;
        return times;
    }

    float32 s_const = distance - 2 * s_acc;
    times.t4 = s_const / v_max;
    times.t5 = times.t3;
    times.t6 = times.t2;
    times.t7 = times.t1;

    return times;
}

float32 SevenSegmentSCurveProfile::CalculateVelocityAt(
    float32 t,
    const SegmentTimes& times,
    float32 v_max,
    float32 a_max,
    float32 j_max) const noexcept {

    float32 v = 0;
    float32 t_end1 = times.t1;
    float32 t_end2 = t_end1 + times.t2;
    float32 t_end3 = t_end2 + times.t3;
    float32 t_end4 = t_end3 + times.t4;
    float32 t_end5 = t_end4 + times.t5;
    float32 t_end6 = t_end5 + times.t6;

    if (t <= t_end1) {
        v = 0.5f * j_max * t * t;
    } else if (t <= t_end2) {
        float32 v1 = 0.5f * j_max * times.t1 * times.t1;
        float32 dt = t - t_end1;
        v = v1 + a_max * dt;
    } else if (t <= t_end3) {
        float32 v1 = 0.5f * j_max * times.t1 * times.t1;
        float32 v2 = v1 + a_max * times.t2;
        float32 dt = t - t_end2;
        v = v2 + a_max * dt - 0.5f * j_max * dt * dt;
    } else if (t <= t_end4) {
        v = v_max;
    } else if (t <= t_end5) {
        float32 dt = t - t_end4;
        v = v_max - 0.5f * j_max * dt * dt;
    } else if (t <= t_end6) {
        float32 v5 = v_max - 0.5f * j_max * times.t5 * times.t5;
        float32 dt = t - t_end5;
        v = v5 - a_max * dt;
    } else {
        float32 v5 = v_max - 0.5f * j_max * times.t5 * times.t5;
        float32 v6 = v5 - a_max * times.t6;
        float32 dt = t - t_end6;
        v = v6 - a_max * dt + 0.5f * j_max * dt * dt;
    }

    return std::max(0.0f, v);
}

float32 SevenSegmentSCurveProfile::CalculateAccelerationAt(
    float32 t,
    const SegmentTimes& times,
    float32 a_max,
    float32 j_max) const noexcept {

    float32 a = 0;
    float32 t_end1 = times.t1;
    float32 t_end2 = t_end1 + times.t2;
    float32 t_end3 = t_end2 + times.t3;
    float32 t_end4 = t_end3 + times.t4;
    float32 t_end5 = t_end4 + times.t5;
    float32 t_end6 = t_end5 + times.t6;

    if (t <= t_end1) {
        a = j_max * t;
    } else if (t <= t_end2) {
        a = a_max;
    } else if (t <= t_end3) {
        float32 dt = t - t_end2;
        a = a_max - j_max * dt;
    } else if (t <= t_end4) {
        a = 0;
    } else if (t <= t_end5) {
        float32 dt = t - t_end4;
        a = -j_max * dt;
    } else if (t <= t_end6) {
        a = -a_max;
    } else {
        float32 dt = t - t_end6;
        a = -a_max + j_max * dt;
    }

    return a;
}

}  // namespace Siligen::Domain::Motion::DomainServices
