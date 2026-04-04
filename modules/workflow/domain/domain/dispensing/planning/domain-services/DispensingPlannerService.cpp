#include "DispensingPlannerService.h"
#include "ContourOptimizationService.h"
#include "../../../../../../../dispense-packaging/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.h"

#include "application/services/motion_planning/CmpInterpolationFacade.h"
#include "application/services/motion_planning/InterpolationProgramFacade.h"
#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/motion_planning/TrajectoryInterpolationFacade.h"
#include "domain/dispensing/domain-services/TriggerPlanner.h"
#include "process_path/contracts/GeometryUtils.h"
#include "process_path/contracts/Path.h"
#include "process_path/contracts/PathPrimitiveMeta.h"
#include "process_path/contracts/Primitive.h"
#include "process_path/contracts/ProcessPath.h"
#include "shared/geometry/BoostGeometryAdapter.h"
#include "shared/types/CMPTypes.h"
#include "shared/types/Error.h"
#include "shared/types/TrajectoryTriggerUtils.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <unordered_map>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "DispensingPlannerService"

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::uint32;
using Siligen::ProcessPath::Contracts::ArcPoint;
using Siligen::ProcessPath::Contracts::ComputeArcLength;
using Siligen::ProcessPath::Contracts::ComputeArcSweep;
using Siligen::ProcessPath::Contracts::NormalizeAngle;
using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::ProcessPath::Contracts::ProcessTag;
using Siligen::ProcessPath::Contracts::Segment;
using Siligen::ProcessPath::Contracts::SegmentEnd;
using Siligen::ProcessPath::Contracts::SegmentStart;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint;
using Siligen::Domain::Dispensing::DomainServices::UnifiedTrajectoryPlanRequest;
using Siligen::Domain::Dispensing::DomainServices::UnifiedTrajectoryPlannerService;
using Siligen::Domain::Dispensing::DomainServices::TriggerPlanner;
using Siligen::Domain::Dispensing::ValueObjects::TriggerPlan;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Geometry::ComputeSegmentIntersection;
using Siligen::Shared::Geometry::Distance;
using Siligen::Shared::Geometry::IsPointOnSegment;

namespace {
constexpr float32 kEpsilon = 1e-6f;
constexpr float32 kPi = 3.14159265359f;
constexpr float32 kDegToRad = kPi / 180.0f;
constexpr float32 kCoordinateSafetyMarginMm = 1.0f;
constexpr float32 kBoundsToleranceMm = 1e-4f;
constexpr float32 kPreviewPointDedupToleranceMm = 1e-4f;

float32 SegmentLength(const Segment& segment) {
    if (segment.length > kEpsilon) {
        return segment.length;
    }
    if (segment.type == SegmentType::Line) {
        return segment.line.start.DistanceTo(segment.line.end);
    }
    return ComputeArcLength(segment.arc);
}

Point2D SegmentEndPoint(const Segment& segment) {
    return SegmentEnd(segment);
}

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
    return Point2D();
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
    return Point2D();
}

Point2D PrimitiveStartPoint(const Primitive& primitive) {
    using Siligen::ProcessPath::Contracts::PrimitiveType;
    if (primitive.type == PrimitiveType::Line) {
        return primitive.line.start;
    }
    if (primitive.type == PrimitiveType::Arc) {
        return ArcPoint(primitive.arc, primitive.arc.start_angle_deg);
    }
    if (primitive.type == PrimitiveType::Spline && !primitive.spline.control_points.empty()) {
        return primitive.spline.control_points.front();
    }
    if (primitive.type == PrimitiveType::Circle) {
        float32 angle_rad = primitive.circle.start_angle_deg * kDegToRad;
        return Point2D(primitive.circle.center.x + primitive.circle.radius * std::cos(angle_rad),
                       primitive.circle.center.y + primitive.circle.radius * std::sin(angle_rad));
    }
    if (primitive.type == PrimitiveType::Ellipse) {
        return Siligen::ProcessPath::Contracts::EllipsePoint(primitive.ellipse, primitive.ellipse.start_param);
    }
    if (primitive.type == PrimitiveType::Point) {
        return primitive.point.position;
    }
    if (primitive.type == PrimitiveType::Contour && !primitive.contour.elements.empty()) {
        return ContourElementStartPoint(primitive.contour.elements.front());
    }
    return Point2D();
}

Point2D PrimitiveEndPoint(const Primitive& primitive) {
    using Siligen::ProcessPath::Contracts::PrimitiveType;
    if (primitive.type == PrimitiveType::Line) {
        return primitive.line.end;
    }
    if (primitive.type == PrimitiveType::Arc) {
        return ArcPoint(primitive.arc, primitive.arc.end_angle_deg);
    }
    if (primitive.type == PrimitiveType::Spline && !primitive.spline.control_points.empty()) {
        return primitive.spline.control_points.back();
    }
    if (primitive.type == PrimitiveType::Circle) {
        float32 angle_rad = (primitive.circle.start_angle_deg + 360.0f) * kDegToRad;
        return Point2D(primitive.circle.center.x + primitive.circle.radius * std::cos(angle_rad),
                       primitive.circle.center.y + primitive.circle.radius * std::sin(angle_rad));
    }
    if (primitive.type == PrimitiveType::Ellipse) {
        return Siligen::ProcessPath::Contracts::EllipsePoint(primitive.ellipse, primitive.ellipse.end_param);
    }
    if (primitive.type == PrimitiveType::Point) {
        return primitive.point.position;
    }
    if (primitive.type == PrimitiveType::Contour && !primitive.contour.elements.empty()) {
        return ContourElementEndPoint(primitive.contour.elements.back());
    }
    return Point2D();
}

