#include "GeometryNormalizer.h"
#include "SplineApproximation.h"

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
using Siligen::ProcessPath::Contracts::SplinePrimitive;
using Siligen::Shared::Types::Point2D;

namespace {
constexpr float32 kEpsilon = 1e-6f;
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
    if (a <= kEpsilon || b <= kEpsilon) {
        return 0.0f;
    }
    float32 h = (a - b) * (a - b) / ((a + b) * (a + b));
    return kPi * (a + b) * (1.0f + (3.0f * h) / (10.0f + std::sqrt(4.0f - 3.0f * h)));
}

}  // namespace

NormalizedPath GeometryNormalizer::Normalize(const std::vector<Primitive>& primitives,
                                             const NormalizationConfig& config) const noexcept {
    NormalizedPath result;
    result.path.segments.reserve(primitives.size());

    if (config.unit_scale <= kEpsilon) {
        result.report.invalid_unit_scale = true;
        return result;
    }
    const float32 scale = config.unit_scale;

    SplineApproximation spline_approximation;
    SplineApproximationConfig spline_config;
    spline_config.max_step_mm = config.spline_max_step_mm;
    spline_config.max_error_mm = config.spline_max_error_mm;
    spline_config.min_segment_mm = 0.0f;

    auto append_line_segment = [&](const Point2D& start, const Point2D& end, bool is_point) {
        Segment segment;
        segment.type = SegmentType::Line;
        segment.line.start = start;
        segment.line.end = end;
        segment.length = segment.line.start.DistanceTo(segment.line.end);
        segment.is_point = is_point;
        if (segment.length <= kEpsilon && !segment.is_point) {
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
        if (segment.length <= kEpsilon) {
            return;
        }
        result.path.segments.push_back(segment);
        result.report.consumable_segment_count++;
    };

    auto append_spline_segments = [&](const SplinePrimitive& spline) {
        if (!config.approximate_splines) {
            result.report.skipped_spline_count++;
            return;
        }
        auto approx_segments = spline_approximation.Approximate(spline, spline_config);
        for (auto& seg : approx_segments) {
            if (seg.length <= kEpsilon) {
                continue;
            }
            result.path.segments.push_back(seg);
            result.report.consumable_segment_count++;
        }
    };

    auto append_ellipse_segments = [&](const EllipsePrimitive& ellipse) {
        float32 a = ellipse.major_axis.Length();
        float32 b = a * ellipse.ratio;
        if (a <= kEpsilon || b <= kEpsilon) {
            return;
        }
        float32 start = ellipse.start_param;
        float32 end = ellipse.end_param;
        if (std::abs(end - start) <= kEpsilon) {
            end = start + 2.0f * kPi;
        }
        float32 sweep = end - start;
        if (ellipse.clockwise) {
            if (sweep > 0.0f) {
                sweep -= 2.0f * kPi;
            }
        } else {
            if (sweep < 0.0f) {
                sweep += 2.0f * kPi;
            }
        }
        float32 perimeter = EstimateEllipsePerimeter(a, b);
        float32 length_est = perimeter * (std::abs(sweep) / (2.0f * kPi));
        float32 step_mm = (config.spline_max_step_mm > kEpsilon) ? config.spline_max_step_mm : 1.0f;
        if (step_mm <= kEpsilon) {
            step_mm = 1.0f;
        }
        constexpr float32 kMinEllipseStepMm = 0.01f;
        if (step_mm < kMinEllipseStepMm) {
            step_mm = kMinEllipseStepMm;
        }
        size_t segments = static_cast<size_t>(std::max(4.0f, std::ceil(length_est / step_mm)));
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
                SplinePrimitive spline = primitive.spline;
                for (auto& pt : spline.control_points) {
                    pt = pt * scale;
                }
                append_spline_segments(spline);
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
