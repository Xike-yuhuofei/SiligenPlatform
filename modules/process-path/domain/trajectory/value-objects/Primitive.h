#pragma once

#include "shared/types/Point2D.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Trajectory::ValueObjects {

using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::float32;

struct LinePrimitive {
    Point2D start{};
    Point2D end{};
};

struct ArcPrimitive {
    Point2D center{};
    float32 radius = 0.0f;
    float32 start_angle_deg = 0.0f;
    float32 end_angle_deg = 0.0f;
    bool clockwise = false;
};

struct SplinePrimitive {
    std::vector<Point2D> control_points{};
    bool closed = false;
};

struct CirclePrimitive {
    Point2D center{};
    float32 radius = 0.0f;
    float32 start_angle_deg = 0.0f;
    bool clockwise = false;
};

struct EllipsePrimitive {
    Point2D center{};
    Point2D major_axis{};
    float32 ratio = 1.0f;
    float32 start_param = 0.0f;
    float32 end_param = 6.28318530718f;
    bool clockwise = false;
};

struct PointPrimitive {
    Point2D position{};
};

enum class ContourElementType {
    Line,
    Arc,
    Spline
};

struct ContourElement {
    ContourElementType type = ContourElementType::Line;
    LinePrimitive line{};
    ArcPrimitive arc{};
    SplinePrimitive spline{};
};

struct ContourPrimitive {
    std::vector<ContourElement> elements{};
    bool closed = false;
};

enum class PrimitiveType {
    Line,
    Arc,
    Spline,
    Circle,
    Ellipse,
    Point,
    Contour
};

struct Primitive {
    PrimitiveType type = PrimitiveType::Line;
    LinePrimitive line{};
    ArcPrimitive arc{};
    SplinePrimitive spline{};
    CirclePrimitive circle{};
    EllipsePrimitive ellipse{};
    PointPrimitive point{};
    ContourPrimitive contour{};

    static Primitive MakeLine(const Point2D& start, const Point2D& end) {
        Primitive p;
        p.type = PrimitiveType::Line;
        p.line.start = start;
        p.line.end = end;
        return p;
    }

    static Primitive MakeArc(const Point2D& center,
                             float32 radius,
                             float32 start_angle_deg,
                             float32 end_angle_deg,
                             bool clockwise) {
        Primitive p;
        p.type = PrimitiveType::Arc;
        p.arc.center = center;
        p.arc.radius = radius;
        p.arc.start_angle_deg = start_angle_deg;
        p.arc.end_angle_deg = end_angle_deg;
        p.arc.clockwise = clockwise;
        return p;
    }

    static Primitive MakeSpline(const std::vector<Point2D>& control_points, bool closed = false) {
        Primitive p;
        p.type = PrimitiveType::Spline;
        p.spline.control_points = control_points;
        p.spline.closed = closed;
        return p;
    }

    static Primitive MakeCircle(const Point2D& center, float32 radius, float32 start_angle_deg = 0.0f, bool clockwise = false) {
        Primitive p;
        p.type = PrimitiveType::Circle;
        p.circle.center = center;
        p.circle.radius = radius;
        p.circle.start_angle_deg = start_angle_deg;
        p.circle.clockwise = clockwise;
        return p;
    }

    static Primitive MakeEllipse(const Point2D& center,
                                 const Point2D& major_axis,
                                 float32 ratio,
                                 float32 start_param = 0.0f,
                                 float32 end_param = 6.28318530718f,
                                 bool clockwise = false) {
        Primitive p;
        p.type = PrimitiveType::Ellipse;
        p.ellipse.center = center;
        p.ellipse.major_axis = major_axis;
        p.ellipse.ratio = ratio;
        p.ellipse.start_param = start_param;
        p.ellipse.end_param = end_param;
        p.ellipse.clockwise = clockwise;
        return p;
    }

    static Primitive MakePoint(const Point2D& position) {
        Primitive p;
        p.type = PrimitiveType::Point;
        p.point.position = position;
        return p;
    }

    static Primitive MakeContour(const std::vector<ContourElement>& elements, bool closed = false) {
        Primitive p;
        p.type = PrimitiveType::Contour;
        p.contour.elements = elements;
        p.contour.closed = closed;
        return p;
    }
};

}  // namespace Siligen::Domain::Trajectory::ValueObjects
