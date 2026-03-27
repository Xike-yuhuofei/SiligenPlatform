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

TEST(MotionPlannerConstraintTest, AppliesVelocityFactorConstraint) {
    using Siligen::Shared::Types::Point2D;
    using Siligen::Domain::Motion::DomainServices::MotionPlanner;
    using Siligen::Domain::Motion::ValueObjects::TimePlanningConfig;
    using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;
    using Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
    using Siligen::Domain::Trajectory::ValueObjects::ProcessTag;
    using Siligen::Domain::Trajectory::ValueObjects::SegmentType;

    ProcessPath path;
    ProcessSegment seg;
    seg.dispense_on = true;
    seg.flow_rate = 1.0f;
    seg.tag = ProcessTag::Normal;
    seg.constraint.has_velocity_factor = true;
    seg.constraint.max_velocity_factor = 0.5f;

    seg.geometry.type = SegmentType::Line;
    seg.geometry.line.start = Point2D(0.0f, 0.0f);
    seg.geometry.line.end = Point2D(1000.0f, 0.0f);
    seg.geometry.length = 1000.0f;
    path.segments.push_back(seg);

    TimePlanningConfig cfg;
    cfg.vmax = 100.0f;
    cfg.amax = 1000.0f;
    cfg.sample_dt = 0.01f;

    MotionPlanner planner;
    auto traj = planner.Plan(path, cfg);

    float max_speed = 0.0f;
    for (const auto& pt : traj.points) {
        max_speed = std::max(max_speed, SpeedMagnitude(pt.velocity));
    }

    const float expected_limit = cfg.vmax * seg.constraint.max_velocity_factor;
    EXPECT_LE(max_speed, expected_limit + kEpsilon);
    EXPECT_GT(max_speed, 1.0f);
}
