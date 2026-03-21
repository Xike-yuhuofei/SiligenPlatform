#include "ContourOptimizationService.h"
#include "domain/dispensing/domain-services/PathOptimizationStrategy.h"
#include "domain/trajectory/value-objects/GeometryUtils.h"
#include "shared/types/MathConstants.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Domain::Trajectory::Ports::PathPrimitiveMeta;
using Siligen::Domain::Trajectory::ValueObjects::Primitive;
using Siligen::Domain::Trajectory::ValueObjects::PrimitiveType;
using Siligen::Domain::Trajectory::ValueObjects::CirclePrimitive;
using Siligen::Domain::Trajectory::ValueObjects::EllipsePrimitive;
using Siligen::Domain::Trajectory::ValueObjects::EllipsePoint;
using Siligen::Domain::Trajectory::ValueObjects::ContourElement;
using Siligen::Domain::Trajectory::ValueObjects::ContourElementType;
using Siligen::Shared::Types::DXFEntityType;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::kDegToRad;
using Siligen::Shared::Types::kPi;

namespace {
constexpr float32 kEpsilon = 1e-6f;

Point2D PrimitiveStartPoint(const Primitive& primitive) {
    if (primitive.type == PrimitiveType::Line) {
        return primitive.line.start;
    }
    if (primitive.type == PrimitiveType::Arc) {
        const float32 angle_rad = primitive.arc.start_angle_deg * kDegToRad;
        return Point2D(primitive.arc.center.x + primitive.arc.radius * std::cos(angle_rad),
                       primitive.arc.center.y + primitive.arc.radius * std::sin(angle_rad));
    }
    if (!primitive.spline.control_points.empty()) {
        return primitive.spline.control_points.front();
    }
    if (primitive.type == PrimitiveType::Circle) {
        const float32 angle_rad = primitive.circle.start_angle_deg * kDegToRad;
        return Point2D(primitive.circle.center.x + primitive.circle.radius * std::cos(angle_rad),
                       primitive.circle.center.y + primitive.circle.radius * std::sin(angle_rad));
    }
    if (primitive.type == PrimitiveType::Ellipse) {
        return EllipsePoint(primitive.ellipse, primitive.ellipse.start_param);
    }
    if (primitive.type == PrimitiveType::Point) {
        return primitive.point.position;
    }
    if (primitive.type == PrimitiveType::Contour && !primitive.contour.elements.empty()) {
        const auto& elem = primitive.contour.elements.front();
        if (elem.type == ContourElementType::Line) {
            return elem.line.start;
        }
        if (elem.type == ContourElementType::Arc) {
            float32 angle_rad = elem.arc.start_angle_deg * kDegToRad;
            return Point2D(elem.arc.center.x + elem.arc.radius * std::cos(angle_rad),
                           elem.arc.center.y + elem.arc.radius * std::sin(angle_rad));
        }
        if (elem.type == ContourElementType::Spline && !elem.spline.control_points.empty()) {
            return elem.spline.control_points.front();
        }
    }
    return Point2D();
}

Point2D PrimitiveEndPoint(const Primitive& primitive) {
    if (primitive.type == PrimitiveType::Line) {
        return primitive.line.end;
    }
    if (primitive.type == PrimitiveType::Arc) {
        const float32 angle_rad = primitive.arc.end_angle_deg * kDegToRad;
        return Point2D(primitive.arc.center.x + primitive.arc.radius * std::cos(angle_rad),
                       primitive.arc.center.y + primitive.arc.radius * std::sin(angle_rad));
    }
    if (!primitive.spline.control_points.empty()) {
        return primitive.spline.control_points.back();
    }
    if (primitive.type == PrimitiveType::Circle) {
        const float32 angle_rad = primitive.circle.start_angle_deg * kDegToRad;
        return Point2D(primitive.circle.center.x + primitive.circle.radius * std::cos(angle_rad),
                       primitive.circle.center.y + primitive.circle.radius * std::sin(angle_rad));
    }
    if (primitive.type == PrimitiveType::Ellipse) {
        return EllipsePoint(primitive.ellipse, primitive.ellipse.end_param);
    }
    if (primitive.type == PrimitiveType::Point) {
        return primitive.point.position;
    }
    if (primitive.type == PrimitiveType::Contour && !primitive.contour.elements.empty()) {
        const auto& elem = primitive.contour.elements.back();
        if (elem.type == ContourElementType::Line) {
            return elem.line.end;
        }
        if (elem.type == ContourElementType::Arc) {
            float32 angle_rad = elem.arc.end_angle_deg * kDegToRad;
            return Point2D(elem.arc.center.x + elem.arc.radius * std::cos(angle_rad),
                           elem.arc.center.y + elem.arc.radius * std::sin(angle_rad));
        }
        if (elem.type == ContourElementType::Spline && !elem.spline.control_points.empty()) {
            return elem.spline.control_points.back();
        }
    }
    return Point2D();
}

Primitive ReversePrimitive(const Primitive& primitive) {
    Primitive reversed = primitive;
    if (primitive.type == PrimitiveType::Line) {
        reversed.line.start = primitive.line.end;
        reversed.line.end = primitive.line.start;
    } else if (primitive.type == PrimitiveType::Arc) {
        std::swap(reversed.arc.start_angle_deg, reversed.arc.end_angle_deg);
        reversed.arc.clockwise = !reversed.arc.clockwise;
    } else if (primitive.type == PrimitiveType::Spline) {
        std::reverse(reversed.spline.control_points.begin(), reversed.spline.control_points.end());
    } else if (primitive.type == PrimitiveType::Circle) {
        reversed.circle.clockwise = !reversed.circle.clockwise;
    } else if (primitive.type == PrimitiveType::Ellipse) {
        std::swap(reversed.ellipse.start_param, reversed.ellipse.end_param);
        reversed.ellipse.clockwise = !reversed.ellipse.clockwise;
    } else if (primitive.type == PrimitiveType::Point) {
        return reversed;
    } else if (primitive.type == PrimitiveType::Contour) {
        std::vector<ContourElement> elements;
        elements.reserve(primitive.contour.elements.size());
        for (auto it = primitive.contour.elements.rbegin();
             it != primitive.contour.elements.rend();
             ++it) {
            ContourElement elem = *it;
            if (elem.type == ContourElementType::Line) {
                std::swap(elem.line.start, elem.line.end);
            } else if (elem.type == ContourElementType::Arc) {
                std::swap(elem.arc.start_angle_deg, elem.arc.end_angle_deg);
                elem.arc.clockwise = !elem.arc.clockwise;
            } else if (elem.type == ContourElementType::Spline) {
                std::reverse(elem.spline.control_points.begin(), elem.spline.control_points.end());
            }
            elements.push_back(std::move(elem));
        }
        reversed.contour.elements = std::move(elements);
    }
    return reversed;
}

struct IndexedPrimitive {
    int segment_index = 0;
    size_t order = 0;
    Primitive primitive{};
};

struct Contour {
    int entity_id = -1;
    DXFEntityType entity_type = DXFEntityType::Unknown;
    bool closed = false;
    size_t original_index = 0;
    std::vector<IndexedPrimitive> items;
    std::vector<Primitive> primitives;
};

Point2D ContourStart(const Contour& contour) {
    if (contour.primitives.empty()) {
        return Point2D();
    }
    return PrimitiveStartPoint(contour.primitives.front());
}

Point2D ContourEnd(const Contour& contour) {
    if (contour.primitives.empty()) {
        return Point2D();
    }
    return PrimitiveEndPoint(contour.primitives.back());
}

size_t FindClosestStartIndex(const Contour& contour, const Point2D& current, float32& out_dist) {
    out_dist = std::numeric_limits<float32>::max();
    size_t best_index = 0;
    if (contour.primitives.empty()) {
        return best_index;
    }
    for (size_t i = 0; i < contour.primitives.size(); ++i) {
        Point2D start = PrimitiveStartPoint(contour.primitives[i]);
        float32 dist = start.DistanceTo(current);
        if (dist < out_dist) {
            out_dist = dist;
            best_index = i;
        }
    }
    return best_index;
}

float32 NormalizeParam(float32 param) {
    float32 mod = std::fmod(param, 2.0f * kPi);
    if (mod < 0.0f) {
        mod += 2.0f * kPi;
    }
    return mod;
}

void RotateSingleClosedPrimitive(Primitive& primitive, const Point2D& current) {
    if (primitive.type == PrimitiveType::Circle) {
        Point2D rel = current - primitive.circle.center;
        float32 angle = std::atan2(rel.y, rel.x) * 180.0f / kPi;
        primitive.circle.start_angle_deg = angle;
        return;
    }
    if (primitive.type == PrimitiveType::Ellipse) {
        float32 a = primitive.ellipse.major_axis.Length();
        float32 b = a * primitive.ellipse.ratio;
        if (a <= kEpsilon || b <= kEpsilon) {
            return;
        }
        float32 theta = std::atan2(primitive.ellipse.major_axis.y, primitive.ellipse.major_axis.x);
        float32 cos_theta = std::cos(-theta);
        float32 sin_theta = std::sin(-theta);
        Point2D rel = current - primitive.ellipse.center;
        float32 xr = rel.x * cos_theta - rel.y * sin_theta;
        float32 yr = rel.x * sin_theta + rel.y * cos_theta;
        float32 param = std::atan2(yr / b, xr / a);
        float32 delta = NormalizeParam(primitive.ellipse.end_param - primitive.ellipse.start_param);
        primitive.ellipse.start_param = param;
        primitive.ellipse.end_param = param + delta;
        return;
    }
    if (primitive.type == PrimitiveType::Contour && !primitive.contour.elements.empty()) {
        auto element_start = [](const ContourElement& elem) -> Point2D {
            if (elem.type == ContourElementType::Line) {
                return elem.line.start;
            }
        if (elem.type == ContourElementType::Arc) {
            float32 angle_rad = elem.arc.start_angle_deg * kDegToRad;
            return Point2D(elem.arc.center.x + elem.arc.radius * std::cos(angle_rad),
                           elem.arc.center.y + elem.arc.radius * std::sin(angle_rad));
        }
            if (elem.type == ContourElementType::Spline && !elem.spline.control_points.empty()) {
                return elem.spline.control_points.front();
            }
            return Point2D();
        };

        size_t best_index = 0;
        float32 best_dist = std::numeric_limits<float32>::max();
        for (size_t i = 0; i < primitive.contour.elements.size(); ++i) {
            float32 dist = element_start(primitive.contour.elements[i]).DistanceTo(current);
            if (dist < best_dist) {
                best_dist = dist;
                best_index = i;
            }
        }
        if (best_index > 0) {
            std::rotate(primitive.contour.elements.begin(),
                        primitive.contour.elements.begin() + static_cast<std::ptrdiff_t>(best_index),
                        primitive.contour.elements.end());
        }
    }
}

void ApplyRotation(Contour& contour, size_t index) {
    if (contour.primitives.size() <= 1 || index == 0) {
        return;
    }
    std::rotate(contour.primitives.begin(),
                contour.primitives.begin() + static_cast<std::ptrdiff_t>(index),
                contour.primitives.end());
}

void ApplyReverse(Contour& contour) {
    if (contour.primitives.size() <= 1) {
        return;
    }
    std::vector<Primitive> reversed;
    reversed.reserve(contour.primitives.size());
    for (auto it = contour.primitives.rbegin(); it != contour.primitives.rend(); ++it) {
        reversed.push_back(ReversePrimitive(*it));
    }
    contour.primitives = std::move(reversed);
}

}  // namespace

