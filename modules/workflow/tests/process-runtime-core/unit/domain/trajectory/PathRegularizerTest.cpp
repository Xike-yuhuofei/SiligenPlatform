#include "domain/trajectory/domain-services/PathRegularizer.h"

#include <gtest/gtest.h>

namespace {
using Siligen::Domain::Trajectory::DomainServices::PathRegularizer;
using Siligen::Domain::Trajectory::DomainServices::RegularizerConfig;
using Siligen::Shared::Types::DXFSegment;
using Siligen::Shared::Types::DXFSegmentType;
using Siligen::Shared::Types::Point2D;

DXFSegment MakeLine(float x0, float y0, float x1, float y1) {
    DXFSegment seg;
    seg.type = DXFSegmentType::LINE;
    seg.start_point = Point2D(x0, y0);
    seg.end_point = Point2D(x1, y1);
    seg.length = seg.start_point.DistanceTo(seg.end_point);
    return seg;
}
}

TEST(PathRegularizerTest, MergeLinePreservesOffset) {
    std::vector<DXFSegment> segments;
    segments.push_back(MakeLine(0.0f, 10.0f, 5.0f, 10.0f));
    segments.push_back(MakeLine(5.02f, 10.0f, 10.0f, 10.0f));

    RegularizerConfig config;
    config.collinear_angle_tolerance_deg = 0.5f;
    config.collinear_distance_tolerance = 0.05f;
    config.overlap_tolerance = 0.05f;
    config.gap_tolerance = 0.05f;
    config.connectivity_tolerance = 0.1f;

    PathRegularizer regularizer;
    auto result = regularizer.Regularize(segments, config);

    ASSERT_EQ(result.contours.size(), 1u);
    ASSERT_EQ(result.contours[0].segments.size(), 1u);
    const auto& merged = result.contours[0].segments[0];
    EXPECT_NEAR(merged.start_point.y, 10.0f, 1e-4f);
    EXPECT_NEAR(merged.end_point.y, 10.0f, 1e-4f);
    EXPECT_NEAR(merged.start_point.x, 0.0f, 1e-2f);
    EXPECT_NEAR(merged.end_point.x, 10.0f, 1e-2f);
}
