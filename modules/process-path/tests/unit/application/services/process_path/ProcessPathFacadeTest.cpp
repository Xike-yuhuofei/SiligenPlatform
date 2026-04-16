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

std::size_t CountPointSegments(const Siligen::ProcessPath::Contracts::ProcessPath& path) {
    std::size_t count = 0;
    for (const auto& segment : path.segments) {
        if (segment.geometry.is_point) {
            ++count;
        }
    }
    return count;
}

bool HasPointSegmentAt(const Siligen::ProcessPath::Contracts::ProcessPath& path,
                       const Point2D& target,
                       float tolerance) {
    for (const auto& segment : path.segments) {
        if (!segment.geometry.is_point) {
            continue;
        }
        if (segment.geometry.line.start.DistanceTo(target) <= tolerance &&
            segment.geometry.line.end.DistanceTo(target) <= tolerance) {
            return true;
        }
    }
    return false;
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

TEST(ProcessPathFacadeTest, SupportsPointOnlyInputAndPreservesPointSegment) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::PathGenerationStatus;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.primitives.push_back(Primitive::MakePoint(Point2D(1.0f, 2.0f)));

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_EQ(result.status, PathGenerationStatus::Success);
    EXPECT_EQ(result.failed_stage, Siligen::ProcessPath::Contracts::PathGenerationStage::None);
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_EQ(result.normalized.report.point_primitive_count, 1);
    EXPECT_EQ(result.normalized.report.consumable_segment_count, 0);
    ASSERT_EQ(result.normalized.path.segments.size(), 1U);
    EXPECT_TRUE(result.normalized.path.segments.front().is_point);
    EXPECT_EQ(result.normalized.path.segments.front().line.start, Point2D(1.0f, 2.0f));
    ASSERT_EQ(result.process_path.segments.size(), 1U);
    EXPECT_TRUE(result.process_path.segments.front().geometry.is_point);
    EXPECT_TRUE(result.process_path.segments.front().dispense_on);
    EXPECT_EQ(result.process_path.segments.front().geometry.line.start, Point2D(1.0f, 2.0f));
    ASSERT_EQ(result.shaped_path.segments.size(), 1U);
    EXPECT_TRUE(result.shaped_path.segments.front().geometry.is_point);
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
    request.topology_repair.policy = Siligen::ProcessPath::Contracts::TopologyRepairPolicy::Off;
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
    request.topology_repair.policy = Siligen::ProcessPath::Contracts::TopologyRepairPolicy::Auto;
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

TEST(ProcessPathFacadeTest, RepairAutoRequiresMetadataForEveryPrimitive) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::PathGenerationStage;
    using Siligen::ProcessPath::Contracts::PathGenerationStatus;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.normalization.continuity_tolerance = 0.1f;
    request.topology_repair.policy = Siligen::ProcessPath::Contracts::TopologyRepairPolicy::Auto;
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(5.0f, 0.0f)));
    request.primitives.push_back(Primitive::MakeLine(Point2D(5.2f, 0.0f), Point2D(10.0f, 0.0f)));

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_EQ(result.status, PathGenerationStatus::InvalidInput);
    EXPECT_EQ(result.failed_stage, PathGenerationStage::InputValidation);
    EXPECT_EQ(result.error_message, "topology repair policy Auto requires metadata for every primitive");
    EXPECT_TRUE(result.topology_diagnostics.repair_requested);
    EXPECT_FALSE(result.topology_diagnostics.repair_applied);
    EXPECT_FALSE(result.topology_diagnostics.metadata_valid);
    EXPECT_TRUE(result.process_path.segments.empty());
    EXPECT_TRUE(result.shaped_path.segments.empty());
}

TEST(ProcessPathFacadeTest, RepairOffAllowsMissingMetadataToFlowThrough) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::PathGenerationStatus;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.normalization.continuity_tolerance = 0.1f;
    request.topology_repair.policy = Siligen::ProcessPath::Contracts::TopologyRepairPolicy::Off;
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(5.0f, 0.0f)));
    request.primitives.push_back(Primitive::MakeLine(Point2D(5.2f, 0.0f), Point2D(10.0f, 0.0f)));

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_EQ(result.status, PathGenerationStatus::Success);
    EXPECT_FALSE(result.topology_diagnostics.repair_requested);
    EXPECT_FALSE(result.topology_diagnostics.repair_applied);
    EXPECT_FALSE(result.topology_diagnostics.metadata_valid);
    EXPECT_TRUE(result.process_path.segments.size() >= 2u);
}

TEST(ProcessPathFacadeTest, PreservesPointNoiseThroughTopologyRepairPipeline) {
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
    using Siligen::ProcessPath::Contracts::Primitive;
    using Siligen::Shared::Types::Point2D;

    ProcessPathBuildRequest request;
    request.normalization.continuity_tolerance = 0.1f;
    request.process.rapid_gap_threshold = 0.1f;
    request.topology_repair.policy = Siligen::ProcessPath::Contracts::TopologyRepairPolicy::Auto;
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

    EXPECT_EQ(result.status, Siligen::ProcessPath::Contracts::PathGenerationStatus::Success);
    EXPECT_EQ(result.failed_stage, Siligen::ProcessPath::Contracts::PathGenerationStage::None);
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_TRUE(result.topology_diagnostics.repair_requested);
    EXPECT_EQ(result.normalized.report.point_primitive_count, 2);
    EXPECT_EQ(CountPointSegments(result.process_path), 2U);
    EXPECT_EQ(CountPointSegments(result.shaped_path), 2U);
    EXPECT_TRUE(HasPointSegmentAt(result.process_path, Point2D(5.0f, 5.0f), 1e-4f));
    EXPECT_TRUE(HasPointSegmentAt(result.process_path, Point2D(6.0f, 6.0f), 1e-4f));
    EXPECT_TRUE(HasPointSegmentAt(result.shaped_path, Point2D(5.0f, 5.0f), 1e-4f));
    EXPECT_TRUE(HasPointSegmentAt(result.shaped_path, Point2D(6.0f, 6.0f), 1e-4f));
}

