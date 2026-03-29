#include "application/services/process_path/ProcessPathFacade.h"

#include <gtest/gtest.h>

TEST(ProcessPathFacadeTest, BuildsNormalizedAnnotatedAndShapedPath) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::CoordinateAlignment::Contracts::CoordinateTransform;
    using Siligen::CoordinateAlignment::Contracts::CoordinateTransformSet;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.normalization.continuity_tolerance = 0.1f;
    CoordinateTransformSet transform_set;
    transform_set.alignment_id = "ALIGN-001";
    transform_set.plan_id = "PLAN-001";
    transform_set.valid = true;
    CoordinateTransform transform;
    transform.transform_id = "ALIGN-001-primary";
    transform.source_frame = "plan";
    transform.target_frame = "machine";
    transform_set.transforms.push_back(transform);
    request.alignment = transform_set;

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    ASSERT_EQ(result.normalized.path.segments.size(), 1u);
    ASSERT_EQ(result.process_path.segments.size(), 1u);
    ASSERT_EQ(result.shaped_path.segments.size(), 1u);
    EXPECT_TRUE(result.shaped_path.segments.front().dispense_on);
    ASSERT_TRUE(request.alignment.has_value());
    EXPECT_EQ(request.alignment->owner_module, "M5");
}
