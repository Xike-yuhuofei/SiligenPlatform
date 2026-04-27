#include "domain/trajectory/domain-services/TopologyRepairService.h"

#include "process_path/contracts/GeometryUtils.h"
#include "shared/geometry/BoostGeometryAdapter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::ProcessPath::Contracts::ArcPoint;
using Siligen::ProcessPath::Contracts::ComputeArcSweep;
using Siligen::ProcessPath::Contracts::EllipsePoint;
using Siligen::ProcessPath::Contracts::NormalizeAngle;
using Siligen::ProcessPath::Contracts::TopologyRepairPolicy;
using Siligen::ProcessPath::Contracts::PrimitiveType;
using Siligen::Shared::Geometry::Distance;
using Siligen::Shared::Types::Point2D;

namespace {

constexpr float32 kEpsilon = 1e-6f;

Point2D ContourElementStartPoint(const Siligen::ProcessPath::Contracts::ContourElement& element) {
    using Siligen::ProcessPath::Contracts::ContourElementType;
    if (element.type == ContourElementType::Line) {
        return element.line.start;
    }
    if (element.type == ContourElementType::Arc) {
        return ArcPoint(element.arc, element.arc.start_angle_deg);
    }
    if (!element.spline.control_points.empty()) {
        return element.spline.control_points.front();
    }
    return {};
}

Point2D ContourElementEndPoint(const Siligen::ProcessPath::Contracts::ContourElement& element) {
    using Siligen::ProcessPath::Contracts::ContourElementType;
    if (element.type == ContourElementType::Line) {
        return element.line.end;
    }
    if (element.type == ContourElementType::Arc) {
        return ArcPoint(element.arc, element.arc.end_angle_deg);
    }
    if (!element.spline.control_points.empty()) {
        return element.spline.control_points.back();
    }
    return {};
}

Point2D PrimitiveStartPoint(const ContractsPrimitive& primitive) {
    switch (primitive.type) {
        case PrimitiveType::Line:
            return primitive.line.start;
        case PrimitiveType::Arc:
            return ArcPoint(primitive.arc, primitive.arc.start_angle_deg);
        case PrimitiveType::Spline:
            if (!primitive.spline.control_points.empty()) {
                return primitive.spline.control_points.front();
            }
            break;
        case PrimitiveType::Circle:
            return ArcPoint(primitive.circle.center, primitive.circle.radius, primitive.circle.start_angle_deg);
        case PrimitiveType::Ellipse:
            return EllipsePoint(primitive.ellipse, primitive.ellipse.start_param);
        case PrimitiveType::Point:
            return primitive.point.position;
        case PrimitiveType::Contour:
            if (!primitive.contour.elements.empty()) {
                return ContourElementStartPoint(primitive.contour.elements.front());
            }
            break;
    }
    return {};
}

Point2D PrimitiveEndPoint(const ContractsPrimitive& primitive) {
    switch (primitive.type) {
        case PrimitiveType::Line:
            return primitive.line.end;
        case PrimitiveType::Arc:
            return ArcPoint(primitive.arc, primitive.arc.end_angle_deg);
        case PrimitiveType::Spline:
            if (!primitive.spline.control_points.empty()) {
                return primitive.spline.control_points.back();
            }
            break;
        case PrimitiveType::Circle:
            return ArcPoint(primitive.circle.center,
                            primitive.circle.radius,
                            primitive.circle.start_angle_deg + 360.0f);
        case PrimitiveType::Ellipse:
            return EllipsePoint(primitive.ellipse, primitive.ellipse.end_param);
        case PrimitiveType::Point:
            return primitive.point.position;
        case PrimitiveType::Contour:
            if (!primitive.contour.elements.empty()) {
                return ContourElementEndPoint(primitive.contour.elements.back());
            }
            break;
    }
    return {};
}

ContractsPrimitive ReversePrimitive(const ContractsPrimitive& primitive) {
    using Siligen::ProcessPath::Contracts::ContourElement;
    using Siligen::ProcessPath::Contracts::ContourElementType;

    ContractsPrimitive reversed = primitive;
    switch (primitive.type) {
        case PrimitiveType::Line:
            reversed.line.start = primitive.line.end;
            reversed.line.end = primitive.line.start;
            break;
        case PrimitiveType::Arc:
            std::swap(reversed.arc.start_angle_deg, reversed.arc.end_angle_deg);
            reversed.arc.clockwise = !reversed.arc.clockwise;
            break;
        case PrimitiveType::Spline:
            std::reverse(reversed.spline.control_points.begin(), reversed.spline.control_points.end());
            break;
        case PrimitiveType::Circle:
            reversed.circle.clockwise = !reversed.circle.clockwise;
            break;
        case PrimitiveType::Ellipse:
            std::swap(reversed.ellipse.start_param, reversed.ellipse.end_param);
            reversed.ellipse.clockwise = !reversed.ellipse.clockwise;
            break;
        case PrimitiveType::Point:
            break;
        case PrimitiveType::Contour: {
            std::vector<ContourElement> elements;
            elements.reserve(primitive.contour.elements.size());
            for (auto it = primitive.contour.elements.rbegin(); it != primitive.contour.elements.rend(); ++it) {
                auto element = *it;
                if (element.type == ContourElementType::Line) {
                    std::swap(element.line.start, element.line.end);
                } else if (element.type == ContourElementType::Arc) {
                    std::swap(element.arc.start_angle_deg, element.arc.end_angle_deg);
                    element.arc.clockwise = !element.arc.clockwise;
                } else if (element.type == ContourElementType::Spline) {
                    std::reverse(element.spline.control_points.begin(), element.spline.control_points.end());
                }
                elements.push_back(std::move(element));
            }
            reversed.contour.elements = std::move(elements);
            break;
        }
    }
    return reversed;
}

int CountPrimitiveDiscontinuity(const std::vector<ContractsPrimitive>& primitives, float32 tolerance) {
    if (primitives.size() < 2) {
        return 0;
    }

    int count = 0;
    for (size_t i = 1; i < primitives.size(); ++i) {
        if (Distance(PrimitiveEndPoint(primitives[i - 1]), PrimitiveStartPoint(primitives[i])) > tolerance) {
            ++count;
        }
    }
    return count;
}

bool IsMetadataValid(const std::vector<ContractsPrimitive>& primitives, const std::vector<PathPrimitiveMeta>& metadata) {
    return !primitives.empty() && metadata.size() == primitives.size();
}

bool WouldReverseCandidateRetraceCurrent(const ContractsPrimitive& current,
                                         const ContractsPrimitive& candidate,
                                         float32 tolerance) {
    if (current.type != PrimitiveType::Line || candidate.type != PrimitiveType::Line) {
        return false;
    }

    const auto current_start = current.line.start;
    const auto current_end = current.line.end;
    const auto candidate_start = candidate.line.start;
    const auto candidate_end = candidate.line.end;

    return Distance(current_end, candidate_end) <= tolerance &&
           Distance(current_start, candidate_start) <= tolerance;
}

bool FormsImmediateLineBacktrack(const ContractsPrimitive& previous,
                                 const ContractsPrimitive& next,
                                 float32 tolerance) {
    if (previous.type != PrimitiveType::Line || next.type != PrimitiveType::Line) {
        return false;
    }

    const auto prev_vec = previous.line.end - previous.line.start;
    const auto next_vec = next.line.end - next.line.start;
    const float32 prev_len = prev_vec.Length();
    const float32 next_len = next_vec.Length();
    if (prev_len <= kEpsilon || next_len <= kEpsilon) {
        return false;
    }

    if (Distance(previous.line.end, next.line.start) > tolerance) {
        return false;
    }

    const float32 normalized_cross = std::abs(prev_vec.Cross(next_vec)) / std::max(kEpsilon, prev_len * next_len);
    const float32 normalized_dot = prev_vec.Dot(next_vec) / (prev_len * next_len);
    return normalized_cross <= 1e-3f && normalized_dot < -0.95f;
}

bool SupportsConnectivityRebuild(const ContractsPrimitive& primitive) {
    switch (primitive.type) {
        case PrimitiveType::Line:
        case PrimitiveType::Arc:
        case PrimitiveType::Spline:
        case PrimitiveType::Circle:
        case PrimitiveType::Ellipse:
        case PrimitiveType::Contour:
            return true;
        case PrimitiveType::Point:
        default:
            return false;
    }
}

bool ShouldRebuildByConnectivity(const std::vector<ContractsPrimitive>& primitives,
                                 const std::vector<PathPrimitiveMeta>& metadata,
                                 int discontinuity_count) {
    if (primitives.size() < 2 || metadata.size() != primitives.size() || discontinuity_count <= 0) {
        return false;
    }

    size_t supported_count = 0;
    for (size_t i = 0; i < primitives.size(); ++i) {
        if (SupportsConnectivityRebuild(primitives[i])) {
            ++supported_count;
            continue;
        }
        if (primitives[i].type != PrimitiveType::Point) {
            return false;
        }
    }
    return supported_count >= 2;
}

struct SplitLineResult {
    std::vector<ContractsPrimitive> primitives;
    std::vector<PathPrimitiveMeta> metadata;
    int original_count = 0;
    int split_count = 0;
    int intersection_pairs = 0;
    int collinear_pairs = 0;
    bool applied = false;
};

SplitLineResult SplitLinePrimitivesByIntersection(const std::vector<ContractsPrimitive>& primitives,
                                                  const std::vector<PathPrimitiveMeta>& metadata,
                                                  float32 tolerance) {
    SplitLineResult result;
    result.original_count = static_cast<int>(primitives.size());
    if (primitives.size() < 2 || metadata.size() != primitives.size()) {
        return result;
    }

    for (const auto& primitive : primitives) {
        if (primitive.type != PrimitiveType::Line) {
            return result;
        }
    }

    const size_t count = primitives.size();
    std::vector<std::vector<Point2D>> split_points(count);
    for (size_t i = 0; i < count; ++i) {
        split_points[i].push_back(primitives[i].line.start);
        split_points[i].push_back(primitives[i].line.end);
    }

    for (size_t i = 0; i < count; ++i) {
        const auto& a = primitives[i].line.start;
        const auto& b = primitives[i].line.end;
        for (size_t j = i + 1; j < count; ++j) {
            const auto& c = primitives[j].line.start;
            const auto& d = primitives[j].line.end;

            bool collinear = false;
            Point2D intersection;
            if (Siligen::Shared::Geometry::ComputeSegmentIntersection(a, b, c, d, tolerance, intersection, collinear)) {
                split_points[i].push_back(intersection);
                split_points[j].push_back(intersection);
                result.intersection_pairs += 1;
                continue;
            }
            if (collinear) {
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
                    result.collinear_pairs += 1;
                }
            }
        }
    }

