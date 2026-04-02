#include "application/services/dispensing/AuthorityPreviewAssemblyService.h"
#include "application/services/dispensing/ExecutionAssemblyService.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

namespace {

using Siligen::Application::Services::Dispensing::AuthorityPreviewAssemblyService;
using Siligen::Application::Services::Dispensing::AuthorityPreviewBuildInput;
using Siligen::Application::Services::Dispensing::AuthorityPreviewBuildResult;
using Siligen::Application::Services::Dispensing::AuthorityTriggerPoint;
using Siligen::Application::Services::Dispensing::ExecutionAssemblyBuildInput;
using Siligen::Application::Services::Dispensing::ExecutionAssemblyService;
using Siligen::Application::Services::Dispensing::SpacingValidationGroup;
using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;
using Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout;
using Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile;
using Siligen::Domain::Motion::InterpolationAlgorithm;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint;
using Siligen::Domain::Dispensing::ValueObjects::StrongAnchorRole;
using Siligen::Domain::Dispensing::ValueObjects::TopologyDispatchType;
using Siligen::MotionPlanning::Contracts::MotionPlan;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
using Siligen::Domain::Trajectory::ValueObjects::Segment;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::Shared::Types::DispensingStrategy;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::uint32;

constexpr char kMixedExplicitBoundaryWithReorderedBranchFamily[] =
    "mixed_explicit_boundary_with_reordered_branch_family";

struct PlanningAssemblyTestInput {
    ProcessPath process_path;
    ProcessPath authority_process_path;
    MotionPlan motion_plan;
    std::string source_path;
    std::string dxf_filename;
    float32 dispensing_velocity = 0.0f;
    float32 acceleration = 0.0f;
    uint32 dispenser_interval_ms = 0;
    uint32 dispenser_duration_ms = 0;
    float32 trigger_spatial_interval_mm = 0.0f;
    float32 valve_response_ms = 0.0f;
    float32 safety_margin_ms = 0.0f;
    float32 min_interval_ms = 0.0f;
    float32 max_jerk = 0.0f;
    float32 sample_dt = 0.01f;
    float32 sample_ds = 0.0f;
    float32 spline_max_step_mm = 0.0f;
    float32 spline_max_error_mm = 0.0f;
    float32 estimated_time_s = 0.0f;
    DispensingStrategy dispensing_strategy = DispensingStrategy::BASELINE;
    int subsegment_count = 8;
    bool dispense_only_cruise = false;
    bool downgrade_on_violation = true;
    bool use_interpolation_planner = false;
    InterpolationAlgorithm interpolation_algorithm = InterpolationAlgorithm::LINEAR;
    DispenseCompensationProfile compensation_profile{};
    float32 spacing_tol_ratio = 0.0f;
    float32 spacing_min_mm = 0.0f;
    float32 spacing_max_mm = 0.0f;
};

struct PlanningAssemblyTestResult {
    ExecutionPackageValidated execution_package;
    int segment_count = 0;
    float32 total_length = 0.0f;
    float32 estimated_time = 0.0f;
    std::vector<Siligen::TrajectoryPoint> trajectory_points;
    std::vector<Point2D> glue_points;
    int trigger_count = 0;
    std::string dxf_filename;
    int32 timestamp = 0;
    bool preview_authority_ready = false;
    bool preview_authority_shared_with_execution = false;
    bool preview_binding_ready = false;
    bool preview_spacing_valid = false;
    bool preview_has_short_segment_exceptions = false;
    std::string preview_validation_classification;
    std::string preview_exception_reason;
    std::string preview_failure_reason;
    AuthorityTriggerLayout authority_trigger_layout;
    std::vector<AuthorityTriggerPoint> authority_trigger_points;
    std::vector<SpacingValidationGroup> spacing_validation_groups;
};

class PlanningAssemblyFacade {
public:
    Result<PlanningAssemblyTestResult> BuildPlanningArtifacts(const PlanningAssemblyTestInput& input) const {
        AuthorityPreviewAssemblyService authority_service;
        ExecutionAssemblyService execution_service;

        AuthorityPreviewBuildInput authority_input;
        authority_input.process_path = input.process_path;
        authority_input.authority_process_path = input.authority_process_path;
        authority_input.source_path = input.source_path;
        authority_input.dxf_filename = input.dxf_filename;
        authority_input.dispensing_velocity = input.dispensing_velocity;
        authority_input.acceleration = input.acceleration;
        authority_input.dispenser_interval_ms = input.dispenser_interval_ms;
        authority_input.dispenser_duration_ms = input.dispenser_duration_ms;
        authority_input.trigger_spatial_interval_mm = input.trigger_spatial_interval_mm;
        authority_input.valve_response_ms = input.valve_response_ms;
        authority_input.safety_margin_ms = input.safety_margin_ms;
        authority_input.min_interval_ms = input.min_interval_ms;
        authority_input.sample_dt = input.sample_dt;
        authority_input.sample_ds = input.sample_ds;
        authority_input.spline_max_step_mm = input.spline_max_step_mm;
        authority_input.spline_max_error_mm = input.spline_max_error_mm;
        authority_input.dispensing_strategy = input.dispensing_strategy;
        authority_input.subsegment_count = input.subsegment_count;
        authority_input.dispense_only_cruise = input.dispense_only_cruise;
        authority_input.downgrade_on_violation = input.downgrade_on_violation;
        authority_input.compensation_profile = input.compensation_profile;
        authority_input.spacing_tol_ratio = input.spacing_tol_ratio;
        authority_input.spacing_min_mm = input.spacing_min_mm;
        authority_input.spacing_max_mm = input.spacing_max_mm;

        auto authority_result = authority_service.BuildAuthorityPreviewArtifacts(authority_input);
        if (authority_result.IsError()) {
            return Result<PlanningAssemblyTestResult>::Failure(authority_result.GetError());
        }

        ExecutionAssemblyBuildInput execution_input;
        execution_input.process_path = input.process_path;
        execution_input.motion_plan = input.motion_plan;
        execution_input.source_path = input.source_path;
        execution_input.dxf_filename = input.dxf_filename;
        execution_input.dispensing_velocity = input.dispensing_velocity;
        execution_input.acceleration = input.acceleration;
        execution_input.dispenser_interval_ms = input.dispenser_interval_ms;
        execution_input.dispenser_duration_ms = input.dispenser_duration_ms;
        execution_input.trigger_spatial_interval_mm = input.trigger_spatial_interval_mm;
        execution_input.valve_response_ms = input.valve_response_ms;
        execution_input.safety_margin_ms = input.safety_margin_ms;
        execution_input.min_interval_ms = input.min_interval_ms;
        execution_input.max_jerk = input.max_jerk;
        execution_input.sample_dt = input.sample_dt;
        execution_input.sample_ds = input.sample_ds;
        execution_input.spline_max_step_mm = input.spline_max_step_mm;
        execution_input.spline_max_error_mm = input.spline_max_error_mm;
        execution_input.estimated_time_s = input.estimated_time_s;
        execution_input.use_interpolation_planner = input.use_interpolation_planner;
        execution_input.interpolation_algorithm = input.interpolation_algorithm;
        execution_input.compensation_profile = input.compensation_profile;
        execution_input.authority_preview = authority_result.Value();

        auto execution_result = execution_service.BuildExecutionArtifactsFromAuthority(execution_input);
        if (execution_result.IsError()) {
            return Result<PlanningAssemblyTestResult>::Failure(execution_result.GetError());
        }

        const auto& authority = authority_result.Value();
        const auto& execution = execution_result.Value();

        PlanningAssemblyTestResult result;
        result.execution_package = execution.execution_package;
        result.segment_count = authority.segment_count;
        result.total_length = authority.total_length;
        result.estimated_time = execution.execution_package.estimated_time_s;
        result.trajectory_points = execution.execution_trajectory_points;
        result.glue_points = authority.glue_points;
        result.trigger_count = authority.trigger_count;
        result.dxf_filename = authority.dxf_filename;
        result.timestamp = authority.timestamp;
        result.preview_authority_ready = authority.preview_authority_ready;
        result.preview_authority_shared_with_execution =
            execution.preview_authority_shared_with_execution;
        result.preview_binding_ready = execution.execution_binding_ready;
        result.preview_spacing_valid = authority.preview_spacing_valid;
        result.preview_has_short_segment_exceptions = authority.preview_has_short_segment_exceptions;
        result.preview_validation_classification = authority.preview_validation_classification;
        result.preview_exception_reason = authority.preview_exception_reason;
        result.preview_failure_reason = execution.execution_failure_reason.empty()
            ? authority.preview_failure_reason
            : execution.execution_failure_reason;
        result.authority_trigger_layout = execution.authority_trigger_layout;
        result.authority_trigger_points = authority.authority_trigger_points;
        result.spacing_validation_groups = authority.spacing_validation_groups;
        return Result<PlanningAssemblyTestResult>::Success(std::move(result));
    }
};

MotionTrajectoryPoint BuildMotionPoint(float t, float x, float y, bool dispense_on = true) {
    MotionTrajectoryPoint point;
    point.t = t;
    point.position = Siligen::Point3D(x, y, 0.0f);
    point.velocity = Siligen::Point3D(10.0f, 0.0f, 0.0f);
    point.dispense_on = dispense_on;
    return point;
}

ProcessSegment BuildLineSegment(const Point2D& start, const Point2D& end, bool dispense_on = true) {
    Segment segment;
    segment.type = SegmentType::Line;
    segment.line.start = start;
    segment.line.end = end;
    segment.length = start.DistanceTo(end);

    ProcessSegment process_segment;
    process_segment.geometry = segment;
    process_segment.dispense_on = dispense_on;
    return process_segment;
}

ProcessSegment BuildSplineSegment(const std::vector<Point2D>& control_points, bool dispense_on = true) {
    Segment segment;
    segment.type = SegmentType::Spline;
    segment.spline.control_points = control_points;
    segment.length = 0.0f;

    ProcessSegment process_segment;
    process_segment.geometry = segment;
    process_segment.dispense_on = dispense_on;
    return process_segment;
}

ProcessSegment BuildPointSegment(const Point2D& point, bool dispense_on = true) {
    Segment segment;
    segment.type = SegmentType::Line;
    segment.line.start = point;
    segment.line.end = point;
    segment.length = 0.0f;
    segment.is_point = true;

    ProcessSegment process_segment;
    process_segment.geometry = segment;
    process_segment.dispense_on = dispense_on;
    return process_segment;
}

PlanningAssemblyTestInput BuildInput() {
    PlanningAssemblyTestInput input;
    input.source_path = "sample.pb";
    input.dxf_filename = "sample.pb";
    input.dispensing_velocity = 10.0f;
    input.acceleration = 200.0f;
    input.dispenser_interval_ms = 100;
    input.dispenser_duration_ms = 100;
    input.trigger_spatial_interval_mm = 5.0f;
    input.sample_dt = 0.01f;
    input.max_jerk = 5000.0f;
    input.use_interpolation_planner = true;
    input.interpolation_algorithm = InterpolationAlgorithm::LINEAR;
    input.dispensing_strategy = DispensingStrategy::BASELINE;
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f),
        BuildMotionPoint(1.0f, 10.0f, 0.0f),
    };
    input.motion_plan.total_length = 10.0f;
    input.motion_plan.total_time = 1.0f;
    input.estimated_time_s = 1.25f;
    return input;
}
PlanningAssemblyTestInput BuildPolylineInput(
    const std::vector<Point2D>& polyline,
    float spacing_mm = 3.0f) {
    auto input = BuildInput();
    input.process_path.segments.clear();
    input.motion_plan.points.clear();
    input.trigger_spatial_interval_mm = spacing_mm;

    float total_length = 0.0f;
    float timestamp = 0.0f;
    for (std::size_t index = 1; index < polyline.size(); ++index) {
        input.process_path.segments.push_back(BuildLineSegment(polyline[index - 1], polyline[index]));
        if (index == 1) {
            input.motion_plan.points.push_back(BuildMotionPoint(timestamp, polyline[index - 1].x, polyline[index - 1].y));
        }
        total_length += polyline[index - 1].DistanceTo(polyline[index]);
        timestamp += 1.0f;
        input.motion_plan.points.push_back(BuildMotionPoint(timestamp, polyline[index].x, polyline[index].y));
    }
    input.motion_plan.total_length = total_length;
    input.motion_plan.total_time = std::max(1.0f, total_length / 10.0f);
    input.estimated_time_s = input.motion_plan.total_time;
    return input;
}

