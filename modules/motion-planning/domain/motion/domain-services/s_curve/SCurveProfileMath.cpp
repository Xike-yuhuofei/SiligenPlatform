#include "SCurveProfileMath.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Motion::DomainServices::SCurveMath {

namespace {

constexpr float32 kEps = 1e-6f;

float32 Clamp(float32 v, float32 lo, float32 hi) {
    return std::max(lo, std::min(hi, v));
}

/// Compute distance for one accel or decel phase (triangular or trapezoidal jerk profile).
/// @param v_start  Initial velocity.
/// @param v_end    Final velocity (must be >= v_start for accel; for decel caller mirrors).
/// @param amax     Max acceleration limit.
/// @param jmax     Max jerk limit.
/// @param[out] t1  Jerk-up time.
/// @param[out] t2  Constant-acceleration time.
/// @param[out] t3  Jerk-down time.
/// @param[out] a_peak  Peak acceleration reached.
/// @return         Distance covered during this phase.
float32 ComputePhase(float32 v_start, float32 v_end, float32 amax, float32 jmax,
                     float32& t1, float32& t2, float32& t3, float32& a_peak) {
    t1 = t2 = t3 = a_peak = 0;
    float32 delta = v_end - v_start;
    if (delta <= kEps) {
        return 0.0f;
    }

    float32 v_tri = amax * amax / jmax;  // delta_v threshold: triangular vs trapezoidal

    if (delta >= v_tri) {
        // --- Trapezoidal: reach full amax ---
        t1 = amax / jmax;
        t3 = t1;
        t2 = delta / amax - t1;         // CORRECTED: was t_a - 2*t_j in old code (bug)
        a_peak = amax;

        float32 v1 = v_start + jmax * t1 * t1 * 0.5f;              // end of jerk-up
        float32 v2 = v1 + amax * t2;                               // end of constant accel

        float32 s1 = v_start * t1 + jmax * t1 * t1 * t1 / 6.0f;   // 0 → t1
        float32 s2 = v1 * t2 + amax * t2 * t2 * 0.5f;             // t1 → t1+t2
        float32 s3 = v2 * t3 + amax * t3 * t3 * 0.5f              // t1+t2 → t1+t2+t3
                     - jmax * t3 * t3 * t3 / 6.0f;

        return s1 + s2 + s3;
    }

    // --- Triangular: never reach amax ---
    t1 = std::sqrt(delta / jmax);
    t3 = t1;
    t2 = 0;
    a_peak = jmax * t1;

    float32 v_half = v_start + delta * 0.5f;  // velocity at transition (end of t1)

    float32 s1 = v_start * t1 + jmax * t1 * t1 * t1 / 6.0f;  // 0 → t1
    float32 s3 = v_half * t3 + jmax * t1 * t1 * t1 / 3.0f;    // t1 → 2*t1 (CORRECTED: was /3 vs /6)

    // s3 derivation: v(t) = v_half + a_peak*τ - j*τ²/2, τ = t - t1
    // ∫v dτ from 0 to t1 = v_half*t1 + a_peak*t1²/2 - j*t1³/6
    // = v_half*t1 + j*t1³/2 - j*t1³/6 = v_half*t1 + j*t1³/3

    return s1 + s3;
}

}  // namespace