    int next_entity_id = -1;
    const float32 min_segment_length = std::max(1e-4f, tolerance * 0.5f);
    for (size_t i = 0; i < count; ++i) {
        const Point2D start = primitives[i].line.start;
        const Point2D end = primitives[i].line.end;
        const Point2D dir = end - start;
        const float32 len2 = dir.Dot(dir);
        if (len2 <= kEpsilon) {
            continue;
        }

        std::vector<std::pair<float32, Point2D>> ordered;
        ordered.reserve(split_points[i].size());
        for (const auto& point : split_points[i]) {
            const float32 t = (point - start).Dot(dir) / len2;
            ordered.emplace_back(t, point);
        }
        std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.first < rhs.first;
        });

        std::vector<Point2D> unique_points;
        unique_points.reserve(ordered.size());
        for (const auto& item : ordered) {
            if (unique_points.empty() || Distance(item.second, unique_points.back()) > tolerance) {
                unique_points.push_back(item.second);
            }
        }

        for (size_t index = 1; index < unique_points.size(); ++index) {
            const auto& p0 = unique_points[index - 1];
            const auto& p1 = unique_points[index];
            if (Distance(p0, p1) <= min_segment_length) {
                continue;
            }

            result.primitives.push_back(ContractsPrimitive::MakeLine(p0, p1));
            auto meta = metadata[i];
            meta.entity_id = next_entity_id--;
            meta.entity_segment_index = 0;
            meta.entity_closed = false;
            result.metadata.push_back(meta);
        }
    }

    result.split_count = static_cast<int>(result.primitives.size());
    result.applied = result.split_count > 0 &&
                     (result.split_count != result.original_count ||
                      result.intersection_pairs > 0 ||
                      result.collinear_pairs > 0);
    return result;
}

