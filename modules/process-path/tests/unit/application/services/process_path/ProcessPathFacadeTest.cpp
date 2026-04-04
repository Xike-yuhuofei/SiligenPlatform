#include "application/services/process_path/ProcessPathFacade.h"
#include "infrastructure/adapters/planning/dxf/PbPathSourceAdapter.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <iostream>
#include <map>

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

    EXPECT_EQ(result.status, Siligen::ProcessPath::Contracts::PathGenerationStatus::Success);
    EXPECT_EQ(result.failed_stage, Siligen::ProcessPath::Contracts::PathGenerationStage::None);
    EXPECT_TRUE(result.error_message.empty());
    ASSERT_EQ(result.normalized.path.segments.size(), 1u);
    ASSERT_EQ(result.process_path.segments.size(), 1u);
    ASSERT_EQ(result.shaped_path.segments.size(), 1u);
    EXPECT_TRUE(result.shaped_path.segments.front().dispense_on);
    ASSERT_TRUE(request.alignment.has_value());
    EXPECT_EQ(request.alignment->owner_module, "M5");
}

TEST(ProcessPathFacadeTest, EmptyPrimitiveInputReturnsInvalidInputStatus) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::PathGenerationStage;
    using Siligen::ProcessPath::Contracts::PathGenerationStatus;

    ProcessPathBuildRequest request;

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_EQ(result.status, PathGenerationStatus::InvalidInput);
    EXPECT_EQ(result.failed_stage, PathGenerationStage::InputValidation);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_TRUE(result.normalized.path.segments.empty());
    EXPECT_TRUE(result.process_path.segments.empty());
    EXPECT_TRUE(result.shaped_path.segments.empty());
}

TEST(ProcessPathFacadeTest, RejectsAlignmentInputsNotOwnedByM5) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::CoordinateAlignment::Contracts::CoordinateTransformSet;
    using Siligen::ProcessPath::Contracts::PathGenerationStage;
    using Siligen::ProcessPath::Contracts::PathGenerationStatus;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    CoordinateTransformSet transform_set;
    transform_set.valid = true;
    transform_set.owner_module = "M4";
    request.alignment = transform_set;

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_EQ(result.status, PathGenerationStatus::InvalidInput);
    EXPECT_EQ(result.failed_stage, PathGenerationStage::InputValidation);
    EXPECT_EQ(result.error_message, "alignment input must remain owned by M5");
}

TEST(ProcessPathFacadeTest, RejectsNonPositiveNormalizationUnitScale) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::PathGenerationStage;
    using Siligen::ProcessPath::Contracts::PathGenerationStatus;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.normalization.unit_scale = 0.0f;

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_EQ(result.status, PathGenerationStatus::InvalidInput);
    EXPECT_EQ(result.failed_stage, PathGenerationStage::Normalization);
    EXPECT_EQ(result.error_message, "normalization.unit_scale must be greater than zero");
    EXPECT_TRUE(result.normalized.path.segments.empty());
    EXPECT_TRUE(result.normalized.report.invalid_unit_scale);
}

TEST(ProcessPathFacadeTest, RejectsSplineInputWhenApproximationIsDisabled) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::PathGenerationStage;
    using Siligen::ProcessPath::Contracts::PathGenerationStatus;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.primitives.push_back(Primitive::MakeSpline({
        Point2D(0.0f, 0.0f),
        Point2D(5.0f, 5.0f),
        Point2D(10.0f, 0.0f),
    }));
    request.normalization.approximate_splines = false;

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_EQ(result.status, PathGenerationStatus::StageFailure);
    EXPECT_EQ(result.failed_stage, PathGenerationStage::Normalization);
    EXPECT_EQ(result.error_message,
              "normalization skipped spline primitives because approximate_splines is disabled");
    EXPECT_EQ(result.normalized.report.skipped_spline_count, 1);
    EXPECT_EQ(result.normalized.report.consumable_segment_count, 0);
}

TEST(ProcessPathFacadeTest, UnsupportedPointOnlyInputReturnsNormalizationFailure) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::PathGenerationStage;
    using Siligen::ProcessPath::Contracts::PathGenerationStatus;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.primitives.push_back(Primitive::MakePoint(Point2D(1.0f, 2.0f)));

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_EQ(result.status, PathGenerationStatus::StageFailure);
    EXPECT_EQ(result.failed_stage, PathGenerationStage::Normalization);
    EXPECT_EQ(result.error_message, "normalization produced no consumable path segments");
    EXPECT_EQ(result.normalized.report.point_primitive_count, 1);
    EXPECT_EQ(result.normalized.report.consumable_segment_count, 0);
    EXPECT_TRUE(result.process_path.segments.empty());
    EXPECT_TRUE(result.shaped_path.segments.empty());
}

