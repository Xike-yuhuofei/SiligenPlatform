#include "ClosedLoopCornerAnchorResolver.h"

#include "ClosedLoopPlanningUtils.h"

#include "process_path/contracts/GeometryUtils.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Dispensing::DomainServices::Internal {

using Siligen::Domain::Dispensing::ValueObjects::StrongAnchorRole;
using Siligen::ProcessPath::Contracts::ArcTangent;
using Siligen::ProcessPath::Contracts::LineDirection;
using Siligen::ProcessPath::Contracts::Segment;
using Siligen::ProcessPath::Contracts::SegmentStart;
using Siligen::ProcessPath::Contracts::SegmentType;

namespace {

constexpr float32 kRadToDeg = 57.29577951308232f;

bool PointsNear(const Point2D& lhs, const Point2D& rhs, float32 tolerance_mm) {
    return lhs.DistanceTo(rhs) <= tolerance_mm;
}

std::vector<float32> BuildSegmentStartDistances(const TopologySpanSlice& span) {
    std::vector<float32> distances;
    distances.reserve(span.segment_lengths_mm.size());
    float32 accumulated_mm = 0.0f;
    for (float32 segment_length_mm : span.segment_lengths_mm) {
        distances.push_back(accumulated_mm);
        accumulated_mm += segment_length_mm;
    }
    return distances;
}

Point2D TangentAtSegmentStart(const Segment& segment) {
    switch (segment.type) {
        case SegmentType::Line:
            return LineDirection(segment.line);
        case SegmentType::Arc:
            return ArcTangent(segment.arc, segment.arc.start_angle_deg).Normalized();
        default:
            return Point2D(0.0f, 0.0f);
    }
}

Point2D TangentAtSegmentEnd(const Segment& segment) {
    switch (segment.type) {
        case SegmentType::Line:
            return LineDirection(segment.line);
        case SegmentType::Arc:
            return ArcTangent(segment.arc, segment.arc.end_angle_deg).Normalized();
        default:
            return Point2D(0.0f, 0.0f);
    }
}

float32 ComputeCornerAngleDeg(const Point2D& incoming, const Point2D& outgoing) {
    const Point2D incoming_unit = incoming.Normalized();
    const Point2D outgoing_unit = outgoing.Normalized();
    if (incoming_unit.Length() <= kClosedLoopPlanningEpsilon ||
        outgoing_unit.Length() <= kClosedLoopPlanningEpsilon) {
        return 0.0f;
    }
    const float32 dot = std::clamp(incoming_unit.Dot(outgoing_unit), -1.0f, 1.0f);
    return std::acos(dot) * kRadToDeg;
}

bool SpanContainsSpline(const TopologySpanSlice& span) {
    return std::any_of(
        span.segments.begin(),
        span.segments.end(),
        [](const auto& process_segment) { return process_segment.geometry.type == SegmentType::Spline; });
}

}  // namespace