struct ConnectivityRebuildResult {
    std::vector<ContractsPrimitive> primitives;
    std::vector<PathPrimitiveMeta> metadata;
    int contour_count = 0;
    int reversed_count = 0;
    bool applied = false;
};

ConnectivityRebuildResult RebuildPrimitivesByConnectivity(const std::vector<ContractsPrimitive>& primitives,
                                                          const std::vector<PathPrimitiveMeta>& metadata,
                                                          float32 tolerance) {
    ConnectivityRebuildResult result;
    if (primitives.empty() || metadata.size() != primitives.size()) {
        return result;
    }

    const size_t count = primitives.size();
    std::vector<bool> used(count, false);
    result.primitives.reserve(count);
    result.metadata.reserve(count);

    int contour_id = 0;
    std::vector<size_t> deferred_standalone_indices;
    for (size_t i = 0; i < count; ++i) {
        if (used[i]) {
            continue;
        }
        if (!SupportsConnectivityRebuild(primitives[i])) {
            used[i] = true;
            deferred_standalone_indices.push_back(i);
            continue;
        }

        const size_t contour_start = result.primitives.size();
        ContractsPrimitive current = primitives[i];
        Point2D contour_start_point = PrimitiveStartPoint(current);
        Point2D current_end = PrimitiveEndPoint(current);

        used[i] = true;
        PathPrimitiveMeta contour_meta = metadata[i];
        contour_meta.entity_id = -(contour_id + 1);
        contour_meta.entity_segment_index = 0;
        contour_meta.entity_closed = false;
        result.primitives.push_back(current);
        result.metadata.push_back(contour_meta);

        int segment_index = 1;
        while (true) {
            size_t best_index = count;
            float32 best_distance = std::numeric_limits<float32>::max();
            bool best_reverse = false;

            for (size_t j = 0; j < count; ++j) {
                if (used[j]) {
                    continue;
                }
                if (!SupportsConnectivityRebuild(primitives[j])) {
                    continue;
                }

                const float32 start_distance = Distance(current_end, PrimitiveStartPoint(primitives[j]));
                const float32 end_distance = Distance(current_end, PrimitiveEndPoint(primitives[j]));
                const bool can_forward = start_distance <= tolerance;
                const bool can_reverse = end_distance <= tolerance;
                if (!can_forward && !can_reverse) {
                    continue;
                }
                if (!can_forward && can_reverse &&
                    WouldReverseCandidateRetraceCurrent(current, primitives[j], tolerance)) {
                    continue;
                }

                const float32 candidate_distance = std::min(start_distance, end_distance);
                if (candidate_distance < best_distance) {
                    best_distance = candidate_distance;
                    best_index = j;
                    best_reverse = (!can_forward || end_distance < start_distance);
                }
            }

            if (best_index == count) {
                break;
            }

            ContractsPrimitive next = primitives[best_index];
            if (best_reverse) {
                next = ReversePrimitive(next);
                result.reversed_count += 1;
            }

            used[best_index] = true;
            current_end = PrimitiveEndPoint(next);

            PathPrimitiveMeta next_meta = metadata[best_index];
            next_meta.entity_id = -(contour_id + 1);
            next_meta.entity_segment_index = segment_index++;
            next_meta.entity_closed = false;

            result.primitives.push_back(next);
            result.metadata.push_back(next_meta);
        }

        const bool closed = Distance(contour_start_point, current_end) <= tolerance;
        for (size_t index = contour_start; index < result.metadata.size(); ++index) {
            result.metadata[index].entity_closed = closed;
        }
        ++contour_id;
    }

    // POINT primitives are standalone process semantics. They must not veto
    // connectivity rebuild for path-carrying geometry, and they must not be
    // silently folded into a carrier path.
    for (const auto index : deferred_standalone_indices) {
        result.primitives.push_back(primitives[index]);
        result.metadata.push_back(metadata[index]);
    }

    result.contour_count = contour_id;
    result.applied = result.primitives.size() == primitives.size() &&
                     (result.reversed_count > 0 ||
                      !std::equal(result.primitives.begin(), result.primitives.end(), primitives.begin(),
                                  [](const ContractsPrimitive& lhs, const ContractsPrimitive& rhs) {
                                      return PrimitiveStartPoint(lhs) == PrimitiveStartPoint(rhs) &&
                                             PrimitiveEndPoint(lhs) == PrimitiveEndPoint(rhs);
                                  }));
    return result;
}