Primitive ReversePrimitive(const Primitive& primitive) {
    using Siligen::ProcessPath::Contracts::PrimitiveType;
    using Siligen::ProcessPath::Contracts::ContourElementType;
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
    } else if (primitive.type == PrimitiveType::Contour) {
        std::vector<Siligen::ProcessPath::Contracts::ContourElement> elements;
        elements.reserve(primitive.contour.elements.size());
        for (auto it = primitive.contour.elements.rbegin();
             it != primitive.contour.elements.rend();
             ++it) {
            auto elem = *it;
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

int CountPrimitiveDiscontinuity(const std::vector<Primitive>& primitives, float32 tolerance) {
    if (primitives.size() < 2) {
        return 0;
    }
    int count = 0;
    for (size_t i = 1; i < primitives.size(); ++i) {
        Point2D prev_end = PrimitiveEndPoint(primitives[i - 1]);
        Point2D curr_start = PrimitiveStartPoint(primitives[i]);
        if (Distance(prev_end, curr_start) > tolerance) {
            ++count;
        }
    }
    return count;
}

bool ShouldRebuildByConnectivity(const std::vector<Primitive>& primitives,
                                 const std::vector<PathPrimitiveMeta>& metadata,
                                 float32 tolerance,
                                 int discontinuity_count) {
    if (primitives.size() < 2) {
        return false;
    }
    if (metadata.size() != primitives.size()) {
        return false;
    }
    if (discontinuity_count <= 0) {
        return false;
    }
    using Siligen::ProcessPath::Contracts::PrimitiveType;
    for (const auto& prim : primitives) {
        if (prim.type != PrimitiveType::Line && prim.type != PrimitiveType::Arc) {
            return false;
        }
    }
    std::unordered_map<int, int> entity_counts;
    entity_counts.reserve(metadata.size());
    for (const auto& meta : metadata) {
        entity_counts[meta.entity_id] += 1;
    }
    for (const auto& entry : entity_counts) {
        if (entry.second > 1) {
            return false;
        }
    }
    (void)tolerance;
    return true;
}

struct ConnectivityRebuildResult {
    std::vector<Primitive> primitives;
    std::vector<PathPrimitiveMeta> metadata;
    size_t contour_count = 0;
    size_t chained_count = 0;
    size_t reversed_count = 0;
};

ConnectivityRebuildResult RebuildPrimitivesByConnectivity(
    const std::vector<Primitive>& primitives,
    const std::vector<PathPrimitiveMeta>& metadata,
    float32 tolerance) {
    ConnectivityRebuildResult result;
    if (primitives.empty() || primitives.size() != metadata.size()) {
        return result;
    }

    const size_t count = primitives.size();
    result.primitives.reserve(count);
    result.metadata.reserve(count);

    std::vector<bool> used(count, false);
    int contour_id = 0;
    for (size_t i = 0; i < count; ++i) {
        if (used[i]) {
            continue;
        }

        const size_t contour_start_index = result.metadata.size();
        Primitive current = primitives[i];
        used[i] = true;

        Point2D contour_start = PrimitiveStartPoint(current);
        Point2D current_end = PrimitiveEndPoint(current);

        auto meta = metadata[i];
        meta.entity_id = -static_cast<int>(contour_id + 1);
        meta.entity_segment_index = 0;
        meta.entity_closed = false;
        result.primitives.push_back(current);
        result.metadata.push_back(meta);

        size_t segment_index = 1;
        while (true) {
            size_t best_index = count;
            float32 best_distance = std::numeric_limits<float32>::max();
            bool best_reverse = false;

            for (size_t j = 0; j < count; ++j) {
                if (used[j]) {
                    continue;
                }
                Point2D start = PrimitiveStartPoint(primitives[j]);
                Point2D end = PrimitiveEndPoint(primitives[j]);
                float32 dist_start = Distance(current_end, start);
                float32 dist_end = Distance(current_end, end);
                bool can_start = dist_start <= tolerance;
                bool can_end = dist_end <= tolerance;
                if (!can_start && !can_end) {
                    continue;
                }
                float32 dist = std::min(dist_start, dist_end);
                if (dist < best_distance) {
                    best_distance = dist;
                    best_index = j;
                    best_reverse = (!can_start || dist_end < dist_start);
                }
            }

            if (best_index == count) {
                break;
            }

            Primitive next = primitives[best_index];
            if (best_reverse) {
                next = ReversePrimitive(next);
                result.reversed_count += 1;
            }

            current_end = PrimitiveEndPoint(next);
            used[best_index] = true;

            auto next_meta = metadata[best_index];
            next_meta.entity_id = -static_cast<int>(contour_id + 1);
            next_meta.entity_segment_index = static_cast<int>(segment_index);
            next_meta.entity_closed = false;

            result.primitives.push_back(next);
            result.metadata.push_back(next_meta);

            segment_index += 1;
            result.chained_count += 1;
        }

        bool closed = Distance(contour_start, current_end) <= tolerance;
        for (size_t k = contour_start_index; k < result.metadata.size(); ++k) {
            result.metadata[k].entity_closed = closed;
        }
        contour_id += 1;
    }

    result.contour_count = static_cast<size_t>(contour_id);
    return result;
}


struct BoundsReport {
    bool has = false;
    float32 min_x = 0.0f;
    float32 min_y = 0.0f;
    float32 max_x = 0.0f;
    float32 max_y = 0.0f;
    int min_x_segment = -1;
    int min_y_segment = -1;
    const char* min_x_label = "";
    const char* min_y_label = "";
};

struct SplitLineResult {
    std::vector<Primitive> primitives;
    std::vector<PathPrimitiveMeta> metadata;
    size_t original_count = 0;
    size_t split_count = 0;
    size_t intersection_pairs = 0;
    size_t collinear_pairs = 0;
    bool applied = false;
};

SplitLineResult SplitLinePrimitivesByIntersection(
    const std::vector<Primitive>& primitives,
    const std::vector<PathPrimitiveMeta>& metadata,
    float32 tolerance) {
    SplitLineResult result;
    result.original_count = primitives.size();
    if (primitives.size() < 2 || primitives.size() != metadata.size()) {
        return result;
    }

    using Siligen::ProcessPath::Contracts::PrimitiveType;
    for (const auto& prim : primitives) {
        if (prim.type != PrimitiveType::Line) {
            return result;
        }
    }

    const size_t count = primitives.size();
    std::vector<std::vector<Point2D>> split_points(count);
    split_points.reserve(count);

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
            if (ComputeSegmentIntersection(a, b, c, d, tolerance, intersection, collinear)) {
                split_points[i].push_back(intersection);
                split_points[j].push_back(intersection);
                result.intersection_pairs += 1;
                continue;
            }
            if (collinear) {
                bool added = false;
                if (IsPointOnSegment(c, a, b, tolerance)) {
                    split_points[i].push_back(c);
                    added = true;
                }
                if (IsPointOnSegment(d, a, b, tolerance)) {
                    split_points[i].push_back(d);
                    added = true;
                }
                if (IsPointOnSegment(a, c, d, tolerance)) {
                    split_points[j].push_back(a);
                    added = true;
                }
                if (IsPointOnSegment(b, c, d, tolerance)) {
                    split_points[j].push_back(b);
                    added = true;
                }
                if (added) {
                    result.collinear_pairs += 1;
                }
            }
        }
    }

    result.primitives.clear();
    result.metadata.clear();
    result.primitives.reserve(primitives.size());
    result.metadata.reserve(primitives.size());

    int32 next_entity_id = -1;
    const float32 min_segment_length = std::max(1e-4f, tolerance * 0.5f);

    for (size_t i = 0; i < count; ++i) {
        const Point2D start = primitives[i].line.start;
        const Point2D end = primitives[i].line.end;
        Point2D dir = end - start;
        float32 len2 = dir.Dot(dir);
        if (len2 <= kEpsilon) {
            continue;
        }

        auto& pts = split_points[i];
        std::vector<std::pair<float32, Point2D>> ordered;
        ordered.reserve(pts.size());
        for (const auto& p : pts) {
            float32 t = (p - start).Dot(dir) / len2;
            ordered.emplace_back(t, p);
        }
        std::sort(ordered.begin(),
                  ordered.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

        std::vector<Point2D> unique_pts;
        unique_pts.reserve(ordered.size());
        for (const auto& item : ordered) {
            if (unique_pts.empty() || Distance(item.second, unique_pts.back()) > tolerance) {
                unique_pts.push_back(item.second);
            }
        }

        for (size_t k = 1; k < unique_pts.size(); ++k) {
            const auto& p0 = unique_pts[k - 1];
            const auto& p1 = unique_pts[k];
            if (Distance(p0, p1) <= min_segment_length) {
                continue;
            }

            Primitive seg = Primitive::MakeLine(p0, p1);
            result.primitives.push_back(seg);

            auto meta = metadata[i];
            meta.entity_id = next_entity_id--;
            meta.entity_segment_index = 0;
            meta.entity_closed = false;
            result.metadata.push_back(meta);
        }
    }

    result.split_count = result.primitives.size();
    result.applied = result.split_count > 0 &&
                     (result.split_count != result.original_count ||
                      result.intersection_pairs > 0 ||
                      result.collinear_pairs > 0);
    return result;
}

void UpdateBounds(BoundsReport& report, const Point2D& point, int seg_idx, const char* label) {
    if (!report.has) {
        report.has = true;
        report.min_x = report.max_x = point.x;
        report.min_y = report.max_y = point.y;
        report.min_x_segment = seg_idx;
        report.min_y_segment = seg_idx;
        report.min_x_label = label;
        report.min_y_label = label;
        return;
    }
    if (point.x < report.min_x) {
        report.min_x = point.x;
        report.min_x_segment = seg_idx;
        report.min_x_label = label;
    }
    if (point.y < report.min_y) {
        report.min_y = point.y;
        report.min_y_segment = seg_idx;
        report.min_y_label = label;
    }
    if (point.x > report.max_x) {
        report.max_x = point.x;
    }
    if (point.y > report.max_y) {
        report.max_y = point.y;
    }
}

bool IsAngleOnArc(float32 start_angle, float32 end_angle, bool clockwise, float32 angle) {
    float32 total = ComputeArcSweep(start_angle, end_angle, clockwise);
    float32 sweep = ComputeArcSweep(start_angle, angle, clockwise);
    return sweep <= total + 1e-3f;
}

void UpdateBoundsForArc(const Siligen::ProcessPath::Contracts::ArcPrimitive& arc,
                        int seg_idx,
                        BoundsReport& report) {
    float32 start_angle = NormalizeAngle(arc.start_angle_deg);
    float32 end_angle = NormalizeAngle(arc.end_angle_deg);
    UpdateBounds(report, ArcPoint(arc, start_angle), seg_idx, "arc.start");
    UpdateBounds(report, ArcPoint(arc, end_angle), seg_idx, "arc.end");

    const float32 cardinal_angles[] = {0.0f, 90.0f, 180.0f, 270.0f};
    for (float32 angle : cardinal_angles) {
        float32 norm = NormalizeAngle(angle);
        if (IsAngleOnArc(start_angle, end_angle, arc.clockwise, norm)) {
            UpdateBounds(report, ArcPoint(arc, norm), seg_idx, "arc.cardinal");
        }
    }
}

BoundsReport ComputeBoundsForSegments(const std::vector<Segment>& segments) {
    BoundsReport report;
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& segment = segments[i];
        if (segment.type == SegmentType::Line) {
            UpdateBounds(report, segment.line.start, static_cast<int>(i), "line.start");
            UpdateBounds(report, segment.line.end, static_cast<int>(i), "line.end");
            continue;
        }
        if (segment.type == SegmentType::Arc) {
            UpdateBoundsForArc(segment.arc, static_cast<int>(i), report);
            continue;
        }
        if (!segment.spline.control_points.empty()) {
            for (const auto& pt : segment.spline.control_points) {
                UpdateBounds(report, pt, static_cast<int>(i), "spline.cp");
            }
        }
    }
    return report;
}

