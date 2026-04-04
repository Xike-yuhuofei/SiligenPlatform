#include "application/services/process_path/ProcessPathFacade.h"
#include "process_path/contracts/IPathSourcePort.h"
#include "process_path/contracts/IDXFPathSourcePort.h"
#include "process_path/contracts/PathGenerationRequest.h"
#include "process_path/contracts/PathGenerationResult.h"
#include "process_path/contracts/ProcessPath.h"

#include <gtest/gtest.h>

#include <optional>
#include <type_traits>
#include <utility>

namespace {

using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
using Siligen::Application::Services::ProcessPath::ProcessPathBuildResult;
using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
using Siligen::Domain::Trajectory::Ports::DXFPathSourceResult;
using Siligen::Domain::Trajectory::Ports::DXFValidationResult;
using Siligen::Domain::Trajectory::Ports::IPathSourcePort;
using Siligen::Domain::Trajectory::Ports::IDXFPathSourcePort;
using Siligen::Domain::Trajectory::Ports::PathSourceResult;
using Siligen::ProcessPath::Contracts::PathGenerationRequest;
using Siligen::ProcessPath::Contracts::PathGenerationResult;
using Siligen::ProcessPath::Contracts::PathGenerationStage;
using Siligen::ProcessPath::Contracts::PathGenerationStatus;
using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
using Siligen::ProcessPath::Contracts::ProcessPath;

static_assert(std::is_same_v<ProcessPathBuildRequest, PathGenerationRequest>);
static_assert(std::is_same_v<ProcessPathBuildResult, PathGenerationResult>);
static_assert(std::is_default_constructible_v<PathGenerationRequest>);
static_assert(std::is_default_constructible_v<PathGenerationResult>);
static_assert(std::is_abstract_v<IPathSourcePort>);
static_assert(std::is_abstract_v<IDXFPathSourcePort>);
static_assert(std::is_same_v<typename decltype(std::declval<PathSourceResult>().metadata)::value_type, PathPrimitiveMeta>);
static_assert(std::is_default_constructible_v<DXFValidationResult>);
static_assert(std::is_default_constructible_v<DXFPathSourceResult>);
static_assert(std::is_same_v<decltype(std::declval<const ProcessPathFacade&>().Build(
                                 std::declval<const ProcessPathBuildRequest&>())),
                             ProcessPathBuildResult>);
TEST(ProcessPathContractsTest, PathGenerationRequestDefaultsRemainOwnerNeutral) {
    const ProcessPathBuildRequest request;

    EXPECT_TRUE(request.primitives.empty());
    EXPECT_TRUE(request.metadata.empty());
    EXPECT_FALSE(request.alignment.has_value());
    EXPECT_FLOAT_EQ(request.normalization.unit_scale, 1.0f);
    EXPECT_FLOAT_EQ(request.normalization.continuity_tolerance, 0.1f);
    EXPECT_FLOAT_EQ(request.process.default_flow, 1.0f);
    EXPECT_TRUE(request.process.corner_slowdown);
    EXPECT_FALSE(request.topology_repair.enable);
    EXPECT_FLOAT_EQ(request.shaping.corner_smoothing_radius, 0.5f);
}

TEST(ProcessPathContractsTest, PathGenerationResultDefaultsExposeEmptyProcessPathSurfaces) {
    const ProcessPathBuildResult result;

    EXPECT_EQ(result.status, PathGenerationStatus::InvalidInput);
    EXPECT_EQ(result.failed_stage, PathGenerationStage::InputValidation);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_TRUE(result.normalized.path.segments.empty());
    EXPECT_FALSE(result.normalized.path.closed);
    EXPECT_EQ(result.normalized.report.discontinuity_count, 0);
    EXPECT_FALSE(result.normalized.report.invalid_unit_scale);
    EXPECT_EQ(result.normalized.report.skipped_spline_count, 0);
    EXPECT_EQ(result.normalized.report.point_primitive_count, 0);
    EXPECT_EQ(result.normalized.report.consumable_segment_count, 0);
    EXPECT_TRUE(result.process_path.segments.empty());
    EXPECT_TRUE(result.shaped_path.segments.empty());
    EXPECT_FALSE(result.topology_diagnostics.repair_applied);
    EXPECT_EQ(result.topology_diagnostics.dispense_fragment_count, 0);
}

TEST(ProcessPathContractsTest, ProcessPathContractUsesCanonicalProcessSegmentContainer) {
    ProcessPath path;
    EXPECT_TRUE(path.segments.empty());

    path.segments.emplace_back();
    EXPECT_EQ(path.segments.size(), 1u);
    EXPECT_TRUE(path.segments.front().dispense_on);
}

TEST(ProcessPathContractsTest, PathGenerationResultCanRepresentDistinctFailureStates) {
    PathGenerationResult invalid;
    invalid.status = PathGenerationStatus::InvalidInput;
    invalid.failed_stage = PathGenerationStage::InputValidation;
    invalid.error_message = "invalid";

    PathGenerationResult stage_failure;
    stage_failure.status = PathGenerationStatus::StageFailure;
    stage_failure.failed_stage = PathGenerationStage::Normalization;
    stage_failure.error_message = "failed";

    EXPECT_EQ(invalid.status, PathGenerationStatus::InvalidInput);
    EXPECT_EQ(stage_failure.status, PathGenerationStatus::StageFailure);
    EXPECT_EQ(stage_failure.failed_stage, PathGenerationStage::Normalization);
}

TEST(ProcessPathContractsTest, NormalizationReportCanExposeExplicitDiagnosticSignals) {
    Siligen::ProcessPath::Contracts::NormalizationReport report;
    report.invalid_unit_scale = true;
    report.skipped_spline_count = 2;
    report.point_primitive_count = 1;
    report.consumable_segment_count = 3;

    EXPECT_TRUE(report.invalid_unit_scale);
    EXPECT_EQ(report.skipped_spline_count, 2);
    EXPECT_EQ(report.point_primitive_count, 1);
    EXPECT_EQ(report.consumable_segment_count, 3);
}

}  // namespace
