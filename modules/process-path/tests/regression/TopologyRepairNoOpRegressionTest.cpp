#include "application/services/process_path/ProcessPathFacade.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
using Siligen::ProcessPath::Contracts::PathGenerationStatus;
using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
using Siligen::ProcessPath::Contracts::ContourElement;
using Siligen::ProcessPath::Contracts::ContourElementType;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::ProcessPath::Contracts::TopologyRepairPolicy;
using Siligen::Shared::Types::Point2D;

ProcessPathBuildRequest MakeSimpleClosedContourRequest() {
    ProcessPathBuildRequest request;
    request.normalization.continuity_tolerance = 0.1f;
    request.topology_repair.policy = TopologyRepairPolicy::Auto;
    request.topology_repair.start_position = Point2D(0.0f, 0.0f);
    request.topology_repair.split_intersections = true;
    request.topology_repair.rebuild_by_connectivity = true;
    request.topology_repair.reorder_contours = true;

    ContourElement bottom;
    bottom.type = ContourElementType::Line;
    bottom.line.start = Point2D(0.0f, 0.0f);
    bottom.line.end = Point2D(10.0f, 0.0f);
    ContourElement right;
    right.type = ContourElementType::Line;
    right.line.start = Point2D(10.0f, 0.0f);
    right.line.end = Point2D(10.0f, 10.0f);
    ContourElement top;
    top.type = ContourElementType::Line;
    top.line.start = Point2D(10.0f, 10.0f);
    top.line.end = Point2D(0.0f, 10.0f);
    ContourElement left;
    left.type = ContourElementType::Line;
    left.line.start = Point2D(0.0f, 10.0f);
    left.line.end = Point2D(0.0f, 0.0f);

    request.primitives = {
        Primitive::MakeContour({bottom, right, top, left}, true),
    };

    PathPrimitiveMeta meta;
    meta.entity_id = 100;
    meta.entity_segment_index = 0;
    meta.entity_closed = true;
    request.metadata = {meta};
    return request;
}

TEST(TopologyRepairNoOpRegressionTest, AutoPolicyDoesNotApplyRepairOnSimpleClosedContour) {
    ProcessPathFacade facade;
    const auto result = facade.Build(MakeSimpleClosedContourRequest());

    ASSERT_EQ(result.status, PathGenerationStatus::Success);
    EXPECT_TRUE(result.topology_diagnostics.repair_requested);
    EXPECT_FALSE(result.topology_diagnostics.repair_applied);
    EXPECT_EQ(result.topology_diagnostics.discontinuity_before, 0);
    EXPECT_EQ(result.topology_diagnostics.discontinuity_after, 0);
    EXPECT_EQ(result.topology_diagnostics.intersection_pairs, 0);
    EXPECT_EQ(result.topology_diagnostics.collinear_pairs, 0);
    EXPECT_EQ(result.topology_diagnostics.reordered_contours, 0);
    EXPECT_EQ(result.topology_diagnostics.reversed_contours, 0);
    EXPECT_EQ(result.topology_diagnostics.rotated_contours, 0);
}

TEST(TopologyRepairNoOpRegressionTest, AutoPolicyPreservesContinuousSingleContourDiagnostics) {
    ProcessPathFacade facade;
    const auto result = facade.Build(MakeSimpleClosedContourRequest());

    ASSERT_EQ(result.status, PathGenerationStatus::Success);
    EXPECT_TRUE(result.topology_diagnostics.metadata_valid);
    EXPECT_EQ(result.topology_diagnostics.original_primitive_count, 1);
    EXPECT_EQ(result.topology_diagnostics.repaired_primitive_count, 1);
    EXPECT_EQ(result.topology_diagnostics.contour_count, 1);
    EXPECT_FALSE(result.topology_diagnostics.fragmentation_suspected);
}

}  // namespace
