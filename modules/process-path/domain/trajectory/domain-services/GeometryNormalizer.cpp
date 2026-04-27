#include "GeometryNormalizer.h"

#include "process_path/contracts/GeometryUtils.h"
#include "shared/types/MathConstants.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::ProcessPath::Contracts::ArcPrimitive;
using Siligen::ProcessPath::Contracts::CirclePrimitive;
using Siligen::ProcessPath::Contracts::ComputeArcLength;
using Siligen::ProcessPath::Contracts::ContourElement;
using Siligen::ProcessPath::Contracts::ContourElementType;
using Siligen::ProcessPath::Contracts::ContourPrimitive;
using Siligen::ProcessPath::Contracts::EllipsePoint;
using Siligen::ProcessPath::Contracts::EllipsePrimitive;
using Siligen::ProcessPath::Contracts::LinePrimitive;
using Siligen::ProcessPath::Contracts::NormalizeAngle;
using Siligen::ProcessPath::Contracts::PointPrimitive;
using Siligen::ProcessPath::Contracts::Segment;
using Siligen::ProcessPath::Contracts::SegmentEnd;
using Siligen::ProcessPath::Contracts::SegmentStart;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::Shared::Types::Point2D;

namespace {
using Siligen::ProcessPath::Contracts::kGeometryEpsilon;
using Siligen::Shared::Types::kDegToRad;
using Siligen::Shared::Types::kPi;
using Siligen::Shared::Types::kRadToDeg;

bool UpdateArcStart(ArcPrimitive& arc, const Point2D& start, float32 tolerance) {
    float32 dist = arc.center.DistanceTo(start);
    if (std::abs(dist - arc.radius) > tolerance) {
        return false;
    }
    float32 angle = std::atan2(start.y - arc.center.y, start.x - arc.center.x) * kRadToDeg;
    arc.start_angle_deg = NormalizeAngle(angle);
    return true;
}

float32 EstimateEllipsePerimeter(float32 a, float32 b) {
    if (a <= kGeometryEpsilon || b <= kGeometryEpsilon) {
        return 0.0f;
    }
    float32 h = (a - b) * (a - b) / ((a + b) * (a + b));
    return kPi * (a + b) * (1.0f + (3.0f * h) / (10.0f + std::sqrt(4.0f - 3.0f * h)));
}

float32 ResolveEllipseSweep(const EllipsePrimitive& ellipse) {
    float32 start = ellipse.start_param;
    float32 end = ellipse.end_param;
    if (std::abs(end - start) <= kGeometryEpsilon) {
        end = start + 2.0f * kPi;
    }
    float32 sweep = end - start;
    if (ellipse.clockwise) {
        if (sweep > 0.0f) {
            sweep -= 2.0f * kPi;
        }
    } else if (sweep < 0.0f) {
        sweep += 2.0f * kPi;
    }
    return sweep;
}

size_t EstimateEllipseSegmentCount(const EllipsePrimitive& ellipse, const NormalizationConfig& config) {
    const float32 a = ellipse.major_axis.Length();
    const float32 b = a * ellipse.ratio;
    if (a <= kGeometryEpsilon || b <= kGeometryEpsilon) {
        return 0U;
    }
    const float32 sweep = ResolveEllipseSweep(ellipse);
    const float32 perimeter = EstimateEllipsePerimeter(a, b);
    const float32 length_est = perimeter * (std::abs(sweep) / (2.0f * kPi));
    float32 step_mm = (config.curve_flatten_max_step_mm > kGeometryEpsilon) ? config.curve_flatten_max_step_mm : 1.0f;
    if (config.curve_flatten_max_error_mm > kGeometryEpsilon) {
        const float32 major = std::max(a, b);
        const float32 minor = std::min(a, b);
        const float32 min_curvature_radius = (minor * minor) / std::max(major, kGeometryEpsilon);
        const float32 error_step = std::sqrt(8.0f * min_curvature_radius * config.curve_flatten_max_error_mm);
        if (error_step > kGeometryEpsilon && std::isfinite(error_step)) {
            step_mm = std::min(step_mm, error_step);
        }
    }
    constexpr float32 kMinEllipseStepMm = 0.01f;
    step_mm = std::max(step_mm, kMinEllipseStepMm);
    return static_cast<size_t>(std::max(4.0f, std::ceil(length_est / step_mm)));
}

size_t EstimatePrimitiveSegmentCount(const Primitive& primitive, const NormalizationConfig& config) {
    using PrimitiveType = Siligen::ProcessPath::Contracts::PrimitiveType;
    switch (primitive.type) {
        case PrimitiveType::Line:
        case PrimitiveType::Arc:
        case PrimitiveType::Circle:
        case PrimitiveType::Point:
            return 1U;
        case PrimitiveType::Ellipse:
            return EstimateEllipseSegmentCount(primitive.ellipse, config);
        case PrimitiveType::Contour: {
            size_t count = 0U;
            for (const auto& element : primitive.contour.elements) {
                Primitive nested;
                if (element.type == ContourElementType::Line) {
                    nested.type = PrimitiveType::Line;
                    nested.line = element.line;
                } else if (element.type == ContourElementType::Arc) {
                    nested.type = PrimitiveType::Arc;
                    nested.arc = element.arc;
                } else if (element.type == ContourElementType::Spline) {
                    nested.type = PrimitiveType::Spline;
                }
                count += EstimatePrimitiveSegmentCount(nested, config);
            }
            return count;
        }
        case PrimitiveType::Spline:
        default:
            return 0U;
    }
}

}  // namespace

