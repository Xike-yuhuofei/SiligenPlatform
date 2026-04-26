#include "application/services/dispensing/WorkflowPlanningAssemblyOperationsProvider.h"

#include "PlanningAssemblyTestFixtures.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

namespace {

using Siligen::Application::Services::Dispensing::WorkflowPlanningAssemblyOperationsProvider;
using Siligen::Domain::Dispensing::Contracts::ExecutionPackageBuilt;
using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;
using Siligen::Domain::Dispensing::ValueObjects::StrongAnchorRole;
using Siligen::Domain::Dispensing::ValueObjects::TopologyDispatchType;
using Siligen::Shared::Types::DispensingExecutionGeometryKind;
using Siligen::Shared::Types::DispensingExecutionStrategy;
using Siligen::Shared::Types::ErrorCode;
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

TEST(
    WorkflowPlanningAssemblyOperationsContractTest,
    BuildExecutionArtifactsFromAuthoritySurfacesNonMonotonicAuthorityBindingAsExecutionFailureReason) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    auto authority = operations->BuildAuthorityPreviewArtifacts(BuildWorkflowAuthorityPreviewInput(input));
    ASSERT_TRUE(authority.IsSuccess()) << authority.GetError().GetMessage();

    auto corrupted_authority = authority.Value();
    ASSERT_GE(corrupted_authority.authority_trigger_layout.trigger_points.size(), 3U);
    std::swap(
        corrupted_authority.authority_trigger_layout.trigger_points[1],
        corrupted_authority.authority_trigger_layout.trigger_points[2]);

    const auto result = operations->BuildExecutionArtifactsFromAuthority(
        BuildWorkflowExecutionInput(input, corrupted_authority));

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_EQ(result.GetError().GetMessage(), "authority trigger binding non-monotonic");
}

TEST(
    WorkflowPlanningAssemblyOperationsContractTest,
    BuildExecutionArtifactsFromAuthoritySplitsClosedLoopIntoFormalCompareReadyBoundarySpans) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(5.0f, 5.0f),
        Point2D(10.0f, 0.0f),
        Point2D(5.0f, -5.0f),
        Point2D(0.0f, 0.0f),
    }, 5.0f);
    input.authority_process_path = input.process_path;

    auto authority = operations->BuildAuthorityPreviewArtifacts(BuildWorkflowAuthorityPreviewInput(input));
    ASSERT_TRUE(authority.IsSuccess()) << authority.GetError().GetMessage();

    auto execution_input = BuildWorkflowExecutionInput(input, authority.Value());
    execution_input.profile_compare_runtime_contract.max_future_compare_count = 256U;

    const auto result = operations->BuildExecutionArtifactsFromAuthority(execution_input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_TRUE(payload.preview_authority_shared_with_execution);
    EXPECT_TRUE(payload.execution_binding_ready);
    EXPECT_TRUE(payload.execution_contract_ready);
    EXPECT_EQ(payload.execution_diagnostic_code, "");
    EXPECT_TRUE(payload.execution_failure_reason.empty());
    EXPECT_FALSE(payload.formal_compare_gate.HasValue());
    EXPECT_FALSE(payload.execution_package.execution_plan.profile_compare_program.trigger_points.empty());
    EXPECT_FALSE(payload.execution_package.execution_plan.profile_compare_program.spans.empty());
    EXPECT_TRUE(payload.execution_package.Validate().IsSuccess());
}

