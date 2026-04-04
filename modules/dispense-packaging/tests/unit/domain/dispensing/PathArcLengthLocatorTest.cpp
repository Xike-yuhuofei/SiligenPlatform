#include "domain/dispensing/planning/domain-services/PathArcLengthLocator.h"
#include "process_path/contracts/GeometryUtils.h"

#include <gtest/gtest.h>

#include <limits>
#include <vector>

namespace {

using Siligen::Domain::Dispensing::DomainServices::PathArcLengthLocator;
using Siligen::ProcessPath::Contracts::ArcPoint;
using Siligen::ProcessPath::Contracts::ComputeArcLength;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::ProcessPath::Contracts::ProcessSegment;
using Siligen::ProcessPath::Contracts::Segment;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::Shared::Types::Point2D;

ProcessSegment BuildArcSegment(
    const Point2D& center,
    float radius,
    float start_angle_deg,
    float end_angle_deg,
    bool clockwise = false) {
    Segment segment;
    segment.type = SegmentType::Arc;
    segment.arc.center = center;
    segment.arc.radius = radius;
    segment.arc.start_angle_deg = start_angle_deg;
    segment.arc.end_angle_deg = end_angle_deg;
    segment.arc.clockwise = clockwise;
    segment.length = ComputeArcLength(segment.arc);

    ProcessSegment process_segment;
    process_segment.geometry = segment;
    process_segment.dispense_on = true;
    return process_segment;
}

ProcessSegment BuildSplineSegment(
    const std::vector<Point2D>& control_points,
    bool closed = false) {
    Segment segment;
    segment.type = SegmentType::Spline;
    segment.spline.control_points = control_points;
    segment.spline.closed = closed;

    ProcessSegment process_segment;
    process_segment.geometry = segment;
    process_segment.dispense_on = true;
    return process_segment;
}

}  // namespace

TEST(PathArcLengthLocatorTest, LocatesArcMidpointOnVisiblePath) {
    PathArcLengthLocator locator;
    ProcessPath path;
    path.segments.push_back(BuildArcSegment(Point2D(0.0f, 0.0f), 10.0f, 0.0f, 90.0f));

    const auto result = locator.Locate(path, ComputeArcLength(path.segments.front().geometry.arc) * 0.5f, 0.05f, 1.0f);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto expected = ArcPoint(path.segments.front().geometry.arc, 45.0f);
    EXPECT_NEAR(result.Value().position.x, expected.x, 1e-3f);
    EXPECT_NEAR(result.Value().position.y, expected.y, 1e-3f);
    EXPECT_EQ(result.Value().segment_index, 0U);
}

TEST(PathArcLengthLocatorTest, RejectsDegenerateSplineWhenFlattenedPathIsUnavailable) {
    PathArcLengthLocator locator;
    Segment segment;
    segment.type = SegmentType::Spline;
    segment.spline.control_points = {
        Point2D(5.0f, 5.0f),
        Point2D(5.0f, 5.0f),
        Point2D(5.0f, 5.0f),
    };

    const auto result = locator.LocateOnSegment(segment, 0.0f, 0.05f, 1.0f);

    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().GetMessage().find("spline"), std::string::npos);
}

TEST(PathArcLengthLocatorTest, RejectsNonFiniteSplineControlPoints) {
    PathArcLengthLocator locator;
    ProcessPath path;
    path.segments.push_back(BuildSplineSegment({
        Point2D(0.0f, 0.0f),
        Point2D(std::numeric_limits<float>::infinity(), 5.0f),
        Point2D(10.0f, 0.0f),
    }));

    const auto result = locator.Locate(path, 2.0f, 0.05f, 1.0f);

    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().GetMessage().find("finite"), std::string::npos);
}