BoundsReport ComputeBoundsForPrimitives(const std::vector<Primitive>& primitives) {
    BoundsReport report;
    for (size_t i = 0; i < primitives.size(); ++i) {
        const auto& prim = primitives[i];
        if (prim.type == Siligen::ProcessPath::Contracts::PrimitiveType::Line) {
            UpdateBounds(report, prim.line.start, static_cast<int>(i), "line.start");
            UpdateBounds(report, prim.line.end, static_cast<int>(i), "line.end");
            continue;
        }
        if (prim.type == Siligen::ProcessPath::Contracts::PrimitiveType::Arc) {
            UpdateBoundsForArc(prim.arc, static_cast<int>(i), report);
            continue;
        }
        if (prim.type == Siligen::ProcessPath::Contracts::PrimitiveType::Circle) {
            UpdateBounds(report,
                         Point2D(prim.circle.center.x + prim.circle.radius, prim.circle.center.y),
                         static_cast<int>(i),
                         "circle.e");
            UpdateBounds(report,
                         Point2D(prim.circle.center.x - prim.circle.radius, prim.circle.center.y),
                         static_cast<int>(i),
                         "circle.w");
            UpdateBounds(report,
                         Point2D(prim.circle.center.x, prim.circle.center.y + prim.circle.radius),
                         static_cast<int>(i),
                         "circle.n");
            UpdateBounds(report,
                         Point2D(prim.circle.center.x, prim.circle.center.y - prim.circle.radius),
                         static_cast<int>(i),
                         "circle.s");
            continue;
        }
        if (prim.type == Siligen::ProcessPath::Contracts::PrimitiveType::Ellipse) {
            const auto& e = prim.ellipse;
            Point2D major = e.major_axis;
            float32 a = major.Length();
            float32 b = a * e.ratio;
            if (a > kEpsilon && b > kEpsilon) {
                float32 theta = std::atan2(major.y, major.x);
                float32 cos_t = std::cos(theta);
                float32 sin_t = std::sin(theta);
                Point2D ex(cos_t, sin_t);
                Point2D ey(-sin_t, cos_t);
                UpdateBounds(report, e.center + ex * a, static_cast<int>(i), "ellipse.ex");
                UpdateBounds(report, e.center - ex * a, static_cast<int>(i), "ellipse.-ex");
                UpdateBounds(report, e.center + ey * b, static_cast<int>(i), "ellipse.ey");
                UpdateBounds(report, e.center - ey * b, static_cast<int>(i), "ellipse.-ey");
            }
            continue;
        }
        if (prim.type == Siligen::ProcessPath::Contracts::PrimitiveType::Point) {
            UpdateBounds(report, prim.point.position, static_cast<int>(i), "point");
            continue;
        }
        if (prim.type == Siligen::ProcessPath::Contracts::PrimitiveType::Contour) {
            const auto& elements = prim.contour.elements;
            for (const auto& elem : elements) {
                if (elem.type == Siligen::ProcessPath::Contracts::ContourElementType::Line) {
                    UpdateBounds(report, elem.line.start, static_cast<int>(i), "contour.line.start");
                    UpdateBounds(report, elem.line.end, static_cast<int>(i), "contour.line.end");
                } else if (elem.type == Siligen::ProcessPath::Contracts::ContourElementType::Arc) {
                    UpdateBoundsForArc(elem.arc, static_cast<int>(i), report);
                } else if (elem.type == Siligen::ProcessPath::Contracts::ContourElementType::Spline) {
                    for (const auto& pt : elem.spline.control_points) {
                        UpdateBounds(report, pt, static_cast<int>(i), "contour.spline.cp");
                    }
                }
            }
            continue;
        }
        if (!prim.spline.control_points.empty()) {
            for (const auto& pt : prim.spline.control_points) {
                UpdateBounds(report, pt, static_cast<int>(i), "spline.cp");
            }
        }
    }
    return report;
}

void ApplyOffsetToPrimitive(Primitive& prim, const Point2D& offset) {
    if (prim.type == Siligen::ProcessPath::Contracts::PrimitiveType::Line) {
        prim.line.start += offset;
        prim.line.end += offset;
        return;
    }
    if (prim.type == Siligen::ProcessPath::Contracts::PrimitiveType::Arc) {
        prim.arc.center += offset;
        return;
    }
    if (prim.type == Siligen::ProcessPath::Contracts::PrimitiveType::Circle) {
        prim.circle.center += offset;
        return;
    }
    if (prim.type == Siligen::ProcessPath::Contracts::PrimitiveType::Ellipse) {
        prim.ellipse.center += offset;
        return;
    }
    if (prim.type == Siligen::ProcessPath::Contracts::PrimitiveType::Point) {
        prim.point.position += offset;
        return;
    }
    if (prim.type == Siligen::ProcessPath::Contracts::PrimitiveType::Contour) {
        for (auto& elem : prim.contour.elements) {
            if (elem.type == Siligen::ProcessPath::Contracts::ContourElementType::Line) {
                elem.line.start += offset;
                elem.line.end += offset;
            } else if (elem.type == Siligen::ProcessPath::Contracts::ContourElementType::Arc) {
                elem.arc.center += offset;
            } else if (elem.type == Siligen::ProcessPath::Contracts::ContourElementType::Spline) {
                for (auto& pt : elem.spline.control_points) {
                    pt += offset;
                }
            }
        }
        return;
    }
    if (!prim.spline.control_points.empty()) {
        for (auto& pt : prim.spline.control_points) {
            pt += offset;
        }
    }
}

void ApplyOffsetToPrimitives(std::vector<Primitive>& primitives, const Point2D& offset) {
    if (std::abs(offset.x) <= kEpsilon && std::abs(offset.y) <= kEpsilon) {
        return;
    }
    for (auto& prim : primitives) {
        ApplyOffsetToPrimitive(prim, offset);
    }
}

BoundsReport ComputeBoundsForProcessPath(const ProcessPath& path) {
    BoundsReport report;
    for (size_t i = 0; i < path.segments.size(); ++i) {
        const auto& segment = path.segments[i].geometry;
        if (segment.type == SegmentType::Line) {
            UpdateBounds(report, segment.line.start, static_cast<int>(i), "line.start");
            UpdateBounds(report, segment.line.end, static_cast<int>(i), "line.end");
            continue;
        }
        if (segment.type == SegmentType::Arc) {
            UpdateBoundsForArc(segment.arc, static_cast<int>(i), report);
            continue;
        }
        if (!segment.spline.control_points.empty()) {
            for (const auto& pt : segment.spline.control_points) {
                UpdateBounds(report, pt, static_cast<int>(i), "spline.cp");
            }
        }
    }
    return report;
}

BoundsReport ComputeBoundsForTrajectoryPoints(const std::vector<TrajectoryPoint>& points) {
    BoundsReport report;
    for (size_t i = 0; i < points.size(); ++i) {
        UpdateBounds(report, points[i].position, static_cast<int>(i), "traj.point");
    }
    return report;
}

BoundsReport ComputeBoundsForMotionTrajectory(const MotionTrajectory& trajectory) {
    BoundsReport report;
    for (size_t i = 0; i < trajectory.points.size(); ++i) {
        const auto& pos = trajectory.points[i].position;
        UpdateBounds(report, Point2D(pos.x, pos.y), static_cast<int>(i), "motion.point");
    }
    return report;
}

void LogBoundsReport(const char* name, const BoundsReport& report) {
    if (!report.has) {
        SILIGEN_LOG_INFO_FMT_HELPER("DXF范围[%s]: empty", name);
        return;
    }
    SILIGEN_LOG_INFO_FMT_HELPER("DXF范围[%s]: min=(%.6f, %.6f) max=(%.6f, %.6f)",
                                name,
                                report.min_x,
                                report.min_y,
                                report.max_x,
                                report.max_y);
    if (report.min_x < -1e-4f || report.min_y < -1e-4f) {
        SILIGEN_LOG_WARNING_FMT_HELPER(
            "DXF范围[%s]出现负值: min_x=%.6f(seg=%d,%s) min_y=%.6f(seg=%d,%s)",
            name,
            report.min_x,
            report.min_x_segment,
            report.min_x_label,
            report.min_y,
            report.min_y_segment,
            report.min_y_label);
    }
}

const char* SegmentTypeName(SegmentType type) {
    switch (type) {
        case SegmentType::Line:
            return "line";
        case SegmentType::Arc:
            return "arc";
        case SegmentType::Spline:
            return "spline";
        default:
            return "unknown";
    }
}

const char* ProcessTagName(ProcessTag tag) {
    switch (tag) {
        case ProcessTag::Normal:
            return "normal";
        case ProcessTag::Start:
            return "start";
        case ProcessTag::End:
            return "end";
        case ProcessTag::Corner:
            return "corner";
        case ProcessTag::Rapid:
            return "rapid";
        default:
            return "unknown";
    }
}

void LogProcessSegmentDetail(const char* name,
                             const ProcessPath& path,
                             int seg_idx,
                             const char* label) {
    if (seg_idx < 0 || static_cast<size_t>(seg_idx) >= path.segments.size()) {
        SILIGEN_LOG_WARNING_FMT_HELPER("DXF段详情[%s]: seg=%d(label=%s) 超出范围", name, seg_idx, label);
        return;
    }

    const auto& seg = path.segments[static_cast<size_t>(seg_idx)];
    const auto& geom = seg.geometry;
    SILIGEN_LOG_WARNING_FMT_HELPER(
        "DXF段详情[%s]: seg=%d label=%s type=%s tag=%s dispense=%d flow=%.3f len=%.3f curv_r=%.6f spline_approx=%d",
        name,
        seg_idx,
        label,
        SegmentTypeName(geom.type),
        ProcessTagName(seg.tag),
        seg.dispense_on ? 1 : 0,
        seg.flow_rate,
        SegmentLength(geom),
        geom.curvature_radius,
        geom.is_spline_approx ? 1 : 0);

    if (geom.type == SegmentType::Line) {
        SILIGEN_LOG_WARNING_FMT_HELPER("DXF段详情[%s]: line start=(%.6f,%.6f) end=(%.6f,%.6f)",
                                       name,
                                       geom.line.start.x,
                                       geom.line.start.y,
                                       geom.line.end.x,
                                       geom.line.end.y);
    } else if (geom.type == SegmentType::Arc) {
        Point2D start_pt = ArcPoint(geom.arc, geom.arc.start_angle_deg);
        Point2D end_pt = ArcPoint(geom.arc, geom.arc.end_angle_deg);
        SILIGEN_LOG_WARNING_FMT_HELPER(
            "DXF段详情[%s]: arc center=(%.6f,%.6f) r=%.6f start=%.6f end=%.6f cw=%d",
            name,
            geom.arc.center.x,
            geom.arc.center.y,
            geom.arc.radius,
            geom.arc.start_angle_deg,
            geom.arc.end_angle_deg,
            geom.arc.clockwise ? 1 : 0);
        SILIGEN_LOG_WARNING_FMT_HELPER("DXF段详情[%s]: arc start_pt=(%.6f,%.6f) end_pt=(%.6f,%.6f)",
                                       name,
                                       start_pt.x,
                                       start_pt.y,
                                       end_pt.x,
                                       end_pt.y);
    } else {
        const auto& cps = geom.spline.control_points;
        SILIGEN_LOG_WARNING_FMT_HELPER("DXF段详情[%s]: spline cps=%zu", name, cps.size());
        if (!cps.empty()) {
            const auto& p0 = cps.front();
            const auto& p1 = cps.back();
            SILIGEN_LOG_WARNING_FMT_HELPER("DXF段详情[%s]: spline first=(%.6f,%.6f) last=(%.6f,%.6f)",
                                           name,
                                           p0.x,
                                           p0.y,
                                           p1.x,
                                           p1.y);
        }
    }

    if (seg.constraint.has_velocity_factor || seg.constraint.has_local_speed_hint) {
        SILIGEN_LOG_WARNING_FMT_HELPER(
            "DXF段详情[%s]: constraint vel_factor=%d max_factor=%.3f local_hint=%d local_factor=%.3f",
            name,
            seg.constraint.has_velocity_factor ? 1 : 0,
            seg.constraint.max_velocity_factor,
            seg.constraint.has_local_speed_hint ? 1 : 0,
            seg.constraint.local_speed_factor);
    }
}