TEST(ProcessPathFacadeTest, PreservesBranchRevisitLikePrimitiveSequenceThroughBuild) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.normalization.continuity_tolerance = 0.1f;
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.primitives.push_back(Primitive::MakeLine(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)));
    request.primitives.push_back(Primitive::MakeLine(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)));
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)));
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 10.0f)));

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    ASSERT_EQ(result.normalized.path.segments.size(), 5u);
    ASSERT_EQ(result.process_path.segments.size(), 5u);
    ASSERT_GE(result.shaped_path.segments.size(), 5u);
    EXPECT_TRUE(result.process_path.segments.front().dispense_on);
    EXPECT_TRUE(result.process_path.segments.back().dispense_on);
}

TEST(ProcessPathFacadeTest, RepairDisabledPreservesFragmentedRapidInsertion) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.normalization.continuity_tolerance = 0.1f;
    request.process.rapid_gap_threshold = 0.1f;
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)));
    request.primitives.push_back(Primitive::MakeLine(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)));
    request.primitives.push_back(Primitive::MakeLine(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)));
    request.metadata = {
        PathPrimitiveMeta{1},
        PathPrimitiveMeta{2},
        PathPrimitiveMeta{3},
        PathPrimitiveMeta{4},
    };

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_FALSE(result.topology_diagnostics.repair_requested);
    EXPECT_FALSE(result.topology_diagnostics.repair_applied);
    EXPECT_GT(result.topology_diagnostics.rapid_segment_count, 0);
    EXPECT_GT(result.topology_diagnostics.dispense_fragment_count, 1);
}

TEST(ProcessPathFacadeTest, RepairEnabledReducesFragmentedDispenseChains) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.normalization.continuity_tolerance = 0.1f;
    request.process.rapid_gap_threshold = 0.1f;
    request.topology_repair.enable = true;
    request.topology_repair.start_position = Point2D(0.0f, 0.0f);
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)));
    request.primitives.push_back(Primitive::MakeLine(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)));
    request.primitives.push_back(Primitive::MakeLine(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)));
    request.metadata = {
        PathPrimitiveMeta{1},
        PathPrimitiveMeta{2},
        PathPrimitiveMeta{3},
        PathPrimitiveMeta{4},
    };

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_TRUE(result.topology_diagnostics.repair_requested);
    EXPECT_EQ(result.topology_diagnostics.discontinuity_before, 2);
    EXPECT_LE(result.topology_diagnostics.discontinuity_after, 1);
    EXPECT_TRUE(result.topology_diagnostics.repair_applied);
    EXPECT_LE(result.topology_diagnostics.rapid_segment_count, 1);
    EXPECT_EQ(result.topology_diagnostics.dispense_fragment_count, 1);
}

TEST(ProcessPathFacadeTest, RepairToleratesMissingMetadataWithoutCrashing) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.normalization.continuity_tolerance = 0.1f;
    request.topology_repair.enable = true;
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(5.0f, 0.0f)));
    request.primitives.push_back(Primitive::MakeLine(Point2D(5.2f, 0.0f), Point2D(10.0f, 0.0f)));

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_FALSE(result.topology_diagnostics.metadata_valid);
    EXPECT_TRUE(result.process_path.segments.size() >= 2u);
}

TEST(ProcessPathFacadeTest, RepairDropsPointNoiseBeforeConnectivityRebuild) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.normalization.continuity_tolerance = 0.1f;
    request.process.rapid_gap_threshold = 0.1f;
    request.topology_repair.enable = true;
    request.topology_repair.start_position = Point2D(0.0f, 0.0f);
    request.primitives = {
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
        Primitive::MakePoint(Point2D(5.0f, 5.0f)),
        Primitive::MakeLine(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)),
        Primitive::MakePoint(Point2D(6.0f, 6.0f)),
        Primitive::MakeLine(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)),
        Primitive::MakeLine(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)),
    };
    request.metadata = {
        PathPrimitiveMeta{1},
        PathPrimitiveMeta{2},
        PathPrimitiveMeta{3},
        PathPrimitiveMeta{4},
        PathPrimitiveMeta{5},
        PathPrimitiveMeta{6},
    };

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_TRUE(result.topology_diagnostics.repair_applied);
    EXPECT_EQ(result.topology_diagnostics.original_primitive_count, 6);
    EXPECT_EQ(result.topology_diagnostics.repaired_primitive_count, 4);
    EXPECT_GT(result.topology_diagnostics.contour_count, 0);
    EXPECT_EQ(result.topology_diagnostics.rapid_segment_count, 0);
    EXPECT_EQ(result.topology_diagnostics.dispense_fragment_count, 1);
}