struct Contour {
    int entity_id = -1;
    bool closed = false;
    size_t original_index = 0;
    std::vector<ContractsPrimitive> primitives;
    std::vector<PathPrimitiveMeta> metadata;
};

Point2D ContourStartPoint(const Contour& contour) {
    if (contour.primitives.empty()) {
        return {};
    }
    return PrimitiveStartPoint(contour.primitives.front());
}

Point2D ContourEndPoint(const Contour& contour) {
    if (contour.primitives.empty()) {
        return {};
    }
    return PrimitiveEndPoint(contour.primitives.back());
}

size_t FindClosestStartIndex(const Contour& contour, const Point2D& current, float32* out_distance = nullptr) {
    size_t best_index = 0;
    float32 best_distance = std::numeric_limits<float32>::max();
    for (size_t i = 0; i < contour.primitives.size(); ++i) {
        const float32 distance = Distance(PrimitiveStartPoint(contour.primitives[i]), current);
        if (distance < best_distance) {
            best_distance = distance;
            best_index = i;
        }
    }
    if (out_distance != nullptr) {
        *out_distance = best_distance;
    }
    return best_index;
}

void ReindexContourMetadata(Contour& contour) {
    for (size_t i = 0; i < contour.metadata.size(); ++i) {
        contour.metadata[i].entity_segment_index = static_cast<int>(i);
        contour.metadata[i].entity_closed = contour.closed;
    }
}