// ---------------------------------------------------------------------------
// ComputeSegmentTimes
// ---------------------------------------------------------------------------
SCurveProfileData ComputeSegmentTimes(float32 distance, const SCurveConfig& config) noexcept {
    SCurveProfileData data;

    float32 v0 = Clamp(config.start_velocity, 0.0f, config.max_velocity);
    float32 vmax = std::max(0.0f, config.max_velocity);
    float32 v1 = Clamp(config.end_velocity, 0.0f, config.max_velocity);
    float32 amax = std::max(kEps, config.max_acceleration);
    float32 jmax = std::max(kEps, config.max_jerk);

    if (distance <= kEps || vmax <= kEps) {
        return data;  // all zeros — no motion
    }

    // --- attempt with full vmax ---
    float32 v_peak = vmax;

    float32 a_t1, a_t2, a_t3, a_peak;
    float32 d_t1, d_t2, d_t3, d_peak;

    float32 s_acc = ComputePhase(v0, v_peak, amax, jmax, a_t1, a_t2, a_t3, a_peak);
    float32 s_dec = ComputePhase(v1, v_peak, amax, jmax, d_t1, d_t2, d_t3, d_peak);
    // Note: decel phase is computed symmetrically — mirror the accel math.
    // In the motion profile, t5..t7 are the mirrored decel phases.
    // s_dec above is the distance covered during deceleration from v_peak to v1.

    if (s_acc + s_dec >= distance) {
        // Cannot reach vmax — binary search for achievable v_peak
        float32 lo = std::max(v0, v1);
        float32 hi = vmax;
        for (int iter = 0; iter < 50; ++iter) {
            v_peak = (lo + hi) * 0.5f;
            if (v_peak - lo <= kEps) break;

            s_acc = ComputePhase(v0, v_peak, amax, jmax, a_t1, a_t2, a_t3, a_peak);
            s_dec = ComputePhase(v1, v_peak, amax, jmax, d_t1, d_t2, d_t3, d_peak);

            if (s_acc + s_dec >= distance) {
                hi = v_peak;
            } else {
                lo = v_peak;
            }
        }
        v_peak = lo;
        static_cast<void>(ComputePhase(v0, v_peak, amax, jmax, a_t1, a_t2, a_t3, a_peak));
        static_cast<void>(ComputePhase(v1, v_peak, amax, jmax, d_t1, d_t2, d_t3, d_peak));
        data.t4 = 0;
    } else {
        data.t4 = (distance - s_acc - s_dec) / v_peak;
    }

    data.t1 = a_t1;
    data.t2 = a_t2;
    data.t3 = a_t3;
    data.t5 = d_t1;
    data.t6 = d_t2;
    data.t7 = d_t3;
    data.v_peak = v_peak;
    data.a_peak_accel = a_peak;
    data.a_peak_decel = d_peak;

    return data;
}

// ---------------------------------------------------------------------------
// VelocityAt
// ---------------------------------------------------------------------------
float32 VelocityAt(float32 t, const SCurveProfileData& d, const SCurveConfig& cfg) noexcept {
    float32 j = cfg.max_jerk;
    float32 a_max_accel = d.a_peak_accel > 0 ? d.a_peak_accel : cfg.max_acceleration;
    float32 a_max_decel = d.a_peak_decel > 0 ? d.a_peak_decel : cfg.max_acceleration;
    float32 v0 = Clamp(cfg.start_velocity, 0.0f, cfg.max_velocity);
    float32 v_peak = d.v_peak;

    float32 e1 = d.t1;
    float32 e2 = e1 + d.t2;
    float32 e3 = e2 + d.t3;
    float32 e4 = e3 + d.t4;
    float32 e5 = e4 + d.t5;
    float32 e6 = e5 + d.t6;

    if (t <= e1) {
        // segment 1: jerk-up
        return v0 + 0.5f * j * t * t;
    }
    if (t <= e2) {
        // segment 2: constant acceleration
        float32 v1 = v0 + 0.5f * j * d.t1 * d.t1;
        return v1 + a_max_accel * (t - e1);
    }
    if (t <= e3) {
        // segment 3: jerk-down
        float32 v1 = v0 + 0.5f * j * d.t1 * d.t1;
        float32 v2 = v1 + a_max_accel * d.t2;
        float32 dt = t - e2;
        return v2 + a_max_accel * dt - 0.5f * j * dt * dt;
    }
    if (t <= e4) {
        // segment 4: constant velocity
        return v_peak;
    }
    if (t <= e5) {
        // segment 5: jerk-down (decel)
        float32 dt = t - e4;
        return v_peak - 0.5f * j * dt * dt;
    }
    if (t <= e6) {
        // segment 6: constant deceleration
        float32 v5 = v_peak - 0.5f * j * d.t5 * d.t5;
        return v5 - a_max_decel * (t - e5);
    }
    // segment 7: jerk-up (decel)
    float32 v5 = v_peak - 0.5f * j * d.t5 * d.t5;
    float32 v6 = v5 - a_max_decel * d.t6;
    float32 dt = t - e6;
    return std::max(0.0f, v6 - a_max_decel * dt + 0.5f * j * dt * dt);
}

