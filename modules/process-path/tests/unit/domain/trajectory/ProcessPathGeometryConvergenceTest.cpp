#include "domain/trajectory/domain-services/GeometryNormalizer.h"
#include "domain/trajectory/domain-services/TopologyRepairService.h"
#include "process_path/contracts/GeometryUtils.h"
#include "shared/geometry/BoostGeometryAdapter.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using Siligen::Domain::Trajectory::DomainServices::GeometryNormalizer;
using Siligen::Domain::Trajectory::DomainServices::TopologyRepairService;
using Siligen::ProcessPath::Contracts::ArcPrimitive;
using Siligen::ProcessPath::Contracts::GeometryTolerance;
using Siligen::ProcessPath::Contracts::IsAngleOnArc;
using Siligen::ProcessPath::Contracts::IsDegenerateLine;
using Siligen::ProcessPath::Contracts::IsPointOnSegment;
using Siligen::ProcessPath::Contracts::NormalizationConfig;
using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::ProcessPath::Contracts::TopologyRepairConfig;
using Siligen::ProcessPath::Contracts::TopologyRepairPolicy;
using Siligen::Shared::Types::Point2D;

struct BruteForceTopologyExpectation {
    int intersection_pairs = 0;
    int collinear_pairs = 0;
    int split_segment_count = 0;
};

TopologyRepairConfig SplitOnlyRepairConfig() {
    TopologyRepairConfig config;
    config.policy = TopologyRepairPolicy::Auto;
    config.split_intersections = true;
    config.rebuild_by_connectivity = false;
    config.reorder_contours = false;
    return config;
}

std::vector<PathPrimitiveMeta> MakeMetadata(const std::vector<Primitive>& primitives) {
    std::vector<PathPrimitiveMeta> metadata;
    metadata.reserve(primitives.size());
    for (size_t index = 0; index < primitives.size(); ++index) {
        metadata.push_back(PathPrimitiveMeta{static_cast<int>(index + 1U)});
    }
    return metadata;
}

BruteForceTopologyExpectation BuildBruteForceTopologyExpectation(
    const std::vector<Primitive>& primitives,
    const float tolerance) {
    BruteForceTopologyExpectation expectation;
    if (primitives.size() < 2U) {
        expectation.split_segment_count = static_cast<int>(primitives.size());
        return expectation;
    }

    std::vector<std::vector<Point2D>> split_points(primitives.size());
    for (size_t index = 0; index < primitives.size(); ++index) {
        split_points[index].push_back(primitives[index].line.start);
        split_points[index].push_back(primitives[index].line.end);
    }

    for (size_t i = 0; i < primitives.size(); ++i) {
        const auto& a = primitives[i].line.start;
        const auto& b = primitives[i].line.end;
        for (size_t j = i + 1U; j < primitives.size(); ++j) {
            const auto& c = primitives[j].line.start;
            const auto& d = primitives[j].line.end;

            bool collinear = false;
            Point2D intersection;
            if (Siligen::Shared::Geometry::ComputeSegmentIntersection(
                    a, b, c, d, tolerance, intersection, collinear)) {
                split_points[i].push_back(intersection);
                split_points[j].push_back(intersection);
                expectation.intersection_pairs += 1;
                continue;
            }

            if (!collinear) {
                continue;
            }

            bool added = false;
            if (Siligen::Shared::Geometry::IsPointOnSegment(c, a, b, tolerance)) {
                split_points[i].push_back(c);
                added = true;
            }
            if (Siligen::Shared::Geometry::IsPointOnSegment(d, a, b, tolerance)) {
                split_points[i].push_back(d);
                added = true;
            }
            if (Siligen::Shared::Geometry::IsPointOnSegment(a, c, d, tolerance)) {
                split_points[j].push_back(a);
                added = true;
            }
            if (Siligen::Shared::Geometry::IsPointOnSegment(b, c, d, tolerance)) {
                split_points[j].push_back(b);
                added = true;
            }
            if (added) {
                expectation.collinear_pairs += 1;
            }
        }
    }

    const float min_segment_length = std::max(1e-4f, tolerance * 0.5f);
    for (size_t index = 0; index < primitives.size(); ++index) {
        const Point2D start = primitives[index].line.start;
        const Point2D end = primitives[index].line.end;
        const Point2D direction = end - start;
        const float len2 = direction.Dot(direction);
        if (len2 <= Siligen::ProcessPath::Contracts::kGeometryEpsilon) {
            continue;
        }

        std::vector<std::pair<float, Point2D>> ordered;
        ordered.reserve(split_points[index].size());
        for (const auto& point : split_points[index]) {
            ordered.emplace_back((point - start).Dot(direction) / len2, point);
        }
        std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.first < rhs.first;
        });

        std::vector<Point2D> unique_points;
        unique_points.reserve(ordered.size());
        for (const auto& item : ordered) {
            if (unique_points.empty() ||
                Siligen::Shared::Geometry::Distance(item.second, unique_points.back()) > tolerance) {
                unique_points.push_back(item.second);
            }
        }

        for (size_t point_index = 1U; point_index < unique_points.size(); ++point_index) {
            if (Siligen::Shared::Geometry::Distance(
                    unique_points[point_index - 1U], unique_points[point_index]) > min_segment_length) {
                expectation.split_segment_count += 1;
            }
        }
    }
    return expectation;
}