std::vector<Primitive> ContourOptimizationService::Optimize(
    const std::vector<Primitive>& primitives,
    const std::vector<PathPrimitiveMeta>& metadata,
    const Point2D& start_pos,
    bool enable,
    int two_opt_iterations,
    ContourOptimizationStats* stats) {
    // Complexity: O(n log n) for per-contour sorting plus O(c^2) greedy selection (worst-case O(n^2)).
    if (stats) {
        *stats = ContourOptimizationStats{};
    }
    if (!enable || primitives.empty()) {
        return primitives;
    }
    if (metadata.size() != primitives.size()) {
        return primitives;
    }

    std::vector<Contour> contours;
    contours.reserve(primitives.size());
    std::unordered_map<int, size_t> entity_map;
    size_t unknown_counter = 0;

    for (size_t i = 0; i < primitives.size(); ++i) {
        const auto& meta = metadata[i];
        size_t contour_index = contours.size();
        bool has_entity = meta.entity_id >= 0;

        if (has_entity) {
            auto it = entity_map.find(meta.entity_id);
            if (it != entity_map.end()) {
                contour_index = it->second;
            } else {
                contour_index = contours.size();
                Contour contour;
                contour.entity_id = meta.entity_id;
                contour.entity_type = meta.entity_type;
                contour.closed = meta.entity_closed;
                contour.original_index = contour_index;
                contours.push_back(std::move(contour));
                entity_map.emplace(meta.entity_id, contour_index);
            }
        } else {
            Contour contour;
            contour.entity_id = -static_cast<int>(++unknown_counter);
            contour.entity_type = meta.entity_type;
            contour.closed = meta.entity_closed;
            contour.original_index = contour_index;
            contours.push_back(std::move(contour));
        }

        Contour& contour = contours[contour_index];
        contour.closed = contour.closed || meta.entity_closed;
        IndexedPrimitive item;
        item.segment_index = meta.entity_segment_index;
        item.order = contour.items.size();
        item.primitive = primitives[i];
        contour.items.push_back(std::move(item));
    }

    for (auto& contour : contours) {
        std::stable_sort(contour.items.begin(),
                         contour.items.end(),
                         [](const IndexedPrimitive& a, const IndexedPrimitive& b) {
                             if (a.segment_index != b.segment_index) {
                                 return a.segment_index < b.segment_index;
                             }
                             return a.order < b.order;
                         });
        contour.primitives.reserve(contour.items.size());
        for (auto& item : contour.items) {
            contour.primitives.push_back(std::move(item.primitive));
        }
        contour.items.clear();
    }

    if (stats) {
        stats->contour_count = contours.size();
        stats->applied = true;
        stats->metadata_valid = true;
    }

    if (contours.empty()) {
        return primitives;
    }

    std::vector<size_t> contour_indices;
    std::vector<Shared::Types::DXFSegment> contour_segments;
    contour_indices.reserve(contours.size());
    contour_segments.reserve(contours.size());

    for (size_t i = 0; i < contours.size(); ++i) {
        if (contours[i].primitives.empty()) {
            continue;
        }
        Shared::Types::DXFSegment seg;
        seg.start_point = ContourStart(contours[i]);
        seg.end_point = contours[i].closed ? seg.start_point : ContourEnd(contours[i]);
        contour_indices.push_back(i);
        contour_segments.push_back(seg);
    }

    if (contour_segments.empty()) {
        return primitives;
    }

    Domain::Dispensing::DomainServices::PathOptimizationStrategy optimizer;
    auto order = optimizer.OptimizeByNearestNeighbor(contour_segments, start_pos);
    int iterations = std::max(0, two_opt_iterations);
    if (iterations > 0) {
        order = optimizer.TwoOptImprove(order, contour_segments, iterations);
    }

    Point2D current = start_pos;
    std::vector<Primitive> output;
    output.reserve(primitives.size());

    for (const auto& entry : order) {
        const size_t contour_index = contour_indices[entry.index];
        Contour& contour = contours[contour_index];

        if (contour.closed) {
            float32 dist = 0.0f;
            size_t rotate_index = FindClosestStartIndex(contour, current, dist);
            if (contour.primitives.size() == 1) {
                RotateSingleClosedPrimitive(contour.primitives.front(), current);
                if (stats) {
                    stats->rotated_contours++;
                }
            } else {
                if (rotate_index > 0 && stats) {
                    stats->rotated_contours++;
                }
                ApplyRotation(contour, rotate_index);
            }
        } else if (entry.reversed) {
            if (stats) {
                stats->reversed_contours++;
            }
            ApplyReverse(contour);
        }

        for (const auto& prim : contour.primitives) {
            output.push_back(prim);
        }
        current = ContourEnd(contour);
    }

    if (stats) {
        for (size_t i = 0; i < order.size(); ++i) {
            const size_t contour_index = contour_indices[order[i].index];
            if (contours[contour_index].original_index != i) {
                stats->reordered_contours++;
            }
        }
    }

    return output;
}

}  // namespace Siligen::Domain::Dispensing::DomainServices