ClosedLoopCornerResolution ClosedLoopCornerAnchorResolver::Resolve(
    const ClosedLoopCornerAnchorResolverInput& input) const {
    ClosedLoopCornerResolution resolution;
    if (!input.enabled ||
        input.span == nullptr ||
        input.layout_span == nullptr ||
        input.span->segments.size() < 2U ||
        SpanContainsSpline(*input.span) ||
        input.total_length_mm <= kClosedLoopPlanningEpsilon) {
        return resolution;
    }

    const auto segment_start_distances = BuildSegmentStartDistances(*input.span);
    std::vector<ClosedLoopCornerCandidate> candidates;
    candidates.reserve(input.span->segments.size());
    for (std::size_t index = 0; index < input.span->segments.size(); ++index) {
        const auto& previous_segment =
            input.span->segments[(index + input.span->segments.size() - 1U) % input.span->segments.size()].geometry;
        const auto& current_segment = input.span->segments[index].geometry;
        const float32 angle_deg = ComputeCornerAngleDeg(
            TangentAtSegmentEnd(previous_segment),
            TangentAtSegmentStart(current_segment));
        if (angle_deg + kClosedLoopPlanningEpsilon < input.angle_threshold_deg) {
            continue;
        }

        ClosedLoopCornerCandidate candidate;
        candidate.position = SegmentStart(current_segment);
        candidate.distance_mm_span = index < segment_start_distances.size()
            ? segment_start_distances[index]
            : 0.0f;
        candidate.significance_angle_deg = angle_deg;
        candidate.source_segment_index = input.span->source_segment_indices[index];
        candidates.push_back(candidate);
    }

    resolution.candidate_corner_count = candidates.size();
    if (candidates.empty()) {
        return resolution;
    }

    std::vector<ClosedLoopCornerCandidate> filtered_candidates;
    filtered_candidates.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        bool suppressed_by_priority_anchor = false;
        for (const auto& existing_anchor : input.layout_span->strong_anchors) {
            if (existing_anchor.role == StrongAnchorRole::ClosedLoopCorner) {
                continue;
            }
            const float32 anchor_distance_mm =
                WrapLoopDistance(existing_anchor.distance_mm_span, input.total_length_mm);
            const float32 candidate_distance_mm =
                WrapLoopDistance(candidate.distance_mm_span, input.total_length_mm);
            const float32 circular_distance_mm =
                CircularDistance(anchor_distance_mm, candidate_distance_mm, input.total_length_mm);
            if (circular_distance_mm <= input.anchor_tolerance_mm + kClosedLoopPlanningEpsilon ||
                PointsNear(existing_anchor.position, candidate.position, input.vertex_tolerance_mm)) {
                continue;
            }
            if (circular_distance_mm < input.cluster_distance_mm - kClosedLoopPlanningEpsilon) {
                suppressed_by_priority_anchor = true;
                break;
            }
        }

        if (suppressed_by_priority_anchor) {
            ++resolution.suppressed_corner_count;
            ++resolution.dense_corner_cluster_count;
            continue;
        }

        filtered_candidates.push_back(candidate);
    }

    if (filtered_candidates.empty() ||
        input.cluster_distance_mm <= input.anchor_tolerance_mm + kClosedLoopPlanningEpsilon) {
        resolution.accepted_candidates = std::move(filtered_candidates);
        return resolution;
    }

    std::vector<std::size_t> parent(filtered_candidates.size(), 0U);
    for (std::size_t index = 0; index < parent.size(); ++index) {
        parent[index] = index;
    }
    const auto find_root = [&](std::size_t index) {
        std::size_t root = index;
        while (parent[root] != root) {
            root = parent[root];
        }
        while (parent[index] != index) {
            const std::size_t next = parent[index];
            parent[index] = root;
            index = next;
        }
        return root;
    };
    const auto unite = [&](std::size_t lhs, std::size_t rhs) {
        const std::size_t lhs_root = find_root(lhs);
        const std::size_t rhs_root = find_root(rhs);
        if (lhs_root != rhs_root) {
            parent[rhs_root] = lhs_root;
        }
    };

    for (std::size_t index = 0; index + 1U < filtered_candidates.size(); ++index) {
        const float32 gap_mm =
            filtered_candidates[index + 1U].distance_mm_span - filtered_candidates[index].distance_mm_span;
        if (gap_mm < input.cluster_distance_mm - kClosedLoopPlanningEpsilon) {
            unite(index, index + 1U);
        }
    }
    if (filtered_candidates.size() > 1U) {
        const float32 wrap_gap_mm =
            input.total_length_mm - filtered_candidates.back().distance_mm_span +
            filtered_candidates.front().distance_mm_span;
        if (wrap_gap_mm < input.cluster_distance_mm - kClosedLoopPlanningEpsilon) {
            unite(0U, filtered_candidates.size() - 1U);
        }
    }

    std::vector<std::size_t> component_roots;
    std::vector<std::vector<std::size_t>> components;
    for (std::size_t index = 0; index < filtered_candidates.size(); ++index) {
        const std::size_t root = find_root(index);
        auto existing = std::find(component_roots.begin(), component_roots.end(), root);
        if (existing == component_roots.end()) {
            component_roots.push_back(root);
            components.push_back({index});
            continue;
        }
        components[static_cast<std::size_t>(std::distance(component_roots.begin(), existing))].push_back(index);
    }

    for (const auto& component : components) {
        if (component.empty()) {
            continue;
        }
        if (component.size() == 1U) {
            resolution.accepted_candidates.push_back(filtered_candidates[component.front()]);
            continue;
        }

        ++resolution.dense_corner_cluster_count;
        std::size_t best_index = component.front();
        for (std::size_t candidate_index : component) {
            const auto& best = filtered_candidates[best_index];
            const auto& current = filtered_candidates[candidate_index];
            if (current.significance_angle_deg > best.significance_angle_deg + kClosedLoopPlanningEpsilon ||
                (std::fabs(current.significance_angle_deg - best.significance_angle_deg) <=
                     kClosedLoopPlanningEpsilon &&
                 current.distance_mm_span < best.distance_mm_span)) {
                best_index = candidate_index;
            }
        }

        resolution.accepted_candidates.push_back(filtered_candidates[best_index]);
        resolution.suppressed_corner_count += component.size() - 1U;
    }

    std::sort(
        resolution.accepted_candidates.begin(),
        resolution.accepted_candidates.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.distance_mm_span < rhs.distance_mm_span;
        });
    return resolution;
}

}  // namespace Siligen::Domain::Dispensing::DomainServices::Internal
