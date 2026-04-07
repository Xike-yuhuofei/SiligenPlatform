#include "domain/dispensing/planning/domain-services/ContourOptimizationService.h"
#include "process_path/contracts/Primitive.h"

#include <gtest/gtest.h>

#include <cmath>

namespace {
using Siligen::Domain::Dispensing::DomainServices::ContourOptimizationService;
using Siligen::Domain::Dispensing::DomainServices::ContourOptimizationStats;
using Siligen::Domain::Trajectory::Ports::PathPrimitiveMeta;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::Shared::Types::DXFEntityType;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::float32;

Primitive MakeLine(float x1, float y1, float x2, float y2) {
    return Primitive::MakeLine(Point2D(x1, y1), Point2D(x2, y2));
}

Primitive MakeCircle(float cx, float cy, float r, float start_angle = 0.0f) {
    return Primitive::MakeCircle(Point2D(cx, cy), r, start_angle, false);
}

Primitive MakeEllipse(float cx, float cy, float ax, float ay, float ratio, float start_param, float end_param) {
    return Primitive::MakeEllipse(Point2D(cx, cy), Point2D(ax, ay), ratio, start_param, end_param, false);
}

PathPrimitiveMeta MakeMeta(int entity_id, int segment_index, bool closed) {
    PathPrimitiveMeta meta;
    meta.entity_id = entity_id;
    meta.entity_segment_index = segment_index;
    meta.entity_closed = closed;
    meta.entity_type = DXFEntityType::Unknown;
    return meta;
}

constexpr float32 kPi = 3.14159265359f;
}  // namespace

TEST(ContourPathOptimizerTest, EmptyInputReturnsEmpty) {
    std::vector<Primitive> primitives;
    std::vector<PathPrimitiveMeta> metadata;

    ContourOptimizationStats stats;
    auto optimized = ContourOptimizationService::Optimize(primitives, metadata, Point2D(0.0f, 0.0f), true, 0, &stats);

    EXPECT_TRUE(optimized.empty());
    EXPECT_FALSE(stats.applied);
}

TEST(ContourPathOptimizerTest, MetadataMismatchReturnsOriginal) {
    std::vector<Primitive> primitives = {
        MakeLine(0.0f, 0.0f, 10.0f, 0.0f),
    };
    std::vector<PathPrimitiveMeta> metadata;

    ContourOptimizationStats stats;
    auto optimized = ContourOptimizationService::Optimize(primitives, metadata, Point2D(0.0f, 0.0f), true, 0, &stats);

    ASSERT_EQ(optimized.size(), primitives.size());
    EXPECT_NEAR(optimized[0].line.start.x, primitives[0].line.start.x, 1e-4f);
    EXPECT_NEAR(optimized[0].line.end.x, primitives[0].line.end.x, 1e-4f);
    EXPECT_FALSE(stats.metadata_valid);
}

TEST(ContourPathOptimizerTest, SingleOpenContourKeepsOrderWhenStartMatches) {
    std::vector<Primitive> primitives = {
        MakeLine(0.0f, 0.0f, 5.0f, 0.0f),
        MakeLine(5.0f, 0.0f, 10.0f, 0.0f),
    };
    std::vector<PathPrimitiveMeta> metadata = {
        MakeMeta(1, 0, false),
        MakeMeta(1, 1, false),
    };

    ContourOptimizationStats stats;
    auto optimized = ContourOptimizationService::Optimize(primitives, metadata, Point2D(0.0f, 0.0f), true, 0, &stats);

    ASSERT_EQ(optimized.size(), primitives.size());
    EXPECT_NEAR(optimized[0].line.start.x, 0.0f, 1e-4f);
    EXPECT_NEAR(optimized[0].line.end.x, 5.0f, 1e-4f);
}

TEST(ContourPathOptimizerTest, RotatesCircleClosedContourToNearestStart) {
    std::vector<Primitive> primitives = {
        MakeCircle(0.0f, 0.0f, 10.0f, 0.0f),
    };
    std::vector<PathPrimitiveMeta> metadata = {MakeMeta(1, 0, true)};

    ContourOptimizationStats stats;
    auto optimized = ContourOptimizationService::Optimize(
        primitives, metadata, Point2D(0.0f, 10.0f), true, 0, &stats);

    ASSERT_EQ(optimized.size(), 1u);
    EXPECT_NEAR(optimized[0].circle.start_angle_deg, 90.0f, 1e-2f);
    EXPECT_TRUE(stats.rotated_contours >= 1);
}

TEST(ContourPathOptimizerTest, RotatesEllipseClosedContourToNearestStart) {
    std::vector<Primitive> primitives = {
        MakeEllipse(0.0f, 0.0f, 10.0f, 0.0f, 0.5f, 0.0f, 2.0f * kPi),
    };
    std::vector<PathPrimitiveMeta> metadata = {MakeMeta(1, 0, true)};

    ContourOptimizationStats stats;
    auto optimized = ContourOptimizationService::Optimize(
        primitives, metadata, Point2D(0.0f, 5.0f), true, 0, &stats);

    ASSERT_EQ(optimized.size(), 1u);
    EXPECT_NEAR(optimized[0].ellipse.start_param, 0.5f * kPi, 1e-3f);
    EXPECT_TRUE(stats.rotated_contours >= 1);
}

