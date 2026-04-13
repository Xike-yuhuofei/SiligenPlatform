#include "domain/motion/domain-services/MotionPlanner.h"
#include "motion_planning/contracts/MotionTrajectory.h"
#include "motion_planning/contracts/TimePlanningConfig.h"
#include "process_path/contracts/GeometryUtils.h"
#include "process_path/contracts/ProcessPath.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

namespace {

using Siligen::Domain::Motion::DomainServices::MotionPlanner;
using Siligen::MotionPlanning::Contracts::MotionTrajectory;
using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
using Siligen::ProcessPath::Contracts::ArcPrimitive;
using Siligen::ProcessPath::Contracts::ComputeArcLength;
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

TimePlanningConfig BuildMotionConfig(float sample_dt, float sample_ds, float arc_tolerance_mm) {
    TimePlanningConfig config;
    config.vmax = 20.0f;
    config.amax = 100.0f;
    config.jmax = 0.0f;
    config.sample_dt = sample_dt;
    config.sample_ds = sample_ds;
    config.arc_tolerance_mm = arc_tolerance_mm;
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
    const auto trajectory = planner.Plan(path, BuildMotionConfig(0.01f, 5.0f, 0.01f));

    ASSERT_GT(trajectory.points.size(), 2U);
    EXPECT_LT(MaxRadialError(trajectory, arc.center, arc.radius), 1e-3f);
}
