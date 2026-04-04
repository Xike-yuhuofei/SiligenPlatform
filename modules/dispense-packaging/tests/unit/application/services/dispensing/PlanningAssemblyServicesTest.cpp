#include "application/services/dispensing/AuthorityPreviewAssemblyService.h"
#include "application/services/dispensing/DispensePlanningFacade.h"
#include "application/services/dispensing/ExecutionAssemblyService.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace {

using Siligen::Application::Services::Dispensing::AuthorityPreviewAssemblyService;
using Siligen::Application::Services::Dispensing::AuthorityPreviewBuildInput;
using Siligen::Application::Services::Dispensing::AuthorityPreviewBuildResult;
using Siligen::Application::Services::Dispensing::DispensePlanningFacade;
using Siligen::Application::Services::Dispensing::ExecutionAssemblyBuildInput;
using Siligen::Application::Services::Dispensing::ExecutionAssemblyService;
using Siligen::Application::Services::Dispensing::PlanningArtifactsBuildInput;
using Siligen::Domain::Motion::InterpolationAlgorithm;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint;
using Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
using Siligen::Domain::Trajectory::ValueObjects::Segment;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::Shared::Types::DispensingStrategy;
using Siligen::Shared::Types::Point2D;

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

PlanningArtifactsBuildInput BuildPlanningInput() {
    PlanningArtifactsBuildInput input;
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
    input.authority_process_path = input.process_path;
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f),
        BuildMotionPoint(1.0f, 10.0f, 0.0f),
    };
    input.motion_plan.total_length = 10.0f;
    input.motion_plan.total_time = 1.0f;
    input.estimated_time_s = 1.25f;
    return input;
}

AuthorityPreviewBuildInput BuildAuthorityPreviewInput(const PlanningArtifactsBuildInput& planning) {
    AuthorityPreviewBuildInput input;
    input.process_path = planning.process_path;
    input.authority_process_path = planning.authority_process_path;
    input.source_path = planning.source_path;
    input.dxf_filename = planning.dxf_filename;
    input.dispensing_velocity = planning.dispensing_velocity;
    input.acceleration = planning.acceleration;
    input.dispenser_interval_ms = planning.dispenser_interval_ms;
    input.dispenser_duration_ms = planning.dispenser_duration_ms;
    input.trigger_spatial_interval_mm = planning.trigger_spatial_interval_mm;
    input.valve_response_ms = planning.valve_response_ms;
    input.safety_margin_ms = planning.safety_margin_ms;
    input.min_interval_ms = planning.min_interval_ms;
    input.sample_dt = planning.sample_dt;
    input.sample_ds = planning.sample_ds;
    input.spline_max_step_mm = planning.spline_max_step_mm;
    input.spline_max_error_mm = planning.spline_max_error_mm;
    input.dispensing_strategy = planning.dispensing_strategy;
    input.subsegment_count = planning.subsegment_count;
    input.dispense_only_cruise = planning.dispense_only_cruise;
    input.downgrade_on_violation = planning.downgrade_on_violation;
    input.compensation_profile = planning.compensation_profile;
    input.spacing_tol_ratio = planning.spacing_tol_ratio;
    input.spacing_min_mm = planning.spacing_min_mm;
    input.spacing_max_mm = planning.spacing_max_mm;
    return input;
}

ExecutionAssemblyBuildInput BuildExecutionAssemblyInput(
    const PlanningArtifactsBuildInput& planning,
    const AuthorityPreviewBuildResult& authority_preview) {
    ExecutionAssemblyBuildInput input;
    input.process_path = planning.process_path;
    input.motion_plan = planning.motion_plan;
    input.source_path = planning.source_path;
    input.dxf_filename = planning.dxf_filename;
    input.dispensing_velocity = planning.dispensing_velocity;
    input.acceleration = planning.acceleration;
    input.dispenser_interval_ms = planning.dispenser_interval_ms;
    input.dispenser_duration_ms = planning.dispenser_duration_ms;
    input.trigger_spatial_interval_mm = planning.trigger_spatial_interval_mm;
    input.valve_response_ms = planning.valve_response_ms;
    input.safety_margin_ms = planning.safety_margin_ms;
    input.min_interval_ms = planning.min_interval_ms;
    input.max_jerk = planning.max_jerk;
    input.sample_dt = planning.sample_dt;
    input.sample_ds = planning.sample_ds;
    input.spline_max_step_mm = planning.spline_max_step_mm;
    input.spline_max_error_mm = planning.spline_max_error_mm;
    input.estimated_time_s = planning.estimated_time_s;
    input.use_interpolation_planner = planning.use_interpolation_planner;
    input.interpolation_algorithm = planning.interpolation_algorithm;
    input.compensation_profile = planning.compensation_profile;
    input.authority_preview = authority_preview;
    return input;
}