void ReverseContour(Contour& contour) {
    std::reverse(contour.primitives.begin(), contour.primitives.end());
    std::reverse(contour.metadata.begin(), contour.metadata.end());
    for (auto& primitive : contour.primitives) {
        primitive = ReversePrimitive(primitive);
    }
    ReindexContourMetadata(contour);
}

void RotateContour(Contour& contour, size_t start_index) {
    if (start_index == 0 || start_index >= contour.primitives.size()) {
        return;
    }
    std::rotate(contour.primitives.begin(),
                contour.primitives.begin() + static_cast<std::ptrdiff_t>(start_index),
                contour.primitives.end());
    std::rotate(contour.metadata.begin(),
                contour.metadata.begin() + static_cast<std::ptrdiff_t>(start_index),
                contour.metadata.end());
    ReindexContourMetadata(contour);
}

std::vector<Contour> BuildContoursFromMetadata(const std::vector<ContractsPrimitive>& primitives,
                                               const std::vector<PathPrimitiveMeta>& metadata) {
    std::vector<Contour> contours;
    if (primitives.empty() || metadata.size() != primitives.size()) {
        return contours;
    }

    std::unordered_map<int, size_t> contour_index_by_entity;
    contour_index_by_entity.reserve(metadata.size());

    for (size_t i = 0; i < primitives.size(); ++i) {
        const int entity_id = metadata[i].entity_id;
        size_t contour_index = contours.size();
        auto found = contour_index_by_entity.find(entity_id);
        if (found != contour_index_by_entity.end()) {
            contour_index = found->second;
        } else {
            Contour contour;
            contour.entity_id = entity_id;
            contour.closed = metadata[i].entity_closed;
            contour.original_index = contours.size();
            contours.push_back(std::move(contour));
            contour_index_by_entity.emplace(entity_id, contour_index);
        }

        contours[contour_index].closed = contours[contour_index].closed || metadata[i].entity_closed;
        contours[contour_index].primitives.push_back(primitives[i]);
        contours[contour_index].metadata.push_back(metadata[i]);
    }

    for (auto& contour : contours) {
        std::vector<size_t> order(contour.metadata.size());
        for (size_t i = 0; i < order.size(); ++i) {
            order[i] = i;
        }
        std::stable_sort(order.begin(), order.end(), [&](size_t lhs, size_t rhs) {
            return contour.metadata[lhs].entity_segment_index < contour.metadata[rhs].entity_segment_index;
        });

        std::vector<ContractsPrimitive> sorted_primitives;
        std::vector<PathPrimitiveMeta> sorted_metadata;
        sorted_primitives.reserve(order.size());
        sorted_metadata.reserve(order.size());
        for (const auto index : order) {
            sorted_primitives.push_back(contour.primitives[index]);
            sorted_metadata.push_back(contour.metadata[index]);
        }
        contour.primitives = std::move(sorted_primitives);
        contour.metadata = std::move(sorted_metadata);
        ReindexContourMetadata(contour);
    }

    return contours;
}

struct ContourOrderResult {
    std::vector<ContractsPrimitive> primitives;
    std::vector<PathPrimitiveMeta> metadata;
    int reordered_contours = 0;
    int reversed_contours = 0;
    int rotated_contours = 0;
    bool applied = false;
};

