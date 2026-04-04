#include "domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Domain::Dispensing::DomainServices::UnifiedTrajectoryPlanRequest;
using Siligen::Domain::Dispensing::DomainServices::UnifiedTrajectoryPlannerService;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::Shared::Types::Point2D;

TEST(UnifiedTrajectoryPlannerServiceTest, BuildsContractsPathsThroughProcessPathFacade) {
    UnifiedTrajectoryPlannerService planner;

    UnifiedTrajectoryPlanRequest request;
    request.generate_motion_trajectory = false;
    request.normalization.continuity_tolerance = 0.1f;
    request.process.default_flow = 1.0f;

    const auto result = planner.Plan({
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
    }, request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().normalized.path.segments.size(), 1U);
    EXPECT_EQ(result.Value().process_path.segments.size(), 1U);
    EXPECT_EQ(result.Value().shaped_path.segments.size(), 1U);
    EXPECT_TRUE(result.Value().motion_trajectory.points.empty());
}

TEST(UnifiedTrajectoryPlannerServiceTest, RejectsEmptyPrimitiveInput) {
    UnifiedTrajectoryPlannerService planner;

    const auto result = planner.Plan({}, UnifiedTrajectoryPlanRequest{});

    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().GetMessage().find("input_validation"), std::string::npos);
}

TEST(UnifiedTrajectoryPlannerServiceTest, SurfacesSplineApproximationFailureFromProcessPathFacade) {
    UnifiedTrajectoryPlannerService planner;

    UnifiedTrajectoryPlanRequest request;
    request.generate_motion_trajectory = false;
    request.normalization.approximate_splines = false;

    const auto result = planner.Plan({
        Primitive::MakeSpline({
            Point2D(0.0f, 0.0f),
            Point2D(5.0f, 5.0f),
            Point2D(10.0f, 0.0f),
        }),
    }, request);

    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().GetMessage().find("normalization"), std::string::npos);
    EXPECT_NE(result.GetError().GetMessage().find("approximate_splines"), std::string::npos);
}

}  // namespace
