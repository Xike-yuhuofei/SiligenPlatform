#include "application/services/process_path/ProcessPathFacade.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
using Siligen::CoordinateAlignment::Contracts::CoordinateTransform;
using Siligen::CoordinateAlignment::Contracts::CoordinateTransformSet;
using Siligen::ProcessPath::Contracts::PathGenerationStatus;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::Shared::Types::Point2D;

ProcessPathBuildRequest MakeRequest() {
    ProcessPathBuildRequest request;
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.normalization.continuity_tolerance = 0.1f;
    return request;
}

CoordinateTransformSet MakeAlignment() {
    CoordinateTransformSet transform_set;
    transform_set.alignment_id = "ALIGN-001";
    transform_set.plan_id = "PLAN-001";
    transform_set.valid = true;
    CoordinateTransform transform;
    transform.transform_id = "ALIGN-001-primary";
    transform.source_frame = "plan";
    transform.target_frame = "machine";
    transform_set.transforms.push_back(transform);
    return transform_set;
}

TEST(AlignmentSemanticTest, AlignmentActsAsProvenanceAndDoesNotChangeGeometry) {
    ProcessPathFacade facade;

    auto baseline = MakeRequest();
    const auto baseline_result = facade.Build(baseline);

    auto with_alignment = MakeRequest();
    with_alignment.alignment = MakeAlignment();
    const auto aligned_result = facade.Build(with_alignment);

    ASSERT_EQ(baseline_result.status, PathGenerationStatus::Success);
    ASSERT_EQ(aligned_result.status, PathGenerationStatus::Success);
    ASSERT_EQ(baseline_result.normalized.path.segments.size(), aligned_result.normalized.path.segments.size());
    ASSERT_EQ(baseline_result.process_path.segments.size(), aligned_result.process_path.segments.size());
    ASSERT_EQ(baseline_result.shaped_path.segments.size(), aligned_result.shaped_path.segments.size());
    ASSERT_FALSE(aligned_result.shaped_path.segments.empty());

    const auto& baseline_segment = baseline_result.shaped_path.segments.front().geometry.line;
    const auto& aligned_segment = aligned_result.shaped_path.segments.front().geometry.line;
    EXPECT_EQ(baseline_segment.start, aligned_segment.start);
    EXPECT_EQ(baseline_segment.end, aligned_segment.end);
    ASSERT_TRUE(with_alignment.alignment.has_value());
    EXPECT_EQ(with_alignment.alignment->owner_module, "M5");
}

}  // namespace