void AssertMatchesBruteForceOracle(const std::vector<Primitive>& primitives, const float tolerance) {
    TopologyRepairService repair_service;
    const auto result = repair_service.Repair(
        primitives, MakeMetadata(primitives), tolerance, SplitOnlyRepairConfig());
    const auto expectation = BuildBruteForceTopologyExpectation(primitives, tolerance);

    EXPECT_EQ(result.diagnostics.intersection_pairs, expectation.intersection_pairs);
    EXPECT_EQ(result.diagnostics.collinear_pairs, expectation.collinear_pairs);
    EXPECT_EQ(result.diagnostics.split_segment_count, expectation.split_segment_count);
    EXPECT_EQ(result.diagnostics.intersection_tested_pairs,
              result.diagnostics.intersection_candidate_pairs);
}

std::vector<Primitive> BuildSparseLineGrid(const int line_count) {
    std::vector<Primitive> primitives;
    primitives.reserve(static_cast<size_t>(line_count));
    for (int index = 0; index < line_count; ++index) {
        const float y = static_cast<float>(index) * 10.0f;
        primitives.push_back(Primitive::MakeLine(Point2D(0.0f, y), Point2D(1.0f, y)));
    }
    return primitives;
}

void RunTopologyScaleCase(const std::string& label, const int line_count) {
    const auto primitives = BuildSparseLineGrid(line_count);
    const auto metadata = MakeMetadata(primitives);
    const std::int64_t all_pairs =
        (static_cast<std::int64_t>(line_count) * static_cast<std::int64_t>(line_count - 1)) / 2;

    TopologyRepairService repair_service;
    const auto start = std::chrono::steady_clock::now();
    const auto result = repair_service.Repair(primitives, metadata, 0.1f, SplitOnlyRepairConfig());
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    std::cout << "process_path_topology_scale"
              << " label=" << label
              << " line_count=" << line_count
              << " elapsed_ms=" << elapsed_ms
              << " all_pairs=" << all_pairs
              << " candidate_pairs=" << result.diagnostics.intersection_candidate_pairs
              << " tested_pairs=" << result.diagnostics.intersection_tested_pairs
              << " intersection_pairs=" << result.diagnostics.intersection_pairs
              << " collinear_pairs=" << result.diagnostics.collinear_pairs
              << " split_segment_count=" << result.diagnostics.split_segment_count
              << std::endl;

    EXPECT_EQ(result.diagnostics.intersection_pairs, 0);
    EXPECT_EQ(result.diagnostics.collinear_pairs, 0);
    EXPECT_EQ(result.diagnostics.split_segment_count, line_count);
    EXPECT_EQ(result.diagnostics.intersection_candidate_pairs, 0);
    EXPECT_EQ(result.diagnostics.intersection_tested_pairs, 0);
    EXPECT_LT(static_cast<std::int64_t>(result.diagnostics.intersection_candidate_pairs), all_pairs);
}

}  // namespace