void ExpectPointVectorsNear(
    const std::vector<Point2D>& actual,
    const std::vector<Point2D>& expected,
    float tolerance_mm = 1e-4f) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t index = 0; index < actual.size(); ++index) {
        EXPECT_NEAR(actual[index].x, expected[index].x, tolerance_mm);
        EXPECT_NEAR(actual[index].y, expected[index].y, tolerance_mm);
    }
}

}  // namespace

TEST(PlanningAssemblyServicesTest, AuthorityPreviewAssemblyServiceMatchesFacadePreviewContract) {
    const auto planning_input = BuildPlanningInput();
    const auto authority_input = BuildAuthorityPreviewInput(planning_input);

    AuthorityPreviewAssemblyService direct_service;
    DispensePlanningFacade facade;

    const auto direct = direct_service.BuildAuthorityPreviewArtifacts(authority_input);
    const auto via_facade = facade.BuildAuthorityPreviewArtifacts(authority_input);

    ASSERT_TRUE(direct.IsSuccess()) << direct.GetError().GetMessage();
    ASSERT_TRUE(via_facade.IsSuccess()) << via_facade.GetError().GetMessage();

    const auto& direct_payload = direct.Value();
    const auto& facade_payload = via_facade.Value();
    EXPECT_EQ(direct_payload.authority_trigger_layout.layout_id, facade_payload.authority_trigger_layout.layout_id);
    EXPECT_EQ(direct_payload.trigger_count, facade_payload.trigger_count);
    EXPECT_EQ(direct_payload.preview_authority_ready, facade_payload.preview_authority_ready);
    EXPECT_EQ(direct_payload.preview_binding_ready, facade_payload.preview_binding_ready);
    EXPECT_EQ(direct_payload.preview_spacing_valid, facade_payload.preview_spacing_valid);
    EXPECT_EQ(direct_payload.spacing_validation_groups.size(), facade_payload.spacing_validation_groups.size());
    ASSERT_EQ(direct_payload.glue_points.size(), direct_payload.authority_trigger_layout.trigger_points.size());
    ExpectPointVectorsNear(direct_payload.glue_points, facade_payload.glue_points);
}

TEST(PlanningAssemblyServicesTest, ExecutionAssemblyServiceMatchesFacadeExecutionContract) {
    const auto planning_input = BuildPlanningInput();
    const auto authority_input = BuildAuthorityPreviewInput(planning_input);

    AuthorityPreviewAssemblyService preview_service;
    const auto authority_result = preview_service.BuildAuthorityPreviewArtifacts(authority_input);
    ASSERT_TRUE(authority_result.IsSuccess()) << authority_result.GetError().GetMessage();

    const auto execution_input = BuildExecutionAssemblyInput(planning_input, authority_result.Value());

    ExecutionAssemblyService direct_service;
    DispensePlanningFacade facade;

    const auto direct = direct_service.BuildExecutionArtifactsFromAuthority(execution_input);
    const auto via_facade = facade.BuildExecutionArtifactsFromAuthority(execution_input);

    ASSERT_TRUE(direct.IsSuccess()) << direct.GetError().GetMessage();
    ASSERT_TRUE(via_facade.IsSuccess()) << via_facade.GetError().GetMessage();

    const auto& direct_payload = direct.Value();
    const auto& facade_payload = via_facade.Value();
    EXPECT_TRUE(direct_payload.execution_package.Validate().IsSuccess());
    EXPECT_TRUE(facade_payload.execution_package.Validate().IsSuccess());
    EXPECT_EQ(direct_payload.execution_package.source_path, facade_payload.execution_package.source_path);
    EXPECT_FLOAT_EQ(direct_payload.execution_package.total_length_mm, facade_payload.execution_package.total_length_mm);
    EXPECT_FLOAT_EQ(direct_payload.execution_package.estimated_time_s, facade_payload.execution_package.estimated_time_s);
    EXPECT_EQ(direct_payload.authority_trigger_layout.layout_id, facade_payload.authority_trigger_layout.layout_id);
    EXPECT_EQ(direct_payload.preview_authority_shared_with_execution,
              facade_payload.preview_authority_shared_with_execution);
    EXPECT_EQ(direct_payload.execution_binding_ready, facade_payload.execution_binding_ready);
    EXPECT_EQ(direct_payload.execution_trajectory_points.size(), facade_payload.execution_trajectory_points.size());
    EXPECT_EQ(direct_payload.motion_trajectory_points.size(), facade_payload.motion_trajectory_points.size());
    EXPECT_EQ(direct_payload.export_request.glue_points.size(), facade_payload.export_request.glue_points.size());
    EXPECT_EQ(direct_payload.export_request.execution_trajectory_points.size(),
              facade_payload.export_request.execution_trajectory_points.size());
}
