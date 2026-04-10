#include "application/services/process_path/ProcessPathFacade.h"
#include "support/ProcessPathTestSupport.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
using Siligen::ProcessPath::Contracts::PathGenerationStage;
using Siligen::ProcessPath::Contracts::PathGenerationStatus;
using Siligen::ProcessPath::Contracts::TopologyRepairPolicy;
using Siligen::Shared::Types::Point2D;
using Siligen::ProcessPath::Tests::Support::EngineeringFixtureCasePath;
using Siligen::ProcessPath::Tests::Support::LoadFixtureRequest;

int CountRapidSegments(const Siligen::ProcessPath::Contracts::ProcessPath& path) {
    int count = 0;
    for (const auto& segment : path.segments) {
        if (!segment.dispense_on) {
            count += 1;
        }
    }
    return count;
}

int CountDispenseFragments(const Siligen::ProcessPath::Contracts::ProcessPath& path) {
    int count = 0;
    bool previous_dispense = false;
    bool has_previous = false;
    for (const auto& segment : path.segments) {
        if (segment.dispense_on && (!has_previous || !previous_dispense)) {
            count += 1;
        }
        previous_dispense = segment.dispense_on;
        has_previous = true;
    }
    return count;
}

Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest LoadRepairFixtureRequest(
    const char* case_name,
    const TopologyRepairPolicy policy) {
    auto request = LoadFixtureRequest(EngineeringFixtureCasePath(case_name, std::string(case_name) + ".pb"));
    request.normalization.continuity_tolerance = 0.1f;
    request.process.rapid_gap_threshold = 0.1f;
    request.topology_repair.policy = policy;
    request.topology_repair.start_position = Point2D(0.0f, 0.0f);
    return request;
}

TEST(PbFixtureRegressionTest, RectDiagFixtureBuildsShapedPathWithoutExternalWorkspaceDependency) {
    auto request = LoadFixtureRequest(EngineeringFixtureCasePath("rect_diag", "rect_diag.pb"));
    request.normalization.continuity_tolerance = 0.1f;
    request.topology_repair.policy = TopologyRepairPolicy::Auto;

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    ASSERT_EQ(result.status, PathGenerationStatus::Success);
    ASSERT_FALSE(result.shaped_path.segments.empty());
    EXPECT_TRUE(result.topology_diagnostics.metadata_valid);
    EXPECT_EQ(result.topology_diagnostics.original_primitive_count, static_cast<int>(request.primitives.size()));
    EXPECT_EQ(result.topology_diagnostics.rapid_segment_count, CountRapidSegments(result.shaped_path));
    EXPECT_EQ(result.topology_diagnostics.dispense_fragment_count, CountDispenseFragments(result.shaped_path));
}

TEST(PbFixtureRegressionTest, PointMixedFixtureIsRejectedUnderOffAndAutoPolicies) {
    auto off_request = LoadFixtureRequest(EngineeringFixtureCasePath("point_mixed", "point_mixed.pb"));
    off_request.topology_repair.policy = TopologyRepairPolicy::Off;

    auto auto_request = LoadFixtureRequest(EngineeringFixtureCasePath("point_mixed", "point_mixed.pb"));
    auto_request.topology_repair.policy = TopologyRepairPolicy::Auto;

    ProcessPathFacade facade;
    const auto off_result = facade.Build(off_request);
    const auto auto_result = facade.Build(auto_request);

    ASSERT_EQ(off_result.status, PathGenerationStatus::InvalidInput);
    ASSERT_EQ(auto_result.status, PathGenerationStatus::InvalidInput);
    EXPECT_EQ(off_result.failed_stage, PathGenerationStage::InputValidation);
    EXPECT_EQ(auto_result.failed_stage, PathGenerationStage::InputValidation);
    EXPECT_EQ(off_result.error_message, "point primitive is not a supported live process-path input");
    EXPECT_EQ(auto_result.error_message, off_result.error_message);
    EXPECT_FALSE(off_result.topology_diagnostics.repair_requested);
    EXPECT_FALSE(auto_result.topology_diagnostics.repair_requested);
}

