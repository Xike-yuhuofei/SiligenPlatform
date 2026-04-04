#include "domain/motion/domain-services/MotionPlanner.h"
#include "domain/trajectory/value-objects/GeometryUtils.h"

#include <gtest/gtest.h>

#include <cmath>

namespace {

using Siligen::Domain::Motion::DomainServices::MotionPlanner;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;
using Siligen::Domain::Trajectory::ValueObjects::ArcPrimitive;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcLength;
using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::ProcessPath::Contracts::ProcessSegment;
using Siligen::ProcessPath::Contracts::ProcessTag;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::Shared::Types::Point2D;

ArcPrimitive BuildQuarterArc(float radius = 10.0f) {
    ArcPrimitive arc;
    arc.center = Point2D(0.0f, 0.0f);
    arc.radius = radius;
    arc.start_angle_deg = 0.0f;
    arc.end_angle_deg = 90.0f;
    arc.clockwise = false;
    return arc;
}

ProcessPath BuildArcProcessPath(const ArcPrimitive& arc) {
    ProcessPath path;

    ProcessSegment segment;
    segment.dispense_on = true;
    segment.flow_rate = 1.0f;
    segment.tag = ProcessTag::Normal;
    segment.geometry.type = SegmentType::Arc;
    segment.geometry.arc = arc;
    segment.geometry.length = ComputeArcLength(arc);
    path.segments.push_back(segment);

    return path;
}

TimePlanningConfig BuildMotionConfig() {
    TimePlanningConfig config;
    config.vmax = 20.0f;
    config.amax = 100.0f;
    config.jmax = 0.0f;
    config.sample_dt = 0.01f;
    config.sample_ds = 5.0f;
    config.arc_tolerance_mm = 0.01f;
    config.enforce_jerk_limit = true;
    return config;
}

float MaxRadialError(const MotionTrajectory& trajectory, const Point2D& center, float radius) {
    float max_error = 0.0f;
    for (const auto& point : trajectory.points) {
        const float dx = point.position.x - center.x;
        const float dy = point.position.y - center.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        max_error = std::max(max_error, std::abs(distance - radius));
    }
    return max_error;
}

}  // namespace

TEST(MainlineTrajectoryAuditTest, CanonicalMotionPlannerKeepsArcSamplesOnTrueCircle) {
    const auto arc = BuildQuarterArc();
    const auto path = BuildArcProcessPath(arc);

    MotionPlanner planner;
    const auto trajectory = planner.Plan(path, BuildMotionConfig());

    ASSERT_GT(trajectory.points.size(), 2U);
    EXPECT_LT(MaxRadialError(trajectory, arc.center, arc.radius), 1e-3f);
}
