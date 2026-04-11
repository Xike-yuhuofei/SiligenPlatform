#include "domain/trajectory/domain-services/TopologyRepairService.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Domain::Trajectory::DomainServices::TopologyRepairService;
using Siligen::ProcessPath::Contracts::DXFEntityType;
using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::ProcessPath::Contracts::TopologyRepairConfig;
using Siligen::ProcessPath::Contracts::TopologyRepairPolicy;
using Siligen::Shared::Types::Point2D;

bool IsImmediateLineBacktrack(const Primitive& previous, const Primitive& current) {
    if (previous.type != Siligen::ProcessPath::Contracts::PrimitiveType::Line ||
        current.type != Siligen::ProcessPath::Contracts::PrimitiveType::Line) {
        return false;
    }

    const auto prev_vec = previous.line.end - previous.line.start;
    const auto curr_vec = current.line.end - current.line.start;
    const auto prev_len = prev_vec.Length();
    const auto curr_len = curr_vec.Length();
    if (prev_len <= 1e-4f || curr_len <= 1e-4f) {
        return false;
    }

    const auto junction_gap = previous.line.end.DistanceTo(current.line.start);
    const auto normalized_cross = std::abs(prev_vec.Cross(curr_vec)) / std::max(1e-4f, prev_len * curr_len);
    const auto normalized_dot = prev_vec.Dot(curr_vec) / (prev_len * curr_len);
    return junction_gap <= 0.2f && normalized_cross <= 1e-3f && normalized_dot < -0.95f;
}

std::size_t CountImmediateLineBacktracks(const std::vector<Primitive>& primitives) {
    std::size_t count = 0;
    for (size_t index = 1; index < primitives.size(); ++index) {
        if (IsImmediateLineBacktrack(primitives[index - 1], primitives[index])) {
            count += 1;
        }
    }
    return count;
}

TEST(TopologyRepairContourOrderingRegressionTest, OpenContourDirectionFlipUpdatesReversedContourDiagnostics) {
    std::vector<Primitive> primitives{
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
        Primitive::MakeLine(Point2D(10.0f, 0.0f), Point2D(20.0f, 0.0f)),
        Primitive::MakeLine(Point2D(20.0f, 0.0f), Point2D(10.0f, 0.0f)),
    };

    std::vector<PathPrimitiveMeta> metadata(3);
    metadata[0].entity_id = 100;
    metadata[0].entity_type = DXFEntityType::Line;
    metadata[0].entity_segment_index = 0;
    metadata[1].entity_id = 100;
    metadata[1].entity_type = DXFEntityType::Line;
    metadata[1].entity_segment_index = 1;
    metadata[2].entity_id = 200;
    metadata[2].entity_type = DXFEntityType::Line;
    metadata[2].entity_segment_index = 0;

    TopologyRepairConfig config;
    config.policy = TopologyRepairPolicy::Auto;
    config.split_intersections = false;
    config.rebuild_by_connectivity = false;
    config.reorder_contours = true;
    config.start_position = Point2D(0.0f, 0.0f);
    config.two_opt_iterations = 0;

    TopologyRepairService service;
    const auto repair = service.Repair(primitives, metadata, 0.1f, config);

    ASSERT_EQ(repair.primitives.size(), primitives.size());
    EXPECT_EQ(CountImmediateLineBacktracks(repair.primitives), 0u);
    EXPECT_GT(repair.diagnostics.reversed_contours, 0);
    EXPECT_EQ(repair.primitives[2].line.start, Point2D(10.0f, 0.0f));
    EXPECT_EQ(repair.primitives[2].line.end, Point2D(20.0f, 0.0f));
}

TEST(TopologyRepairContourOrderingRegressionTest, ClosedContourRotationUpdatesRotatedContourDiagnostics) {
    std::vector<Primitive> primitives{
        Primitive::MakeLine(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)),
        Primitive::MakeLine(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)),
        Primitive::MakeLine(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)),
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
    };

    std::vector<PathPrimitiveMeta> metadata(4);
    for (int i = 0; i < 4; ++i) {
        metadata[i].entity_id = 300;
        metadata[i].entity_type = DXFEntityType::Line;
        metadata[i].entity_segment_index = i;
        metadata[i].entity_closed = true;
    }

    TopologyRepairConfig config;
    config.policy = TopologyRepairPolicy::Auto;
    config.split_intersections = false;
    config.rebuild_by_connectivity = false;
    config.reorder_contours = true;
    config.start_position = Point2D(0.0f, 0.0f);
    config.two_opt_iterations = 0;

    TopologyRepairService service;
    const auto repair = service.Repair(primitives, metadata, 0.1f, config);

    ASSERT_EQ(repair.primitives.size(), primitives.size());
    EXPECT_GT(repair.diagnostics.rotated_contours, 0);
    EXPECT_EQ(repair.primitives.front().line.start, Point2D(0.0f, 0.0f));
    EXPECT_EQ(repair.primitives.front().line.end, Point2D(10.0f, 0.0f));
}

TEST(TopologyRepairContourOrderingRegressionTest, DuplicateForwardLinesDoNotReportFalseContourReversal) {
    std::vector<Primitive> primitives{
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
    };

    std::vector<PathPrimitiveMeta> metadata(2);
    metadata[0].entity_id = 100;
    metadata[0].entity_type = DXFEntityType::Line;
    metadata[1].entity_id = 101;
    metadata[1].entity_type = DXFEntityType::Line;

    TopologyRepairConfig config;
    config.policy = TopologyRepairPolicy::Auto;
    config.reorder_contours = false;
    config.start_position = Point2D(0.0f, 0.0f);

    TopologyRepairService service;
    const auto repair = service.Repair(primitives, metadata, 0.1f, config);

    ASSERT_EQ(repair.primitives.size(), primitives.size());
    EXPECT_EQ(CountImmediateLineBacktracks(repair.primitives), 0u);
    EXPECT_EQ(repair.diagnostics.reversed_contours, 0);
}

}  // namespace
