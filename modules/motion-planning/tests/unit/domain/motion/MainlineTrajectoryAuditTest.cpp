#include "domain/motion/domain-services/MotionPlanner.h"
#include "domain/dispensing/planning/domain-services/DispensingPlannerService.h"
#include "process_path/contracts/IPathSourcePort.h"
#include "process_path/contracts/GeometryUtils.h"
#include "process_path/contracts/Primitive.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlanner;
using Siligen::Domain::Motion::DomainServices::MotionPlanner;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;
using Siligen::Domain::Trajectory::Ports::IPathSourcePort;
using Siligen::Domain::Trajectory::Ports::PathPrimitiveMeta;
using Siligen::Domain::Trajectory::Ports::PathSourceResult;
using Siligen::ProcessPath::Contracts::ArcPrimitive;
using Siligen::ProcessPath::Contracts::ComputeArcLength;
using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::ProcessPath::Contracts::ProcessSegment;
using Siligen::ProcessPath::Contracts::ProcessTag;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;

constexpr float kEpsilon = 1e-4f;

class SinglePrimitivePathSourceStub final : public IPathSourcePort {
   public:
    explicit SinglePrimitivePathSourceStub(Primitive primitive)
        : primitive_(std::move(primitive)) {}

    Result<PathSourceResult> LoadFromFile(const std::string& filepath) override {
        loaded_paths.push_back(filepath);

        PathSourceResult result;
        result.success = true;
        result.primitives.push_back(primitive_);

        PathPrimitiveMeta meta;
        meta.entity_id = 1;
        meta.entity_segment_index = 0;
        meta.entity_closed = false;
        result.metadata.push_back(meta);
        return Result<PathSourceResult>::Success(result);
    }

    std::vector<std::string> loaded_paths;

   private:
    Primitive primitive_;
};

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

DispensingPlanRequest BuildPlanRequest(float sample_dt, float sample_ds, float arc_tolerance_mm) {
    DispensingPlanRequest request;
    request.dxf_filepath = "arc-audit.dxf";
    request.dispensing_velocity = 20.0f;
    request.acceleration = 100.0f;
    request.sample_dt = sample_dt;
    request.sample_ds = sample_ds;
    request.arc_tolerance_mm = arc_tolerance_mm;
    request.max_jerk = 0.0f;
    request.trigger_spatial_interval_mm = 1.0f;
    request.use_interpolation_planner = false;
    request.approximate_splines = false;
    request.spline_max_step_mm = 0.0f;
    request.spline_max_error_mm = 0.0f;
    request.optimize_path = false;
    return request;
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

float MaxPositionDelta(const MotionTrajectory& lhs, const MotionTrajectory& rhs) {
    if (lhs.points.size() != rhs.points.size()) {
        return std::numeric_limits<float>::infinity();
    }

    float max_delta = 0.0f;
    for (size_t index = 0; index < lhs.points.size(); ++index) {
        const float dx = lhs.points[index].position.x - rhs.points[index].position.x;
        const float dy = lhs.points[index].position.y - rhs.points[index].position.y;
        max_delta = std::max(max_delta, std::sqrt(dx * dx + dy * dy));
    }
    return max_delta;
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

TEST(MainlineTrajectoryAuditTest, DxFMainlineMotionTrajectoryKeepsArcSamplesOnTrueCircle) {
    const auto arc = BuildQuarterArc();
    auto path_source = std::make_shared<SinglePrimitivePathSourceStub>(
        Primitive::MakeArc(arc.center, arc.radius, arc.start_angle_deg, arc.end_angle_deg, arc.clockwise));
    DispensingPlanner planner(path_source);

    const auto result = planner.Plan(BuildPlanRequest(0.01f, 5.0f, 0.01f));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    ASSERT_GT(result.Value().motion_trajectory.points.size(), 2U);
    EXPECT_LT(MaxRadialError(result.Value().motion_trajectory, arc.center, arc.radius), 1e-3f);
}

TEST(MainlineTrajectoryAuditTest, DxFMainlineMotionTrajectoryIsCurrentlyInsensitiveToArcToleranceRequest) {
    const auto arc = BuildQuarterArc();
    auto path_source = std::make_shared<SinglePrimitivePathSourceStub>(
        Primitive::MakeArc(arc.center, arc.radius, arc.start_angle_deg, arc.end_angle_deg, arc.clockwise));
    DispensingPlanner planner(path_source);

    auto fine_result = planner.Plan(BuildPlanRequest(0.01f, 0.0f, 0.01f));
    auto coarse_result = planner.Plan(BuildPlanRequest(0.01f, 0.0f, 0.50f));

    ASSERT_TRUE(fine_result.IsSuccess()) << fine_result.GetError().GetMessage();
    ASSERT_TRUE(coarse_result.IsSuccess()) << coarse_result.GetError().GetMessage();
    ASSERT_EQ(fine_result.Value().motion_trajectory.points.size(),
              coarse_result.Value().motion_trajectory.points.size());

    EXPECT_NEAR(fine_result.Value().motion_trajectory.total_time,
                coarse_result.Value().motion_trajectory.total_time,
                kEpsilon);
    EXPECT_LT(MaxPositionDelta(fine_result.Value().motion_trajectory,
                               coarse_result.Value().motion_trajectory),
              1e-6f);
}