struct SegmentWithDirection {
    size_t index = 0;
    bool reversed = false;
};

struct ContourSegmentProxy {
    Point2D start_point{};
    Point2D end_point{};
};

float32 DistanceCost(const Point2D& a, const Point2D& b) {
    return Distance(a, b);
}

std::vector<SegmentWithDirection> OptimizeContoursByNearestNeighbor(
    const std::vector<ContourSegmentProxy>& segments,
    const Point2D& start_position) {
    std::vector<SegmentWithDirection> order;
    if (segments.empty()) {
        return order;
    }

    std::vector<bool> visited(segments.size(), false);
    Point2D current = start_position;
    Point2D last_direction{};
    bool has_last_direction = false;

    constexpr float32 kPi = 3.14159265359f;
    constexpr float32 kDegToRad = kPi / 180.0f;
    constexpr float32 kDirectionPenaltyAngleDeg = 150.0f;
    constexpr float32 kDirectionPenaltyWeight = 5.0f;
    const float32 cos_threshold = std::cos(kDirectionPenaltyAngleDeg * kDegToRad);

    auto clamp = [](float32 value, float32 lo, float32 hi) {
        return std::max(lo, std::min(hi, value));
    };
    auto direction_penalty = [&](const Point2D& candidate_direction) -> float32 {
        if (!has_last_direction) {
            return 0.0f;
        }
        const float32 last_length = last_direction.Length();
        const float32 candidate_length = candidate_direction.Length();
        if (last_length <= kEpsilon || candidate_length <= kEpsilon) {
            return 0.0f;
        }
        const Point2D n1 = last_direction / last_length;
        const Point2D n2 = candidate_direction / candidate_length;
        const float32 dot = clamp(n1.Dot(n2), -1.0f, 1.0f);
        if (dot >= cos_threshold) {
            return 0.0f;
        }
        return (cos_threshold - dot) / (cos_threshold + 1.0f);
    };
    auto immediate_backtrack_penalty = [&](const Point2D& entry_point, const Point2D& candidate_direction) -> float32 {
        if (!has_last_direction || Distance(current, entry_point) > kEpsilon) {
            return 0.0f;
        }

        const float32 last_length = last_direction.Length();
        const float32 candidate_length = candidate_direction.Length();
        if (last_length <= kEpsilon || candidate_length <= kEpsilon) {
            return 0.0f;
        }

        const Point2D n1 = last_direction / last_length;
        const Point2D n2 = candidate_direction / candidate_length;
        const float32 dot = clamp(n1.Dot(n2), -1.0f, 1.0f);
        return dot <= -0.95f ? 1e6f : 0.0f;
    };

    for (size_t i = 0; i < segments.size(); ++i) {
        size_t nearest_index = 0;
        bool reversed = false;
        float32 best_cost = std::numeric_limits<float32>::max();

        for (size_t j = 0; j < segments.size(); ++j) {
            if (visited[j]) {
                continue;
            }

            const auto& segment = segments[j];
            const float32 forward_distance = DistanceCost(current, segment.start_point);
            const float32 reverse_distance = DistanceCost(current, segment.end_point);
            const Point2D forward_direction = segment.end_point - segment.start_point;
            const Point2D reverse_direction = segment.start_point - segment.end_point;
            const float32 forward_cost =
                forward_distance +
                kDirectionPenaltyWeight * direction_penalty(forward_direction) +
                immediate_backtrack_penalty(segment.start_point, forward_direction);
            const float32 reverse_cost =
                reverse_distance +
                kDirectionPenaltyWeight * direction_penalty(reverse_direction) +
                immediate_backtrack_penalty(segment.end_point, reverse_direction);

            if (forward_cost < best_cost) {
                best_cost = forward_cost;
                nearest_index = j;
                reversed = false;
            }
            if (reverse_cost < best_cost) {
                best_cost = reverse_cost;
                nearest_index = j;
                reversed = true;
            }
        }

        order.push_back({nearest_index, reversed});
        visited[nearest_index] = true;
        current = reversed ? segments[nearest_index].start_point : segments[nearest_index].end_point;
        last_direction = reversed
            ? (segments[nearest_index].start_point - segments[nearest_index].end_point)
            : (segments[nearest_index].end_point - segments[nearest_index].start_point);
        has_last_direction = last_direction.Length() > kEpsilon;
    }

    return order;
}

