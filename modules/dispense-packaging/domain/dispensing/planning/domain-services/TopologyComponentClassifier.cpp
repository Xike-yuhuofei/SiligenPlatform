#include "TopologyComponentClassifier.h"

#include "process_path/contracts/GeometryUtils.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>
#include <utility>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::ProcessPath::Contracts::SegmentEnd;
using Siligen::ProcessPath::Contracts::SegmentStart;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::Shared::Types::Point2D;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpanSplitReason;

namespace {

constexpr float32 kDefaultVertexToleranceMm = 1e-4f;
constexpr float32 kEpsilon = 1e-6f;
constexpr char kMixedExplicitBoundaryWithReorderedBranchFamily[] =
    "mixed_explicit_boundary_with_reordered_branch_family";

struct SplitChild {
    TopologySpanSlice span;
    float32 offset_mm = 0.0f;
    float32 total_length_mm = 0.0f;
};

struct ComponentDispatchResolution {
    TopologyDispatchType dispatch_type = TopologyDispatchType::UnsupportedMixedTopology;
    std::string blocking_reason;
};

bool CandidateBelongsToSpanGeometry(
    const TopologySpanSlice& span,
    const Point2D& position,
    float32 tolerance_mm);

bool PointsNear(const Point2D& lhs, const Point2D& rhs, float32 tolerance_mm) {
    return lhs.DistanceTo(rhs) <= tolerance_mm;
}

void AppendPointIfDistinct(
    std::vector<Point2D>& points,
    const Point2D& candidate,
    float32 tolerance_mm) {
    const auto it = std::find_if(
        points.begin(),
        points.end(),
        [&](const auto& point) { return PointsNear(point, candidate, tolerance_mm); });
    if (it == points.end()) {
        points.push_back(candidate);
    }
}

bool SpanIsClosed(const TopologySpanSlice& span, float32 tolerance_mm) {
    if (span.segments.empty()) {
        return false;
    }
    if (span.segments.size() == 1U &&
        span.segments.front().geometry.type == SegmentType::Spline &&
        span.segments.front().geometry.spline.closed) {
        return true;
    }
    return SegmentStart(span.segments.front().geometry).DistanceTo(
               SegmentEnd(span.segments.back().geometry)) <= tolerance_mm;
}

float32 ComputeLength(const TopologySpanSlice& span) {
    return std::accumulate(
        span.segment_lengths_mm.begin(),
        span.segment_lengths_mm.end(),
        0.0f);
}

std::vector<SplitChild> SplitOnDisconnectedGeometry(
    const TopologySpanSlice& span,
    float32 tolerance_mm,
    bool& split_applied) {
    std::vector<SplitChild> children;
    if (span.segments.empty()) {
        return children;
    }

    SplitChild current;
    current.offset_mm = 0.0f;
    current.span.start_distance_mm = span.start_distance_mm;

    float32 accumulated_length_mm = 0.0f;
    for (std::size_t index = 0; index < span.segments.size(); ++index) {
        current.span.segments.push_back(span.segments[index]);
        current.span.source_segment_indices.push_back(span.source_segment_indices[index]);
        current.span.segment_lengths_mm.push_back(span.segment_lengths_mm[index]);
        current.total_length_mm += span.segment_lengths_mm[index];
        accumulated_length_mm += span.segment_lengths_mm[index];

        const bool split_here =
            index + 1U < span.segments.size() &&
            !PointsNear(
                SegmentEnd(span.segments[index].geometry),
                SegmentStart(span.segments[index + 1U].geometry),
                tolerance_mm);
        if (!split_here) {
            continue;
        }

        split_applied = true;
        children.push_back(std::move(current));
        current = SplitChild{};
        current.offset_mm = accumulated_length_mm;
        current.span.start_distance_mm = span.start_distance_mm + accumulated_length_mm;
    }

    if (!current.span.segments.empty()) {
        children.push_back(std::move(current));
    }

    const bool derived_from_multi_contour =
        children.size() > 1U && span.split_reason != DispenseSpanSplitReason::BranchOrRevisit;

    for (auto& child : children) {
        child.span.split_reason = derived_from_multi_contour
            ? DispenseSpanSplitReason::MultiContourBoundary
            : span.split_reason;
    }

    for (const auto& candidate : span.strong_anchor_candidates) {
        for (auto& child : children) {
            const auto segment_it = std::find(
                child.span.source_segment_indices.begin(),
                child.span.source_segment_indices.end(),
                candidate.source_segment_index);
            if (segment_it == child.span.source_segment_indices.end()) {
                continue;
            }

            if (CandidateBelongsToSpanGeometry(child.span, candidate.position, tolerance_mm)) {
                auto reassigned = candidate;
                reassigned.distance_mm_span = std::max(
                    0.0f,
                    std::min(candidate.distance_mm_span - child.offset_mm, child.total_length_mm));
                child.span.strong_anchor_candidates.push_back(reassigned);
            }
            break;
        }
    }

    return children;
}

std::vector<Point2D> CollectSpanVertices(const TopologySpanSlice& span, float32 tolerance_mm) {
    std::vector<Point2D> vertices;
    for (const auto& segment : span.segments) {
        AppendPointIfDistinct(vertices, SegmentStart(segment.geometry), tolerance_mm);
        AppendPointIfDistinct(vertices, SegmentEnd(segment.geometry), tolerance_mm);
    }
    return vertices;
}

bool SharesVertex(
    const std::vector<Point2D>& lhs,
    const std::vector<Point2D>& rhs,
    float32 tolerance_mm) {
    for (const auto& lhs_point : lhs) {
        for (const auto& rhs_point : rhs) {
            if (PointsNear(lhs_point, rhs_point, tolerance_mm)) {
                return true;
            }
        }
    }
    return false;
}

bool CandidateBelongsToSpanGeometry(
    const TopologySpanSlice& span,
    const Point2D& position,
    float32 tolerance_mm) {
    for (const auto& segment : span.segments) {
        if (PointsNear(SegmentStart(segment.geometry), position, tolerance_mm) ||
            PointsNear(SegmentEnd(segment.geometry), position, tolerance_mm)) {
            return true;
        }
    }
    return false;
}

std::size_t FindRoot(std::vector<std::size_t>& parents, std::size_t node) {
    while (parents[node] != node) {
        parents[node] = parents[parents[node]];
        node = parents[node];
    }
    return node;
}

void UnionRoots(std::vector<std::size_t>& parents, std::size_t lhs, std::size_t rhs) {
    const std::size_t lhs_root = FindRoot(parents, lhs);
    const std::size_t rhs_root = FindRoot(parents, rhs);
    if (lhs_root != rhs_root) {
        parents[rhs_root] = lhs_root;
    }
}

bool AnySpanUsesSplitReason(
    const std::vector<TopologySpanSlice>& spans,
    DispenseSpanSplitReason split_reason) {
    return std::any_of(
        spans.begin(),
        spans.end(),
        [&](const auto& span) { return span.split_reason == split_reason; });
}

bool AllSpansUseSplitReason(
    const std::vector<TopologySpanSlice>& spans,
    DispenseSpanSplitReason split_reason) {
    return !spans.empty() &&
        std::all_of(
            spans.begin(),
            spans.end(),
            [&](const auto& span) { return span.split_reason == split_reason; });
}

ComponentDispatchResolution ResolveComponentDispatch(
    const std::vector<TopologySpanSlice>& spans,
    float32 tolerance_mm) {
    if (spans.empty()) {
        return {};
    }
    if (spans.size() == 1U) {
        return {
            SpanIsClosed(spans.front(), tolerance_mm)
                ? TopologyDispatchType::SingleClosedLoop
                : TopologyDispatchType::SingleOpenChain,
            {},
        };
    }

    if (AnySpanUsesSplitReason(spans, DispenseSpanSplitReason::BranchOrRevisit)) {
        return {TopologyDispatchType::BranchOrRevisit, {}};
    }

    if (AllSpansUseSplitReason(spans, DispenseSpanSplitReason::MultiContourBoundary)) {
        return {TopologyDispatchType::BranchOrRevisit, {}};
    }

    const bool has_explicit_boundary =
        AnySpanUsesSplitReason(spans, DispenseSpanSplitReason::ExplicitProcessBoundary);
    const bool has_unsplit_open_chain =
        AnySpanUsesSplitReason(spans, DispenseSpanSplitReason::None);
    const bool only_explicit_boundary_family =
        has_explicit_boundary &&
        has_unsplit_open_chain &&
        std::all_of(
            spans.begin(),
            spans.end(),
            [](const auto& span) {
                return span.split_reason == DispenseSpanSplitReason::None ||
                    span.split_reason == DispenseSpanSplitReason::ExplicitProcessBoundary;
            });
    if (only_explicit_boundary_family) {
        // A single open-chain candidate can share a vertex with rapid-separated
        // explicit boundary spans in the same connected component. The component
        // is still one explicit process-boundary family and should preserve that
        // dispatch instead of being blocked as a mixed topology.
        return {TopologyDispatchType::ExplicitProcessBoundary, {}};
    }

    if (AllSpansUseSplitReason(spans, DispenseSpanSplitReason::ExplicitProcessBoundary)) {
        return {TopologyDispatchType::ExplicitProcessBoundary, {}};
    }

    ComponentDispatchResolution resolution;
    resolution.dispatch_type = TopologyDispatchType::UnsupportedMixedTopology;
    resolution.blocking_reason = kMixedExplicitBoundaryWithReorderedBranchFamily;
    return resolution;
}

bool IsAuxiliaryGeometryCandidate(
    const TopologyComponent& component,
    float32 tolerance_mm,
    float32 min_spacing_mm) {
    if (component.dispatch_type != TopologyDispatchType::SingleOpenChain ||
        component.spans.size() != 1U) {
        return false;
    }

    const auto& span = component.spans.front();
    if (span.split_reason == DispenseSpanSplitReason::BranchOrRevisit ||
        SpanIsClosed(span, tolerance_mm)) {
        return false;
    }

    return ComputeLength(span) + kEpsilon < min_spacing_mm;
}

}  // namespace

