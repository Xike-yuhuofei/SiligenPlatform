#include "application/services/process_path/ProcessPathFacade.h"

#include <gtest/gtest.h>

TEST(ProcessPathFacadeTest, BuildsNormalizedAnnotatedAndShapedPath) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::Domain::Trajectory::ValueObjects::Primitive;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.normalization.continuity_tolerance = 0.1f;

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    ASSERT_EQ(result.normalized.path.segments.size(), 1u);
    ASSERT_EQ(result.process_path.segments.size(), 1u);
    ASSERT_EQ(result.shaped_path.segments.size(), 1u);
    EXPECT_TRUE(result.shaped_path.segments.front().dispense_on);
}