TEST(
    WorkflowPlanningAssemblyOperationsContractTest,
    BuildExecutionArtifactsFromAuthorityBuildsFormalCompareSpansForSharedVertexReorderedBranchComponent) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
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
    input.execution_nominal_time_s = 3.0f;

    auto authority = operations->BuildAuthorityPreviewArtifacts(BuildWorkflowAuthorityPreviewInput(input));
    ASSERT_TRUE(authority.IsSuccess()) << authority.GetError().GetMessage();

    const auto result = operations->BuildExecutionArtifactsFromAuthority(
        BuildWorkflowExecutionInput(input, authority.Value()));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_TRUE(payload.preview_authority_shared_with_execution);
    EXPECT_TRUE(payload.execution_binding_ready);
    EXPECT_TRUE(payload.execution_contract_ready);
    EXPECT_EQ(payload.execution_diagnostic_code, "");
    EXPECT_FALSE(payload.formal_compare_gate.HasValue());
    EXPECT_EQ(
        payload.execution_package.execution_plan.production_trigger_mode,
        Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode::PROFILE_COMPARE);
    EXPECT_EQ(payload.execution_package.execution_plan.profile_compare_program.spans.size(), 2U);
    const auto& first_span = payload.execution_package.execution_plan.profile_compare_program.spans[0];
    const auto& second_span = payload.execution_package.execution_plan.profile_compare_program.spans[1];
    EXPECT_EQ(first_span.compare_source_axis, 1);
    EXPECT_EQ(first_span.start_boundary_trigger_count, 1U);
    EXPECT_EQ(second_span.compare_source_axis, 2);
    EXPECT_EQ(second_span.start_boundary_trigger_count, 1U);
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, BuildExecutionArtifactsFromAuthorityKeepsOverlappedReturnBoundaryMonotonic) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    input.trigger_spatial_interval_mm = 5.0f;
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(5.0f, 5.0f), false));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(5.0f, 5.0f), Point2D(15.0f, -5.0f)));
    input.authority_process_path = input.process_path;
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f),
        BuildMotionPoint(1.0f, 5.0f, 0.0f),
        BuildMotionPoint(2.0f, 7.0f, 1.0f),
        BuildMotionPoint(3.0f, 5.0f, 5.0f, false),
        BuildMotionPoint(4.0f, 10.0f, 0.0f),
        BuildMotionPoint(5.0f, 15.0f, -5.0f),
    };
    input.motion_plan.total_length =
        5.0f +
        std::sqrt(5.0f) +
        std::sqrt(20.0f) +
        std::sqrt(50.0f) +
        std::sqrt(50.0f);
    input.motion_plan.total_time = 5.0f;
    input.execution_nominal_time_s = 5.0f;

    auto authority = operations->BuildAuthorityPreviewArtifacts(BuildWorkflowAuthorityPreviewInput(input));
    ASSERT_TRUE(authority.IsSuccess()) << authority.GetError().GetMessage();

    const auto result = operations->BuildExecutionArtifactsFromAuthority(
        BuildWorkflowExecutionInput(input, authority.Value()));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_TRUE(payload.execution_binding_ready);
    EXPECT_TRUE(payload.preview_authority_shared_with_execution);
    EXPECT_TRUE(std::all_of(
        payload.authority_trigger_layout.bindings.begin(),
        payload.authority_trigger_layout.bindings.end(),
        [](const auto& binding) { return binding.bound && binding.monotonic; }));
    EXPECT_TRUE(std::all_of(
        payload.export_request.execution_trigger_metadata.begin(),
        payload.export_request.execution_trigger_metadata.end(),
        [](const auto& metadata) { return metadata.binding_monotonic; }));
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, BuildExecutionArtifactsFromAuthorityUsesCanonicalExecutionPathAsExecutionTruth) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f),
        BuildMotionPoint(1.0f, 5.0f, 2.0f),
        BuildMotionPoint(2.0f, 10.0f, 0.0f),
    };
    input.motion_plan.total_length = 12.0f;
    input.motion_plan.total_time = 2.0f;
    input.execution_nominal_time_s = 2.5f;
    input.use_interpolation_planner = true;
    input.interpolation_algorithm = InterpolationAlgorithm::LINEAR;
    input.sample_ds = 50.0f;
    input.trigger_spatial_interval_mm = 100.0f;

    auto authority = operations->BuildAuthorityPreviewArtifacts(BuildWorkflowAuthorityPreviewInput(input));
    ASSERT_TRUE(authority.IsSuccess()) << authority.GetError().GetMessage();

    const auto result = operations->BuildExecutionArtifactsFromAuthority(
        BuildWorkflowExecutionInput(input, authority.Value()));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_EQ(payload.motion_trajectory_points.size(), input.motion_plan.points.size());
    EXPECT_FLOAT_EQ(payload.motion_trajectory_points[1].position.x, 5.0f);
    EXPECT_FLOAT_EQ(payload.motion_trajectory_points[1].position.y, 2.0f);
    EXPECT_TRUE(std::all_of(
        payload.execution_trajectory_points.begin(),
        payload.execution_trajectory_points.end(),
        [](const auto& point) {
            return std::abs(point.position.y) <= 1e-4f;
        }));
    EXPECT_FALSE(std::any_of(
        payload.execution_trajectory_points.begin(),
        payload.execution_trajectory_points.end(),
        [](const auto& point) {
            return std::abs(point.position.x - 5.0f) <= 1e-4f &&
                   std::abs(point.position.y - 2.0f) <= 1e-4f;
        }));
    EXPECT_FLOAT_EQ(payload.execution_trajectory_points.front().timestamp, 0.0f);
    EXPECT_NEAR(payload.execution_trajectory_points.back().timestamp, input.motion_plan.total_time, 1e-4f);
    EXPECT_EQ(
        std::adjacent_find(
            payload.execution_trajectory_points.begin(),
            payload.execution_trajectory_points.end(),
            [](const auto& lhs, const auto& rhs) {
                return rhs.timestamp < lhs.timestamp;
            }),
        payload.execution_trajectory_points.end());
    EXPECT_EQ(payload.export_request.execution_trajectory_points.size(), payload.execution_trajectory_points.size());
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, BuildExecutionArtifactsFromAuthorityRejectsLinearPathWithoutMotionTimingTruth) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f),
        BuildMotionPoint(0.0f, 10.0f, 0.0f),
    };
    input.motion_plan.total_length = 10.0f;
    input.motion_plan.total_time = 0.0f;

    auto authority = operations->BuildAuthorityPreviewArtifacts(BuildWorkflowAuthorityPreviewInput(input));
    ASSERT_TRUE(authority.IsSuccess()) << authority.GetError().GetMessage();

    const auto result = operations->BuildExecutionArtifactsFromAuthority(
        BuildWorkflowExecutionInput(input, authority.Value()));

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, ExecutionPackageValidateRejectsPathInterpolationPointsWithoutTiming) {
    ExecutionPackageBuilt built;
    built.execution_plan.interpolation_points.emplace_back(0.0f, 0.0f, 10.0f);
    built.execution_plan.interpolation_points.emplace_back(10.0f, 0.0f, 10.0f);
    built.total_length_mm = 10.0f;
    built.execution_nominal_time_s = 1.0f;

    ExecutionPackageValidated package(std::move(built));
    const auto result = package.Validate();

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
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

TEST(WorkflowPlanningAssemblyOperationsContractTest, BuildExecutionArtifactsFromAuthorityUsesCanonicalExecutionProcessPathForExport) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(4.0f, 0.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(4.0f, 0.0f), Point2D(10.0f, 0.0f)));
    input.authority_process_path.segments.clear();
    input.authority_process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));

    auto authority = operations->BuildAuthorityPreviewArtifacts(BuildWorkflowAuthorityPreviewInput(input));
    ASSERT_TRUE(authority.IsSuccess()) << authority.GetError().GetMessage();
    ASSERT_EQ(authority.Value().canonical_execution_process_path.segments.size(), 1U);

    const auto result = operations->BuildExecutionArtifactsFromAuthority(
        BuildWorkflowExecutionInput(input, authority.Value()));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_FALSE(payload.execution_package.execution_plan.interpolation_segments.empty());
    ASSERT_EQ(payload.export_request.process_path.segments.size(), 1U);
    EXPECT_FLOAT_EQ(payload.export_request.process_path.segments.front().geometry.line.start.x, 0.0f);
    EXPECT_FLOAT_EQ(payload.export_request.process_path.segments.front().geometry.line.end.x, 10.0f);
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, BuildAuthorityPreviewArtifactsUsesCanonicalExecutionPathForPreviewPolyline) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(0.0f, 5.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 5.0f), Point2D(5.0f, 5.0f)));
    input.authority_process_path.segments.clear();
    input.authority_process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    input.trigger_spatial_interval_mm = 5.0f;

    const auto result = operations->BuildAuthorityPreviewArtifacts(BuildWorkflowAuthorityPreviewInput(input));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_EQ(payload.canonical_execution_process_path.segments.size(), 1U);
    ASSERT_FALSE(payload.authority_trigger_points.empty());
    EXPECT_EQ(payload.trigger_count, static_cast<int>(payload.authority_trigger_points.size()));
    EXPECT_TRUE(std::all_of(
        payload.authority_trigger_points.begin(),
        payload.authority_trigger_points.end(),
        [](const auto& trigger) { return std::abs(trigger.position.y) <= 1e-4f; }));
    EXPECT_TRUE(std::all_of(
        payload.preview_trajectory_points.begin(),
        payload.preview_trajectory_points.end(),
        [](const auto& point) { return std::abs(point.position.y) <= 1e-4f; }));
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, BuildExecutionArtifactsRejectsMissingCanonicalExecutionPath) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(0.0f, 5.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 5.0f), Point2D(5.0f, 5.0f)));
    input.authority_process_path.segments.clear();
    input.authority_process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));

    auto authority = operations->BuildAuthorityPreviewArtifacts(BuildWorkflowAuthorityPreviewInput(input));
    ASSERT_TRUE(authority.IsSuccess()) << authority.GetError().GetMessage();

    auto execution_input = BuildWorkflowExecutionInput(input, authority.Value());
    execution_input.canonical_execution_process_path.segments.clear();
    const auto result = operations->BuildExecutionArtifactsFromAuthority(execution_input);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetMessage(), "canonical execution process path为空");
}