TEST(ContourPathOptimizerTest, RotatesClosedContourToNearestStart) {
    std::vector<Primitive> primitives = {
        MakeLine(0.0f, 0.0f, 10.0f, 0.0f),
        MakeLine(10.0f, 0.0f, 10.0f, 10.0f),
        MakeLine(10.0f, 10.0f, 0.0f, 10.0f),
        MakeLine(0.0f, 10.0f, 0.0f, 0.0f),
    };
    std::vector<PathPrimitiveMeta> metadata(primitives.size());
    for (size_t i = 0; i < metadata.size(); ++i) {
        metadata[i].entity_id = 1;
        metadata[i].entity_segment_index = static_cast<int>(i);
        metadata[i].entity_closed = true;
    }

    ContourOptimizationStats stats;
    auto optimized = ContourOptimizationService::Optimize(
        primitives, metadata, Point2D(10.0f, 10.0f), true, 0, &stats);

    ASSERT_EQ(optimized.size(), primitives.size());
    EXPECT_NEAR(optimized.front().line.start.x, 10.0f, 1e-4f);
    EXPECT_NEAR(optimized.front().line.start.y, 10.0f, 1e-4f);
    EXPECT_TRUE(stats.rotated_contours >= 1);
}

TEST(ContourPathOptimizerTest, MixedOpenClosedContoursReorders) {
    std::vector<Primitive> primitives = {
        MakeLine(0.0f, 0.0f, 10.0f, 0.0f),
        MakeLine(10.0f, 0.0f, 10.0f, 10.0f),
        MakeLine(10.0f, 10.0f, 0.0f, 10.0f),
        MakeLine(0.0f, 10.0f, 0.0f, 0.0f),
        MakeLine(100.0f, 0.0f, 110.0f, 0.0f),
        MakeLine(110.0f, 0.0f, 120.0f, 0.0f),
    };
    std::vector<PathPrimitiveMeta> metadata;
    metadata.reserve(primitives.size());
    for (int i = 0; i < 4; ++i) {
        metadata.push_back(MakeMeta(1, i, true));
    }
    metadata.push_back(MakeMeta(2, 0, false));
    metadata.push_back(MakeMeta(2, 1, false));

    ContourOptimizationStats stats;
    auto optimized = ContourOptimizationService::Optimize(
        primitives, metadata, Point2D(120.0f, 0.0f), true, 0, &stats);

    ASSERT_EQ(optimized.size(), primitives.size());
    EXPECT_TRUE(stats.reordered_contours >= 1);
}

TEST(ContourPathOptimizerTest, ReordersAndReversesOpenContours) {
    std::vector<Primitive> primitives = {
        MakeLine(0.0f, 0.0f, 5.0f, 0.0f),
        MakeLine(5.0f, 0.0f, 10.0f, 0.0f),
        MakeLine(20.0f, 0.0f, 25.0f, 0.0f),
        MakeLine(25.0f, 0.0f, 30.0f, 0.0f),
    };
    std::vector<PathPrimitiveMeta> metadata(primitives.size());
    metadata[0].entity_id = 1;
    metadata[0].entity_segment_index = 0;
    metadata[1].entity_id = 1;
    metadata[1].entity_segment_index = 1;
    metadata[2].entity_id = 2;
    metadata[2].entity_segment_index = 0;
    metadata[3].entity_id = 2;
    metadata[3].entity_segment_index = 1;

    ContourOptimizationStats stats;
    auto optimized = ContourOptimizationService::Optimize(
        primitives, metadata, Point2D(30.0f, 0.0f), true, 0, &stats);

    ASSERT_EQ(optimized.size(), primitives.size());
    EXPECT_NEAR(optimized[0].line.start.x, 30.0f, 1e-4f);
    EXPECT_NEAR(optimized[0].line.end.x, 25.0f, 1e-4f);
    EXPECT_NEAR(optimized[1].line.start.x, 25.0f, 1e-4f);
    EXPECT_NEAR(optimized[1].line.end.x, 20.0f, 1e-4f);
    EXPECT_TRUE(stats.reversed_contours >= 1);
    EXPECT_TRUE(stats.reordered_contours >= 1);
}

TEST(ContourPathOptimizerTest, LargeInputDoesNotLoseSegments) {
    std::vector<Primitive> primitives;
    std::vector<PathPrimitiveMeta> metadata;
    for (int i = 0; i < 200; ++i) {
        primitives.push_back(MakeLine(static_cast<float>(i),
                                      0.0f,
                                      static_cast<float>(i + 1),
                                      0.0f));
        metadata.push_back(MakeMeta(i, 0, false));
    }

    auto optimized = ContourOptimizationService::Optimize(primitives, metadata, Point2D(0.0f, 0.0f), true, 0, nullptr);
    EXPECT_EQ(optimized.size(), primitives.size());
}