TEST(ProcessPathGeometryConvergenceTest, GeometryPredicatesUseExplicitToleranceSemantics) {
    EXPECT_TRUE(IsDegenerateLine(
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(0.0001f, 0.0f)).line,
        0.001f));
    EXPECT_TRUE(IsPointOnSegment(
        Point2D(5.0f, 0.0005f),
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        0.001f));
    EXPECT_TRUE(IsPointOnSegment(
        Point2D(100000.0f, 0.0005f),
        Point2D(0.0f, 0.0f),
        Point2D(200000.0f, 0.0f),
        0.001f));
    EXPECT_TRUE(IsPointOnSegment(
        Point2D(10.0005f, 0.0f),
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        0.001f));
    EXPECT_FALSE(IsPointOnSegment(
        Point2D(10.002f, 0.0f),
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        0.001f));
    EXPECT_FALSE(IsPointOnSegment(
        Point2D(10.002f, 0.0005f),
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        0.001f));
    EXPECT_TRUE(IsPointOnSegment(
        Point2D(1.0005f, 1.0005f),
        Point2D(1.0f, 1.0f),
        Point2D(1.0001f, 1.0f),
        0.001f));
    EXPECT_FALSE(IsPointOnSegment(
        Point2D(1.002f, 1.0f),
        Point2D(1.0f, 1.0f),
        Point2D(1.0001f, 1.0f),
        0.001f));

    ArcPrimitive arc;
    arc.center = Point2D(0.0f, 0.0f);
    arc.radius = 10.0f;
    arc.start_angle_deg = 0.0f;
    arc.end_angle_deg = 90.0f;
    arc.clockwise = false;
    EXPECT_TRUE(IsAngleOnArc(arc, 45.0f, GeometryTolerance{}.angular_deg));
    EXPECT_FALSE(IsAngleOnArc(arc, 180.0f, GeometryTolerance{}.angular_deg));
}

TEST(ProcessPathGeometryConvergenceTest, EllipseFlatteningHonorsErrorBound) {
    Primitive ellipse = Primitive::MakeEllipse(
        Point2D(0.0f, 0.0f),
        Point2D(20.0f, 0.0f),
        0.5f);

    NormalizationConfig loose;
    loose.curve_flatten_max_step_mm = 10.0f;
    loose.curve_flatten_max_error_mm = 1.0f;

    NormalizationConfig strict = loose;
    strict.curve_flatten_max_error_mm = 0.01f;

    GeometryNormalizer normalizer;
    const auto loose_result = normalizer.Normalize({ellipse}, loose);
    const auto strict_result = normalizer.Normalize({ellipse}, strict);

    ASSERT_FALSE(loose_result.path.segments.empty());
    ASSERT_FALSE(strict_result.path.segments.empty());
    EXPECT_GT(strict_result.path.segments.size(), loose_result.path.segments.size());
    EXPECT_EQ(strict_result.report.consumable_segment_count,
              static_cast<int>(strict_result.path.segments.size()));
}

TEST(ProcessPathGeometryConvergenceTest, TopologyIntersectionRepairPublishesBroadPhaseDiagnostics) {
    std::vector<Primitive> primitives;
    for (int i = 0; i < 20; ++i) {
        const float y = static_cast<float>(i * 10);
        primitives.push_back(Primitive::MakeLine(Point2D(0.0f, y), Point2D(10.0f, y)));
    }
    primitives.push_back(Primitive::MakeLine(Point2D(5.0f, -1.0f), Point2D(5.0f, 191.0f)));

    TopologyRepairService repair_service;
    const auto result = repair_service.Repair(
        primitives, MakeMetadata(primitives), 0.1f, SplitOnlyRepairConfig());

    const int all_pairs = static_cast<int>((primitives.size() * (primitives.size() - 1U)) / 2U);
    const auto expectation = BuildBruteForceTopologyExpectation(primitives, 0.1f);
    EXPECT_TRUE(result.diagnostics.repair_requested);
    EXPECT_TRUE(result.diagnostics.repair_applied);
    EXPECT_EQ(result.diagnostics.intersection_pairs, expectation.intersection_pairs);
    EXPECT_EQ(result.diagnostics.collinear_pairs, expectation.collinear_pairs);
    EXPECT_EQ(result.diagnostics.split_segment_count, expectation.split_segment_count);
    EXPECT_GT(result.diagnostics.intersection_candidate_pairs, 0);
    EXPECT_EQ(result.diagnostics.intersection_tested_pairs,
              result.diagnostics.intersection_candidate_pairs);
    EXPECT_LT(result.diagnostics.intersection_candidate_pairs, all_pairs);
}

