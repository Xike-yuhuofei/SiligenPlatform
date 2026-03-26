#pragma once

#include "shared/types/Point2D.h"
#include "shared/types/Types.h"

#include <boost/geometry/algorithms/closest_points.hpp>
#include <boost/geometry/algorithms/comparable_distance.hpp>
#include <boost/geometry/algorithms/covered_by.hpp>
#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/algorithms/intersection.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/segment.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace Siligen::Shared::Geometry {

using Point2D = Siligen::Shared::Types::Point2D;
using float32 = Siligen::Shared::Types::float32;
using Segment2D = boost::geometry::model::segment<Point2D>;
using Linestring2D = boost::geometry::model::linestring<Point2D>;
using Polygon2D = boost::geometry::model::polygon<Point2D>;

inline float32 Distance(const Point2D& a, const Point2D& b) {
    return boost::geometry::distance(a, b);
}

inline float32 DistanceSquared(const Point2D& a, const Point2D& b) {
    return boost::geometry::comparable_distance(a, b);
}

inline Segment2D MakeSegment(const Point2D& a, const Point2D& b) {
    return Segment2D(a, b);
}

inline Linestring2D MakeLinestring(const std::vector<Point2D>& points, bool closed = false) {
    Linestring2D line;
    if (points.empty()) {
        return line;
    }
    line.assign(points.begin(), points.end());
    if (closed && points.size() > 1) {
        constexpr float32 kCloseEps = 1e-6f;
        if (DistanceSquared(points.front(), points.back()) > kCloseEps * kCloseEps) {
            line.push_back(points.front());
        }
    }
    return line;
}

inline float32 DistancePointToSegment(const Point2D& point, const Point2D& a, const Point2D& b) {
    return boost::geometry::distance(point, MakeSegment(a, b));
}

inline float32 DistancePointToPolyline(const Point2D& point,
                                       const std::vector<Point2D>& polyline,
                                       bool closed) {
    if (polyline.size() < 2) {
        return std::numeric_limits<float32>::max();
    }
    Linestring2D line = MakeLinestring(polyline, closed);
    if (line.size() < 2) {
        return std::numeric_limits<float32>::max();
    }
    return boost::geometry::distance(point, line);
}

inline bool IsPointInPolygon(const Point2D& point, const std::vector<Point2D>& polygon) {
    if (polygon.size() < 3) {
        return false;
    }
    Polygon2D poly;
    poly.outer().assign(polygon.begin(), polygon.end());
    boost::geometry::correct(poly);
    return boost::geometry::covered_by(point, poly);
}

inline bool IsPointOnSegment(const Point2D& point,
                             const Point2D& a,
                             const Point2D& b,
                             float32 tolerance) {
    return DistancePointToSegment(point, a, b) <= tolerance;
}

inline bool ComputeSegmentIntersection(const Point2D& a,
                                       const Point2D& b,
                                       const Point2D& c,
                                       const Point2D& d,
                                       float32 tolerance,
                                       Point2D& out,
                                       bool& collinear) {
    collinear = false;

    const Segment2D seg_a = MakeSegment(a, b);
    const Segment2D seg_b = MakeSegment(c, d);

    std::vector<Point2D> points;
    boost::geometry::intersection(seg_a, seg_b, points);
    if (!points.empty()) {
        out = points.front();
        return true;
    }

    if (boost::geometry::intersects(seg_a, seg_b)) {
        collinear = true;
        return false;
    }

    if (tolerance <= 0.0f) {
        return false;
    }

    const Point2D dir_a(b.x - a.x, b.y - a.y);
    const Point2D dir_b(d.x - c.x, d.y - c.y);
    const float32 len_a = dir_a.Length();
    const float32 len_b = dir_b.Length();
    const float32 denom = std::max(len_a, len_b);
    if (denom > 0.0f) {
        const float32 cross = std::abs(dir_a.Cross(dir_b));
        if (cross <= tolerance * denom) {
            const bool near_collinear = DistancePointToSegment(a, c, d) <= tolerance ||
                                        DistancePointToSegment(b, c, d) <= tolerance ||
                                        DistancePointToSegment(c, a, b) <= tolerance ||
                                        DistancePointToSegment(d, a, b) <= tolerance;
            if (near_collinear) {
                collinear = true;
                return false;
            }
        }
    }

    const float32 dist = boost::geometry::distance(seg_a, seg_b);
    if (dist > tolerance) {
        return false;
    }

    Segment2D closest;
    boost::geometry::closest_points(seg_a, seg_b, closest);
    const Point2D mid((closest.first.x + closest.second.x) * 0.5f,
                      (closest.first.y + closest.second.y) * 0.5f);
    out = mid;
    return true;
}

}  // namespace Siligen::Shared::Geometry