void LogBoundsSegmentDetails(const char* name,
                             const ProcessPath& path,
                             const BoundsReport& report) {
    if (!report.has) {
        return;
    }
    if (report.min_x < -1e-4f) {
        LogProcessSegmentDetail(name, path, report.min_x_segment, report.min_x_label);
    }
    if (report.min_y < -1e-4f && report.min_y_segment != report.min_x_segment) {
        LogProcessSegmentDetail(name, path, report.min_y_segment, report.min_y_label);
    }
}

void LogFirstNegativePoint(const char* name, const MotionTrajectory& trajectory) {
    for (size_t i = 0; i < trajectory.points.size(); ++i) {
        const auto& pt = trajectory.points[i];
        if (pt.position.x < 0.0f || pt.position.y < 0.0f) {
            SILIGEN_LOG_WARNING_FMT_HELPER(
                "DXF首次负坐标[%s]: index=%zu t=%.6f x=%.6f y=%.6f",
                name,
                i,
                pt.t,
                pt.position.x,
                pt.position.y);
            return;
        }
    }
}

void LogFirstNegativePoint(const char* name, const std::vector<TrajectoryPoint>& points) {
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& pt = points[i];
        if (pt.position.x < 0.0f || pt.position.y < 0.0f) {
            SILIGEN_LOG_WARNING_FMT_HELPER(
                "DXF首次负坐标[%s]: index=%zu t=%.6f x=%.6f y=%.6f",
                name,
                i,
                pt.timestamp,
                pt.position.x,
                pt.position.y);
            return;
        }
    }
}

float32 SpeedMagnitude(const Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint& point) {
    float32 vx = point.velocity.x;
    float32 vy = point.velocity.y;
    return std::sqrt(vx * vx + vy * vy);
}

Domain::Motion::ValueObjects::TimePlanningConfig BuildInterpolationPlanningConfig(
    const InterpolationConfig& config) {
    Domain::Motion::ValueObjects::TimePlanningConfig motion_config;
    motion_config.vmax = config.max_velocity;
    motion_config.amax = config.max_acceleration;
    motion_config.jmax = config.max_jerk;
    motion_config.sample_dt = config.time_step;
    motion_config.sample_ds = 0.0f;
    motion_config.curvature_speed_factor = config.curvature_speed_factor;
    motion_config.corner_speed_factor = 1.0f;
    motion_config.start_speed_factor = 1.0f;
    motion_config.end_speed_factor = 1.0f;
    motion_config.rapid_speed_factor = 1.0f;
    motion_config.arc_tolerance_mm = std::max(config.position_tolerance, 0.0f);
    motion_config.enforce_jerk_limit = true;
    return motion_config;
}

std::vector<TrajectoryPoint> ConvertMotionTrajectoryToTrajectoryPoints(const MotionTrajectory& trajectory) {
    std::vector<TrajectoryPoint> points;
    points.reserve(trajectory.points.size());
    for (size_t index = 0; index < trajectory.points.size(); ++index) {
        const auto& sample = trajectory.points[index];
        TrajectoryPoint point;
        point.position = Point2D(sample.position.x, sample.position.y);
        point.timestamp = sample.t;
        point.sequence_id = static_cast<uint32>(index);
        point.velocity = SpeedMagnitude(sample);
        points.push_back(point);
    }
    return points;
}

float32 ComputeTrajectoryPointLength(const MotionTrajectory& trajectory) {
    if (trajectory.points.size() < 2) {
        return 0.0f;
    }

    float32 total = 0.0f;
    for (size_t i = 1; i < trajectory.points.size(); ++i) {
        const auto& prev = trajectory.points[i - 1];
        const auto& curr = trajectory.points[i];
        total += prev.position.DistanceTo3D(curr.position);
    }
    return total;
}

float32 ResolveInterpolationStep(const DispensingPlanRequest& request) {
    if (request.sample_ds > kEpsilon) {
        return request.sample_ds;
    }
    if (request.spline_max_step_mm > kEpsilon) {
        return request.spline_max_step_mm;
    }
    return 1.0f;
}

void AppendUniquePoint(std::vector<Point2D>& points, const Point2D& point) {
    if (points.empty()) {
        points.push_back(point);
        return;
    }
    if (Distance(points.back(), point) > kEpsilon) {
        points.push_back(point);
    }
}

void SampleLinePoints(const Point2D& start,
                      const Point2D& end,
                      float32 step,
                      std::vector<Point2D>& points) {
    float32 length = Distance(start, end);
    if (length <= kEpsilon) {
        return;
    }
    if (step <= kEpsilon || length <= step) {
        AppendUniquePoint(points, start);
        AppendUniquePoint(points, end);
        return;
    }
    int slices = static_cast<int>(std::ceil(length / step));
    slices = std::max(1, slices);
    for (int i = 0; i <= slices; ++i) {
        float32 ratio = static_cast<float32>(i) / static_cast<float32>(slices);
        Point2D p = start + (end - start) * ratio;
        AppendUniquePoint(points, p);
    }
}

void SampleArcPoints(const Siligen::ProcessPath::Contracts::ArcPrimitive& arc,
                     float32 step,
                     std::vector<Point2D>& points) {
    float32 length = ComputeArcLength(arc);
    if (length <= kEpsilon) {
        return;
    }
    float32 sweep = ComputeArcSweep(arc.start_angle_deg, arc.end_angle_deg, arc.clockwise);
    int slices = 1;
    if (step > kEpsilon) {
        slices = static_cast<int>(std::ceil(length / step));
        slices = std::max(1, slices);
    }
    float32 direction = arc.clockwise ? -1.0f : 1.0f;
    for (int i = 0; i <= slices; ++i) {
        float32 ratio = static_cast<float32>(i) / static_cast<float32>(slices);
        float32 angle = arc.start_angle_deg + direction * sweep * ratio;
        AppendUniquePoint(points, ArcPoint(arc, angle));
    }
}

bool ResolveSegmentPosition(const Segment& segment,
                            float32 local_distance,
                            Point2D& out) {
    float32 segment_length = SegmentLength(segment);
    if (segment_length <= kEpsilon) {
        out = SegmentEnd(segment);
        return true;
    }

    float32 ratio = std::clamp(local_distance / segment_length, 0.0f, 1.0f);
    switch (segment.type) {
        case SegmentType::Line:
            out = segment.line.start + (segment.line.end - segment.line.start) * ratio;
            return true;
        case SegmentType::Arc: {
            float32 sweep = ComputeArcSweep(segment.arc.start_angle_deg,
                                            segment.arc.end_angle_deg,
                                            segment.arc.clockwise);
            float32 direction = segment.arc.clockwise ? -1.0f : 1.0f;
            float32 angle = segment.arc.start_angle_deg + direction * sweep * ratio;
            out = ArcPoint(segment.arc, angle);
            return true;
        }
        default:
            out = SegmentEnd(segment);
            return true;
    }
}

bool ResolveProcessPathPosition(const ProcessPath& path, float32 target_distance, Point2D& out) {
    if (path.segments.empty()) {
        return false;
    }

    if (target_distance <= 0.0f) {
        const auto& first = path.segments.front().geometry;
        if (first.type == SegmentType::Line) {
            out = first.line.start;
        } else {
            out = SegmentStart(first);
        }
        return true;
    }

    float32 accumulated = 0.0f;
    for (const auto& process_segment : path.segments) {
        const auto& segment = process_segment.geometry;
        float32 segment_length = SegmentLength(segment);
        if (segment_length <= kEpsilon) {
            continue;
        }

        if (accumulated + segment_length >= target_distance - kEpsilon) {
            return ResolveSegmentPosition(segment, target_distance - accumulated, out);
        }
        accumulated += segment_length;
    }

    out = SegmentEnd(path.segments.back().geometry);
    return true;
}

