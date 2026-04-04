#pragma once

#include "../../../../../../process-path/domain/trajectory/value-objects/GeometryUtils.h"
#include "../../../../../../process-path/domain/trajectory/value-objects/Path.h"
#include "shared/Geometry/BoostGeometryAdapter.h"

#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/algorithms/intersects.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/segment.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace Siligen::Domain::Trajectory::Geometry {

using Siligen::Domain::Trajectory::ValueObjects::ArcPoint;
using Siligen::Domain::Trajectory::ValueObjects::ArcPrimitive;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcSweep;
using Siligen::Domain::Trajectory::ValueObjects::LinePrimitive;
using Siligen::Domain::Trajectory::ValueObjects::NormalizeAngle;
using Siligen::Domain::Trajectory::ValueObjects::Segment;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::kRadToDeg;

inline boost::geometry::model::segment<Point2D> ToSegment(const LinePrimitive& line) {
    return boost::geometry::model::segment<Point2D>(line.start, line.end);
}

inline float32 LineLength(const LinePrimitive& line) {
    return boost::geometry::distance(line.start, line.end);
}

inline float32 DistancePointToLine(const Point2D& point, const LinePrimitive& line) {
    return boost::geometry::distance(point, ToSegment(line));
}

inline bool Intersects(const LinePrimitive& left, const LinePrimitive& right) {
    return boost::geometry::intersects(ToSegment(left), ToSegment(right));
}

inline bool TryToSegment(const Segment& segment, boost::geometry::model::segment<Point2D>& out) {
    if (segment.type != SegmentType::Line) {
        return false;
    }
    out = ToSegment(segment.line);
    return true;
}

inline bool TryDistancePointToSegment(const Point2D& point, const Segment& segment, float32& out) {
    boost::geometry::model::segment<Point2D> line_seg;
    if (!TryToSegment(segment, line_seg)) {
        return false;
    }
    out = boost::geometry::distance(point, line_seg);
    return true;
}

inline bool TryIntersects(const Segment& left, const Segment& right, bool& out) {
    boost::geometry::model::segment<Point2D> left_seg;
    boost::geometry::model::segment<Point2D> right_seg;
    if (!TryToSegment(left, left_seg) || !TryToSegment(right, right_seg)) {
        return false;
    }
    out = boost::geometry::intersects(left_seg, right_seg);
    return true;
}

struct ArcSampleOptions {
    float32 max_angle_step_deg = 5.0f;
    float32 max_chord_length = 0.0f;
    bool include_endpoints = true;
};

inline float32 ResolveArcStepDeg(const ArcPrimitive& arc, float32 total_sweep, const ArcSampleOptions& options) {
    float32 step = options.max_angle_step_deg;
    if (step <= 0.0f) {
        step = 5.0f;
    }

    if (options.max_chord_length > 0.0f && arc.radius > 0.0f) {
        float32 ratio = options.max_chord_length / (2.0f * arc.radius);
        if (ratio > 0.0f) {
            ratio = std::min(1.0f, ratio);
            float32 step_rad = 2.0f * std::asin(ratio);
            float32 step_deg = step_rad * kRadToDeg;
            if (step_deg > 0.0f) {
                step = std::min(step, step_deg);
            }
        }
    }

    if (step <= 0.0f) {
        step = total_sweep;
    }

    return step;
}

inline std::vector<Point2D> SampleArcPoints(const ArcPrimitive& arc, const ArcSampleOptions& options = {}) {
    std::vector<Point2D> points;
    if (arc.radius <= 0.0f) {
        points.push_back(arc.center);
        return points;
    }

    float32 sweep = ComputeArcSweep(arc.start_angle_deg, arc.end_angle_deg, arc.clockwise);
    if (sweep <= 0.0f) {
        if (options.include_endpoints) {
            points.push_back(ArcPoint(arc, arc.start_angle_deg));
        }
        return points;
    }

    float32 step = ResolveArcStepDeg(arc, sweep, options);
    size_t steps = static_cast<size_t>(std::ceil(sweep / step));
    if (steps < 1) {
        steps = 1;
    }

    if (options.include_endpoints) {
        points.push_back(ArcPoint(arc, arc.start_angle_deg));
    }

    for (size_t i = 1; i < steps; ++i) {
        float32 ratio = static_cast<float32>(i) / static_cast<float32>(steps);
        float32 delta = sweep * ratio;
        float32 angle = arc.clockwise ? (arc.start_angle_deg - delta) : (arc.start_angle_deg + delta);
        points.push_back(ArcPoint(arc, NormalizeAngle(angle)));
    }

    if (options.include_endpoints) {
        points.push_back(ArcPoint(arc, arc.end_angle_deg));
    }

    if (points.size() == 1) {
        points.push_back(points.front());
    }

    return points;
}

inline boost::geometry::model::linestring<Point2D> ToLinestring(
    const ArcPrimitive& arc,
    const ArcSampleOptions& options = {}) {
    boost::geometry::model::linestring<Point2D> line;
    auto points = SampleArcPoints(arc, options);
    line.assign(points.begin(), points.end());
    return line;
}

inline float32 DistancePointToArcApprox(const Point2D& point, const ArcPrimitive& arc, const ArcSampleOptions& options = {}) {
    auto line = ToLinestring(arc, options);
    return boost::geometry::distance(point, line);
}

inline float32 DistanceLineToArcApprox(
    const LinePrimitive& line,
    const ArcPrimitive& arc,
    const ArcSampleOptions& options = {}) {
    auto arc_line = ToLinestring(arc, options);
    return boost::geometry::distance(ToSegment(line), arc_line);
}

inline bool IntersectsLineArcApprox(const LinePrimitive& line, const ArcPrimitive& arc, const ArcSampleOptions& options = {}) {
    auto arc_line = ToLinestring(arc, options);
    return boost::geometry::intersects(ToSegment(line), arc_line);
}

inline float32 DistanceArcToArcApprox(
    const ArcPrimitive& left,
    const ArcPrimitive& right,
    const ArcSampleOptions& options = {}) {
    auto left_line = ToLinestring(left, options);
    auto right_line = ToLinestring(right, options);
    return boost::geometry::distance(left_line, right_line);
}

inline bool IntersectsArcArcApprox(const ArcPrimitive& left, const ArcPrimitive& right, const ArcSampleOptions& options = {}) {
    auto left_line = ToLinestring(left, options);
    auto right_line = ToLinestring(right, options);
    return boost::geometry::intersects(left_line, right_line);
}

}  // namespace Siligen::Domain::Trajectory::Geometry
