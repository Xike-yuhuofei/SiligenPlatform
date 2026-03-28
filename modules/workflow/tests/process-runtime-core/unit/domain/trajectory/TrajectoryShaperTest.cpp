#include "domain/trajectory/domain-services/TrajectoryShaper.h"
#include "domain/trajectory/value-objects/GeometryUtils.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Domain::Trajectory::DomainServices::TrajectoryShaper;
using Siligen::Domain::Trajectory::DomainServices::TrajectoryShaperConfig;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcSweep;
using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;
using Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
using Siligen::Domain::Trajectory::ValueObjects::ProcessTag;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::Shared::Types::Point2D;

ProcessSegment MakeDispenseLine(float x0, float y0, float x1, float y1) {
    ProcessSegment segment;
    segment.geometry.type = SegmentType::Line;
    segment.geometry.line.start = Point2D(x0, y0);
    segment.geometry.line.end = Point2D(x1, y1);
    segment.geometry.length = segment.geometry.line.start.DistanceTo(segment.geometry.line.end);
    segment.dispense_on = true;
    segment.tag = ProcessTag::Normal;
    return segment;
}

float TotalLength(const ProcessPath& path) {
    float total = 0.0f;
    for (const auto& segment : path.segments) {
        total += segment.geometry.length;
    }
    return total;
}

}  // namespace

TEST(TrajectoryShaperTest, LineLineBlendUsesInnerMinorArc) {
    ProcessPath input;
    input.segments.push_back(MakeDispenseLine(0.0f, 0.0f, 100.0f, 0.0f));
    input.segments.push_back(MakeDispenseLine(100.0f, 0.0f, 100.0f, 100.0f));

    TrajectoryShaper shaper;
    TrajectoryShaperConfig config;
    const auto output = shaper.Shape(input, config);

    ASSERT_EQ(output.segments.size(), 3U);
    const auto& blended_prev = output.segments[0];
    const auto& blend_arc = output.segments[1];
    const auto& blended_next = output.segments[2];

    ASSERT_EQ(blend_arc.geometry.type, SegmentType::Arc);
    EXPECT_EQ(blend_arc.tag, ProcessTag::Corner);

    EXPECT_LT(blend_arc.geometry.arc.center.x, 100.0f);
    EXPECT_GT(blend_arc.geometry.arc.center.y, 0.0f);
    EXPECT_NEAR(blend_arc.geometry.arc.center.x + blend_arc.geometry.arc.radius, 100.0f, 1e-3f);
    EXPECT_NEAR(blend_arc.geometry.arc.center.y - blend_arc.geometry.arc.radius, 0.0f, 1e-3f);

    const float sweep_deg = ComputeArcSweep(
        blend_arc.geometry.arc.start_angle_deg,
        blend_arc.geometry.arc.end_angle_deg,
        blend_arc.geometry.arc.clockwise);
    EXPECT_LT(sweep_deg, 180.0f);
    EXPECT_NEAR(sweep_deg, 90.0f, 1e-3f);

    EXPECT_LT(blended_prev.geometry.length + blend_arc.geometry.length + blended_next.geometry.length, 200.0f);
    EXPECT_LT(TotalLength(output), TotalLength(input));
}