std::size_t CountPointsNear(
    const std::vector<Point2D>& points,
    const Point2D& target,
    float tolerance_mm) {
    std::size_t count = 0;
    for (const auto& point : points) {
        if (point.DistanceTo(target) <= tolerance_mm) {
            ++count;
        }
    }
    return count;
}

bool HasConsecutiveNearDuplicatePoints(
    const std::vector<Point2D>& points,
    float tolerance_mm) {
    for (std::size_t index = 1; index < points.size(); ++index) {
        if (points[index - 1].DistanceTo(points[index]) <= tolerance_mm) {
            return true;
        }
    }
    return false;
}

std::size_t CountTriggerMarkers(const std::vector<Siligen::TrajectoryPoint>& points) {
    std::size_t count = 0;
    for (const auto& point : points) {
        if (point.enable_position_trigger) {
            ++count;
        }
    }
    return count;
}

std::size_t CountAnchorRoles(
    const Siligen::Domain::Dispensing::ValueObjects::DispenseSpan& span,
    StrongAnchorRole role) {
    return static_cast<std::size_t>(std::count_if(
        span.strong_anchors.begin(),
        span.strong_anchors.end(),
        [&](const auto& anchor) { return anchor.role == role; }));
}

}  // namespace

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsProducesValidatedExecutionPackage) {
    PlanningAssemblyFacade facade;

    const auto result = facade.BuildPlanningArtifacts(BuildInput());

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_EQ(payload.execution_package.source_path, "sample.pb");
    EXPECT_FLOAT_EQ(payload.execution_package.total_length_mm, 10.0f);
    EXPECT_FLOAT_EQ(payload.execution_package.estimated_time_s, 1.25f);
    EXPECT_TRUE(payload.execution_package.Validate().IsSuccess());
    EXPECT_FALSE(payload.execution_package.execution_plan.interpolation_segments.empty());
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsBuildsPreviewPayloadAndExportRequest) {
    PlanningAssemblyFacade facade;

    const auto result = facade.BuildPlanningArtifacts(BuildInput());

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_EQ(payload.segment_count, 1);
    EXPECT_FLOAT_EQ(payload.total_length, 10.0f);
    EXPECT_FLOAT_EQ(payload.estimated_time, 1.25f);
    EXPECT_FALSE(payload.trajectory_points.empty());
    EXPECT_FALSE(payload.glue_points.empty());
    EXPECT_EQ(payload.trigger_count, static_cast<int>(payload.glue_points.size()));
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsAnchorsGluePointsAtSegmentEndpointsAndUniformSpacing) {
    auto input = BuildInput();
    input.trigger_spatial_interval_mm = 3.0f;

    PlanningAssemblyFacade facade;
    const auto result = facade.BuildPlanningArtifacts(input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_GT(payload.trajectory_points.size(), payload.glue_points.size());
    ASSERT_EQ(payload.glue_points.size(), 4U);
    EXPECT_EQ(payload.trigger_count, 4);
    EXPECT_TRUE(payload.preview_authority_ready);
    EXPECT_TRUE(payload.preview_authority_shared_with_execution);
    EXPECT_TRUE(payload.preview_spacing_valid);
    EXPECT_NEAR(payload.glue_points[0].x, 0.0f, 1e-4f);
    EXPECT_NEAR(payload.glue_points[1].x, 10.0f / 3.0f, 1e-4f);
    EXPECT_NEAR(payload.glue_points[2].x, 20.0f / 3.0f, 1e-4f);
    EXPECT_NEAR(payload.glue_points[3].x, 10.0f, 1e-4f);
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsLinearInterpolationOnlyMarksDiscreteTriggerPoints) {
    auto input = BuildInput();
    input.use_interpolation_planner = true;
    input.interpolation_algorithm = InterpolationAlgorithm::LINEAR;
    input.trigger_spatial_interval_mm = 3.0f;
    input.max_jerk = 500.0f;
    input.sample_ds = 10.0f;

    PlanningAssemblyFacade facade;
    const auto result = facade.BuildPlanningArtifacts(input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_EQ(payload.glue_points.size(), 4U);
    EXPECT_EQ(CountTriggerMarkers(payload.trajectory_points), payload.glue_points.size());
    EXPECT_LT(payload.glue_points.size(), payload.trajectory_points.size());
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsDeduplicatesSharedVerticesAcrossAnchoredSegments) {
    auto input = BuildInput();
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)));
    input.trigger_spatial_interval_mm = 5.0f;
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f),
        BuildMotionPoint(1.0f, 10.0f, 0.0f),
        BuildMotionPoint(2.0f, 10.0f, 10.0f),
    };
    input.motion_plan.total_length = 20.0f;
    input.motion_plan.total_time = 2.0f;

    PlanningAssemblyFacade facade;
    const auto result = facade.BuildPlanningArtifacts(input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_EQ(payload.glue_points.size(), 5U);
    EXPECT_EQ(CountPointsNear(payload.glue_points, Point2D(10.0f, 0.0f), 1e-4f), 1U);
    EXPECT_FALSE(HasConsecutiveNearDuplicatePoints(payload.glue_points, 1e-4f));
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsMarksShortSegmentsAsSpacingExceptions) {
    auto input = BuildInput();
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(2.0f, 0.0f)));
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f),
        BuildMotionPoint(1.0f, 2.0f, 0.0f),
    };
    input.motion_plan.total_length = 2.0f;
    input.motion_plan.total_time = 1.0f;
    input.trigger_spatial_interval_mm = 3.0f;

    PlanningAssemblyFacade facade;
    const auto result = facade.BuildPlanningArtifacts(input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_EQ(payload.glue_points.size(), 2U);
    EXPECT_TRUE(payload.preview_authority_ready);
    EXPECT_TRUE(payload.preview_spacing_valid);
    EXPECT_TRUE(payload.preview_has_short_segment_exceptions);
    ASSERT_EQ(payload.spacing_validation_groups.size(), 1U);
    EXPECT_TRUE(payload.spacing_validation_groups.front().short_segment_exception);
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsUsesAuthorityLayoutAsGluePointTruth) {
    PlanningAssemblyFacade facade;

    const auto result = facade.BuildPlanningArtifacts(BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
    }));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_EQ(payload.glue_points.size(), payload.authority_trigger_layout.trigger_points.size());
    EXPECT_TRUE(payload.preview_binding_ready);
    for (std::size_t index = 0; index < payload.glue_points.size(); ++index) {
        EXPECT_NEAR(payload.glue_points[index].x, payload.authority_trigger_layout.trigger_points[index].position.x, 1e-4f);
        EXPECT_NEAR(payload.glue_points[index].y, payload.authority_trigger_layout.trigger_points[index].position.y, 1e-4f);
    }
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsKeepsEquivalentSubdivisionGluePointsStable) {
    PlanningAssemblyFacade facade;

    const auto single = facade.BuildPlanningArtifacts(BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
    }));
    const auto subdivided = facade.BuildPlanningArtifacts(BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(5.0f, 0.0f),
        Point2D(10.0f, 0.0f),
    }));

    ASSERT_TRUE(single.IsSuccess()) << single.GetError().GetMessage();
    ASSERT_TRUE(subdivided.IsSuccess()) << subdivided.GetError().GetMessage();
    ASSERT_EQ(single.Value().glue_points.size(), subdivided.Value().glue_points.size());
    for (std::size_t index = 0; index < single.Value().glue_points.size(); ++index) {
        EXPECT_NEAR(single.Value().glue_points[index].x, subdivided.Value().glue_points[index].x, 1e-4f);
        EXPECT_NEAR(single.Value().glue_points[index].y, subdivided.Value().glue_points[index].y, 1e-4f);
    }
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsUsesStableClosedLoopPhaseWithoutDuplicateTail) {
    PlanningAssemblyFacade facade;

    auto square = BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        Point2D(10.0f, 10.0f),
        Point2D(0.0f, 10.0f),
        Point2D(0.0f, 0.0f),
    }, 5.0f);
    square.spline_max_step_mm = 1.0f;

    auto rotated_square = BuildPolylineInput({
        Point2D(10.0f, 0.0f),
        Point2D(10.0f, 10.0f),
        Point2D(0.0f, 10.0f),
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
    }, 5.0f);
    rotated_square.spline_max_step_mm = 1.0f;

    const auto square_result = facade.BuildPlanningArtifacts(square);
    const auto rotated_result = facade.BuildPlanningArtifacts(rotated_square);

    ASSERT_TRUE(square_result.IsSuccess()) << square_result.GetError().GetMessage();
    ASSERT_TRUE(rotated_result.IsSuccess()) << rotated_result.GetError().GetMessage();

    const auto& square_payload = square_result.Value();
    const auto& rotated_payload = rotated_result.Value();
    ASSERT_EQ(square_payload.authority_trigger_layout.spans.size(), 1U);
    ASSERT_EQ(rotated_payload.authority_trigger_layout.spans.size(), 1U);
    const auto& square_span = square_payload.authority_trigger_layout.spans.front();
    const auto& rotated_span = rotated_payload.authority_trigger_layout.spans.front();
    EXPECT_TRUE(square_span.closed);
    EXPECT_EQ(
        square_payload.glue_points.size(),
        square_span.interval_count);
    EXPECT_EQ(square_span.phase_strategy,
              Siligen::Domain::Dispensing::ValueObjects::DispenseSpanPhaseStrategy::AnchorConstrained);
    EXPECT_GE(square_span.phase_mm, 0.0f);
    EXPECT_LT(square_span.phase_mm, square_span.actual_spacing_mm);
    EXPECT_EQ(CountAnchorRoles(square_span, StrongAnchorRole::ClosedLoopCorner), 4U);
    EXPECT_GT(
        square_payload.glue_points.front().DistanceTo(square_payload.glue_points.back()),
        1e-3f);
    EXPECT_EQ(square_span.phase_mm, rotated_span.phase_mm);
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsBindsAuthorityAcrossClosedLoopPhaseShiftedExecution) {
    PlanningAssemblyFacade facade;

    auto authority_square = BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        Point2D(10.0f, 10.0f),
        Point2D(0.0f, 10.0f),
        Point2D(0.0f, 0.0f),
    }, 5.0f);
    authority_square.spline_max_step_mm = 1.0f;

    auto rotated_execution_square = BuildPolylineInput({
        Point2D(10.0f, 0.0f),
        Point2D(10.0f, 10.0f),
        Point2D(0.0f, 10.0f),
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
    }, 5.0f);
    rotated_execution_square.spline_max_step_mm = 1.0f;
    rotated_execution_square.authority_process_path = authority_square.process_path;

    const auto result = facade.BuildPlanningArtifacts(rotated_execution_square);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_TRUE(payload.preview_authority_ready);
    EXPECT_TRUE(payload.preview_binding_ready);
    ASSERT_EQ(
        payload.authority_trigger_layout.trigger_points.size(),
        payload.authority_trigger_layout.bindings.size());
    EXPECT_GT(
        std::count_if(
            payload.authority_trigger_layout.bindings.begin(),
            payload.authority_trigger_layout.bindings.end(),
            [](const auto& binding) { return !binding.monotonic; }),
        0U);
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsSplitsRectDiagBranchRevisitWithoutBreakingPreviewTruth) {
    PlanningAssemblyFacade facade;

    auto input = BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        Point2D(10.0f, 10.0f),
        Point2D(0.0f, 10.0f),
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 10.0f),
    }, 5.0f);
    input.authority_process_path = input.process_path;

    const auto result = facade.BuildPlanningArtifacts(input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_TRUE(payload.authority_trigger_layout.branch_revisit_split_applied);
    ASSERT_EQ(payload.authority_trigger_layout.spans.size(), 2U);
    EXPECT_TRUE(payload.preview_authority_ready);
    EXPECT_TRUE(payload.preview_binding_ready);
    EXPECT_TRUE(payload.preview_authority_shared_with_execution);
    EXPECT_EQ(payload.glue_points.size(), payload.authority_trigger_layout.trigger_points.size());
    const auto& closed_span = payload.authority_trigger_layout.spans.front();
    EXPECT_TRUE(closed_span.closed);
    EXPECT_FALSE(payload.authority_trigger_layout.spans.back().closed);
    EXPECT_EQ(CountAnchorRoles(closed_span, StrongAnchorRole::ClosedLoopCorner), 4U);
    EXPECT_EQ(closed_span.accepted_corner_count, 4U);
    EXPECT_TRUE(closed_span.anchor_constraints_satisfied);
    EXPECT_TRUE(std::any_of(
        payload.authority_trigger_layout.spans.begin(),
        payload.authority_trigger_layout.spans.end(),
        [](const auto& span) {
            return span.split_reason ==
                Siligen::Domain::Dispensing::ValueObjects::DispenseSpanSplitReason::BranchOrRevisit;
        }));
    std::vector<Point2D> authority_points;
    authority_points.reserve(payload.authority_trigger_layout.trigger_points.size());
    for (const auto& trigger : payload.authority_trigger_layout.trigger_points) {
        authority_points.push_back(trigger.position);
    }
    EXPECT_EQ(CountPointsNear(payload.glue_points, Point2D(0.0f, 0.0f), 1e-4f),
              CountPointsNear(authority_points, Point2D(0.0f, 0.0f), 1e-4f));
    EXPECT_EQ(CountPointsNear(payload.glue_points, Point2D(10.0f, 10.0f), 1e-4f),
              CountPointsNear(authority_points, Point2D(10.0f, 10.0f), 1e-4f));
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsIgnoresAuxiliaryGeometryWithoutBreakingGluePointTruth) {
    auto input = BuildInput();
    input.trigger_spatial_interval_mm = 5.0f;
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(25.0f, 0.0f), Point2D(27.0f, 0.0f)));
    input.authority_process_path = input.process_path;
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f),
        BuildMotionPoint(1.0f, 10.0f, 0.0f),
        BuildMotionPoint(2.0f, 10.0f, 10.0f),
        BuildMotionPoint(3.0f, 0.0f, 10.0f),
        BuildMotionPoint(4.0f, 0.0f, 0.0f),
    };
    input.motion_plan.total_length = 40.0f;
    input.motion_plan.total_time = 4.0f;
    input.estimated_time_s = 4.0f;

    PlanningAssemblyFacade facade;
    const auto result = facade.BuildPlanningArtifacts(input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    const auto& layout = payload.authority_trigger_layout;
    EXPECT_EQ(layout.dispatch_type, TopologyDispatchType::AuxiliaryGeometry);
    EXPECT_EQ(layout.effective_component_count, 1U);
    EXPECT_EQ(layout.ignored_component_count, 1U);
    ASSERT_EQ(layout.components.size(), 2U);
    ASSERT_EQ(layout.spans.size(), 1U);
    EXPECT_TRUE(layout.components[1].ignored);
    EXPECT_FALSE(layout.components[1].ignored_reason.empty());
    EXPECT_TRUE(payload.preview_authority_ready);
    EXPECT_TRUE(payload.preview_binding_ready);
    EXPECT_TRUE(payload.preview_spacing_valid);
    EXPECT_EQ(payload.glue_points.size(), layout.trigger_points.size());
    EXPECT_EQ(
        std::count_if(
            payload.glue_points.begin(),
            payload.glue_points.end(),
            [](const auto& point) {
                return point.DistanceTo(Point2D(25.0f, 0.0f)) <= 1e-4f ||
                    point.DistanceTo(Point2D(27.0f, 0.0f)) <= 1e-4f;
            }),
        0);
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsAllowsExplicitProcessBoundarySharedVertexComponent) {
    auto input = BuildInput();
    input.trigger_spatial_interval_mm = 5.0f;
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(0.0f, 0.0f), false));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(0.0f, 10.0f)));
    input.authority_process_path = input.process_path;
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f, true),
        BuildMotionPoint(1.0f, 10.0f, 0.0f, true),
        BuildMotionPoint(2.0f, 0.0f, 0.0f, false),
        BuildMotionPoint(3.0f, 0.0f, 10.0f, true),
    };
    input.motion_plan.total_length = 30.0f;
    input.motion_plan.total_time = 3.0f;
    input.estimated_time_s = 3.0f;

    PlanningAssemblyFacade facade;
    const auto result = facade.BuildPlanningArtifacts(input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    const auto& layout = payload.authority_trigger_layout;
    EXPECT_EQ(layout.dispatch_type, TopologyDispatchType::ExplicitProcessBoundary);
    EXPECT_EQ(layout.effective_component_count, 1U);
    EXPECT_EQ(layout.ignored_component_count, 0U);
    ASSERT_EQ(layout.components.size(), 1U);
    ASSERT_EQ(layout.spans.size(), 2U);
    EXPECT_EQ(layout.components.front().dispatch_type, TopologyDispatchType::ExplicitProcessBoundary);
    EXPECT_TRUE(layout.components.front().blocking_reason.empty());
    EXPECT_EQ(layout.spans[0].split_reason,
              Siligen::Domain::Dispensing::ValueObjects::DispenseSpanSplitReason::ExplicitProcessBoundary);
    EXPECT_EQ(layout.spans[1].split_reason,
              Siligen::Domain::Dispensing::ValueObjects::DispenseSpanSplitReason::ExplicitProcessBoundary);
    EXPECT_TRUE(payload.preview_authority_ready);
    EXPECT_TRUE(payload.preview_binding_ready);
    EXPECT_TRUE(payload.preview_authority_shared_with_execution);
    EXPECT_TRUE(payload.preview_spacing_valid);
    EXPECT_EQ(payload.glue_points.size(), layout.trigger_points.size());
    EXPECT_EQ(CountPointsNear(payload.glue_points, Point2D(0.0f, 0.0f), 1e-4f), 2U);
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsSupportsSharedVertexReorderedBranchComponent) {
    auto input = BuildInput();
    input.trigger_spatial_interval_mm = 5.0f;
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(0.0f, 10.0f)));
    input.authority_process_path = input.process_path;
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f, true),
        BuildMotionPoint(1.0f, 10.0f, 0.0f, true),
        BuildMotionPoint(2.0f, 0.0f, 0.0f, true),
        BuildMotionPoint(3.0f, 0.0f, 10.0f, true),
    };
    input.motion_plan.total_length = 20.0f;
    input.motion_plan.total_time = 3.0f;
    input.estimated_time_s = 3.0f;

    PlanningAssemblyFacade facade;
    const auto result = facade.BuildPlanningArtifacts(input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    const auto& layout = payload.authority_trigger_layout;
    EXPECT_EQ(layout.dispatch_type, TopologyDispatchType::BranchOrRevisit);
    EXPECT_FALSE(layout.branch_revisit_split_applied);
    EXPECT_EQ(layout.effective_component_count, 1U);
    EXPECT_EQ(layout.ignored_component_count, 0U);
    ASSERT_EQ(layout.components.size(), 1U);
    ASSERT_EQ(layout.spans.size(), 2U);
    EXPECT_EQ(layout.components.front().dispatch_type, TopologyDispatchType::BranchOrRevisit);
    EXPECT_TRUE(layout.components.front().blocking_reason.empty());
    EXPECT_EQ(layout.spans[0].split_reason,
              Siligen::Domain::Dispensing::ValueObjects::DispenseSpanSplitReason::MultiContourBoundary);
    EXPECT_EQ(layout.spans[1].split_reason,
              Siligen::Domain::Dispensing::ValueObjects::DispenseSpanSplitReason::MultiContourBoundary);
    EXPECT_TRUE(payload.preview_authority_ready);
    EXPECT_TRUE(payload.preview_binding_ready);
    EXPECT_TRUE(payload.preview_authority_shared_with_execution);
    EXPECT_TRUE(payload.preview_spacing_valid);
    EXPECT_EQ(payload.glue_points.size(), layout.trigger_points.size());
    EXPECT_EQ(CountPointsNear(payload.glue_points, Point2D(0.0f, 0.0f), 1e-4f), 2U);
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsSupportsMixedOpenChainAndExplicitBoundaryFamily) {
    auto input = BuildInput();
    input.trigger_spatial_interval_mm = 5.0f;
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(20.0f, 0.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(20.0f, 0.0f), Point2D(0.0f, 0.0f), false));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(0.0f, 10.0f)));
    input.authority_process_path = input.process_path;
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f, true),
        BuildMotionPoint(1.0f, 10.0f, 0.0f, true),
        BuildMotionPoint(2.0f, 20.0f, 0.0f, true),
        BuildMotionPoint(3.0f, 0.0f, 0.0f, false),
        BuildMotionPoint(4.0f, 0.0f, 10.0f, true),
    };
    input.motion_plan.total_length = 50.0f;
    input.motion_plan.total_time = 4.0f;
    input.estimated_time_s = 4.0f;

    PlanningAssemblyFacade facade;
    const auto result = facade.BuildPlanningArtifacts(input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    const auto& layout = payload.authority_trigger_layout;
    EXPECT_TRUE(layout.authority_ready);
    EXPECT_EQ(layout.dispatch_type, TopologyDispatchType::ExplicitProcessBoundary);
    EXPECT_EQ(layout.effective_component_count, 1U);
    EXPECT_EQ(layout.ignored_component_count, 0U);
    ASSERT_EQ(layout.components.size(), 1U);
    ASSERT_EQ(layout.spans.size(), 2U);
    ASSERT_EQ(layout.validation_outcomes.size(), 2U);
    ASSERT_EQ(layout.trigger_points.size(), 8U);
    EXPECT_EQ(layout.components.front().dispatch_type, TopologyDispatchType::ExplicitProcessBoundary);
    EXPECT_TRUE(layout.components.front().blocking_reason.empty());
    EXPECT_EQ(layout.spans[0].dispatch_type, TopologyDispatchType::ExplicitProcessBoundary);
    EXPECT_EQ(layout.spans[1].dispatch_type, TopologyDispatchType::ExplicitProcessBoundary);
    EXPECT_EQ(layout.spans[0].split_reason,
              Siligen::Domain::Dispensing::ValueObjects::DispenseSpanSplitReason::None);
    EXPECT_EQ(layout.spans[1].split_reason,
              Siligen::Domain::Dispensing::ValueObjects::DispenseSpanSplitReason::ExplicitProcessBoundary);
    EXPECT_TRUE(payload.preview_authority_ready);
    EXPECT_TRUE(payload.preview_binding_ready);
    EXPECT_TRUE(payload.preview_authority_shared_with_execution);
    EXPECT_TRUE(payload.preview_spacing_valid);
    EXPECT_TRUE(payload.preview_failure_reason.empty());
    EXPECT_EQ(payload.glue_points.size(), layout.trigger_points.size());
    EXPECT_EQ(CountPointsNear(payload.glue_points, Point2D(0.0f, 0.0f), 1e-4f), 2U);
    const bool has_exception = std::any_of(
        layout.validation_outcomes.begin(),
        layout.validation_outcomes.end(),
        [](const auto& outcome) {
            return outcome.classification ==
                Siligen::Domain::Dispensing::ValueObjects::SpacingValidationClassification::PassWithException;
        });
    EXPECT_EQ(payload.preview_validation_classification, has_exception ? "pass_with_exception" : "pass");
    for (const auto& outcome : layout.validation_outcomes) {
        EXPECT_EQ(outcome.dispatch_type, TopologyDispatchType::ExplicitProcessBoundary);
        EXPECT_TRUE(outcome.blocking_reason.empty());
        EXPECT_EQ(outcome.component_id, layout.components.front().component_id);
    }
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsKeepsExplicitBoundaryMixedWithReorderedBranchFamilyBlocked) {
    auto input = BuildInput();
    input.trigger_spatial_interval_mm = 5.0f;
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(0.0f, 0.0f), false));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(0.0f, 10.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(-10.0f, 0.0f)));
    input.authority_process_path = input.process_path;
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f, true),
        BuildMotionPoint(1.0f, 10.0f, 0.0f, true),
        BuildMotionPoint(2.0f, 0.0f, 0.0f, false),
        BuildMotionPoint(3.0f, 0.0f, 10.0f, true),
        BuildMotionPoint(4.0f, 0.0f, 0.0f, true),
        BuildMotionPoint(5.0f, -10.0f, 0.0f, true),
    };
    input.motion_plan.total_length = 40.0f;
    input.motion_plan.total_time = 5.0f;
    input.estimated_time_s = 5.0f;

    PlanningAssemblyFacade facade;
    const auto result = facade.BuildPlanningArtifacts(input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    const auto& layout = payload.authority_trigger_layout;
    EXPECT_FALSE(layout.authority_ready);
    EXPECT_EQ(layout.dispatch_type, TopologyDispatchType::UnsupportedMixedTopology);
    EXPECT_EQ(layout.effective_component_count, 1U);
    EXPECT_EQ(layout.ignored_component_count, 0U);
    ASSERT_EQ(layout.components.size(), 1U);
    ASSERT_EQ(layout.spans.size(), 3U);
    ASSERT_EQ(layout.validation_outcomes.size(), 3U);
    EXPECT_TRUE(layout.trigger_points.empty());
    EXPECT_EQ(layout.components.front().dispatch_type, TopologyDispatchType::UnsupportedMixedTopology);
    EXPECT_EQ(layout.components.front().blocking_reason, kMixedExplicitBoundaryWithReorderedBranchFamily);
    EXPECT_FALSE(payload.preview_authority_ready);
    EXPECT_FALSE(payload.preview_binding_ready);
    EXPECT_FALSE(payload.preview_spacing_valid);
    EXPECT_EQ(payload.preview_validation_classification, "fail");
    EXPECT_TRUE(payload.glue_points.empty());
    EXPECT_EQ(payload.preview_failure_reason, kMixedExplicitBoundaryWithReorderedBranchFamily);
    for (const auto& outcome : layout.validation_outcomes) {
        EXPECT_EQ(outcome.dispatch_type, TopologyDispatchType::UnsupportedMixedTopology);
        EXPECT_EQ(outcome.blocking_reason, kMixedExplicitBoundaryWithReorderedBranchFamily);
        EXPECT_EQ(outcome.component_id, layout.components.front().component_id);
    }
}

TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsReturnsFailClassificationForDegenerateSplineAuthority) {
    auto input = BuildInput();
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildSplineSegment({
        Point2D(5.0f, 5.0f),
        Point2D(5.0f, 5.0f),
        Point2D(5.0f, 5.0f),
    }));
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 5.0f, 5.0f),
        BuildMotionPoint(1.0f, 5.0f, 5.0f),
    };
    input.motion_plan.total_length = 0.0f;
    input.motion_plan.total_time = 1.0f;
    input.spline_max_step_mm = 1.0f;
    input.spline_max_error_mm = 0.05f;

    PlanningAssemblyFacade facade;
    const auto result = facade.BuildPlanningArtifacts(input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_EQ(payload.preview_validation_classification, "fail");
    EXPECT_FALSE(payload.preview_authority_ready);
    EXPECT_FALSE(payload.preview_binding_ready);
    EXPECT_FALSE(payload.preview_spacing_valid);
    EXPECT_TRUE(payload.glue_points.empty());
    EXPECT_FALSE(payload.preview_failure_reason.empty());
}
TEST(PlanningAssemblyServicesTest, AssemblePlanningArtifactsRejectsZeroLengthDispenseSegments) {
    auto input = BuildInput();
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildPointSegment(Point2D(10.0f, 0.0f)));
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 10.0f, 0.0f),
        BuildMotionPoint(1.0f, 10.0f, 0.0f),
    };
    input.motion_plan.total_length = 0.0f;
    input.motion_plan.total_time = 1.0f;

    PlanningAssemblyFacade facade;
    const auto result = facade.BuildPlanningArtifacts(input);

    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().GetMessage().find("长度为0"), std::string::npos);
}