TEST(ProcessPathFacadeTest, Demo1PbRealFileRegressionReportsTopologyDiagnostics) {
    namespace fs = std::filesystem;

    const fs::path pb_path = "D:\\Projects\\SiligenSuite\\uploads\\dxf\\archive\\Demo-1.pb";
    if (!fs::exists(pb_path)) {
        GTEST_SKIP() << "Demo-1.pb 不存在: " << pb_path.string();
    }

    Siligen::Infrastructure::Adapters::Parsing::PbPathSourceAdapter adapter;
    const auto load_result = adapter.LoadFromFile(pb_path.string());
    ASSERT_TRUE(load_result.IsSuccess()) << load_result.GetError().ToString();

    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;

    ProcessPathBuildRequest request;
    request.primitives = load_result.Value().primitives;
    request.metadata.reserve(load_result.Value().metadata.size());
    for (const auto& item : load_result.Value().metadata) {
        PathPrimitiveMeta meta;
        meta.entity_id = item.entity_id;
        meta.entity_type = item.entity_type;
        meta.entity_segment_index = item.entity_segment_index;
        meta.entity_closed = item.entity_closed;
        request.metadata.push_back(meta);
    }
    request.normalization.continuity_tolerance = 0.1f;
    request.topology_repair.enable = true;
    request.topology_repair.start_position = Siligen::Shared::Types::Point2D(0.0f, 0.0f);
    request.topology_repair.two_opt_iterations = 0;

    std::map<int, int> primitive_type_counts;
    for (const auto& primitive : request.primitives) {
        primitive_type_counts[static_cast<int>(primitive.type)] += 1;
    }

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    std::cout << "Demo-1 primitive_types";
    for (const auto& [type_id, count] : primitive_type_counts) {
        std::cout << " type" << type_id << '=' << count;
    }
    std::cout << std::endl;

    std::cout
        << "Demo-1 topology_diagnostics"
        << " repair_requested=" << result.topology_diagnostics.repair_requested
        << " repair_applied=" << result.topology_diagnostics.repair_applied
        << " metadata_valid=" << result.topology_diagnostics.metadata_valid
        << " fragmentation_suspected=" << result.topology_diagnostics.fragmentation_suspected
        << " original_primitive_count=" << result.topology_diagnostics.original_primitive_count
        << " repaired_primitive_count=" << result.topology_diagnostics.repaired_primitive_count
        << " contour_count=" << result.topology_diagnostics.contour_count
        << " discontinuity_before=" << result.topology_diagnostics.discontinuity_before
        << " discontinuity_after=" << result.topology_diagnostics.discontinuity_after
        << " intersection_pairs=" << result.topology_diagnostics.intersection_pairs
        << " collinear_pairs=" << result.topology_diagnostics.collinear_pairs
        << " rapid_segment_count=" << result.topology_diagnostics.rapid_segment_count
        << " dispense_fragment_count=" << result.topology_diagnostics.dispense_fragment_count
        << " reordered_contours=" << result.topology_diagnostics.reordered_contours
        << " reversed_contours=" << result.topology_diagnostics.reversed_contours
        << " rotated_contours=" << result.topology_diagnostics.rotated_contours
        << " shaped_segments=" << result.shaped_path.segments.size()
        << std::endl;

    EXPECT_TRUE(result.topology_diagnostics.repair_requested);
    EXPECT_TRUE(result.topology_diagnostics.metadata_valid);
    EXPECT_EQ(result.topology_diagnostics.original_primitive_count, static_cast<int>(request.primitives.size()));
    EXPECT_EQ(result.topology_diagnostics.repaired_primitive_count, static_cast<int>(result.normalized.path.segments.size()));
}
