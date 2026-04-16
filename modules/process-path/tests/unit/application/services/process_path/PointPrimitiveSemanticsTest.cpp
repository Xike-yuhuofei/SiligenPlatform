#include "application/services/process_path/ProcessPathFacade.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
using Siligen::ProcessPath::Contracts::PathGenerationStage;
using Siligen::ProcessPath::Contracts::PathGenerationStatus;
using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::ProcessPath::Contracts::TopologyRepairPolicy;
using Siligen::Shared::Types::Point2D;

TEST(PointPrimitiveSemanticsTest, MixedPointInputIsAcceptedWhenRepairPolicyIsOff) {
    ProcessPathBuildRequest request;
    request.topology_repair.policy = TopologyRepairPolicy::Off;
    request.primitives = {
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
        Primitive::MakePoint(Point2D(5.0f, 5.0f)),
    };

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_EQ(result.status, PathGenerationStatus::Success);
    EXPECT_EQ(result.failed_stage, PathGenerationStage::None);
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_EQ(result.normalized.report.point_primitive_count, 1);
    EXPECT_EQ(result.normalized.report.consumable_segment_count, 1);
    ASSERT_EQ(result.normalized.path.segments.size(), 2U);
    EXPECT_TRUE(result.normalized.path.segments.back().is_point);
    EXPECT_EQ(result.normalized.path.segments.back().line.start, Point2D(5.0f, 5.0f));
    EXPECT_EQ(result.normalized.path.segments.back().line.end, Point2D(5.0f, 5.0f));
    ASSERT_FALSE(result.process_path.segments.empty());
    EXPECT_TRUE(result.process_path.segments.back().geometry.is_point);
    EXPECT_EQ(result.process_path.segments.back().geometry.line.start, Point2D(5.0f, 5.0f));
    EXPECT_FALSE(result.topology_diagnostics.repair_requested);
}

TEST(PointPrimitiveSemanticsTest, MixedPointInputIsAcceptedWhenRepairPolicyIsAuto) {
    ProcessPathBuildRequest request;
    request.topology_repair.policy = TopologyRepairPolicy::Auto;
    request.primitives = {
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
        Primitive::MakePoint(Point2D(5.0f, 5.0f)),
    };
    request.metadata = {
        PathPrimitiveMeta{1},
        PathPrimitiveMeta{2},
    };

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_EQ(result.status, PathGenerationStatus::Success);
    EXPECT_EQ(result.failed_stage, PathGenerationStage::None);
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_EQ(result.normalized.report.point_primitive_count, 1);
    EXPECT_EQ(result.normalized.report.consumable_segment_count, 1);
    ASSERT_EQ(result.normalized.path.segments.size(), 2U);
    EXPECT_TRUE(result.normalized.path.segments.back().is_point);
    EXPECT_EQ(result.normalized.path.segments.back().line.start, Point2D(5.0f, 5.0f));
    ASSERT_FALSE(result.process_path.segments.empty());
    EXPECT_TRUE(result.process_path.segments.back().geometry.is_point);
    EXPECT_TRUE(result.topology_diagnostics.repair_requested);
}

}  // namespace
