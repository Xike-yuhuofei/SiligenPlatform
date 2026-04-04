#include "TopologySpanSplitter.h"

#include "process_path/contracts/GeometryUtils.h"

#include <utility>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::ProcessPath::Contracts::SegmentEnd;
using Siligen::ProcessPath::Contracts::SegmentStart;

namespace {

constexpr float32 kDefaultVertexToleranceMm = 1e-4f;

bool PointsNear(const Point2D& lhs, const Point2D& rhs, float32 tolerance_mm) {
    return lhs.DistanceTo(rhs) <= tolerance_mm;
}

bool ContainsPoint(
    const std::vector<Point2D>& points,
    const Point2D& point,
    float32 tolerance_mm) {
    for (const auto& existing : points) {
        if (PointsNear(existing, point, tolerance_mm)) {
            return true;
        }
    }
    return false;
}

void AppendStrongAnchorCandidate(
    TopologySpanSlice& span,
    const Point2D& position,
    float32 distance_mm_span,
    std::size_t source_segment_index,
    StrongAnchorRole role,
    float32 tolerance_mm) {
    for (const auto& existing : span.strong_anchor_candidates) {
        if (existing.role == role &&
            existing.source_segment_index == source_segment_index &&
            PointsNear(existing.position, position, tolerance_mm)) {
            return;
        }
    }

    TopologyStrongAnchorCandidate candidate;
    candidate.position = position;
    candidate.distance_mm_span = distance_mm_span;
    candidate.source_segment_index = source_segment_index;
    candidate.role = role;
    span.strong_anchor_candidates.push_back(candidate);
}

std::vector<TopologySpanSlice> ExtractDispenseCandidates(
    const ProcessPath& path,
    const std::vector<float32>& segment_lengths_mm) {
    std::vector<TopologySpanSlice> spans;
    TopologySpanSlice current;
    float32 accumulated_path_length_mm = 0.0f;
    for (std::size_t index = 0; index < path.segments.size(); ++index) {
        const auto& process_segment = path.segments[index];
        const float32 length_mm = index < segment_lengths_mm.size() ? segment_lengths_mm[index] : 0.0f;
        if (process_segment.dispense_on) {
            if (current.segments.empty()) {
                current.start_distance_mm = accumulated_path_length_mm;
            }
            current.segments.push_back(process_segment);
            current.source_segment_indices.push_back(index);
            current.segment_lengths_mm.push_back(length_mm);
        } else if (!current.segments.empty()) {
            spans.push_back(std::move(current));
            current = TopologySpanSlice{};
        }
        accumulated_path_length_mm += length_mm;
    }

    if (!current.segments.empty()) {
        spans.push_back(std::move(current));
    }

    if (spans.size() > 1U) {
        for (auto& span : spans) {
            if (span.split_reason == DispenseSpanSplitReason::None) {
                span.split_reason = DispenseSpanSplitReason::ExplicitProcessBoundary;
            }
        }
    }

    return spans;
}

std::vector<TopologySpanSlice> SplitCandidateOnBranchRevisit(
    const TopologySpanSlice& candidate,
    float32 tolerance_mm,
    bool& branch_revisit_split_applied) {
    std::vector<TopologySpanSlice> result;
    if (candidate.segments.empty()) {
        return result;
    }

    TopologySpanSlice current;
    current.start_distance_mm = candidate.start_distance_mm;

    float32 consumed_length_mm = 0.0f;
    float32 current_length_mm = 0.0f;
    std::vector<Point2D> visited_points;
    visited_points.push_back(SegmentStart(candidate.segments.front().geometry));

    for (std::size_t index = 0; index < candidate.segments.size(); ++index) {
        current.segments.push_back(candidate.segments[index]);
        current.source_segment_indices.push_back(candidate.source_segment_indices[index]);
        current.segment_lengths_mm.push_back(candidate.segment_lengths_mm[index]);
        consumed_length_mm += candidate.segment_lengths_mm[index];
        current_length_mm += candidate.segment_lengths_mm[index];

        const Point2D boundary = SegmentEnd(candidate.segments[index].geometry);
        const bool has_next = index + 1 < candidate.segments.size();
        const bool revisit_boundary = has_next && ContainsPoint(visited_points, boundary, tolerance_mm);
        visited_points.push_back(boundary);

        if (!revisit_boundary) {
            continue;
        }

        branch_revisit_split_applied = true;
        current.split_reason = DispenseSpanSplitReason::BranchOrRevisit;
        AppendStrongAnchorCandidate(
            current,
            boundary,
            current_length_mm,
            current.source_segment_indices.back(),
            StrongAnchorRole::SplitBoundary,
            tolerance_mm);
        result.push_back(std::move(current));

        current = TopologySpanSlice{};
        current.start_distance_mm = candidate.start_distance_mm + consumed_length_mm;
        current.split_reason = DispenseSpanSplitReason::BranchOrRevisit;
        AppendStrongAnchorCandidate(
            current,
            boundary,
            0.0f,
            candidate.source_segment_indices[index + 1],
            StrongAnchorRole::SplitBoundary,
            tolerance_mm);
        current_length_mm = 0.0f;
        visited_points.clear();
        visited_points.push_back(boundary);
    }

    if (!current.segments.empty()) {
        result.push_back(std::move(current));
    }
    return result;
}

}  // namespace

TopologySpanSplitterResult TopologySpanSplitter::Split(const TopologySpanSplitterRequest& request) const {
    TopologySpanSplitterResult result;
    result.topology_dispatch_applied = true;

    const float32 tolerance_mm =
        request.vertex_tolerance_mm > 0.0f ? request.vertex_tolerance_mm : kDefaultVertexToleranceMm;

    const auto candidates = ExtractDispenseCandidates(request.process_path, request.segment_lengths_mm);
    result.spans.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        if (!request.enable_branch_revisit_split || candidate.segments.size() < 2U) {
            result.spans.push_back(candidate);
            continue;
        }

        auto split_spans = SplitCandidateOnBranchRevisit(
            candidate,
            tolerance_mm,
            result.branch_revisit_split_applied);
        result.spans.insert(result.spans.end(), split_spans.begin(), split_spans.end());
    }

    return result;
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
