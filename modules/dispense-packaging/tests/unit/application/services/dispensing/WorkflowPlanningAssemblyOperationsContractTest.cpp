#include "application/services/dispensing/WorkflowPlanningAssemblyOperationsProvider.h"

#include "PlanningAssemblyTestFixtures.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Application::Services::Dispensing::WorkflowPlanningAssemblyOperationsProvider;
using namespace Siligen::Application::Services::Dispensing::TestFixtures;

TEST(WorkflowPlanningAssemblyOperationsContractTest, BuildAuthorityPreviewArtifactsReturnsStableAuthorityTruth) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
    }, 3.0f);

    const auto result = operations->BuildAuthorityPreviewArtifacts(BuildWorkflowAuthorityPreviewInput(input));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_TRUE(payload.preview_authority_ready);
    EXPECT_TRUE(payload.preview_binding_ready);
    EXPECT_TRUE(payload.preview_spacing_valid);
    EXPECT_EQ(payload.trigger_count, static_cast<int>(payload.glue_points.size()));
    EXPECT_EQ(payload.glue_points.size(), payload.authority_trigger_layout.trigger_points.size());
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, BuildExecutionArtifactsFromAuthorityBuildsValidatedExecutionAndExportRequest) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    auto authority = operations->BuildAuthorityPreviewArtifacts(BuildWorkflowAuthorityPreviewInput(input));
    ASSERT_TRUE(authority.IsSuccess()) << authority.GetError().GetMessage();

    const auto result = operations->BuildExecutionArtifactsFromAuthority(
        BuildWorkflowExecutionInput(input, authority.Value()));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_TRUE(payload.execution_package.Validate().IsSuccess());
    EXPECT_TRUE(payload.execution_binding_ready);
    EXPECT_TRUE(payload.preview_authority_shared_with_execution);
    EXPECT_EQ(payload.export_request.source_path, input.source_path);
    EXPECT_EQ(payload.export_request.execution_trajectory_points.size(), payload.execution_trajectory_points.size());
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsMatchesStagedAuthorityAndExecutionSemantics) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    auto authority = operations->BuildAuthorityPreviewArtifacts(BuildWorkflowAuthorityPreviewInput(input));
    ASSERT_TRUE(authority.IsSuccess()) << authority.GetError().GetMessage();
    auto execution = operations->BuildExecutionArtifactsFromAuthority(
        BuildWorkflowExecutionInput(input, authority.Value()));
    ASSERT_TRUE(execution.IsSuccess()) << execution.GetError().GetMessage();

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_EQ(payload.segment_count, authority.Value().segment_count);
    EXPECT_EQ(payload.trigger_count, authority.Value().trigger_count);
    EXPECT_EQ(payload.glue_points.size(), authority.Value().glue_points.size());
    EXPECT_EQ(payload.trajectory_points.size(), execution.Value().execution_trajectory_points.size());
    EXPECT_EQ(payload.export_request.execution_trajectory_points.size(), execution.Value().export_request.execution_trajectory_points.size());
    EXPECT_EQ(payload.preview_binding_ready, execution.Value().execution_binding_ready);
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsPreservesShortSegmentExceptions) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(2.0f, 0.0f)));
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f),
        BuildMotionPoint(1.0f, 2.0f, 0.0f),
    };
    input.motion_plan.total_length = 2.0f;
    input.motion_plan.total_time = 1.0f;
    input.trigger_spatial_interval_mm = 3.0f;

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_TRUE(payload.preview_authority_ready);
    EXPECT_TRUE(payload.preview_spacing_valid);
    EXPECT_TRUE(payload.preview_has_short_segment_exceptions);
    ASSERT_EQ(payload.spacing_validation_groups.size(), 1U);
    EXPECT_TRUE(payload.spacing_validation_groups.front().short_segment_exception);
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsSurfacesUnsupportedMixedTopologyAsFailClassification) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
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

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_FALSE(payload.preview_authority_ready);
    EXPECT_FALSE(payload.preview_binding_ready);
    EXPECT_FALSE(payload.preview_spacing_valid);
    EXPECT_EQ(payload.preview_validation_classification, "fail");
    EXPECT_EQ(payload.preview_failure_reason, kMixedExplicitBoundaryWithReorderedBranchFamily);
    EXPECT_EQ(payload.authority_trigger_layout.components.front().blocking_reason, kMixedExplicitBoundaryWithReorderedBranchFamily);
}

}  // namespace
