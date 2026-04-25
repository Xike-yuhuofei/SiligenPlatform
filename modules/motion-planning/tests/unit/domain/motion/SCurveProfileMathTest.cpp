#include "domain/motion/domain-services/s_curve/SCurveProfileMath.h"
#include "domain/motion/domain-services/SevenSegmentSCurveProfile.h"

#include <cmath>
#include <gtest/gtest.h>

namespace SCurveMath = Siligen::Domain::Motion::DomainServices::SCurveMath;
using SCurveMath::SCurveConfig;
using SCurveMath::SCurveProfileData;

namespace {

constexpr float kEps = 1e-4f;

TEST(SCurveProfileMathTest, SymmetricTrapezoidal) {
    SCurveConfig cfg;
    cfg.start_velocity = 0.0f;
    cfg.max_velocity = 150.0f;  // delta=150 > v_tri=100 -> clearly trapezoidal
    cfg.end_velocity = 0.0f;
    cfg.max_acceleration = 1000.0f;
    cfg.max_jerk = 10000.0f;

    float32 distance = 500.0f;  // large enough for full 7-segment
    auto data = SCurveMath::ComputeSegmentTimes(distance, cfg);

    // All 7 segments present, t4 > 0 (cruise phase)
    EXPECT_GT(data.t1, 0);
    EXPECT_GT(data.t2, 0);
    EXPECT_GT(data.t3, 0);
    EXPECT_GT(data.t4, 0);
    EXPECT_GT(data.t5, 0);
    EXPECT_GT(data.t6, 0);
    EXPECT_GT(data.t7, 0);
    EXPECT_EQ(data.v_peak, 150.0f);

    // Position at total time should equal distance
    float32 total = data.TotalTime();
    float32 pos = SCurveMath::PositionAt(total, data, cfg);
    EXPECT_NEAR(pos, distance, 1.0f);

    // Velocity at start and end should be 0
    EXPECT_NEAR(SCurveMath::VelocityAt(0, data, cfg), 0.0f, kEps);
    EXPECT_NEAR(SCurveMath::VelocityAt(total, data, cfg), 0.0f, 1.0f);
}

TEST(SCurveProfileMathTest, AsymmetricTrapezoidal) {
    SCurveConfig cfg;
    cfg.start_velocity = 10.0f;  // non-zero start
    cfg.max_velocity = 100.0f;
    cfg.end_velocity = 20.0f;    // non-zero end
    cfg.max_acceleration = 1000.0f;
    cfg.max_jerk = 10000.0f;

    float32 distance = 500.0f;
    auto data = SCurveMath::ComputeSegmentTimes(distance, cfg);

    // Accel phase (t1+t2+t3) should differ from decel phase (t5+t6+t7)
    float32 accel_time = data.t1 + data.t2 + data.t3;
    float32 decel_time = data.t5 + data.t6 + data.t7;
    EXPECT_NE(accel_time, decel_time);

    EXPECT_EQ(data.v_peak, 100.0f);

    // Position at total time should equal distance
    float32 total = data.TotalTime();
    float32 pos = SCurveMath::PositionAt(total, data, cfg);
    EXPECT_NEAR(pos, distance, 1.0f);

    // Start and end velocities should match config
    EXPECT_NEAR(SCurveMath::VelocityAt(0, data, cfg), 10.0f, kEps);
    EXPECT_NEAR(SCurveMath::VelocityAt(total, data, cfg), 20.0f, 1.0f);
}

TEST(SCurveProfileMathTest, TriangularProfile) {
    // Distance too short to reach max acceleration -> triangular profile
    SCurveConfig cfg;
    cfg.start_velocity = 0.0f;
    cfg.max_velocity = 100.0f;
    cfg.end_velocity = 0.0f;
    cfg.max_acceleration = 1000.0f;
    cfg.max_jerk = 10000.0f;

    float32 distance = 2.0f;  // short distance
    auto data = SCurveMath::ComputeSegmentTimes(distance, cfg);

    // No constant-acceleration phase (triangular)
    EXPECT_FLOAT_EQ(data.t2, 0.0f);
    EXPECT_FLOAT_EQ(data.t6, 0.0f);

    // Both accel and decel phases exist
    EXPECT_GT(data.t1, 0);
    EXPECT_GT(data.t3, 0);
    EXPECT_GT(data.t5, 0);
    EXPECT_GT(data.t7, 0);

    // Peak velocity should be less than max_velocity
    EXPECT_LT(data.v_peak, 100.0f);

    // Position at total time should equal distance
    float32 total = data.TotalTime();
    float32 pos = SCurveMath::PositionAt(total, data, cfg);
    EXPECT_NEAR(pos, distance, 0.1f);
}

TEST(SCurveProfileMathTest, VeryShortDistanceTriggersBinarySearch) {
    SCurveConfig cfg;
    cfg.start_velocity = 0.0f;
    cfg.max_velocity = 100.0f;
    cfg.end_velocity = 50.0f;  // non-zero end -> asymmetric -> harder to converge
    cfg.max_acceleration = 1000.0f;
    cfg.max_jerk = 10000.0f;

    float32 distance = 8.0f;  // short enough for binary search, above min feasible (~3.5)
    auto data = SCurveMath::ComputeSegmentTimes(distance, cfg);

    // Should have no cruise phase
    EXPECT_FLOAT_EQ(data.t4, 0.0f);

    // Position should match distance
    float32 total = data.TotalTime();
    float32 pos = SCurveMath::PositionAt(total, data, cfg);
    EXPECT_NEAR(pos, distance, 0.05f);

    // Velocities should be bounded
    EXPECT_LE(SCurveMath::VelocityAt(0, data, cfg), 100.0f);
    EXPECT_LE(SCurveMath::VelocityAt(total, data, cfg), 100.0f);
    EXPECT_LE(data.v_peak, 100.0f);
}

TEST(SCurveProfileMathTest, ZeroDistanceReturnsEmpty) {
    SCurveConfig cfg;
    cfg.start_velocity = 0.0f;
    cfg.max_velocity = 100.0f;
    cfg.end_velocity = 0.0f;
    cfg.max_acceleration = 1000.0f;
    cfg.max_jerk = 10000.0f;

    auto data = SCurveMath::ComputeSegmentTimes(0.0f, cfg);
    EXPECT_FLOAT_EQ(data.TotalTime(), 0.0f);
}

TEST(SCurveProfileMathTest, ZeroMaxVelocityReturnsEmpty) {
    SCurveConfig cfg;
    cfg.max_velocity = 0.0f;

    auto data = SCurveMath::ComputeSegmentTimes(100.0f, cfg);
    EXPECT_FLOAT_EQ(data.TotalTime(), 0.0f);
}

TEST(SCurveProfileMathTest, VelocityContinuityAtBoundaries) {
    SCurveConfig cfg;
    cfg.start_velocity = 0.0f;
    cfg.max_velocity = 100.0f;
    cfg.end_velocity = 0.0f;
    cfg.max_acceleration = 1000.0f;
    cfg.max_jerk = 10000.0f;

    float32 distance = 500.0f;
    auto data = SCurveMath::ComputeSegmentTimes(distance, cfg);

    float32 e1 = data.t1;
    float32 e2 = e1 + data.t2;
    float32 e3 = e2 + data.t3;
    float32 e4 = e3 + data.t4;
    float32 e5 = e4 + data.t5;

    // Check velocity continuity at segment boundaries (epsilon before/after)
    const float32 dt = 1e-6f;

    // Boundary 1->2
    EXPECT_NEAR(SCurveMath::VelocityAt(e1 - dt, data, cfg),
                SCurveMath::VelocityAt(e1, data, cfg), 1e-3f);
    // Boundary 2->3
    EXPECT_NEAR(SCurveMath::VelocityAt(e2 - dt, data, cfg),
                SCurveMath::VelocityAt(e2, data, cfg), 1e-3f);
    // Boundary 3->4 (cruise start)
    EXPECT_NEAR(SCurveMath::VelocityAt(e3 - dt, data, cfg),
                SCurveMath::VelocityAt(e3, data, cfg), 1e-3f);
    // Boundary 4->5 (decel start)
    EXPECT_NEAR(SCurveMath::VelocityAt(e4 - dt, data, cfg),
                SCurveMath::VelocityAt(e4, data, cfg), 1e-3f);
    // Boundary 5->6
    EXPECT_NEAR(SCurveMath::VelocityAt(e5 - dt, data, cfg),
                SCurveMath::VelocityAt(e5, data, cfg), 1e-3f);
}

TEST(SCurveProfileMathTest, AccelerationContinuityAtBoundaries) {
    SCurveConfig cfg;
    cfg.start_velocity = 0.0f;
    cfg.max_velocity = 100.0f;
    cfg.end_velocity = 0.0f;
    cfg.max_acceleration = 1000.0f;
    cfg.max_jerk = 10000.0f;

    float32 distance = 500.0f;
    auto data = SCurveMath::ComputeSegmentTimes(distance, cfg);

    float32 e1 = data.t1;
    float32 e2 = e1 + data.t2;
    float32 e3 = e2 + data.t3;
    float32 e4 = e3 + data.t4;
    float32 e5 = e4 + data.t5;
    float32 e6 = e5 + data.t6;

    const float32 dt = 1e-6f;

    // Acceleration is not continuous at t1/t2/t3 boundaries by design
    // (jerk transitions), but should be continuous within each segment.
    // Check continuity at t = e1 (end of jerk-up, start of constant accel):
    // Here acceleration should be continuous: a_max at boundary
    float32 a_at_e1_minus = SCurveMath::AccelerationAt(e1 - dt, data, cfg);
    float32 a_at_e1 = SCurveMath::AccelerationAt(e1, data, cfg);
    EXPECT_NEAR(a_at_e1_minus, a_at_e1, 1.0f);

    // At e3 (end of accel phase, start of cruise): acceleration should be ~0
    float32 a_at_e3 = SCurveMath::AccelerationAt(e3, data, cfg);
    EXPECT_NEAR(a_at_e3, 0.0f, 1.0f);

    // At e4 (end of cruise, start of decel): acceleration should be ~0
    float32 a_at_e4 = SCurveMath::AccelerationAt(e4, data, cfg);
    EXPECT_NEAR(a_at_e4, 0.0f, 1.0f);

    // At e6 (end of constant decel): check continuity
    EXPECT_NEAR(SCurveMath::AccelerationAt(e6 - dt, data, cfg),
                SCurveMath::AccelerationAt(e6, data, cfg), 10.0f);
}

TEST(SCurveProfileMathTest, GenerateProfileCorrectlySamples) {
    SCurveConfig cfg;
    cfg.start_velocity = 0.0f;
    cfg.max_velocity = 100.0f;
    cfg.end_velocity = 0.0f;
    cfg.max_acceleration = 1000.0f;
    cfg.max_jerk = 10000.0f;

    float32 distance = 500.0f;
    float32 dt = 0.01f;

    auto samples = SCurveMath::GenerateProfile(distance, cfg, dt);
    ASSERT_FALSE(samples.empty());

    // Last sample should be at total time and end position ~distance
    auto data = SCurveMath::ComputeSegmentTimes(distance, cfg);
    float32 T = data.TotalTime();
    EXPECT_NEAR(samples.back().t, T, dt);

    // Last position should be close to distance
    EXPECT_NEAR(samples.back().position, distance, 0.5f);

    // First position should be 0
    EXPECT_FLOAT_EQ(samples.front().position, 0.0f);

    // Velocity should never exceed max
    for (const auto& s : samples) {
        EXPECT_LE(s.velocity, 100.0f + kEps);
        EXPECT_GE(s.velocity, -kEps);
    }
}

TEST(SCurveProfileMathTest, FullProfileWithCruisePhase) {
    SCurveConfig cfg;
    cfg.start_velocity = 0.0f;
    cfg.max_velocity = 50.0f;
    cfg.end_velocity = 0.0f;
    cfg.max_acceleration = 500.0f;
    cfg.max_jerk = 5000.0f;

    // Generous distance ensures significant cruise phase
    float32 distance = 200.0f;
    auto data = SCurveMath::ComputeSegmentTimes(distance, cfg);

    // Verify cruise phase is substantial
    EXPECT_GT(data.t4, 0.5f);
    EXPECT_EQ(data.v_peak, 50.0f);

    // Total time sanity: cruise alone takes distance/vmax = 4s,
    // plus accel/decel, so total should be > 4s
    float32 total = data.TotalTime();
    EXPECT_GT(total, 4.0f);
}

TEST(SCurveProfileMathTest, SevenSegmentProfileDelegation) {
    // Verify SevenSegmentSCurveProfile delegates correctly to SCurveMath
    using Siligen::Domain::Motion::DomainServices::SevenSegmentSCurveProfile;

    SevenSegmentSCurveProfile profile;
    Siligen::Domain::Motion::Ports::VelocityProfileConfig pcfg;
    pcfg.max_velocity = 100.0f;
    pcfg.max_acceleration = 1000.0f;
    pcfg.max_jerk = 10000.0f;
    pcfg.time_step = 0.005f;

    float32 distance = 300.0f;
    auto result = profile.GenerateProfile(distance, pcfg);
    ASSERT_TRUE(result.IsSuccess());

    auto& vp = result.Value();
    ASSERT_FALSE(vp.velocities.empty());

    // Velocity profile should start and end near 0
    EXPECT_NEAR(vp.velocities.front(), 0.0f, 1.0f);
    EXPECT_NEAR(vp.velocities.back(), 0.0f, 1.0f);

    // No velocity should exceed max
    for (float32 v : vp.velocities) {
        EXPECT_LE(v, 100.0f + kEps);
    }

    // Verify equivalence with direct SCurveMath call
    SCurveConfig scfg;
    scfg.max_velocity = 100.0f;
    scfg.max_acceleration = 1000.0f;
    scfg.max_jerk = 10000.0f;

    auto direct = SCurveMath::GenerateProfile(distance, scfg, 0.005f);
    ASSERT_EQ(vp.velocities.size(), direct.size());
    for (size_t i = 0; i < vp.velocities.size(); ++i) {
        EXPECT_NEAR(vp.velocities[i], direct[i].velocity, 1e-4f);
    }
}

TEST(SCurveProfileMathTest, EmptyProfileForZeroDistance) {
    using Siligen::Domain::Motion::DomainServices::SevenSegmentSCurveProfile;

    SevenSegmentSCurveProfile profile;
    Siligen::Domain::Motion::Ports::VelocityProfileConfig pcfg;
    pcfg.max_velocity = 100.0f;
    pcfg.max_acceleration = 1000.0f;

    auto result = profile.GenerateProfile(0.0f, pcfg);
    ASSERT_TRUE(result.IsSuccess());

    auto& vp = result.Value();
    EXPECT_TRUE(vp.velocities.empty());
}

}  // namespace