std::vector<Point2D> BuildInterpolationSeedPoints(const ProcessPath& path, float32 step) {
    std::vector<Point2D> points;
    if (path.segments.empty()) {
        return points;
    }

    for (const auto& segment : path.segments) {
        const auto& geom = segment.geometry;
        if (geom.type == SegmentType::Line) {
            SampleLinePoints(geom.line.start, geom.line.end, step, points);
        } else if (geom.type == SegmentType::Arc) {
            SampleArcPoints(geom.arc, step, points);
        } else if (!geom.spline.control_points.empty()) {
            for (size_t i = 1; i < geom.spline.control_points.size(); ++i) {
                SampleLinePoints(geom.spline.control_points[i - 1],
                                 geom.spline.control_points[i],
                                 step,
                                 points);
            }
        }
    }
    return points;
}

std::vector<double> BuildCumulativeLengths(const std::vector<Point2D>& points) {
    std::vector<double> lengths;
    if (points.empty()) {
        return lengths;
    }
    lengths.reserve(points.size());
    lengths.push_back(0.0);
    double total = 0.0;
    for (size_t i = 1; i < points.size(); ++i) {
        const double dx = static_cast<double>(points[i].x - points[i - 1].x);
        const double dy = static_cast<double>(points[i].y - points[i - 1].y);
        total += std::hypot(dx, dy);
        lengths.push_back(total);
    }
    return lengths;
}

size_t FindSegmentIndex(const std::vector<double>& cumulative, double s) {
    if (cumulative.size() < 2) {
        return 0;
    }
    for (size_t i = 1; i < cumulative.size(); ++i) {
        if (s <= cumulative[i]) {
            return i - 1;
        }
    }
    return cumulative.size() - 2;
}

void ApplyProcessInfoToTrajectory(const ProcessPath& path, float32 vmax, MotionTrajectory& trajectory) {
    if (path.segments.empty() || trajectory.points.empty()) {
        return;
    }
    std::vector<float32> cumulative;
    cumulative.reserve(path.segments.size());
    float32 total = 0.0f;
    for (const auto& seg : path.segments) {
        total += SegmentLength(seg.geometry);
        cumulative.push_back(total);
    }

    size_t seg_idx = 0;
    float32 traveled = 0.0f;
    for (size_t i = 0; i < trajectory.points.size(); ++i) {
        if (i > 0) {
            traveled += trajectory.points[i - 1].position.DistanceTo3D(trajectory.points[i].position);
        }
        while (seg_idx + 1 < cumulative.size() && traveled > cumulative[seg_idx] + kEpsilon) {
            ++seg_idx;
        }
        const auto& seg = path.segments[seg_idx];
        auto& pt = trajectory.points[i];
        pt.dispense_on = seg.dispense_on;
        pt.process_tag = seg.tag;
        if (seg.dispense_on && vmax > kEpsilon) {
            float32 speed = SpeedMagnitude(pt);
            pt.flow_rate = seg.flow_rate * (speed / vmax);
        } else {
            pt.flow_rate = 0.0f;
        }
    }
}

UnifiedTrajectoryPlanRequest BuildUnifiedPlanRequest(const DispensingPlanRequest& request) {
    UnifiedTrajectoryPlanRequest plan_request{};
    plan_request.process.default_flow = 1.0f;
    plan_request.process.lead_on_dist = 0.0f;
    plan_request.process.lead_off_dist = 0.0f;
    plan_request.process.corner_slowdown = true;
    if (request.curve_chain_angle_deg > kEpsilon) {
        plan_request.process.curve_chain_angle_deg = request.curve_chain_angle_deg;
    }
    if (request.curve_chain_max_segment_mm > kEpsilon) {
        plan_request.process.curve_chain_max_segment_mm = request.curve_chain_max_segment_mm;
    }
    plan_request.process.start_speed_factor = request.start_speed_factor;
    plan_request.process.end_speed_factor = request.end_speed_factor;
    plan_request.process.corner_speed_factor = request.corner_speed_factor;
    plan_request.process.rapid_speed_factor = request.rapid_speed_factor;
    plan_request.shaping.corner_smoothing_radius = 0.0f;
    plan_request.shaping.corner_max_deviation_mm = 0.0f;

    plan_request.normalization.approximate_splines = request.approximate_splines;
    plan_request.normalization.spline_max_step_mm = request.spline_max_step_mm;
    plan_request.normalization.spline_max_error_mm = request.spline_max_error_mm;
    if (request.continuity_tolerance_mm > kEpsilon) {
        plan_request.normalization.continuity_tolerance = request.continuity_tolerance_mm;
    }

    plan_request.motion.vmax = request.dispensing_velocity;
    plan_request.motion.amax = request.acceleration;
    plan_request.motion.jmax = request.max_jerk;
    plan_request.motion.sample_dt = request.sample_dt;
    plan_request.motion.sample_ds = request.sample_ds;
    plan_request.motion.arc_tolerance_mm = request.arc_tolerance_mm;
    plan_request.motion.curvature_speed_factor = request.compensation_profile.curvature_speed_factor;
    plan_request.motion.start_speed_factor = request.start_speed_factor;
    plan_request.motion.end_speed_factor = request.end_speed_factor;
    plan_request.motion.corner_speed_factor = request.corner_speed_factor;
    plan_request.motion.rapid_speed_factor = request.rapid_speed_factor;
    plan_request.motion.enforce_jerk_limit = true;
    if (!plan_request.process.corner_slowdown) {
        plan_request.motion.corner_speed_factor = 1.0f;
    }
    plan_request.generate_motion_trajectory = true;
    return plan_request;
}

struct TriggerArtifacts {
    std::vector<float32> distances;
    std::vector<Point2D> positions;
    std::vector<AuthorityTriggerPoint> authority_trigger_points;
    std::vector<SpacingValidationGroup> spacing_validation_groups;
    uint32 interval_ms = 0;
    float32 interval_mm = 0.0f;
    bool downgraded = false;
    bool spacing_valid = false;
    bool has_short_segment_exceptions = false;
    std::string failure_reason;
};

bool PointsNear(const Point2D& lhs, const Point2D& rhs, float32 epsilon_mm) {
    return lhs.DistanceTo(rhs) <= epsilon_mm;
}

bool ContainsNearPoint(const std::vector<Point2D>& points, const Point2D& point, float32 epsilon_mm) {
    for (const auto& existing : points) {
        if (PointsNear(existing, point, epsilon_mm)) {
            return true;
        }
    }
    return false;
}

float32 ResolveSpacingMin(const DispensingPlanRequest& request) {
    (void)request;
    return 2.7f;
}

float32 ResolveSpacingMax(const DispensingPlanRequest& request) {
    (void)request;
    return 3.3f;
}

void AppendAuthorityPoint(TriggerArtifacts& artifacts, AuthorityTriggerPoint point) {
    if (!artifacts.authority_trigger_points.empty()) {
        auto& previous = artifacts.authority_trigger_points.back();
        if (PointsNear(previous.position, point.position, kPreviewPointDedupToleranceMm) &&
            std::fabs(previous.trigger_distance_mm - point.trigger_distance_mm) <= kEpsilon) {
            previous.shared_vertex = true;
            return;
        }
        if (PointsNear(previous.position, point.position, kPreviewPointDedupToleranceMm)) {
            previous.shared_vertex = true;
            point.shared_vertex = true;
        }
    }

    artifacts.distances.push_back(point.trigger_distance_mm);
    artifacts.positions.push_back(point.position);
    artifacts.authority_trigger_points.push_back(std::move(point));
}

Result<void> AppendAnchoredAuthorityForSegment(TriggerArtifacts& artifacts,
                                              const Segment& segment,
                                              std::size_t segment_index,
                                              float32 path_offset_mm,
                                              float32 target_spacing_mm,
                                              float32 spacing_min_mm,
                                              float32 spacing_max_mm) {
    const float32 segment_length = SegmentLength(segment);
    if (segment_length <= kEpsilon) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "dispense_on 几何线段长度为0", "DispensingPlanner"));
    }
    if (target_spacing_mm <= kEpsilon) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "trigger_spatial_interval_mm 无效", "DispensingPlanner"));
    }

    const int division_count = std::max(1, static_cast<int>(std::lround(segment_length / target_spacing_mm)));
    const float32 actual_spacing_mm = segment_length / static_cast<float32>(division_count);

    SpacingValidationGroup group;
    group.segment_index = segment_index;
    group.actual_spacing_mm = actual_spacing_mm;
    group.short_segment_exception =
        actual_spacing_mm < spacing_min_mm - kEpsilon || actual_spacing_mm > spacing_max_mm + kEpsilon;
    group.within_window =
        actual_spacing_mm >= spacing_min_mm - kEpsilon && actual_spacing_mm <= spacing_max_mm + kEpsilon;
    group.points.reserve(static_cast<std::size_t>(division_count) + 1U);

    for (int division = 0; division <= division_count; ++division) {
        const float32 local_distance =
            (division == division_count) ? segment_length : actual_spacing_mm * static_cast<float32>(division);
        Point2D position;
        if (!ResolveSegmentPosition(segment, local_distance, position)) {
            return Result<void>::Failure(
                Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "authority trigger 位置解析失败", "DispensingPlanner"));
        }
        group.points.push_back(position);

        AuthorityTriggerPoint authority_point;
        authority_point.position = position;
        authority_point.trigger_distance_mm = path_offset_mm + local_distance;
        authority_point.segment_index = segment_index;
        authority_point.short_segment_exception = group.short_segment_exception;
        AppendAuthorityPoint(artifacts, std::move(authority_point));
    }

    artifacts.spacing_validation_groups.push_back(std::move(group));
    return Result<void>::Success();
}