std::vector<SegmentWithDirection> TwoOptImproveContourOrder(
    const std::vector<SegmentWithDirection>& initial_order,
    const std::vector<ContourSegmentProxy>& segments,
    int max_iterations) {
    if (initial_order.size() < 4 || max_iterations <= 0) {
        return initial_order;
    }

    auto order = initial_order;
    auto get_exit_point = [&](const SegmentWithDirection& entry) -> Point2D {
        return entry.reversed ? segments[entry.index].start_point : segments[entry.index].end_point;
    };
    auto get_entry_point = [&](const SegmentWithDirection& entry) -> Point2D {
        return entry.reversed ? segments[entry.index].end_point : segments[entry.index].start_point;
    };

    bool improved = true;
    while (improved && max_iterations-- > 0) {
        improved = false;
        for (size_t i = 0; i + 2 < order.size(); ++i) {
            for (size_t j = i + 2; j < order.size(); ++j) {
                float32 old_distance = DistanceCost(get_exit_point(order[i]), get_entry_point(order[i + 1]));
                if (j + 1 < order.size()) {
                    old_distance += DistanceCost(get_exit_point(order[j]), get_entry_point(order[j + 1]));
                }

                SegmentWithDirection reversed_j = {order[j].index, !order[j].reversed};
                SegmentWithDirection reversed_i1 = {order[i + 1].index, !order[i + 1].reversed};
                float32 new_distance = DistanceCost(get_exit_point(order[i]), get_entry_point(reversed_j));
                if (j + 1 < order.size()) {
                    new_distance += DistanceCost(get_exit_point(reversed_i1), get_entry_point(order[j + 1]));
                }

                if (new_distance + kEpsilon < old_distance) {
                    std::reverse(order.begin() + static_cast<std::ptrdiff_t>(i + 1),
                                 order.begin() + static_cast<std::ptrdiff_t>(j + 1));
                    for (size_t index = i + 1; index <= j; ++index) {
                        order[index].reversed = !order[index].reversed;
                    }
                    improved = true;
                }
            }
        }
    }

    return order;
}

ContourOrderResult ReorderContours(const std::vector<ContractsPrimitive>& primitives,
                                   const std::vector<PathPrimitiveMeta>& metadata,
                                   const Point2D& start_position,
                                   bool enable,
                                   int two_opt_iterations) {
    ContourOrderResult result;
    if (!enable || primitives.empty() || metadata.size() != primitives.size()) {
        result.primitives = primitives;
        result.metadata = metadata;
        return result;
    }

    auto contours = BuildContoursFromMetadata(primitives, metadata);
    if (contours.empty()) {
        result.primitives = primitives;
        result.metadata = metadata;
        return result;
    }

    std::vector<ContourSegmentProxy> contour_segments;
    contour_segments.reserve(contours.size());
    for (const auto& contour : contours) {
        ContourSegmentProxy segment;
        segment.start_point = ContourStartPoint(contour);
        segment.end_point = contour.closed ? segment.start_point : ContourEndPoint(contour);
        contour_segments.push_back(segment);
    }

    auto order = OptimizeContoursByNearestNeighbor(contour_segments, start_position);
    order = TwoOptImproveContourOrder(order, contour_segments, two_opt_iterations);

    Point2D current = start_position;
    result.primitives.reserve(primitives.size());
    result.metadata.reserve(metadata.size());

    for (size_t selection_index = 0; selection_index < order.size(); ++selection_index) {
        auto contour = contours[order[selection_index].index];
        if (contour.original_index != selection_index) {
            result.reordered_contours += 1;
        }
        if (contour.closed) {
            const size_t best_rotate = FindClosestStartIndex(contour, current);
            if (best_rotate > 0) {
                RotateContour(contour, best_rotate);
                result.rotated_contours += 1;
            }
        } else if (order[selection_index].reversed) {
            ReverseContour(contour);
            result.reversed_contours += 1;
        }
        if (!contour.closed && !result.primitives.empty() &&
            FormsImmediateLineBacktrack(result.primitives.back(), contour.primitives.front(), 1e-3f)) {
            Contour flipped = contour;
            ReverseContour(flipped);
            if (!FormsImmediateLineBacktrack(result.primitives.back(), flipped.primitives.front(), 1e-3f)) {
                contour = std::move(flipped);
                result.reversed_contours += order[selection_index].reversed ? -1 : 1;
            }
        }

        current = ContourEndPoint(contour);
        result.primitives.insert(result.primitives.end(), contour.primitives.begin(), contour.primitives.end());
        result.metadata.insert(result.metadata.end(), contour.metadata.begin(), contour.metadata.end());
    }

    result.applied = result.primitives.size() == primitives.size() &&
                     (result.reordered_contours > 0 || result.reversed_contours > 0 ||
                      result.rotated_contours > 0);
    if (result.primitives.empty()) {
        result.primitives = primitives;
        result.metadata = metadata;
    }
    return result;
}

}  // namespace