TEST(ProcessPathGeometryConvergenceTest, TopologyBroadPhaseMatchesBruteForceOracleAcrossIntersectionShapes) {
    std::vector<Primitive> primitives;
    primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    primitives.push_back(Primitive::MakeLine(Point2D(5.0f, -5.0f), Point2D(5.0f, 5.0f)));
    primitives.push_back(Primitive::MakeLine(Point2D(10.0f, 0.0f), Point2D(14.0f, 4.0f)));
    primitives.push_back(Primitive::MakeLine(Point2D(2.0f, 0.0f), Point2D(8.0f, 0.0f)));
    primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 10.0f), Point2D(10.0f, 10.0f)));
    primitives.push_back(Primitive::MakeLine(Point2D(5.0f, 10.05f), Point2D(15.0f, 10.05f)));

    for (int i = 0; i < 12; ++i) {
        const float base = 100.0f + static_cast<float>(i) * 8.0f;
        primitives.push_back(Primitive::MakeLine(Point2D(base, 0.0f), Point2D(base + 2.0f, 0.0f)));
    }
    for (int i = 0; i < 8; ++i) {
        const float base = 300.0f + static_cast<float>(i) * 3.0f;
        primitives.push_back(Primitive::MakeLine(Point2D(base, 0.0f), Point2D(base + 2.0f, 2.0f)));
        primitives.push_back(Primitive::MakeLine(Point2D(base, 2.0f), Point2D(base + 2.0f, 0.0f)));
    }

    AssertMatchesBruteForceOracle(primitives, 0.1f);
}

TEST(ProcessPathGeometryConvergenceTest, TopologyBroadPhaseMatchesBruteForceOracleForLongLineAcrossGrid) {
    std::vector<Primitive> primitives;
    for (int i = 0; i < 30; ++i) {
        const float y = static_cast<float>(i) * 5.0f;
        primitives.push_back(Primitive::MakeLine(Point2D(0.0f, y), Point2D(3.0f, y)));
    }
    primitives.push_back(Primitive::MakeLine(Point2D(1.5f, -2.0f), Point2D(1.5f, 147.0f)));
    primitives.push_back(Primitive::MakeLine(Point2D(500.0f, 0.0f), Point2D(700.0f, 0.0f)));

    TopologyRepairService repair_service;
    const auto result = repair_service.Repair(
        primitives, MakeMetadata(primitives), 0.1f, SplitOnlyRepairConfig());
    const auto expectation = BuildBruteForceTopologyExpectation(primitives, 0.1f);
    const std::int64_t all_pairs =
        (static_cast<std::int64_t>(primitives.size()) *
         static_cast<std::int64_t>(primitives.size() - 1U)) / 2;

    EXPECT_EQ(result.diagnostics.intersection_pairs, expectation.intersection_pairs);
    EXPECT_EQ(result.diagnostics.collinear_pairs, expectation.collinear_pairs);
    EXPECT_EQ(result.diagnostics.split_segment_count, expectation.split_segment_count);
    EXPECT_LT(static_cast<std::int64_t>(result.diagnostics.intersection_candidate_pairs), all_pairs);
}

TEST(ProcessPathGeometryConvergenceTest, TopologyBroadPhaseScaleEvidenceForRegularLane) {
    RunTopologyScaleCase("1k", 1000);
    RunTopologyScaleCase("10k", 10000);
}

TEST(ProcessPathGeometryConvergenceTest, DISABLED_TopologyBroadPhaseScaleEvidenceForExplicit100kLane) {
    RunTopologyScaleCase("100k", 100000);
}
