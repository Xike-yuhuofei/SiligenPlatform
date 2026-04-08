#include "application/services/process_path/ProcessPathFacade.h"
#include "domain/trajectory/domain-services/TopologyRepairService.h"
#include "dxf_geometry/adapters/planning/dxf/PbPathSourceAdapter.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>

namespace {

using Point2D = Siligen::Shared::Types::Point2D;
namespace fs = std::filesystem;

std::optional<Point2D> SegmentStartPoint(const Siligen::ProcessPath::Contracts::Segment& segment) {
    using Siligen::ProcessPath::Contracts::SegmentType;
    switch (segment.type) {
        case SegmentType::Line:
            return segment.line.start;
        case SegmentType::Arc: {
            const auto start_rad = segment.arc.start_angle_deg * 3.14159265358979323846f / 180.0f;
            return Point2D(
                segment.arc.center.x + segment.arc.radius * std::cos(start_rad),
                segment.arc.center.y + segment.arc.radius * std::sin(start_rad));
        }
        case SegmentType::Spline:
            if (!segment.spline.control_points.empty()) {
                return segment.spline.control_points.front();
            }
            return std::nullopt;
    }
    return std::nullopt;
}

std::optional<Point2D> SegmentEndPoint(const Siligen::ProcessPath::Contracts::Segment& segment) {
    using Siligen::ProcessPath::Contracts::SegmentType;
    switch (segment.type) {
        case SegmentType::Line:
            return segment.line.end;
        case SegmentType::Arc: {
            const auto end_rad = segment.arc.end_angle_deg * 3.14159265358979323846f / 180.0f;
            return Point2D(
                segment.arc.center.x + segment.arc.radius * std::cos(end_rad),
                segment.arc.center.y + segment.arc.radius * std::sin(end_rad));
        }
        case SegmentType::Spline:
            if (!segment.spline.control_points.empty()) {
                return segment.spline.control_points.back();
            }
            return std::nullopt;
    }
    return std::nullopt;
}

bool IsImmediateCollinearReversal(
    const Siligen::ProcessPath::Contracts::Segment& previous,
    const Siligen::ProcessPath::Contracts::Segment& current) {
    using Siligen::ProcessPath::Contracts::SegmentType;
    if (previous.type != SegmentType::Line || current.type != SegmentType::Line) {
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
    const auto normalized_cross =
        std::abs(prev_vec.Cross(curr_vec)) / std::max(1e-4f, prev_len * curr_len);
    const auto normalized_dot = prev_vec.Dot(curr_vec) / (prev_len * curr_len);
    return junction_gap <= 0.2f && normalized_cross <= 1e-3f && normalized_dot < -0.95f;
}

void WriteProcessPathCsv(const fs::path& output_path, const Siligen::ProcessPath::Contracts::ProcessPath& path) {
    std::ofstream out(output_path);
    out << "segment_index,type,dispense_on,tag,start_x,start_y,end_x,end_y,length\n";
    for (size_t index = 0; index < path.segments.size(); ++index) {
        const auto& segment = path.segments[index];
        const auto start = SegmentStartPoint(segment.geometry);
        const auto end = SegmentEndPoint(segment.geometry);
        out << index << ','
            << static_cast<int>(segment.geometry.type) << ','
            << (segment.dispense_on ? 1 : 0) << ','
            << static_cast<int>(segment.tag) << ','
            << (start.has_value() ? start->x : 0.0f) << ','
            << (start.has_value() ? start->y : 0.0f) << ','
            << (end.has_value() ? end->x : 0.0f) << ','
            << (end.has_value() ? end->y : 0.0f) << ','
            << segment.geometry.length << '\n';
    }
}

std::size_t CountImmediateLineReversals(const Siligen::ProcessPath::Contracts::ProcessPath& path) {
    std::size_t immediate_reversal_count = 0;
    for (size_t index = 1; index < path.segments.size(); ++index) {
        if (IsImmediateCollinearReversal(
                path.segments[index - 1].geometry,
                path.segments[index].geometry)) {
            ++immediate_reversal_count;
        }
    }
    return immediate_reversal_count;
}

std::optional<Point2D> PrimitiveStartPoint(const Siligen::ProcessPath::Contracts::Primitive& primitive) {
    using Siligen::ProcessPath::Contracts::PrimitiveType;
    switch (primitive.type) {
        case PrimitiveType::Line:
            return primitive.line.start;
        case PrimitiveType::Arc: {
            const auto start_rad = primitive.arc.start_angle_deg * 3.14159265358979323846f / 180.0f;
            return Point2D(
                primitive.arc.center.x + primitive.arc.radius * std::cos(start_rad),
                primitive.arc.center.y + primitive.arc.radius * std::sin(start_rad));
        }
        case PrimitiveType::Spline:
            if (!primitive.spline.control_points.empty()) {
                return primitive.spline.control_points.front();
            }
            return std::nullopt;
        case PrimitiveType::Circle: {
            const auto start_rad = primitive.circle.start_angle_deg * 3.14159265358979323846f / 180.0f;
            return Point2D(
                primitive.circle.center.x + primitive.circle.radius * std::cos(start_rad),
                primitive.circle.center.y + primitive.circle.radius * std::sin(start_rad));
        }
        case PrimitiveType::Ellipse:
        case PrimitiveType::Point:
        case PrimitiveType::Contour:
            return std::nullopt;
    }
    return std::nullopt;
}

std::optional<Point2D> PrimitiveEndPoint(const Siligen::ProcessPath::Contracts::Primitive& primitive) {
    using Siligen::ProcessPath::Contracts::PrimitiveType;
    switch (primitive.type) {
        case PrimitiveType::Line:
            return primitive.line.end;
        case PrimitiveType::Arc: {
            const auto end_rad = primitive.arc.end_angle_deg * 3.14159265358979323846f / 180.0f;
            return Point2D(
                primitive.arc.center.x + primitive.arc.radius * std::cos(end_rad),
                primitive.arc.center.y + primitive.arc.radius * std::sin(end_rad));
        }
        case PrimitiveType::Spline:
            if (!primitive.spline.control_points.empty()) {
                return primitive.spline.control_points.back();
            }
            return std::nullopt;
        case PrimitiveType::Circle: {
            const auto end_rad = (primitive.circle.start_angle_deg + 360.0f) * 3.14159265358979323846f / 180.0f;
            return Point2D(
                primitive.circle.center.x + primitive.circle.radius * std::cos(end_rad),
                primitive.circle.center.y + primitive.circle.radius * std::sin(end_rad));
        }
        case PrimitiveType::Ellipse:
        case PrimitiveType::Point:
        case PrimitiveType::Contour:
            return std::nullopt;
    }
    return std::nullopt;
}

bool IsImmediateCollinearReversal(const Siligen::ProcessPath::Contracts::Primitive& previous,
                                  const Siligen::ProcessPath::Contracts::Primitive& current) {
    using Siligen::ProcessPath::Contracts::PrimitiveType;
    if (previous.type != PrimitiveType::Line || current.type != PrimitiveType::Line) {
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
    const auto normalized_cross =
        std::abs(prev_vec.Cross(curr_vec)) / std::max(1e-4f, prev_len * curr_len);
    const auto normalized_dot = prev_vec.Dot(curr_vec) / (prev_len * curr_len);
    return junction_gap <= 0.2f && normalized_cross <= 1e-3f && normalized_dot < -0.95f;
}

std::vector<size_t> CollectImmediatePrimitiveLineReversals(
    const std::vector<Siligen::ProcessPath::Contracts::Primitive>& primitives) {
    std::vector<size_t> reversal_indices;
    for (size_t index = 1; index < primitives.size(); ++index) {
        if (IsImmediateCollinearReversal(primitives[index - 1], primitives[index])) {
            reversal_indices.push_back(index);
        }
    }
    return reversal_indices;
}

std::optional<std::pair<size_t, bool>> FindOriginalPrimitiveMatch(
    const Siligen::ProcessPath::Contracts::Primitive& primitive,
    const std::vector<Siligen::ProcessPath::Contracts::Primitive>& originals,
    float tolerance) {
    using Siligen::ProcessPath::Contracts::PrimitiveType;
    if (primitive.type != PrimitiveType::Line) {
        return std::nullopt;
    }

    for (size_t index = 0; index < originals.size(); ++index) {
        const auto& candidate = originals[index];
        if (candidate.type != PrimitiveType::Line) {
            continue;
        }

        const bool forward_match =
            primitive.line.start.DistanceTo(candidate.line.start) <= tolerance &&
            primitive.line.end.DistanceTo(candidate.line.end) <= tolerance;
        if (forward_match) {
            return std::make_pair(index, false);
        }

        const bool reversed_match =
            primitive.line.start.DistanceTo(candidate.line.end) <= tolerance &&
            primitive.line.end.DistanceTo(candidate.line.start) <= tolerance;
        if (reversed_match) {
            return std::make_pair(index, true);
        }
    }

    return std::nullopt;
}

void PrintPrimitiveReversalDiagnostics(
    const char* label,
    const std::vector<Siligen::ProcessPath::Contracts::Primitive>& repaired_primitives,
    const std::vector<Siligen::ProcessPath::Contracts::PathPrimitiveMeta>& repaired_metadata,
    const std::vector<Siligen::ProcessPath::Contracts::Primitive>& original_primitives,
    const std::vector<Siligen::ProcessPath::Contracts::PathPrimitiveMeta>& original_metadata,
    float tolerance) {
    const auto reversals = CollectImmediatePrimitiveLineReversals(repaired_primitives);
    for (const auto index : reversals) {
        const auto& previous = repaired_primitives[index - 1];
        const auto& current = repaired_primitives[index];
        const auto prev_match = FindOriginalPrimitiveMatch(previous, original_primitives, tolerance);
        const auto curr_match = FindOriginalPrimitiveMatch(current, original_primitives, tolerance);

        std::cout
            << "Demo primitive_reversal"
            << " label=" << label
            << " pair=" << (index - 1) << "->" << index
            << " prev=(" << previous.line.start.x << ',' << previous.line.start.y << ")->("
            << previous.line.end.x << ',' << previous.line.end.y << ')'
            << " curr=(" << current.line.start.x << ',' << current.line.start.y << ")->("
            << current.line.end.x << ',' << current.line.end.y << ')';

        if (index - 1 < repaired_metadata.size()) {
            const auto& meta = repaired_metadata[index - 1];
            std::cout
                << " prev_meta={entity_id:" << meta.entity_id
                << ",segment_index:" << meta.entity_segment_index
                << ",closed:" << meta.entity_closed << '}';
        }
        if (index < repaired_metadata.size()) {
            const auto& meta = repaired_metadata[index];
            std::cout
                << " curr_meta={entity_id:" << meta.entity_id
                << ",segment_index:" << meta.entity_segment_index
                << ",closed:" << meta.entity_closed << '}';
        }
        if (prev_match.has_value()) {
            const auto [original_index, reversed] = *prev_match;
            std::cout
                << " prev_src={index:" << original_index
                << ",reversed:" << reversed
                << ",entity_id:" << original_metadata[original_index].entity_id
                << ",segment_index:" << original_metadata[original_index].entity_segment_index
                << ",closed:" << original_metadata[original_index].entity_closed << '}';
        }
        if (curr_match.has_value()) {
            const auto [original_index, reversed] = *curr_match;
            std::cout
                << " curr_src={index:" << original_index
                << ",reversed:" << reversed
                << ",entity_id:" << original_metadata[original_index].entity_id
                << ",segment_index:" << original_metadata[original_index].entity_segment_index
                << ",closed:" << original_metadata[original_index].entity_closed << '}';
        }
        std::cout << std::endl;
    }
}

void PrintJunctionCandidates(const char* label,
                             const std::vector<Siligen::ProcessPath::Contracts::Primitive>& repaired_primitives,
                             const std::vector<Siligen::ProcessPath::Contracts::Primitive>& original_primitives,
                             const std::vector<Siligen::ProcessPath::Contracts::PathPrimitiveMeta>& original_metadata,
                             float tolerance) {
    const auto reversals = CollectImmediatePrimitiveLineReversals(repaired_primitives);
    for (const auto index : reversals) {
        const auto junction = repaired_primitives[index - 1].line.end;
        std::cout
            << "Demo junction_candidates"
            << " label=" << label
            << " pair=" << (index - 1) << "->" << index
            << " junction=(" << junction.x << ',' << junction.y << ')';

        int candidate_count = 0;
        for (size_t original_index = 0; original_index < original_primitives.size(); ++original_index) {
            const auto& primitive = original_primitives[original_index];
            if (primitive.type != Siligen::ProcessPath::Contracts::PrimitiveType::Line) {
                continue;
            }
            const bool touches_start = primitive.line.start.DistanceTo(junction) <= tolerance;
            const bool touches_end = primitive.line.end.DistanceTo(junction) <= tolerance;
            if (!touches_start && !touches_end) {
                continue;
            }
            candidate_count += 1;
            const auto& meta = original_metadata[original_index];
            std::cout
                << " candidate[" << original_index << "]="
                << '(' << primitive.line.start.x << ',' << primitive.line.start.y << ")->("
                << primitive.line.end.x << ',' << primitive.line.end.y << ')'
                << "{entity_id:" << meta.entity_id
                << ",segment_index:" << meta.entity_segment_index
                << ",touches_start:" << touches_start
                << ",touches_end:" << touches_end << '}';
        }
        std::cout << " candidate_count=" << candidate_count << std::endl;
    }
}

}  // namespace

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

TEST(ProcessPathFacadeTest, DemoPbRealFileDiagnosticsExportsShapedPathFacts) {
    const fs::path pb_path = "D:\\Projects\\wt-task157\\tmp\\Demo.pb";
    if (!fs::exists(pb_path)) {
        GTEST_SKIP() << "Demo.pb 不存在: " << pb_path.string();
    }

    Siligen::Infrastructure::Adapters::Parsing::PbPathSourceAdapter adapter;
    const auto load_result = adapter.LoadFromFile(pb_path.string());
    ASSERT_TRUE(load_result.IsSuccess()) << load_result.GetError().ToString();

    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::PathGenerationStatus;
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
    request.topology_repair.start_position = Point2D(0.0f, 0.0f);
    request.topology_repair.two_opt_iterations = 0;

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    ASSERT_EQ(result.status, PathGenerationStatus::Success) << result.error_message;
    ASSERT_FALSE(result.shaped_path.segments.empty());

    const auto immediate_reversal_count = CountImmediateLineReversals(result.shaped_path);

    const fs::path output_dir = "D:\\Projects\\wt-task157\\tmp\\demo-diagnostics";
    fs::create_directories(output_dir);
    const auto csv_path = output_dir / "demo_process_path_shaped.csv";
    WriteProcessPathCsv(csv_path, result.shaped_path);

    std::cout
        << "Demo shaped_path"
        << " segments=" << result.shaped_path.segments.size()
        << " reversal_pairs=" << immediate_reversal_count
        << " repair_applied=" << result.topology_diagnostics.repair_applied
        << " discontinuity_before=" << result.topology_diagnostics.discontinuity_before
        << " discontinuity_after=" << result.topology_diagnostics.discontinuity_after
        << " reordered_contours=" << result.topology_diagnostics.reordered_contours
        << " reversed_contours=" << result.topology_diagnostics.reversed_contours
        << " csv=" << csv_path.string()
        << std::endl;
}

TEST(ProcessPathFacadeTest, DemoPbRepairToggleDiagnosticsIdentifiesFirstBadSubstep) {
    const fs::path pb_path = "D:\\Projects\\wt-task157\\tmp\\Demo.pb";
    if (!fs::exists(pb_path)) {
        GTEST_SKIP() << "Demo.pb 不存在: " << pb_path.string();
    }

    Siligen::Infrastructure::Adapters::Parsing::PbPathSourceAdapter adapter;
    const auto load_result = adapter.LoadFromFile(pb_path.string());
    ASSERT_TRUE(load_result.IsSuccess()) << load_result.GetError().ToString();

    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::PathGenerationStatus;
    using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;

    auto make_request = [&]() {
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
        request.topology_repair.start_position = Point2D(0.0f, 0.0f);
        request.topology_repair.two_opt_iterations = 0;
        return request;
    };

    auto run_case = [&](const char* label, ProcessPathBuildRequest request) {
        ProcessPathFacade facade;
        const auto result = facade.Build(request);
        EXPECT_EQ(result.status, PathGenerationStatus::Success) << label << ": " << result.error_message;
        const auto process_reversal_count = CountImmediateLineReversals(result.process_path);
        const auto shaped_reversal_count = CountImmediateLineReversals(result.shaped_path);
        std::cout
            << "Demo repair_toggle"
            << " label=" << label
            << " process_segments=" << result.process_path.segments.size()
            << " shaped_segments=" << result.shaped_path.segments.size()
            << " process_line_reversals=" << process_reversal_count
            << " shaped_line_reversals=" << shaped_reversal_count
            << " repair_applied=" << result.topology_diagnostics.repair_applied
            << " reordered_contours=" << result.topology_diagnostics.reordered_contours
            << " reversed_contours=" << result.topology_diagnostics.reversed_contours
            << " discontinuity_before=" << result.topology_diagnostics.discontinuity_before
            << " discontinuity_after=" << result.topology_diagnostics.discontinuity_after
            << std::endl;
        return std::pair<std::size_t, std::size_t>(process_reversal_count, shaped_reversal_count);
    };

    auto disabled = make_request();
    disabled.topology_repair.enable = false;

    auto no_rebuild = make_request();
    no_rebuild.topology_repair.rebuild_by_connectivity = false;

    auto no_reorder = make_request();
    no_reorder.topology_repair.reorder_contours = false;

    auto full = make_request();

    const auto disabled_reversals = run_case("repair_off", disabled);
    const auto no_rebuild_reversals = run_case("no_rebuild", no_rebuild);
    const auto no_reorder_reversals = run_case("no_reorder", no_reorder);
    const auto full_reversals = run_case("full", full);

    EXPECT_LE(disabled_reversals.second, full_reversals.second);
    EXPECT_LE(no_rebuild_reversals.second, full_reversals.second);
    EXPECT_LE(no_reorder_reversals.second, full_reversals.second);
}

TEST(ProcessPathFacadeTest, DemoPbConnectivityRepairDiagnosticsMapsReversalsToOriginalPrimitives) {
    const fs::path pb_path = "D:\\Projects\\wt-task157\\tmp\\Demo.pb";
    if (!fs::exists(pb_path)) {
        GTEST_SKIP() << "Demo.pb 不存在: " << pb_path.string();
    }

    Siligen::Infrastructure::Adapters::Parsing::PbPathSourceAdapter adapter;
    const auto load_result = adapter.LoadFromFile(pb_path.string());
    ASSERT_TRUE(load_result.IsSuccess()) << load_result.GetError().ToString();

    using Siligen::Domain::Trajectory::DomainServices::TopologyRepairService;
    using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
    using Siligen::ProcessPath::Contracts::TopologyRepairConfig;

    std::vector<PathPrimitiveMeta> metadata;
    metadata.reserve(load_result.Value().metadata.size());
    for (const auto& item : load_result.Value().metadata) {
        PathPrimitiveMeta meta;
        meta.entity_id = item.entity_id;
        meta.entity_type = item.entity_type;
        meta.entity_segment_index = item.entity_segment_index;
        meta.entity_closed = item.entity_closed;
        metadata.push_back(meta);
    }

    auto run_case = [&](const char* label, const TopologyRepairConfig& config) {
        TopologyRepairService service;
        const auto repair = service.Repair(load_result.Value().primitives, metadata, 0.1f, config);
        const auto reversals = CollectImmediatePrimitiveLineReversals(repair.primitives);
        std::cout
            << "Demo topology_repair_case"
            << " label=" << label
            << " primitive_count=" << repair.primitives.size()
            << " reversal_pairs=" << reversals.size()
            << " repair_applied=" << repair.diagnostics.repair_applied
            << " reversed_contours=" << repair.diagnostics.reversed_contours
            << " contour_count=" << repair.diagnostics.contour_count
            << std::endl;
        PrintPrimitiveReversalDiagnostics(
            label,
            repair.primitives,
            repair.metadata,
            load_result.Value().primitives,
            metadata,
            0.1f);
        PrintJunctionCandidates(
            label,
            repair.primitives,
            load_result.Value().primitives,
            metadata,
            0.1f);
        return reversals.size();
    };

    TopologyRepairConfig repair_off;
    repair_off.enable = false;

    TopologyRepairConfig no_rebuild;
    no_rebuild.enable = true;
    no_rebuild.rebuild_by_connectivity = false;

    TopologyRepairConfig no_reorder;
    no_reorder.enable = true;
    no_reorder.reorder_contours = false;

    TopologyRepairConfig full;
    full.enable = true;

    const auto off_reversal_count = run_case("repair_off", repair_off);
    const auto no_rebuild_reversal_count = run_case("no_rebuild", no_rebuild);
    const auto no_reorder_reversal_count = run_case("no_reorder", no_reorder);
    const auto full_reversal_count = run_case("full", full);

    EXPECT_LE(off_reversal_count, no_reorder_reversal_count);
    EXPECT_EQ(no_reorder_reversal_count, full_reversal_count);
    EXPECT_LE(no_rebuild_reversal_count, no_reorder_reversal_count);
}

TEST(ProcessPathFacadeTest, ConnectivityRepairDoesNotReverseDuplicateForwardLineIntoBacktrack) {
    using Siligen::Domain::Trajectory::DomainServices::TopologyRepairService;
    using Siligen::ProcessPath::Contracts::DXFEntityType;
    using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::ProcessPath::Contracts::TopologyRepairConfig;

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
    config.enable = true;
    config.reorder_contours = false;
    config.start_position = Point2D(0.0f, 0.0f);

    TopologyRepairService service;
    const auto repair = service.Repair(primitives, metadata, 0.1f, config);

    ASSERT_EQ(repair.primitives.size(), primitives.size());
    EXPECT_TRUE(CollectImmediatePrimitiveLineReversals(repair.primitives).empty());
    EXPECT_EQ(repair.diagnostics.contour_count, 2);
    EXPECT_EQ(repair.diagnostics.reversed_contours, 0);
    EXPECT_EQ(repair.primitives[0].line.start, Point2D(0.0f, 0.0f));
    EXPECT_EQ(repair.primitives[0].line.end, Point2D(10.0f, 0.0f));
    EXPECT_EQ(repair.primitives[1].line.start, Point2D(0.0f, 0.0f));
    EXPECT_EQ(repair.primitives[1].line.end, Point2D(10.0f, 0.0f));
}

TEST(ProcessPathFacadeTest, ReorderContoursDoesNotCreateImmediateBacktrackAtOpenContourBoundary) {
    using Siligen::Domain::Trajectory::DomainServices::TopologyRepairService;
    using Siligen::ProcessPath::Contracts::DXFEntityType;
    using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::ProcessPath::Contracts::TopologyRepairConfig;

    std::vector<Primitive> primitives{
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
        Primitive::MakeLine(Point2D(10.0f, 0.0f), Point2D(20.0f, 0.0f)),
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
    };

    std::vector<PathPrimitiveMeta> metadata(3);
    metadata[0].entity_id = 10;
    metadata[0].entity_type = DXFEntityType::Line;
    metadata[0].entity_segment_index = 0;
    metadata[0].entity_closed = false;

    metadata[1].entity_id = 10;
    metadata[1].entity_type = DXFEntityType::Line;
    metadata[1].entity_segment_index = 1;
    metadata[1].entity_closed = false;

    metadata[2].entity_id = 20;
    metadata[2].entity_type = DXFEntityType::Line;
    metadata[2].entity_segment_index = 0;
    metadata[2].entity_closed = false;

    TopologyRepairConfig config;
    config.enable = true;
    config.rebuild_by_connectivity = false;
    config.reorder_contours = true;
    config.start_position = Point2D(0.0f, 0.0f);
    config.two_opt_iterations = 0;

    TopologyRepairService service;
    const auto repair = service.Repair(primitives, metadata, 0.1f, config);

    ASSERT_EQ(repair.primitives.size(), primitives.size());
    EXPECT_TRUE(CollectImmediatePrimitiveLineReversals(repair.primitives).empty());
    ASSERT_GE(repair.primitives.size(), 2u);
    EXPECT_EQ(repair.primitives[0].line.start, Point2D(0.0f, 0.0f));
    EXPECT_EQ(repair.primitives[0].line.end, Point2D(10.0f, 0.0f));
    EXPECT_EQ(repair.primitives[1].line.start, Point2D(10.0f, 0.0f));
    EXPECT_EQ(repair.primitives[1].line.end, Point2D(20.0f, 0.0f));
}

TEST(ProcessPathFacadeTest, ReorderContoursFlipsOpenContourWhenSelectedDirectionWouldBacktrack) {
    using Siligen::Domain::Trajectory::DomainServices::TopologyRepairService;
    using Siligen::ProcessPath::Contracts::DXFEntityType;
    using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::ProcessPath::Contracts::TopologyRepairConfig;

    std::vector<Primitive> primitives{
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
        Primitive::MakeLine(Point2D(10.0f, 0.0f), Point2D(20.0f, 0.0f)),
        Primitive::MakeLine(Point2D(20.0f, 0.0f), Point2D(10.0f, 0.0f)),
    };

    std::vector<PathPrimitiveMeta> metadata(3);
    metadata[0].entity_id = 100;
    metadata[0].entity_type = DXFEntityType::Line;
    metadata[0].entity_segment_index = 0;
    metadata[0].entity_closed = false;

    metadata[1].entity_id = 100;
    metadata[1].entity_type = DXFEntityType::Line;
    metadata[1].entity_segment_index = 1;
    metadata[1].entity_closed = false;

    metadata[2].entity_id = 200;
    metadata[2].entity_type = DXFEntityType::Line;
    metadata[2].entity_segment_index = 0;
    metadata[2].entity_closed = false;

    TopologyRepairConfig config;
    config.enable = true;
    config.rebuild_by_connectivity = false;
    config.reorder_contours = true;
    config.start_position = Point2D(0.0f, 0.0f);
    config.two_opt_iterations = 0;

    TopologyRepairService service;
    const auto repair = service.Repair(primitives, metadata, 0.1f, config);

    ASSERT_EQ(repair.primitives.size(), primitives.size());
    EXPECT_TRUE(CollectImmediatePrimitiveLineReversals(repair.primitives).empty());
    ASSERT_GE(repair.primitives.size(), 3u);
    EXPECT_EQ(repair.primitives[2].line.start, Point2D(10.0f, 0.0f));
    EXPECT_EQ(repair.primitives[2].line.end, Point2D(20.0f, 0.0f));
}
