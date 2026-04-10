#include "application/services/process_path/ProcessPathFacade.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
using Siligen::ProcessPath::Contracts::PathGenerationStatus;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::ProcessPath::Contracts::TopologyRepairPolicy;
using Siligen::Shared::Types::Point2D;

ProcessPathBuildRequest MakeFragmentedRequest(const TopologyRepairPolicy policy) {
    ProcessPathBuildRequest request;
    request.normalization.continuity_tolerance = 0.1f;
    request.process.rapid_gap_threshold = 0.1f;
    request.topology_repair.policy = policy;
    request.topology_repair.start_position = Point2D(0.0f, 0.0f);
    request.primitives = {
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
        Primitive::MakeLine(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)),
        Primitive::MakeLine(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)),
        Primitive::MakeLine(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)),
    };
    request.metadata = {
        PathPrimitiveMeta{1},
        PathPrimitiveMeta{2},
        PathPrimitiveMeta{3},
        PathPrimitiveMeta{4},
    };
    return request;
}

TEST(RepairPolicyRegressionTest, AutoPolicyReachesMainPathAndReducesFragmentation) {
    ProcessPathFacade facade;
    const auto off_result = facade.Build(MakeFragmentedRequest(TopologyRepairPolicy::Off));
    const auto auto_result = facade.Build(MakeFragmentedRequest(TopologyRepairPolicy::Auto));

    ASSERT_EQ(off_result.status, PathGenerationStatus::Success);
    ASSERT_EQ(auto_result.status, PathGenerationStatus::Success);
    EXPECT_FALSE(off_result.topology_diagnostics.repair_requested);
    EXPECT_TRUE(auto_result.topology_diagnostics.repair_requested);
    EXPECT_FALSE(off_result.topology_diagnostics.repair_applied);
    EXPECT_TRUE(auto_result.topology_diagnostics.repair_applied);
    EXPECT_GT(auto_result.topology_diagnostics.discontinuity_before,
              auto_result.topology_diagnostics.discontinuity_after);
    EXPECT_GT(off_result.topology_diagnostics.dispense_fragment_count, auto_result.topology_diagnostics.dispense_fragment_count);
}

}  // namespace