bool ValidateGlueSpacing(const DispensingPlanRequest& request,
                         TriggerArtifacts& artifacts) {
    if (artifacts.spacing_validation_groups.empty()) {
        artifacts.spacing_valid = false;
        if (artifacts.failure_reason.empty()) {
            artifacts.failure_reason = "authority spacing groups unavailable";
        }
        return false;
    }

    const float32 min_spacing_mm = ResolveSpacingMin(request);
    const float32 max_spacing_mm = ResolveSpacingMax(request);

    bool valid = true;
    std::size_t invalid_group_count = 0;
    std::size_t short_exception_count = 0;
    for (const auto& group : artifacts.spacing_validation_groups) {
        if (group.short_segment_exception) {
            ++short_exception_count;
            continue;
        }
        if (!group.within_window) {
            ++invalid_group_count;
            valid = false;
        }
    }

    artifacts.has_short_segment_exceptions = short_exception_count > 0;
    artifacts.spacing_valid = valid;
    if (!valid) {
        std::ostringstream oss;
        oss << "spacing validation failed: invalid_groups=" << invalid_group_count
            << ", exceptions=" << short_exception_count
            << ", target=" << request.trigger_spatial_interval_mm
            << ", window=[" << min_spacing_mm << ',' << max_spacing_mm << ']';
        artifacts.failure_reason = oss.str();
        SILIGEN_LOG_WARNING(artifacts.failure_reason);
    }
    return valid;
}

Result<TriggerArtifacts> BuildTriggerArtifacts(const ProcessPath& path, const DispensingPlanRequest& request) {
    TriggerArtifacts artifacts;
    if (path.segments.empty()) {
        artifacts.failure_reason = "process path is empty";
        return Result<TriggerArtifacts>::Success(std::move(artifacts));
    }

    TriggerPlan trigger_plan;
    trigger_plan.strategy = request.dispensing_strategy;
    trigger_plan.interval_mm = request.trigger_spatial_interval_mm;
    trigger_plan.subsegment_count = request.subsegment_count;
    trigger_plan.dispense_only_cruise = request.dispense_only_cruise;
    trigger_plan.safety.duration_ms = static_cast<int32>(request.dispenser_duration_ms);
    trigger_plan.safety.valve_response_ms = static_cast<int32>(request.valve_response_ms);
    trigger_plan.safety.margin_ms = static_cast<int32>(request.safety_margin_ms);
    trigger_plan.safety.min_interval_ms = static_cast<int32>(request.min_interval_ms);
    trigger_plan.safety.downgrade_on_violation = request.downgrade_on_violation;

    TriggerPlanner trigger_planner;
    const float32 spacing_min_mm = ResolveSpacingMin(request);
    const float32 spacing_max_mm = ResolveSpacingMax(request);

    float32 path_offset_mm = 0.0f;
    for (std::size_t segment_index = 0; segment_index < path.segments.size(); ++segment_index) {
        const auto& segment = path.segments[segment_index];
        const float32 length_mm = SegmentLength(segment.geometry);

        if (segment.dispense_on) {
            if (length_mm <= kEpsilon) {
                return Result<TriggerArtifacts>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER, "dispense_on 几何线段长度为0", "DispensingPlanner"));
            }

            auto trigger_plan_result = trigger_planner.Plan(length_mm,
                                                            request.dispensing_velocity,
                                                            request.acceleration,
                                                            request.trigger_spatial_interval_mm,
                                                            request.dispenser_interval_ms,
                                                            0.0f,
                                                            trigger_plan,
                                                            request.compensation_profile);
            if (trigger_plan_result.IsError()) {
                return Result<TriggerArtifacts>::Failure(trigger_plan_result.GetError());
            }

            const auto& trigger_result = trigger_plan_result.Value();
            artifacts.interval_ms = std::max<uint32>(artifacts.interval_ms, trigger_result.interval_ms);
            artifacts.interval_mm = std::max(artifacts.interval_mm, trigger_result.plan.interval_mm);
            artifacts.downgraded = artifacts.downgraded || trigger_result.downgrade_applied;

            auto append_result = AppendAnchoredAuthorityForSegment(artifacts,
                                                                   segment.geometry,
                                                                   segment_index,
                                                                   path_offset_mm,
                                                                   request.trigger_spatial_interval_mm,
                                                                   spacing_min_mm,
                                                                   spacing_max_mm);
            if (append_result.IsError()) {
                return Result<TriggerArtifacts>::Failure(append_result.GetError());
            }
        }

        if (length_mm > kEpsilon) {
            path_offset_mm += length_mm;
        }
    }

    if (artifacts.interval_mm <= kEpsilon) {
        artifacts.interval_mm = request.trigger_spatial_interval_mm;
    }
    if (artifacts.interval_ms == 0) {
        artifacts.interval_ms = request.dispenser_interval_ms;
    }
    if (artifacts.authority_trigger_points.empty()) {
        artifacts.failure_reason = "explicit trigger authority unavailable";
    }
    ValidateGlueSpacing(request, artifacts);
    return Result<TriggerArtifacts>::Success(artifacts);
}

Result<std::vector<TrajectoryPoint>> BuildInterpolationPoints(
    const DispensingPlanRequest& request,
    const ProcessPath& path,
    const TriggerArtifacts& trigger_artifacts,
    const std::shared_ptr<Siligen::Domain::Motion::Ports::IVelocityProfilePort>& velocity_profile_port) {
    if (!request.use_interpolation_planner) {
        return Result<std::vector<TrajectoryPoint>>::Success({});
    }

    InterpolationConfig config{};
    config.max_velocity = request.dispensing_velocity;
    config.max_acceleration = request.acceleration;
    config.max_jerk = request.max_jerk;
    config.time_step = (request.sample_dt > kEpsilon) ? request.sample_dt : 0.001f;
    if (request.spline_max_error_mm > kEpsilon) {
        config.position_tolerance = request.spline_max_error_mm;
    }
    if (request.compensation_profile.curvature_speed_factor > kEpsilon) {
        config.curvature_speed_factor = request.compensation_profile.curvature_speed_factor;
    }
    if (request.trigger_spatial_interval_mm > kEpsilon) {
        config.trigger_spacing_mm = request.trigger_spatial_interval_mm;
    }

    if (config.max_velocity <= 0.0f || config.max_acceleration <= 0.0f ||
        config.time_step <= 0.0f || config.trigger_spacing_mm < 0.0f) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensingPlanner"));
    }

    if ((request.interpolation_algorithm == InterpolationAlgorithm::LINEAR ||
         request.interpolation_algorithm == InterpolationAlgorithm::CMP_COORDINATED) &&
        config.max_jerk <= 0.0f) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensingPlanner"));
    }

    std::vector<TrajectoryPoint> points;
    if (request.interpolation_algorithm == InterpolationAlgorithm::CMP_COORDINATED &&
        !trigger_artifacts.positions.empty()) {
        Siligen::CMPConfiguration cmp_config;
        cmp_config.trigger_mode = Siligen::CMPTriggerMode::POSITION_SYNC;
        cmp_config.cmp_channel = 1;
        cmp_config.pulse_width_us = 2000;
        cmp_config.trigger_position_tolerance = 0.1f;
        cmp_config.time_tolerance_ms = 1.0f;
        cmp_config.enable_compensation = true;
        cmp_config.compensation_factor = 1.0f;
        cmp_config.enable_multi_channel = false;

        std::vector<Siligen::DispensingTriggerPoint> triggers;
        triggers.reserve(trigger_artifacts.positions.size());
        for (std::size_t index = 0; index < trigger_artifacts.positions.size(); ++index) {
            Siligen::DispensingTriggerPoint trigger;
            trigger.position = trigger_artifacts.positions[index];
            trigger.trigger_distance = trigger_artifacts.distances[index];
            trigger.sequence_id = static_cast<uint32>(index);
            trigger.pulse_width_us = cmp_config.pulse_width_us;
            trigger.is_enabled = true;
            triggers.push_back(trigger);
        }

        if (triggers.empty()) {
            return Result<std::vector<TrajectoryPoint>>::Failure(
                Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "CMP触发点解析失败", "DispensingPlanner"));
        }

        Siligen::Application::Services::MotionPlanning::CmpInterpolationFacade cmp_interpolator;
        points = cmp_interpolator.PositionTriggeredDispensing(path, triggers, cmp_config, config);
    } else {
        if (request.interpolation_algorithm == InterpolationAlgorithm::CMP_COORDINATED) {
            return Result<std::vector<TrajectoryPoint>>::Failure(
                Error(ErrorCode::TRAJECTORY_GENERATION_FAILED,
                      "CMP插补缺少显式 trigger authority，不能退化为默认轨迹采样触发",
                      "DispensingPlanner"));
        }

        auto seed_points = BuildInterpolationSeedPoints(path, ResolveInterpolationStep(request));
        if (seed_points.size() < 2) {
            return Result<std::vector<TrajectoryPoint>>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensingPlanner"));
        }

        if (request.interpolation_algorithm == InterpolationAlgorithm::LINEAR) {
            Siligen::Application::Services::MotionPlanning::MotionPlanningFacade trajectory_planner(
                velocity_profile_port);
            points = ConvertMotionTrajectoryToTrajectoryPoints(
                trajectory_planner.Plan(path, BuildInterpolationPlanningConfig(config)));
        } else {
            Siligen::Application::Services::MotionPlanning::TrajectoryInterpolationFacade interpolation_facade;
            auto interpolation_result =
                interpolation_facade.Interpolate(seed_points, request.interpolation_algorithm, config);
            if (interpolation_result.IsError()) {
                return Result<std::vector<TrajectoryPoint>>::Failure(interpolation_result.GetError());
            }
            points = interpolation_result.Value();
        }

        if (!trigger_artifacts.positions.empty() &&
            !Siligen::Shared::Types::ApplyTriggerMarkersByPosition(
                points,
                trigger_artifacts.positions,
                trigger_artifacts.distances,
                Siligen::Shared::Types::kTriggerMarkerDedupToleranceMm,
                std::max(config.position_tolerance, Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm))) {
            return Result<std::vector<TrajectoryPoint>>::Failure(
                Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "显式 trigger authority 映射到插补轨迹失败", "DispensingPlanner"));
        }
    }

    if (points.empty()) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "插补结果为空", "DispensingPlanner"));
    }

    return Result<std::vector<TrajectoryPoint>>::Success(std::move(points));
}

}  // namespace