// ---------------------------------------------------------------------------
// AccelerationAt
// ---------------------------------------------------------------------------
float32 AccelerationAt(float32 t, const SCurveProfileData& d, const SCurveConfig& cfg) noexcept {
    float32 j = cfg.max_jerk;
    float32 a_max_accel = d.a_peak_accel > 0 ? d.a_peak_accel : cfg.max_acceleration;
    float32 a_max_decel = d.a_peak_decel > 0 ? d.a_peak_decel : cfg.max_acceleration;

    float32 e1 = d.t1;
    float32 e2 = e1 + d.t2;
    float32 e3 = e2 + d.t3;
    float32 e4 = e3 + d.t4;
    float32 e5 = e4 + d.t5;
    float32 e6 = e5 + d.t6;

    if (t <= e1) {
        return j * t;
    }
    if (t <= e2) {
        return a_max_accel;
    }
    if (t <= e3) {
        return a_max_accel - j * (t - e2);
    }
    if (t <= e4) {
        return 0;
    }
    if (t <= e5) {
        return -j * (t - e4);
    }
    if (t <= e6) {
        return -a_max_decel;
    }
    return -a_max_decel + j * (t - e6);
}

// ---------------------------------------------------------------------------
// PositionAt
// ---------------------------------------------------------------------------
float32 PositionAt(float32 t, const SCurveProfileData& d, const SCurveConfig& cfg) noexcept {
    float32 j = cfg.max_jerk;
    float32 a_max_a = d.a_peak_accel > 0 ? d.a_peak_accel : cfg.max_acceleration;
    float32 a_max_d = d.a_peak_decel > 0 ? d.a_peak_decel : cfg.max_acceleration;
    float32 v0 = Clamp(cfg.start_velocity, 0.0f, cfg.max_velocity);
    float32 v_peak = d.v_peak;

    float32 e1 = d.t1;
    float32 e2 = e1 + d.t2;
    float32 e3 = e2 + d.t3;
    float32 e4 = e3 + d.t4;
    float32 e5 = e4 + d.t5;
    float32 e6 = e5 + d.t6;

    // Pre-compute segment-boundary velocities for position integration
    float32 v1 = v0 + 0.5f * j * d.t1 * d.t1;         // end of seg 1
    float32 v2 = v1 + a_max_a * d.t2;                  // end of seg 2
    float32 v5 = v_peak - 0.5f * j * d.t5 * d.t5;      // end of seg 5
    float32 v6 = v5 - a_max_d * d.t6;                   // end of seg 6

    if (t <= e1) {
        float32 s = v0 * t + (j * t * t * t) / 6.0f;
        return std::max(0.0f, s);
    }
    if (t <= e2) {
        float32 s1 = v0 * d.t1 + (j * d.t1 * d.t1 * d.t1) / 6.0f;
        float32 dt = t - e1;
        return s1 + v1 * dt + 0.5f * a_max_a * dt * dt;
    }
    if (t <= e3) {
        float32 s1 = v0 * d.t1 + (j * d.t1 * d.t1 * d.t1) / 6.0f;
        float32 s2 = v1 * d.t2 + 0.5f * a_max_a * d.t2 * d.t2;
        float32 dt = t - e2;
        return s1 + s2 + v2 * dt + 0.5f * a_max_a * dt * dt - (j * dt * dt * dt) / 6.0f;
    }
    if (t <= e4) {
        float32 s1 = v0 * d.t1 + (j * d.t1 * d.t1 * d.t1) / 6.0f;
        float32 s2 = v1 * d.t2 + 0.5f * a_max_a * d.t2 * d.t2;
        float32 s3 = v2 * d.t3 + 0.5f * a_max_a * d.t3 * d.t3 - (j * d.t3 * d.t3 * d.t3) / 6.0f;
        float32 dt = t - e3;
        return s1 + s2 + s3 + v_peak * dt;
    }
    if (t <= e5) {
        float32 s1 = v0 * d.t1 + (j * d.t1 * d.t1 * d.t1) / 6.0f;
        float32 s2 = v1 * d.t2 + 0.5f * a_max_a * d.t2 * d.t2;
        float32 s3 = v2 * d.t3 + 0.5f * a_max_a * d.t3 * d.t3 - (j * d.t3 * d.t3 * d.t3) / 6.0f;
        float32 s4 = v_peak * d.t4;
        float32 dt = t - e4;
        return s1 + s2 + s3 + s4 + v_peak * dt - (j * dt * dt * dt) / 6.0f;
    }
    if (t <= e6) {
        float32 s1 = v0 * d.t1 + (j * d.t1 * d.t1 * d.t1) / 6.0f;
        float32 s2 = v1 * d.t2 + 0.5f * a_max_a * d.t2 * d.t2;
        float32 s3 = v2 * d.t3 + 0.5f * a_max_a * d.t3 * d.t3 - (j * d.t3 * d.t3 * d.t3) / 6.0f;
        float32 s4 = v_peak * d.t4;
        float32 s5 = v_peak * d.t5 - (j * d.t5 * d.t5 * d.t5) / 6.0f;
        float32 dt = t - e5;
        return s1 + s2 + s3 + s4 + s5 + v5 * dt - 0.5f * a_max_d * dt * dt;
    }
    // segment 7
    float32 s1 = v0 * d.t1 + (j * d.t1 * d.t1 * d.t1) / 6.0f;
    float32 s2 = v1 * d.t2 + 0.5f * a_max_a * d.t2 * d.t2;
    float32 s3 = v2 * d.t3 + 0.5f * a_max_a * d.t3 * d.t3 - (j * d.t3 * d.t3 * d.t3) / 6.0f;
    float32 s4 = v_peak * d.t4;
    float32 s5 = v_peak * d.t5 - (j * d.t5 * d.t5 * d.t5) / 6.0f;
    float32 s6 = v5 * d.t6 - 0.5f * a_max_d * d.t6 * d.t6;
    float32 dt = t - e6;
    return s1 + s2 + s3 + s4 + s5 + s6 + v6 * dt - 0.5f * a_max_d * dt * dt + (j * dt * dt * dt) / 6.0f;
}

// ---------------------------------------------------------------------------
// GenerateProfile
// ---------------------------------------------------------------------------
std::vector<SCurveSample> GenerateProfile(float32 distance,
                                          const SCurveConfig& config,
                                          float32 time_step) noexcept {
    std::vector<SCurveSample> samples;
    if (distance <= kEps || config.max_velocity <= kEps || time_step <= kEps) {
        return samples;
    }

    auto data = ComputeSegmentTimes(distance, config);
    float32 T = data.TotalTime();
    if (T <= kEps) {
        return samples;
    }

    int num_samples = static_cast<int>(std::ceil(T / time_step)) + 1;
    samples.reserve(num_samples);

    for (int i = 0; i < num_samples; ++i) {
        float32 t = std::min(static_cast<float32>(i) * time_step, T);
        SCurveSample pt;
        pt.t = t;
        pt.velocity = VelocityAt(t, data, config);
        pt.acceleration = AccelerationAt(t, data, config);
        pt.position = PositionAt(t, data, config);
        samples.push_back(pt);
    }

    return samples;
}

}  // namespace Siligen::Domain::Motion::DomainServices::SCurveMath