TopologyRepairResult TopologyRepairService::Repair(const std::vector<ContractsPrimitive>& primitives,
                                                   const std::vector<PathPrimitiveMeta>& metadata,
                                                   float32 continuity_tolerance_mm,
                                                   const TopologyRepairConfig& config) const {
    TopologyRepairResult result;
    result.primitives = primitives;
    result.metadata = metadata;
    result.diagnostics.repair_requested = config.policy != TopologyRepairPolicy::Off;
    result.diagnostics.original_primitive_count = static_cast<int>(primitives.size());
    result.diagnostics.repaired_primitive_count = static_cast<int>(primitives.size());
    result.diagnostics.metadata_valid = IsMetadataValid(primitives, metadata);

    const float32 tolerance = continuity_tolerance_mm > kEpsilon ? continuity_tolerance_mm : 0.1f;
    result.diagnostics.discontinuity_before = CountPrimitiveDiscontinuity(primitives, tolerance);
    result.diagnostics.discontinuity_after = result.diagnostics.discontinuity_before;

    if (config.policy == TopologyRepairPolicy::Off || primitives.empty()) {
        return result;
    }

    bool repair_applied = false;

    if (config.split_intersections && IsMetadataValid(result.primitives, result.metadata)) {
        const auto split = SplitLinePrimitivesByIntersection(result.primitives, result.metadata, tolerance);
        result.diagnostics.intersection_pairs = split.intersection_pairs;
        result.diagnostics.collinear_pairs = split.collinear_pairs;
        if (split.applied && !split.primitives.empty()) {
            result.primitives = split.primitives;
            result.metadata = split.metadata;
            repair_applied = true;
        }
    }

    if (config.rebuild_by_connectivity &&
        ShouldRebuildByConnectivity(result.primitives, result.metadata, CountPrimitiveDiscontinuity(result.primitives, tolerance))) {
        const auto rebuilt = RebuildPrimitivesByConnectivity(result.primitives, result.metadata, tolerance);
        if (!rebuilt.primitives.empty()) {
            result.primitives = rebuilt.primitives;
            result.metadata = rebuilt.metadata;
            result.diagnostics.contour_count = rebuilt.contour_count;
            result.diagnostics.reversed_contours += rebuilt.reversed_count;
            repair_applied = repair_applied || rebuilt.applied;
        }
    }

    if (config.reorder_contours && IsMetadataValid(result.primitives, result.metadata)) {
        const auto reordered = ReorderContours(result.primitives,
                                              result.metadata,
                                              config.start_position,
                                              config.reorder_contours,
                                              config.two_opt_iterations);
        result.primitives = reordered.primitives;
        result.metadata = reordered.metadata;
        result.diagnostics.reordered_contours += reordered.reordered_contours;
        result.diagnostics.reversed_contours += reordered.reversed_contours;
        result.diagnostics.rotated_contours += reordered.rotated_contours;
        repair_applied = repair_applied || reordered.applied;
    }

    if (result.diagnostics.contour_count == 0 && IsMetadataValid(result.primitives, result.metadata)) {
        auto contours = BuildContoursFromMetadata(result.primitives, result.metadata);
        result.diagnostics.contour_count = static_cast<int>(contours.size());
    }

    result.diagnostics.repair_applied = repair_applied;
    result.diagnostics.repaired_primitive_count = static_cast<int>(result.primitives.size());
    result.diagnostics.discontinuity_after = CountPrimitiveDiscontinuity(result.primitives, tolerance);
    result.diagnostics.fragmentation_suspected =
        result.diagnostics.discontinuity_after > 0 ||
        (result.diagnostics.discontinuity_before > 0 && !result.diagnostics.repair_applied);
    return result;
}

}  // namespace Siligen::Domain::Trajectory::DomainServices