TEST(
    WorkflowPlanningAssemblyOperationsContractTest,
    BuildAuthorityPreviewArtifactsProjectsBranchRevisitIntoSpanOrderedCanonicalExecutionPath) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        Point2D(10.0f, 10.0f),
        Point2D(0.0f, 10.0f),
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 10.0f),
    }, 5.0f);
    input.authority_process_path = input.process_path;

    const auto result = operations->BuildAuthorityPreviewArtifacts(BuildWorkflowAuthorityPreviewInput(input));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_TRUE(payload.authority_trigger_layout.branch_revisit_split_applied);
    ASSERT_EQ(payload.authority_trigger_layout.spans.size(), 2U);
    ASSERT_EQ(payload.canonical_execution_process_path.segments.size(), input.process_path.segments.size());
    EXPECT_EQ(
        std::count_if(
            payload.canonical_execution_process_path.segments.begin(),
            payload.canonical_execution_process_path.segments.end(),
            [](const auto& segment) { return !segment.dispense_on; }),
        0);
    EXPECT_FLOAT_EQ(
        payload.canonical_execution_process_path.segments.front().geometry.line.start.x,
        input.process_path.segments.front().geometry.line.start.x);
    EXPECT_FLOAT_EQ(
        payload.canonical_execution_process_path.segments.front().geometry.line.start.y,
        input.process_path.segments.front().geometry.line.start.y);
    EXPECT_FLOAT_EQ(
        payload.canonical_execution_process_path.segments.front().geometry.line.end.x,
        input.process_path.segments.front().geometry.line.end.x);
    EXPECT_FLOAT_EQ(
        payload.canonical_execution_process_path.segments.front().geometry.line.end.y,
        input.process_path.segments.front().geometry.line.end.y);
    EXPECT_FLOAT_EQ(
        payload.canonical_execution_process_path.segments.back().geometry.line.start.x,
        input.process_path.segments.back().geometry.line.start.x);
    EXPECT_FLOAT_EQ(
        payload.canonical_execution_process_path.segments.back().geometry.line.start.y,
        input.process_path.segments.back().geometry.line.start.y);
    EXPECT_FLOAT_EQ(
        payload.canonical_execution_process_path.segments.back().geometry.line.end.x,
        input.process_path.segments.back().geometry.line.end.x);
    EXPECT_FLOAT_EQ(
        payload.canonical_execution_process_path.segments.back().geometry.line.end.y,
        input.process_path.segments.back().geometry.line.end.y);
    ASSERT_FALSE(payload.preview_trajectory_points.empty());
    EXPECT_NEAR(payload.preview_trajectory_points.back().position.x, 10.0f, 1e-4f);
    EXPECT_NEAR(payload.preview_trajectory_points.back().position.y, 10.0f, 1e-4f);
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
    input.execution_nominal_time_s = 5.0f;

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

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsProducesValidatedExecutionPackage) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput());

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_EQ(payload.execution_package.source_path, "sample.pb");
    EXPECT_FLOAT_EQ(payload.execution_package.total_length_mm, 10.0f);
    EXPECT_FLOAT_EQ(payload.execution_package.execution_nominal_time_s, 1.25f);
    EXPECT_TRUE(payload.execution_package.Validate().IsSuccess());
    EXPECT_FALSE(payload.execution_package.execution_plan.interpolation_segments.empty());
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsBuildsPreviewPayloadAndExportRequest) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput());

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_EQ(payload.segment_count, 1);
    EXPECT_FLOAT_EQ(payload.total_length, 10.0f);
    EXPECT_FLOAT_EQ(payload.execution_nominal_time_s, 1.25f);
    EXPECT_FALSE(payload.trajectory_points.empty());
    EXPECT_FALSE(payload.glue_points.empty());
    EXPECT_EQ(payload.trigger_count, static_cast<int>(payload.glue_points.size()));
    EXPECT_EQ(payload.export_request.source_path, "sample.pb");
    EXPECT_EQ(payload.export_request.dxf_filename, "sample.pb");
    EXPECT_EQ(payload.export_request.glue_points.size(), payload.glue_points.size());
    EXPECT_EQ(payload.export_request.execution_trajectory_points.size(), payload.trajectory_points.size());
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsAnchorsGluePointsAtSegmentEndpointsAndUniformSpacing) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    input.trigger_spatial_interval_mm = 3.0f;

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_EQ(payload.glue_points.size(), 4U);
    EXPECT_GE(payload.trajectory_points.size(), payload.glue_points.size());
    EXPECT_EQ(CountTriggerMarkers(payload.trajectory_points), payload.glue_points.size());
    EXPECT_EQ(payload.trigger_count, 4);
    EXPECT_TRUE(payload.preview_authority_ready);
    EXPECT_TRUE(payload.preview_authority_shared_with_execution);
    EXPECT_TRUE(payload.preview_spacing_valid);
    EXPECT_NEAR(payload.glue_points[0].x, 0.0f, 1e-4f);
    EXPECT_NEAR(payload.glue_points[1].x, 10.0f / 3.0f, 1e-4f);
    EXPECT_NEAR(payload.glue_points[2].x, 20.0f / 3.0f, 1e-4f);
    EXPECT_NEAR(payload.glue_points[3].x, 10.0f, 1e-4f);
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsLinearInterpolationOnlyMarksDiscreteTriggerPoints) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    input.use_interpolation_planner = true;
    input.interpolation_algorithm = InterpolationAlgorithm::LINEAR;
    input.trigger_spatial_interval_mm = 3.0f;
    input.max_jerk = 500.0f;
    input.sample_ds = 10.0f;

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_EQ(payload.glue_points.size(), 4U);
    EXPECT_EQ(CountTriggerMarkers(payload.trajectory_points), payload.glue_points.size());
    EXPECT_GE(payload.trajectory_points.size(), payload.glue_points.size());
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsDeduplicatesSharedVerticesAcrossAnchoredSegments) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
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

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_EQ(payload.glue_points.size(), 5U);
    EXPECT_EQ(CountPointsNear(payload.glue_points, Point2D(10.0f, 0.0f), 1e-4f), 1U);
    EXPECT_FALSE(HasConsecutiveNearDuplicatePoints(payload.glue_points, 1e-4f));
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsPreservesExplicitBoundarySharedVertexAcrossRapidSeparatedSpans) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(0.0f, 0.0f), false));
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(0.0f, 10.0f)));
    input.trigger_spatial_interval_mm = 5.0f;
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f, true),
        BuildMotionPoint(1.0f, 10.0f, 0.0f, true),
        BuildMotionPoint(2.0f, 0.0f, 0.0f, false),
        BuildMotionPoint(3.0f, 0.0f, 10.0f, true),
    };
    input.motion_plan.total_length = 30.0f;
    input.motion_plan.total_time = 3.0f;
    input.execution_nominal_time_s = 3.0f;

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_EQ(payload.glue_points.size(), 6U);
    EXPECT_EQ(CountPointsNear(payload.glue_points, Point2D(0.0f, 0.0f), 1e-4f), 2U);
    EXPECT_FALSE(HasConsecutiveNearDuplicatePoints(payload.glue_points, 1e-4f));
    EXPECT_EQ(payload.glue_points.size(), payload.authority_trigger_layout.trigger_points.size());
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsUsesAuthorityLayoutAsGluePointTruth) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
    })));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_EQ(payload.glue_points.size(), payload.authority_trigger_layout.trigger_points.size());
    EXPECT_TRUE(payload.preview_binding_ready);
    for (std::size_t index = 0; index < payload.glue_points.size(); ++index) {
        EXPECT_NEAR(payload.glue_points[index].x, payload.authority_trigger_layout.trigger_points[index].position.x, 1e-4f);
        EXPECT_NEAR(payload.glue_points[index].y, payload.authority_trigger_layout.trigger_points[index].position.y, 1e-4f);
    }
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsKeepsEquivalentSubdivisionGluePointsStable) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    const auto single = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
    })));
    const auto subdivided = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(5.0f, 0.0f),
        Point2D(10.0f, 0.0f),
    })));

    ASSERT_TRUE(single.IsSuccess()) << single.GetError().GetMessage();
    ASSERT_TRUE(subdivided.IsSuccess()) << subdivided.GetError().GetMessage();
    ASSERT_EQ(single.Value().glue_points.size(), subdivided.Value().glue_points.size());
    for (std::size_t index = 0; index < single.Value().glue_points.size(); ++index) {
        EXPECT_NEAR(single.Value().glue_points[index].x, subdivided.Value().glue_points[index].x, 1e-4f);
        EXPECT_NEAR(single.Value().glue_points[index].y, subdivided.Value().glue_points[index].y, 1e-4f);
    }
}

