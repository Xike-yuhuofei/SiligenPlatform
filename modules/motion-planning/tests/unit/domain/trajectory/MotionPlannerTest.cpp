#include "domain/motion/domain-services/MotionPlanner.h"
#include "domain/motion/domain-services/SevenSegmentSCurveProfile.h"
#include "domain/motion/domain-services/VelocityProfileService.h"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

namespace {
constexpr float kEpsilon = 1e-3f;

float SpeedMagnitude(const Siligen::Point3D& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

}  // namespace

TEST(MotionPlannerTest, RespectsArcCurvatureLimit) {
    using Siligen::Shared::Types::Point2D;
    using Siligen::Domain::Motion::DomainServices::MotionPlanner;
    using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
    using Siligen::ProcessPath::Contracts::ProcessPath;
    using Siligen::ProcessPath::Contracts::ProcessSegment;
    using Siligen::ProcessPath::Contracts::ProcessTag;
    using Siligen::ProcessPath::Contracts::SegmentType;

    ProcessPath path;
    ProcessSegment seg;
    seg.dispense_on = true;
    seg.flow_rate = 1.0f;
    seg.tag = ProcessTag::Normal;

    seg.geometry.type = SegmentType::Arc;
    seg.geometry.arc.center = Point2D(0.0f, 0.0f);
    seg.geometry.arc.radius = 1.0f;
    seg.geometry.arc.start_angle_deg = 0.0f;
    seg.geometry.arc.end_angle_deg = 90.0f;
    seg.geometry.arc.clockwise = false;
    seg.geometry.length = 0.0f;

    path.segments.push_back(seg);

    TimePlanningConfig cfg;
    cfg.vmax = 100.0f;
    cfg.amax = 4.0f;
    cfg.sample_dt = 0.01f;

    MotionPlanner planner;
    auto traj = planner.Plan(path, cfg);

    float max_speed = 0.0f;
    for (const auto& pt : traj.points) {
        max_speed = std::max(max_speed, SpeedMagnitude(pt.velocity));
    }

    const float curvature_limit = std::sqrt(cfg.amax * seg.geometry.arc.radius);
    EXPECT_LE(max_speed, curvature_limit + kEpsilon);
}

TEST(MotionPlannerTest, EmitsTrajectoryForPointSegment) {
    using Siligen::Shared::Types::Point2D;
    using Siligen::Domain::Motion::DomainServices::MotionPlanner;
    using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
    using Siligen::ProcessPath::Contracts::ProcessPath;
    using Siligen::ProcessPath::Contracts::ProcessSegment;
    using Siligen::ProcessPath::Contracts::ProcessTag;
    using Siligen::ProcessPath::Contracts::SegmentType;

    ProcessPath path;
    ProcessSegment seg;
    seg.dispense_on = true;
    seg.flow_rate = 1.0f;
    seg.tag = ProcessTag::Normal;
    seg.geometry.type = SegmentType::Line;
    seg.geometry.line.start = Point2D(5.0f, 7.5f);
    seg.geometry.line.end = seg.geometry.line.start;
    seg.geometry.is_point = true;
    seg.geometry.length = 0.0f;

    path.segments.push_back(seg);

    TimePlanningConfig cfg;
    cfg.sample_dt = 0.01f;

    MotionPlanner planner;
    auto traj = planner.Plan(path, cfg);

    ASSERT_FALSE(traj.points.empty());
    EXPECT_NEAR(traj.points.front().position.x, 5.0f, kEpsilon);
    EXPECT_NEAR(traj.points.front().position.y, 7.5f, kEpsilon);
}

TEST(MotionPlannerTest, JerkLimitedScanConvergesOnMultiSegmentPath) {
    using Siligen::Shared::Types::Point2D;
    using Siligen::Domain::Motion::DomainServices::MotionPlanner;
    using Siligen::Domain::Motion::DomainServices::SevenSegmentSCurveProfile;
    using Siligen::Domain::Motion::DomainServices::VelocityProfileService;
    using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
    using Siligen::ProcessPath::Contracts::ProcessPath;
    using Siligen::ProcessPath::Contracts::ProcessSegment;
    using Siligen::ProcessPath::Contracts::ProcessTag;
    using Siligen::ProcessPath::Contracts::SegmentType;

    // Multi-segment path with varying lengths to stress the iterative scan
    ProcessPath path;
    auto add_seg = [&](float32 x1, float32 y1, float32 x2, float32 y2) {
        ProcessSegment seg;
        seg.dispense_on = true;
        seg.flow_rate = 1.0f;
        seg.tag = ProcessTag::Normal;
        seg.geometry.type = SegmentType::Line;
        seg.geometry.line.start = Point2D(x1, y1);
        seg.geometry.line.end = Point2D(x2, y2);
        seg.geometry.length = std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
        path.segments.push_back(seg);
    };

    // Alternating long/short segments to create convergence challenge
    add_seg(0.0f, 0.0f, 50.0f, 0.0f);    // long
    add_seg(50.0f, 0.0f, 55.0f, 0.0f);   // short
    add_seg(55.0f, 0.0f, 105.0f, 0.0f);  // long
    add_seg(105.0f, 0.0f, 108.0f, 0.0f); // short
    add_seg(108.0f, 0.0f, 158.0f, 0.0f); // long

    TimePlanningConfig cfg;
    cfg.vmax = 200.0f;
    cfg.amax = 1500.0f;
    cfg.jmax = 20000.0f;
    cfg.sample_dt = 0.005f;
    cfg.enforce_jerk_limit = true;

    auto velocity_port = std::make_shared<SevenSegmentSCurveProfile>();
    auto velocity_service = std::make_shared<VelocityProfileService>(velocity_port);
    MotionPlanner planner(velocity_service);
    auto traj = planner.Plan(path, cfg);

    ASSERT_FALSE(traj.points.empty());

    // Velocity should never exceed vmax
    for (const auto& pt : traj.points) {
        float32 v = std::sqrt(pt.velocity.x * pt.velocity.x +
                              pt.velocity.y * pt.velocity.y);
        EXPECT_LE(v, cfg.vmax * 1.01f);
    }

    // Trajectory should progress monotonically
    float32 prev_t = -1.0f;
    for (const auto& pt : traj.points) {
        EXPECT_GE(pt.t, prev_t);
        prev_t = pt.t;
    }
}
