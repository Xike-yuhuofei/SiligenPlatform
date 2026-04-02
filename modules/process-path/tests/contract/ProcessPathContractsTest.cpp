#include "application/services/process_path/ProcessPathFacade.h"
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
using Siligen::ProcessPath::Contracts::PathGenerationRequest;
using Siligen::ProcessPath::Contracts::PathGenerationResult;
using Siligen::ProcessPath::Contracts::ProcessPath;

static_assert(std::is_same_v<ProcessPathBuildRequest, PathGenerationRequest>);
static_assert(std::is_same_v<ProcessPathBuildResult, PathGenerationResult>);
static_assert(std::is_default_constructible_v<PathGenerationRequest>);
static_assert(std::is_default_constructible_v<PathGenerationResult>);
static_assert(std::is_same_v<decltype(std::declval<const ProcessPathFacade&>().Build(
                                 std::declval<const ProcessPathBuildRequest&>())),
                             ProcessPathBuildResult>);

TEST(ProcessPathContractsTest, PathGenerationRequestDefaultsRemainOwnerNeutral) {
    const ProcessPathBuildRequest request;

    EXPECT_TRUE(request.primitives.empty());
    EXPECT_FALSE(request.alignment.has_value());
    EXPECT_FLOAT_EQ(request.normalization.unit_scale, 1.0f);
    EXPECT_FLOAT_EQ(request.normalization.continuity_tolerance, 0.1f);
    EXPECT_FLOAT_EQ(request.process.default_flow, 1.0f);
    EXPECT_TRUE(request.process.corner_slowdown);
    EXPECT_FLOAT_EQ(request.shaping.corner_smoothing_radius, 0.5f);
}

TEST(ProcessPathContractsTest, PathGenerationResultDefaultsExposeEmptyProcessPathSurfaces) {
    const ProcessPathBuildResult result;

    EXPECT_TRUE(result.normalized.path.segments.empty());
    EXPECT_FALSE(result.normalized.path.closed);
    EXPECT_EQ(result.normalized.report.discontinuity_count, 0);
    EXPECT_TRUE(result.process_path.segments.empty());
    EXPECT_TRUE(result.shaped_path.segments.empty());
}

TEST(ProcessPathContractsTest, ProcessPathContractUsesCanonicalProcessSegmentContainer) {
    ProcessPath path;
    EXPECT_TRUE(path.segments.empty());

    path.segments.emplace_back();
    EXPECT_EQ(path.segments.size(), 1u);
    EXPECT_TRUE(path.segments.front().dispense_on);
}

}  // namespace