DispensingPlanner::DispensingPlanner(
    std::shared_ptr<Domain::Trajectory::Ports::IPathSourcePort> path_source,
    std::shared_ptr<Domain::Motion::Ports::IVelocityProfilePort> velocity_profile_port)
    : path_source_(std::move(path_source)),
      velocity_profile_port_(std::move(velocity_profile_port)) {}

bool DispensingPlanRequest::Validate() const noexcept {
    if (dxf_filepath.empty()) {
        return false;
    }
    if (dispensing_velocity <= kEpsilon || acceleration <= kEpsilon) {
        return false;
    }
    if (dispenser_interval_ms == 0 && trigger_spatial_interval_mm <= kEpsilon) {
        return false;
    }
    if (valve_response_ms < 0.0f || safety_margin_ms < 0.0f || min_interval_ms < 0.0f) {
        return false;
    }
    if (sample_dt < 0.0f || sample_dt > 0.1f) {
        return false;
    }
    if (sample_ds < 0.0f || sample_ds > 5.0f) {
        return false;
    }
    if (arc_tolerance_mm < 0.0f) {
        return false;
    }
    if (continuity_tolerance_mm < 0.0f) {
        return false;
    }
    if (curve_chain_angle_deg < 0.0f || curve_chain_angle_deg > 180.0f) {
        return false;
    }
    if (curve_chain_max_segment_mm < 0.0f) {
        return false;
    }
    if (max_jerk < 0.0f) {
        return false;
    }
    if (start_speed_factor < 0.0f || start_speed_factor > 1.0f) {
        return false;
    }
    if (end_speed_factor < 0.0f || end_speed_factor > 1.0f) {
        return false;
    }
    if (corner_speed_factor < 0.0f || corner_speed_factor > 1.0f) {
        return false;
    }
    if (rapid_speed_factor < 0.0f || rapid_speed_factor > 1.0f) {
        return false;
    }
    if (use_interpolation_planner &&
        (interpolation_algorithm == InterpolationAlgorithm::LINEAR ||
         interpolation_algorithm == InterpolationAlgorithm::CMP_COORDINATED) &&
        max_jerk <= kEpsilon) {
        return false;
    }
    if (bounds_check_enabled) {
        if (bounds_x_max <= bounds_x_min || bounds_y_max <= bounds_y_min) {
            return false;
        }
    }
    return true;
}

