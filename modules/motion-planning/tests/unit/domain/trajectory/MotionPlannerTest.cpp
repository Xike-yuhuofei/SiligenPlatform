#include "domain/motion/domain-services/MotionPlanner.h"

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
    using Siligen::Domain::Trajectory::ValueObjects::MotionConfig;
    using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;
    using Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
    using Siligen::Domain::Trajectory::ValueObjects::ProcessTag;
    using Siligen::Domain::Trajectory::ValueObjects::SegmentType;

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

    MotionConfig cfg;
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
    using Siligen::Domain::Trajectory::ValueObjects::MotionConfig;
    using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;
    using Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
    using Siligen::Domain::Trajectory::ValueObjects::ProcessTag;
    using Siligen::Domain::Trajectory::ValueObjects::SegmentType;

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

    MotionConfig cfg;
    cfg.sample_dt = 0.01f;

    MotionPlanner planner;
    auto traj = planner.Plan(path, cfg);

    ASSERT_FALSE(traj.points.empty());
    EXPECT_NEAR(traj.points.front().position.x, 5.0f, kEpsilon);
    EXPECT_NEAR(traj.points.front().position.y, 7.5f, kEpsilon);
}