TEST(
    WorkflowPlanningAssemblyOperationsContractTest,
    AssemblePlanningArtifactsProjectsClosedLoopIntoFormalCompareReadyLeafsWithoutDuplicateTail) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto square = BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        Point2D(10.0f, 10.0f),
        Point2D(0.0f, 10.0f),
        Point2D(0.0f, 0.0f),
    }, 5.0f);
    square.curve_flatten_max_step_mm = 1.0f;

    auto rotated_square = BuildPolylineInput({
        Point2D(10.0f, 0.0f),
        Point2D(10.0f, 10.0f),
        Point2D(0.0f, 10.0f),
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
    }, 5.0f);
    rotated_square.curve_flatten_max_step_mm = 1.0f;

    const auto square_result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(square));
    const auto rotated_result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(rotated_square));

    ASSERT_TRUE(square_result.IsSuccess()) << square_result.GetError().GetMessage();
    ASSERT_TRUE(rotated_result.IsSuccess()) << rotated_result.GetError().GetMessage();

    const auto& square_payload = square_result.Value();
    const auto& rotated_payload = rotated_result.Value();
    ASSERT_EQ(square_payload.authority_trigger_layout.spans.size(), 1U);
    ASSERT_EQ(rotated_payload.authority_trigger_layout.spans.size(), 1U);
    const auto& square_span = square_payload.authority_trigger_layout.spans.front();
    const auto& rotated_span = rotated_payload.authority_trigger_layout.spans.front();
    EXPECT_TRUE(square_span.closed);
    EXPECT_EQ(square_payload.glue_points.size(), square_span.interval_count);
    EXPECT_EQ(
        square_span.phase_strategy,
        Siligen::Domain::Dispensing::ValueObjects::DispenseSpanPhaseStrategy::AnchorConstrained);
    EXPECT_GE(square_span.phase_mm, 0.0f);
    EXPECT_LT(square_span.phase_mm, square_span.actual_spacing_mm);
    EXPECT_EQ(CountAnchorRoles(square_span, StrongAnchorRole::ClosedLoopCorner), 4U);
    EXPECT_GT(square_payload.glue_points.front().DistanceTo(square_payload.glue_points.back()), 1e-3f);
    EXPECT_EQ(square_span.phase_mm, rotated_span.phase_mm);
    EXPECT_EQ(square_payload.glue_points.size(), rotated_payload.glue_points.size());
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsBindsAuthorityAcrossClosedLoopPhaseShiftedExecution) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto authority_square = BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        Point2D(10.0f, 10.0f),
        Point2D(0.0f, 10.0f),
        Point2D(0.0f, 0.0f),
    }, 5.0f);
    authority_square.curve_flatten_max_step_mm = 1.0f;

    auto rotated_execution_square = BuildPolylineInput({
        Point2D(10.0f, 0.0f),
        Point2D(10.0f, 10.0f),
        Point2D(0.0f, 10.0f),
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
    }, 5.0f);
    rotated_execution_square.curve_flatten_max_step_mm = 1.0f;
    rotated_execution_square.authority_process_path = authority_square.process_path;

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(rotated_execution_square));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_TRUE(payload.preview_authority_ready);
    EXPECT_TRUE(payload.preview_binding_ready);
    ASSERT_EQ(
        payload.authority_trigger_layout.trigger_points.size(),
        payload.authority_trigger_layout.bindings.size());
    EXPECT_TRUE(std::all_of(
        payload.authority_trigger_layout.bindings.begin(),
        payload.authority_trigger_layout.bindings.end(),
        [](const auto& binding) { return binding.bound; }));
    EXPECT_TRUE(std::all_of(
        payload.authority_trigger_layout.bindings.begin(),
        payload.authority_trigger_layout.bindings.end(),
        [](const auto& binding) { return binding.monotonic; }));
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsSplitsRectDiagBranchRevisitWithoutBreakingPreviewTruth) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        Point2D(10.0f, 10.0f),
        Point2D(0.0f, 10.0f),
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 10.0f),
    }, 5.0f);
    input.authority_process_path = input.process_path;

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_TRUE(payload.authority_trigger_layout.branch_revisit_split_applied);
    ASSERT_EQ(payload.authority_trigger_layout.spans.size(), 2U);
    EXPECT_TRUE(payload.preview_authority_ready);
    EXPECT_TRUE(payload.preview_binding_ready);
    EXPECT_TRUE(payload.preview_authority_shared_with_execution);
    EXPECT_EQ(payload.glue_points.size(), payload.authority_trigger_layout.trigger_points.size());
    ASSERT_EQ(
        payload.authority_trigger_layout.trigger_points.size(),
        payload.authority_trigger_layout.bindings.size());
    EXPECT_TRUE(std::all_of(
        payload.authority_trigger_layout.bindings.begin(),
        payload.authority_trigger_layout.bindings.end(),
        [](const auto& binding) { return binding.bound && binding.monotonic; }));
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