Result<DispensingPlan> DispensingPlanner::Plan(const DispensingPlanRequest& request) const noexcept {
    if (!request.Validate()) {
        return Result<DispensingPlan>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "DXF规划参数无效", "DispensingPlanner"));
    }

    if (!path_source_) {
        return Result<DispensingPlan>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "DXF路径源未注入", "DispensingPlanner"));
    }

    auto path_result = path_source_->LoadFromFile(request.dxf_filepath);
    if (path_result.IsError()) {
        return Result<DispensingPlan>::Failure(path_result.GetError());
    }

    auto primitives = path_result.Value().primitives;
    auto metadata = path_result.Value().metadata;
    if (primitives.empty()) {
        return Result<DispensingPlan>::Failure(
            Error(ErrorCode::FILE_FORMAT_INVALID, "DXF无可用路径", "DispensingPlanner"));
    }

    if (std::abs(request.dxf_offset.x) > kEpsilon || std::abs(request.dxf_offset.y) > kEpsilon) {
        ApplyOffsetToPrimitives(primitives, request.dxf_offset);
        SILIGEN_LOG_INFO_FMT_HELPER("DXF坐标平移: offset=(%.6f, %.6f)",
                                    request.dxf_offset.x,
                                    request.dxf_offset.y);
    }

    if (request.optimize_path && !primitives.empty()) {
        const float32 continuity_tolerance =
            (request.continuity_tolerance_mm > kEpsilon) ? request.continuity_tolerance_mm : 0.1f;
        const float32 intersection_tolerance = std::min(0.05f, continuity_tolerance);
        auto split_result = SplitLinePrimitivesByIntersection(primitives, metadata, intersection_tolerance);
        if (split_result.applied) {
            SILIGEN_LOG_INFO_FMT_HELPER(
                "DXF线段交点拆分完成: original=%zu, split=%zu, intersections=%zu, collinear=%zu, tol=%.4f",
                split_result.original_count,
                split_result.split_count,
                split_result.intersection_pairs,
                split_result.collinear_pairs,
                intersection_tolerance);
            primitives = std::move(split_result.primitives);
            metadata = std::move(split_result.metadata);
        }

        const float32 connectivity_tolerance = continuity_tolerance;
        const int discontinuity_before = CountPrimitiveDiscontinuity(primitives, connectivity_tolerance);
        if (ShouldRebuildByConnectivity(primitives, metadata, connectivity_tolerance, discontinuity_before)) {
            auto rebuilt = RebuildPrimitivesByConnectivity(primitives, metadata, connectivity_tolerance);
            if (!rebuilt.primitives.empty()) {
                primitives = std::move(rebuilt.primitives);
                metadata = std::move(rebuilt.metadata);
                const int discontinuity_after = CountPrimitiveDiscontinuity(primitives, connectivity_tolerance);
                SILIGEN_LOG_INFO_FMT_HELPER(
                    "DXF连通重建完成: contours=%zu, chained=%zu, reversed=%zu, discontinuity=%d->%d",
                    rebuilt.contour_count,
                    rebuilt.chained_count,
                    rebuilt.reversed_count,
                    discontinuity_before,
                    discontinuity_after);
            }
        }

        ContourOptimizationStats contour_stats;
        auto optimized = ContourOptimizationService::Optimize(primitives,
                                                        metadata,
                                                        Point2D(request.start_x, request.start_y),
                                                        true,
                                                        request.two_opt_iterations,
                                                        &contour_stats);
        if (!contour_stats.metadata_valid) {
            SILIGEN_LOG_WARNING("DXF缺少实体元数据，跳过轮廓级优化");
        } else {
            SILIGEN_LOG_INFO_FMT_HELPER(
                "DXF轮廓级优化完成: contours=%zu, reordered=%zu, reversed=%zu, rotated=%zu",
                contour_stats.contour_count,
                contour_stats.reordered_contours,
                contour_stats.reversed_contours,
                contour_stats.rotated_contours);
        }
        primitives = std::move(optimized);
    }

    auto primitive_bounds = ComputeBoundsForPrimitives(primitives);
    LogBoundsReport("primitives", primitive_bounds);
    BoundsReport final_bounds = primitive_bounds;
    if (primitive_bounds.has && (primitive_bounds.min_x < -1e-4f || primitive_bounds.min_y < -1e-4f)) {
        float32 offset_x = (primitive_bounds.min_x < -1e-4f)
                               ? (-primitive_bounds.min_x + kCoordinateSafetyMarginMm)
                               : 0.0f;
        float32 offset_y = (primitive_bounds.min_y < -1e-4f)
                               ? (-primitive_bounds.min_y + kCoordinateSafetyMarginMm)
                               : 0.0f;
        Point2D offset(offset_x, offset_y);
        ApplyOffsetToPrimitives(primitives, offset);
        SILIGEN_LOG_WARNING_FMT_HELPER(
            "DXF坐标平移以消除负值: offset=(%.6f, %.6f), min=(%.6f, %.6f)",
            offset_x,
            offset_y,
            primitive_bounds.min_x,
            primitive_bounds.min_y);
        auto shifted_bounds = ComputeBoundsForPrimitives(primitives);
        LogBoundsReport("primitives.shifted", shifted_bounds);
        final_bounds = shifted_bounds;
    }

    if (request.bounds_check_enabled && final_bounds.has) {
        const float32 bounds_width = request.bounds_x_max - request.bounds_x_min;
        const float32 bounds_height = request.bounds_y_max - request.bounds_y_min;
        const float32 shape_width = final_bounds.max_x - final_bounds.min_x;
        const float32 shape_height = final_bounds.max_y - final_bounds.min_y;
        const bool fits_x = (shape_width <= bounds_width + kBoundsToleranceMm);
        const bool fits_y = (shape_height <= bounds_height + kBoundsToleranceMm);
        if (fits_x && fits_y) {
            const float32 tx_min = request.bounds_x_min - final_bounds.min_x;
            const float32 tx_max = request.bounds_x_max - final_bounds.max_x;
            const float32 ty_min = request.bounds_y_min - final_bounds.min_y;
            const float32 ty_max = request.bounds_y_max - final_bounds.max_y;
            float32 tx = std::clamp(0.0f, tx_min, tx_max);
            float32 ty = std::clamp(0.0f, ty_min, ty_max);
            if (std::abs(tx) > kEpsilon || std::abs(ty) > kEpsilon) {
                ApplyOffsetToPrimitives(primitives, Point2D(tx, ty));
                SILIGEN_LOG_WARNING_FMT_HELPER("DXF坐标自动平移至行程内: offset=(%.6f, %.6f)", tx, ty);
                final_bounds = ComputeBoundsForPrimitives(primitives);
                LogBoundsReport("primitives.auto_fit", final_bounds);
            }
        }
    }

    if (request.bounds_check_enabled && final_bounds.has) {
        const bool out_x = (final_bounds.min_x < request.bounds_x_min - kBoundsToleranceMm) ||
                           (final_bounds.max_x > request.bounds_x_max + kBoundsToleranceMm);
        const bool out_y = (final_bounds.min_y < request.bounds_y_min - kBoundsToleranceMm) ||
                           (final_bounds.max_y > request.bounds_y_max + kBoundsToleranceMm);
        if (out_x || out_y) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3);
            oss << "DXF超出行程范围: "
                << "x=[" << final_bounds.min_x << "," << final_bounds.max_x << "] "
                << "y=[" << final_bounds.min_y << "," << final_bounds.max_y << "] "
                << "limits x=[" << request.bounds_x_min << "," << request.bounds_x_max << "] "
                << "y=[" << request.bounds_y_min << "," << request.bounds_y_max << "]";
            SILIGEN_LOG_ERROR(oss.str());
            return Result<DispensingPlan>::Failure(
                Error(ErrorCode::POSITION_OUT_OF_RANGE, oss.str(), "DispensingPlanner"));
        }
    }

    UnifiedTrajectoryPlannerService planner(velocity_profile_port_);
    auto plan_request = BuildUnifiedPlanRequest(request);
    auto plan_result = planner.Plan(primitives, plan_request);
    if (plan_result.IsError()) {
        return Result<DispensingPlan>::Failure(plan_result.GetError());
    }
    auto planned = std::move(plan_result.Value());
    {
        auto& report = planned.motion_trajectory.planning_report;
        report.segment_count = static_cast<int32>(planned.process_path.segments.size());
        report.discontinuity_count = planned.normalized.report.discontinuity_count;
        int32 rapid_count = 0;
        int32 corner_count = 0;
        float32 rapid_length = 0.0f;
        for (const auto& seg : planned.process_path.segments) {
            if (seg.tag == ProcessTag::Rapid) {
                ++rapid_count;
                rapid_length += SegmentLength(seg.geometry);
            } else if (seg.tag == ProcessTag::Corner) {
                ++corner_count;
            }
        }
        report.rapid_segment_count = rapid_count;
        report.rapid_length_mm = rapid_length;
        report.corner_segment_count = corner_count;
    }
    if (planned.motion_trajectory.points.empty() ||
        planned.motion_trajectory.planning_report.jerk_plan_failed) {
        const auto& report = planned.motion_trajectory.planning_report;
        SILIGEN_LOG_WARNING_FMT_HELPER(
            "DXF规划失败: points=%zu, jerk_failed=%d, segments=%zu, total_length=%.3f, total_time=%.3f, "
            "vmax_obs=%.3f, amax_obs=%.3f, jmax_obs=%.3f, violations=%d, time_err=%.6f, fallback=%d",
            planned.motion_trajectory.points.size(),
            report.jerk_plan_failed ? 1 : 0,
            planned.process_path.segments.size(),
            report.total_length_mm,
            report.total_time_s,
            report.max_velocity_observed,
            report.max_acceleration_observed,
            report.max_jerk_observed,
            report.constraint_violations,
            report.time_integration_error_s,
            report.time_integration_fallbacks);
        SILIGEN_LOG_WARNING_FMT_HELPER(
            "DXF规划参数: dispensing_speed_mm_s=%.3f, acceleration_mm_s2=%.3f, max_jerk=%.3f, "
            "sample_dt=%.4f, sample_ds=%.4f, interval_ms=%u, interval_mm=%.3f, use_interp=%d, algo=%d",
            request.dispensing_velocity,
            request.acceleration,
            request.max_jerk,
            request.sample_dt,
            request.sample_ds,
            request.dispenser_interval_ms,
            request.trigger_spatial_interval_mm,
            request.use_interpolation_planner ? 1 : 0,
            static_cast<int>(request.interpolation_algorithm));
        return Result<DispensingPlan>::Failure(
            Error(ErrorCode::INVALID_STATE, "轨迹规划失败", "DispensingPlanner"));
    }

    auto normalized_bounds = ComputeBoundsForSegments(planned.normalized.path.segments);
    auto process_bounds = ComputeBoundsForProcessPath(planned.process_path);
    auto shaped_bounds = ComputeBoundsForProcessPath(planned.shaped_path);
    LogBoundsReport("normalized.path", normalized_bounds);
    LogBoundsReport("process.path", process_bounds);
    LogBoundsReport("shaped.path", shaped_bounds);
    LogBoundsSegmentDetails("shaped.path", planned.shaped_path, shaped_bounds);
    LogBoundsReport("motion.trajectory", ComputeBoundsForMotionTrajectory(planned.motion_trajectory));
    LogFirstNegativePoint("motion.trajectory", planned.motion_trajectory);

    auto trigger_artifacts_result = BuildTriggerArtifacts(planned.process_path, request);
    if (trigger_artifacts_result.IsError()) {
        return Result<DispensingPlan>::Failure(trigger_artifacts_result.GetError());
    }
    auto trigger_artifacts = trigger_artifacts_result.Value();

    auto interpolation_points_result = BuildInterpolationPoints(
        request,
        planned.shaped_path,
        trigger_artifacts,
        velocity_profile_port_);
    if (interpolation_points_result.IsError()) {
        return Result<DispensingPlan>::Failure(interpolation_points_result.GetError());
    }
    auto interpolation_points = interpolation_points_result.Value();
    if (request.use_interpolation_planner) {
        LogBoundsReport("interpolation.points", ComputeBoundsForTrajectoryPoints(interpolation_points));
        LogFirstNegativePoint("interpolation.points", interpolation_points);
    }

    Siligen::Application::Services::MotionPlanning::InterpolationProgramFacade program_planner;
    auto interpolation_program = program_planner.BuildProgram(planned.shaped_path,
                                                              planned.motion_trajectory,
                                                              request.acceleration);
    if (interpolation_program.IsError()) {
        return Result<DispensingPlan>::Failure(interpolation_program.GetError());
    }

    float32 sample_total_length = ComputeTrajectoryPointLength(planned.motion_trajectory);
    float32 path_total_length = 0.0f;
    for (const auto& seg : planned.shaped_path.segments) {
        path_total_length += SegmentLength(seg.geometry);
    }
    if (sample_total_length > kEpsilon && path_total_length > kEpsilon) {
        float32 diff = std::abs(sample_total_length - path_total_length);
        if (diff > 0.5f) {
            SILIGEN_LOG_WARNING("轨迹长度与几何长度差异较大，可能影响速度映射");
        }
    }

    DispensingPlan plan;
    plan.success = true;
    plan.path = planned.normalized.path;
    plan.process_path = planned.shaped_path;
    plan.motion_trajectory = std::move(planned.motion_trajectory);
    plan.interpolation_segments = std::move(interpolation_program.Value());
    plan.interpolation_points = std::move(interpolation_points);
    plan.trigger_distances_mm = std::move(trigger_artifacts.distances);
    plan.trigger_interval_ms = trigger_artifacts.interval_ms;
    plan.trigger_interval_mm = trigger_artifacts.interval_mm;
    plan.trigger_downgrade_applied = trigger_artifacts.downgraded;
    plan.total_length_mm = plan.motion_trajectory.total_length;
    plan.estimated_time_s = plan.motion_trajectory.total_time;
    plan.preview_authority_ready = !trigger_artifacts.authority_trigger_points.empty() &&
                                   !plan.interpolation_points.empty();
    plan.preview_authority_shared_with_execution = !plan.interpolation_points.empty();
    plan.preview_spacing_valid = trigger_artifacts.spacing_valid;
    plan.preview_has_short_segment_exceptions = trigger_artifacts.has_short_segment_exceptions;
    plan.preview_failure_reason = trigger_artifacts.failure_reason;
    if (!plan.preview_authority_ready && plan.preview_failure_reason.empty()) {
        plan.preview_failure_reason = "explicit trigger authority unavailable";
    }
    plan.authority_trigger_points = std::move(trigger_artifacts.authority_trigger_points);
    plan.spacing_validation_groups = std::move(trigger_artifacts.spacing_validation_groups);

    return Result<DispensingPlan>::Success(plan);
}

std::vector<TrajectoryPoint> DispensingPlanner::BuildPreviewPoints(const DispensingPlan& plan,
                                                                   float32 spacing_mm,
                                                                   size_t max_points) const {
    (void)spacing_mm;
    (void)max_points;

    if (!plan.preview_authority_ready || plan.interpolation_points.empty()) {
        return {};
    }

    std::vector<TrajectoryPoint> preview;
    std::vector<Point2D> preview_positions;
    preview.reserve(plan.interpolation_points.size());
    for (const auto& point : plan.interpolation_points) {
        if (!point.enable_position_trigger) {
            continue;
        }
        if (ContainsNearPoint(preview_positions, point.position, kPreviewPointDedupToleranceMm)) {
            continue;
        }
        preview_positions.push_back(point.position);
        preview.push_back(point);
    }

    return preview;
}

}  // namespace Siligen::Domain::Dispensing::DomainServices