TEST(PbFixtureRegressionTest, FragmentedChainFixtureDifferentiatesOffAndAutoRepairSemantics) {
    ProcessPathFacade facade;
    const auto off_result = facade.Build(LoadRepairFixtureRequest("fragmented_chain", TopologyRepairPolicy::Off));
    const auto auto_result = facade.Build(LoadRepairFixtureRequest("fragmented_chain", TopologyRepairPolicy::Auto));

    ASSERT_EQ(off_result.status, PathGenerationStatus::Success);
    ASSERT_EQ(auto_result.status, PathGenerationStatus::Success);
    EXPECT_FALSE(off_result.topology_diagnostics.repair_requested);
    EXPECT_TRUE(auto_result.topology_diagnostics.repair_requested);
    EXPECT_FALSE(off_result.topology_diagnostics.repair_applied);
    EXPECT_TRUE(auto_result.topology_diagnostics.repair_applied);
    EXPECT_GT(auto_result.topology_diagnostics.discontinuity_before,
              auto_result.topology_diagnostics.discontinuity_after);
    EXPECT_GT(off_result.topology_diagnostics.dispense_fragment_count,
              auto_result.topology_diagnostics.dispense_fragment_count);
    EXPECT_EQ(off_result.topology_diagnostics.rapid_segment_count, CountRapidSegments(off_result.shaped_path));
    EXPECT_EQ(auto_result.topology_diagnostics.rapid_segment_count, CountRapidSegments(auto_result.shaped_path));
    EXPECT_EQ(off_result.topology_diagnostics.dispense_fragment_count, CountDispenseFragments(off_result.shaped_path));
    EXPECT_EQ(auto_result.topology_diagnostics.dispense_fragment_count, CountDispenseFragments(auto_result.shaped_path));
}

TEST(PbFixtureRegressionTest, MultiContourReorderFixtureSurfacesContourOrderingDiagnosticsOnlyUnderAutoPolicy) {
    ProcessPathFacade facade;
    const auto off_result = facade.Build(LoadRepairFixtureRequest("multi_contour_reorder", TopologyRepairPolicy::Off));
    auto auto_request = LoadRepairFixtureRequest("multi_contour_reorder", TopologyRepairPolicy::Auto);
    auto_request.topology_repair.split_intersections = false;
    auto_request.topology_repair.rebuild_by_connectivity = false;
    auto_request.topology_repair.reorder_contours = true;
    const auto auto_result = facade.Build(auto_request);

    ASSERT_EQ(off_result.status, PathGenerationStatus::Success);
    ASSERT_EQ(auto_result.status, PathGenerationStatus::Success);
    EXPECT_FALSE(off_result.topology_diagnostics.repair_requested);
    EXPECT_TRUE(auto_result.topology_diagnostics.repair_requested);
    EXPECT_FALSE(off_result.topology_diagnostics.repair_applied);
    EXPECT_TRUE(auto_result.topology_diagnostics.repair_applied);
    EXPECT_EQ(off_result.topology_diagnostics.reordered_contours, 0);
    EXPECT_EQ(off_result.topology_diagnostics.reversed_contours, 0);
    EXPECT_EQ(off_result.topology_diagnostics.rotated_contours, 0);
    EXPECT_GT(auto_result.topology_diagnostics.reordered_contours +
                  auto_result.topology_diagnostics.reversed_contours +
                  auto_result.topology_diagnostics.rotated_contours,
              0);
    EXPECT_EQ(auto_result.topology_diagnostics.rapid_segment_count, CountRapidSegments(auto_result.shaped_path));
    EXPECT_EQ(auto_result.topology_diagnostics.dispense_fragment_count, CountDispenseFragments(auto_result.shaped_path));
}

TEST(PbFixtureRegressionTest, IntersectionSplitFixtureIncreasesRepairedPrimitiveCountOnlyUnderAutoPolicy) {
    auto off_request = LoadRepairFixtureRequest("intersection_split", TopologyRepairPolicy::Off);
    auto auto_request = LoadRepairFixtureRequest("intersection_split", TopologyRepairPolicy::Auto);
    auto_request.topology_repair.split_intersections = true;
    auto_request.topology_repair.rebuild_by_connectivity = true;

    ProcessPathFacade facade;
    const auto off_result = facade.Build(off_request);
    const auto auto_result = facade.Build(auto_request);

    ASSERT_EQ(off_result.status, PathGenerationStatus::Success);
    ASSERT_EQ(auto_result.status, PathGenerationStatus::Success);
    EXPECT_FALSE(off_result.topology_diagnostics.repair_requested);
    EXPECT_TRUE(auto_result.topology_diagnostics.repair_requested);
    EXPECT_FALSE(off_result.topology_diagnostics.repair_applied);
    EXPECT_TRUE(auto_result.topology_diagnostics.repair_applied);
    EXPECT_EQ(off_result.topology_diagnostics.repaired_primitive_count,
              static_cast<int>(off_request.primitives.size()));
    EXPECT_GT(auto_result.topology_diagnostics.intersection_pairs, 0);
    EXPECT_GT(auto_result.topology_diagnostics.repaired_primitive_count,
              auto_result.topology_diagnostics.original_primitive_count);
    EXPECT_EQ(auto_result.topology_diagnostics.rapid_segment_count, CountRapidSegments(auto_result.shaped_path));
    EXPECT_EQ(auto_result.topology_diagnostics.dispense_fragment_count, CountDispenseFragments(auto_result.shaped_path));
}

}  // namespace