TEST(
    WorkflowPlanningAssemblyOperationsContractTest,
    BuildExecutionArtifactsFromAuthorityBuildsFormalCompareContractForRectDiag) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPolylineInput({
        Point2D(0.0f, 0.0f),
        Point2D(100.0f, 0.0f),
        Point2D(100.0f, 100.0f),
        Point2D(0.0f, 100.0f),
        Point2D(0.0f, 0.0f),
        Point2D(100.0f, 100.0f),
    }, 10.0f);
    input.authority_process_path = input.process_path;

    auto authority = operations->BuildAuthorityPreviewArtifacts(BuildWorkflowAuthorityPreviewInput(input));
    ASSERT_TRUE(authority.IsSuccess()) << authority.GetError().GetMessage();

    auto execution_input = BuildWorkflowExecutionInput(input, authority.Value());
    const auto result = operations->BuildExecutionArtifactsFromAuthority(execution_input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_TRUE(payload.preview_authority_shared_with_execution);
    EXPECT_TRUE(payload.execution_binding_ready);
    EXPECT_TRUE(payload.execution_contract_ready);
    EXPECT_EQ(payload.execution_diagnostic_code, "");
    EXPECT_TRUE(payload.execution_failure_reason.empty());
    EXPECT_FALSE(payload.formal_compare_gate.HasValue());
    EXPECT_FALSE(payload.execution_package.execution_plan.profile_compare_program.trigger_points.empty());
    EXPECT_FALSE(payload.execution_package.execution_plan.profile_compare_program.spans.empty());
    EXPECT_TRUE(payload.execution_package.Validate().IsSuccess());
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsIgnoresAuxiliaryGeometryWithoutBreakingGluePointTruth) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
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
    input.execution_nominal_time_s = 4.0f;

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    const auto& layout = payload.authority_trigger_layout;
    EXPECT_EQ(layout.dispatch_type, TopologyDispatchType::AuxiliaryGeometry);
    EXPECT_EQ(layout.effective_component_count, 1U);
    EXPECT_EQ(layout.ignored_component_count, 1U);
    ASSERT_EQ(layout.components.size(), 2U);
    ASSERT_EQ(layout.spans.size(), 1U);
    EXPECT_EQ(layout.components.front().dispatch_type, TopologyDispatchType::SingleClosedLoop);
    EXPECT_EQ(layout.components.front().span_refs.size(), 1U);
    EXPECT_TRUE(layout.components[1].ignored);
    EXPECT_FALSE(layout.components[1].ignored_reason.empty());
    EXPECT_TRUE(payload.preview_authority_ready);
    EXPECT_TRUE(payload.preview_binding_ready);
    EXPECT_TRUE(payload.preview_spacing_valid);
    EXPECT_EQ(payload.glue_points.size(), layout.trigger_points.size());
    EXPECT_TRUE(layout.spans.front().closed);
    EXPECT_EQ(layout.spans.front().dispatch_type, TopologyDispatchType::SingleClosedLoop);
    EXPECT_EQ(CountAnchorRoles(layout.spans.front(), StrongAnchorRole::ClosedLoopCorner), 4U);
    EXPECT_TRUE(std::all_of(
        layout.spans.begin(),
        layout.spans.end(),
        [&](const auto& span) { return span.component_id == layout.components.front().component_id; }));
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

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsAllowsExplicitProcessBoundarySharedVertexComponent) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
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
    input.execution_nominal_time_s = 3.0f;

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

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

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsSupportsSharedVertexReorderedBranchComponent) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
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
    input.execution_nominal_time_s = 3.0f;

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

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

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsSupportsMixedOpenChainAndExplicitBoundaryFamily) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
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
    input.execution_nominal_time_s = 4.0f;

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

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

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsRejectsDegenerateSplineAuthorityWithEmptyInterpolationProgram) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
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
    input.curve_flatten_max_step_mm = 1.0f;
    input.curve_flatten_max_error_mm = 0.05f;

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().GetMessage().find("插补程序为空"), std::string::npos);
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsBuildsPointFlyingShotCarrierFromExplicitPolicy) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildPointSegment(Point2D(10.0f, 0.0f)));
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 10.0f, 0.0f),
        BuildMotionPoint(1.0f, 10.0f, 0.0f),
    };
    input.motion_plan.total_length = 0.0f;
    input.motion_plan.total_time = 1.0f;

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    const auto& execution_plan = result.Value().execution_package.execution_plan;
    EXPECT_EQ(execution_plan.geometry_kind, DispensingExecutionGeometryKind::POINT);
    EXPECT_EQ(execution_plan.execution_strategy, DispensingExecutionStrategy::FLYING_SHOT);
    EXPECT_TRUE(execution_plan.HasFormalTrajectory());
    ASSERT_EQ(execution_plan.trigger_distances_mm.size(), 1U);
    EXPECT_FLOAT_EQ(execution_plan.trigger_distances_mm.front(), 0.0f);
    ASSERT_EQ(result.Value().trajectory_points.size(), 2U);
    EXPECT_NEAR(result.Value().trajectory_points.front().position.x, 10.0f, 1e-4f);
    EXPECT_NEAR(result.Value().trajectory_points.front().position.y, 0.0f, 1e-4f);
    EXPECT_NEAR(result.Value().trajectory_points.back().position.x, 15.0f, 1e-4f);
    EXPECT_NEAR(result.Value().trajectory_points.back().position.y, 0.0f, 1e-4f);
    EXPECT_FLOAT_EQ(execution_plan.total_length_mm, 5.0f);
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsRejectsPointFlyingShotWithoutCarrierPolicy) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildPointSegment(Point2D(10.0f, 0.0f)));
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 10.0f, 0.0f),
        BuildMotionPoint(1.0f, 10.0f, 0.0f),
    };
    input.motion_plan.total_length = 0.0f;
    input.motion_plan.total_time = 1.0f;
    input.point_flying_carrier_policy.reset();

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().GetMessage().find("point_flying_carrier_policy"), std::string::npos);
}

TEST(WorkflowPlanningAssemblyOperationsContractTest, AssemblePlanningArtifactsRejectsPointFlyingShotWhenPlanningStartMatchesPoint) {
    WorkflowPlanningAssemblyOperationsProvider provider;
    const auto operations = provider.CreateOperations();

    auto input = BuildPlanningInput();
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildPointSegment(Point2D(10.0f, 0.0f)));
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 10.0f, 0.0f),
        BuildMotionPoint(1.0f, 10.0f, 0.0f),
    };
    input.motion_plan.total_length = 0.0f;
    input.motion_plan.total_time = 1.0f;
    input.planning_start_position = Point2D(10.0f, 0.0f);

    const auto result = operations->AssemblePlanningArtifacts(BuildWorkflowPlanningInput(input));

    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().GetMessage().find("planning_start_position"), std::string::npos);
}

}  // namespace