TopologyComponentClassifierResult TopologyComponentClassifier::Classify(
    const TopologyComponentClassifierRequest& request) const {
    TopologyComponentClassifierResult result;

    const float32 tolerance_mm =
        request.vertex_tolerance_mm > 0.0f ? request.vertex_tolerance_mm : kDefaultVertexToleranceMm;

    std::vector<TopologySpanSlice> refined_spans;
    for (const auto& span : request.spans) {
        auto children = SplitOnDisconnectedGeometry(
            span,
            tolerance_mm,
            result.multi_contour_split_applied);
        for (auto& child : children) {
            refined_spans.push_back(std::move(child.span));
        }
    }

    if (refined_spans.empty()) {
        result.dispatch_type = TopologyDispatchType::UnsupportedMixedTopology;
        return result;
    }

    std::vector<std::vector<Point2D>> span_vertices;
    span_vertices.reserve(refined_spans.size());
    for (const auto& span : refined_spans) {
        span_vertices.push_back(CollectSpanVertices(span, tolerance_mm));
    }

    std::vector<std::size_t> parents(refined_spans.size());
    for (std::size_t index = 0; index < parents.size(); ++index) {
        parents[index] = index;
    }
    for (std::size_t lhs = 0; lhs < refined_spans.size(); ++lhs) {
        for (std::size_t rhs = lhs + 1U; rhs < refined_spans.size(); ++rhs) {
            if (SharesVertex(span_vertices[lhs], span_vertices[rhs], tolerance_mm)) {
                UnionRoots(parents, lhs, rhs);
            }
        }
    }

    struct Grouping {
        std::vector<std::size_t> span_indices;
        float32 earliest_start_distance_mm = 0.0f;
        std::size_t earliest_source_segment_index = 0;
    };

    std::unordered_map<std::size_t, Grouping> grouped;
    for (std::size_t index = 0; index < refined_spans.size(); ++index) {
        const auto root = FindRoot(parents, index);
        auto& grouping = grouped[root];
        grouping.span_indices.push_back(index);
        const auto earliest_source = refined_spans[index].source_segment_indices.empty()
            ? 0U
            : refined_spans[index].source_segment_indices.front();
        if (grouping.span_indices.size() == 1U ||
            refined_spans[index].start_distance_mm < grouping.earliest_start_distance_mm ||
            (std::abs(refined_spans[index].start_distance_mm - grouping.earliest_start_distance_mm) <= kEpsilon &&
             earliest_source < grouping.earliest_source_segment_index)) {
            grouping.earliest_start_distance_mm = refined_spans[index].start_distance_mm;
            grouping.earliest_source_segment_index = earliest_source;
        }
    }

    std::vector<Grouping> ordered_groups;
    ordered_groups.reserve(grouped.size());
    for (auto& [root, grouping] : grouped) {
        std::sort(
            grouping.span_indices.begin(),
            grouping.span_indices.end(),
            [&](std::size_t lhs, std::size_t rhs) {
                if (std::abs(refined_spans[lhs].start_distance_mm - refined_spans[rhs].start_distance_mm) > kEpsilon) {
                    return refined_spans[lhs].start_distance_mm < refined_spans[rhs].start_distance_mm;
                }
                const auto lhs_source = refined_spans[lhs].source_segment_indices.empty()
                    ? 0U
                    : refined_spans[lhs].source_segment_indices.front();
                const auto rhs_source = refined_spans[rhs].source_segment_indices.empty()
                    ? 0U
                    : refined_spans[rhs].source_segment_indices.front();
                return lhs_source < rhs_source;
            });
        ordered_groups.push_back(std::move(grouping));
    }

    std::sort(
        ordered_groups.begin(),
        ordered_groups.end(),
        [](const auto& lhs, const auto& rhs) {
            if (std::abs(lhs.earliest_start_distance_mm - rhs.earliest_start_distance_mm) > kEpsilon) {
                return lhs.earliest_start_distance_mm < rhs.earliest_start_distance_mm;
            }
            return lhs.earliest_source_segment_index < rhs.earliest_source_segment_index;
        });

    result.components.reserve(ordered_groups.size());
    for (const auto& grouping : ordered_groups) {
        TopologyComponent component;
        for (const auto span_index : grouping.span_indices) {
            component.spans.push_back(refined_spans[span_index]);
            component.source_segment_indices.insert(
                component.source_segment_indices.end(),
                refined_spans[span_index].source_segment_indices.begin(),
                refined_spans[span_index].source_segment_indices.end());
        }
        const auto dispatch_resolution = ResolveComponentDispatch(component.spans, tolerance_mm);
        component.dispatch_type = dispatch_resolution.dispatch_type;
        component.blocking_reason = dispatch_resolution.blocking_reason;
        if (component.spans.size() == 1U &&
            component.spans.front().split_reason == DispenseSpanSplitReason::BranchOrRevisit &&
            component.dispatch_type != TopologyDispatchType::BranchOrRevisit) {
            component.spans.front().split_reason = DispenseSpanSplitReason::MultiContourBoundary;
        }
        result.components.push_back(std::move(component));
    }

    std::vector<bool> auxiliary_candidates(result.components.size(), false);
    bool has_primary_component = false;
    for (std::size_t index = 0; index < result.components.size(); ++index) {
        auxiliary_candidates[index] = IsAuxiliaryGeometryCandidate(
            result.components[index],
            tolerance_mm,
            request.min_spacing_mm);
        if (!auxiliary_candidates[index]) {
            has_primary_component = true;
        }
    }

    for (std::size_t index = 0; index < result.components.size(); ++index) {
        auto& component = result.components[index];
        if (has_primary_component && auxiliary_candidates[index]) {
            component.ignored = true;
            component.ignored_reason = "auxiliary_open_component_shorter_than_min_spacing";
            component.dispatch_type = TopologyDispatchType::AuxiliaryGeometry;
            ++result.ignored_component_count;
            result.auxiliary_geometry_ignored = true;
            continue;
        }
        ++result.effective_component_count;
    }

    if (result.ignored_component_count > 0U) {
        result.dispatch_type = TopologyDispatchType::AuxiliaryGeometry;
    } else if (result.effective_component_count > 1U) {
        result.dispatch_type = TopologyDispatchType::MultiContour;
    } else if (result.effective_component_count == 1U) {
        const auto it = std::find_if(
            result.components.begin(),
            result.components.end(),
            [](const auto& component) { return !component.ignored; });
        result.dispatch_type = it != result.components.end()
            ? it->dispatch_type
            : TopologyDispatchType::AuxiliaryGeometry;
    } else {
        result.dispatch_type = TopologyDispatchType::AuxiliaryGeometry;
    }

    return result;
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