NormalizedPath GeometryNormalizer::Normalize(const std::vector<Primitive>& primitives,
                                             const NormalizationConfig& config) const noexcept {
    NormalizedPath result;
    size_t estimated_segments = 0U;
    for (const auto& primitive : primitives) {
        estimated_segments += EstimatePrimitiveSegmentCount(primitive, config);
    }
    result.path.segments.reserve(std::max(primitives.size(), estimated_segments));

    if (config.unit_scale <= kGeometryEpsilon) {
        result.report.invalid_unit_scale = true;
        return result;
    }
    const float32 scale = config.unit_scale;

    auto append_line_segment = [&](const Point2D& start, const Point2D& end, bool is_point) {
        Segment segment;
        segment.type = SegmentType::Line;
        segment.line.start = start;
        segment.line.end = end;
        segment.length = segment.line.start.DistanceTo(segment.line.end);
        segment.is_point = is_point;
        if (segment.length <= kGeometryEpsilon && !segment.is_point) {
            return;
        }
        result.path.segments.push_back(segment);
        if (!segment.is_point) {
            result.report.consumable_segment_count++;
        }
    };

    auto append_arc_segment = [&](const ArcPrimitive& arc) {
        Segment segment;
        segment.type = SegmentType::Arc;
        segment.arc = arc;
        segment.length = ComputeArcLength(segment.arc);
        if (segment.length <= kGeometryEpsilon) {
            return;
        }
        result.path.segments.push_back(segment);
        result.report.consumable_segment_count++;
    };

    auto reject_spline = [&]() {
        result.report.skipped_spline_count++;
    };

    auto append_ellipse_segments = [&](const EllipsePrimitive& ellipse) {
        float32 a = ellipse.major_axis.Length();
        float32 b = a * ellipse.ratio;
        if (a <= kGeometryEpsilon || b <= kGeometryEpsilon) {
            return;
        }
        float32 start = ellipse.start_param;
        const float32 sweep = ResolveEllipseSweep(ellipse);
        const size_t segments = EstimateEllipseSegmentCount(ellipse, config);
        float32 step = (segments > 0) ? (sweep / static_cast<float32>(segments)) : sweep;
        Point2D prev = EllipsePoint(ellipse, start);
        for (size_t i = 1; i <= segments; ++i) {
            float32 t = start + step * static_cast<float32>(i);
            Point2D curr = EllipsePoint(ellipse, t);
            append_line_segment(prev, curr, false);
            prev = curr;
        }
    };

    auto append_primitive = [&](const Primitive& primitive, auto&& self_ref) -> void {
        using PrimitiveType = Siligen::ProcessPath::Contracts::PrimitiveType;
        switch (primitive.type) {
            case PrimitiveType::Line: {
                append_line_segment(primitive.line.start * scale, primitive.line.end * scale, false);
                break;
            }
            case PrimitiveType::Arc: {
                ArcPrimitive arc = primitive.arc;
                arc.center = primitive.arc.center * scale;
                arc.radius = primitive.arc.radius * scale;
                append_arc_segment(arc);
                break;
            }
            case PrimitiveType::Spline: {
                reject_spline();
                break;
            }
            case PrimitiveType::Circle: {
                CirclePrimitive circle = primitive.circle;
                ArcPrimitive arc{};
                arc.center = circle.center * scale;
                arc.radius = circle.radius * scale;
                arc.start_angle_deg = circle.start_angle_deg;
                arc.end_angle_deg = circle.start_angle_deg + 360.0f;
                arc.clockwise = circle.clockwise;
                append_arc_segment(arc);
                break;
            }
            case PrimitiveType::Ellipse: {
                EllipsePrimitive ellipse = primitive.ellipse;
                ellipse.center = ellipse.center * scale;
                ellipse.major_axis = ellipse.major_axis * scale;
                append_ellipse_segments(ellipse);
                break;
            }
            case PrimitiveType::Point: {
                result.report.point_primitive_count++;
                Point2D pt = primitive.point.position * scale;
                append_line_segment(pt, pt, true);
                break;
            }
            case PrimitiveType::Contour: {
                for (const auto& element : primitive.contour.elements) {
                    Primitive nested;
                    if (element.type == ContourElementType::Line) {
                        nested.type = PrimitiveType::Line;
                        nested.line = element.line;
                    } else if (element.type == ContourElementType::Arc) {
                        nested.type = PrimitiveType::Arc;
                        nested.arc = element.arc;
                    } else if (element.type == ContourElementType::Spline) {
                        nested.type = PrimitiveType::Spline;
                        nested.spline = element.spline;
                    } else {
                        continue;
                    }
                    self_ref(nested, self_ref);
                }
                break;
            }
            default:
                break;
        }
    };

    for (const auto& primitive : primitives) {
        append_primitive(primitive, append_primitive);
    }

    if (result.path.segments.empty()) {
        return result;
    }

    // continuity check & snapping
    for (size_t i = 1; i < result.path.segments.size(); ++i) {
        auto& prev = result.path.segments[i - 1];
        auto& curr = result.path.segments[i];
        Point2D prev_end = SegmentEnd(prev);
        Point2D curr_start = SegmentStart(curr);
        float32 gap = prev_end.DistanceTo(curr_start);
        if (gap > config.continuity_tolerance) {
            result.report.discontinuity_count++;
            continue;
        }

        if (curr.type == SegmentType::Line) {
            curr.line.start = prev_end;
            curr.length = curr.line.start.DistanceTo(curr.line.end);
        } else {
            UpdateArcStart(curr.arc, prev_end, config.continuity_tolerance);
            curr.length = ComputeArcLength(curr.arc);
        }
    }

    Point2D first_start = SegmentStart(result.path.segments.front());
    Point2D last_end = SegmentEnd(result.path.segments.back());
    result.report.closed = first_start.DistanceTo(last_end) <= config.continuity_tolerance;
    result.path.closed = result.report.closed;

    return result;
}

}  // namespace Siligen::Domain::Trajectory::DomainServices
