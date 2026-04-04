#pragma once

#include "shared/types/Point2D.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::ProcessPath::Contracts {

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
        Primitive primitive;
        primitive.type = PrimitiveType::Line;
        primitive.line.start = start;
        primitive.line.end = end;
        return primitive;
    }

    static Primitive MakeArc(const Point2D& center,
                             const float32 radius,
                             const float32 start_angle_deg,
                             const float32 end_angle_deg,
                             const bool clockwise) {
        Primitive primitive;
        primitive.type = PrimitiveType::Arc;
        primitive.arc.center = center;
        primitive.arc.radius = radius;
        primitive.arc.start_angle_deg = start_angle_deg;
        primitive.arc.end_angle_deg = end_angle_deg;
        primitive.arc.clockwise = clockwise;
        return primitive;
    }

    static Primitive MakeSpline(const std::vector<Point2D>& control_points, const bool closed = false) {
        Primitive primitive;
        primitive.type = PrimitiveType::Spline;
        primitive.spline.control_points = control_points;
        primitive.spline.closed = closed;
        return primitive;
    }

    static Primitive MakeCircle(const Point2D& center,
                                const float32 radius,
                                const float32 start_angle_deg = 0.0f,
                                const bool clockwise = false) {
        Primitive primitive;
        primitive.type = PrimitiveType::Circle;
        primitive.circle.center = center;
        primitive.circle.radius = radius;
        primitive.circle.start_angle_deg = start_angle_deg;
        primitive.circle.clockwise = clockwise;
        return primitive;
    }

    static Primitive MakeEllipse(const Point2D& center,
                                 const Point2D& major_axis,
                                 const float32 ratio,
                                 const float32 start_param = 0.0f,
                                 const float32 end_param = 6.28318530718f,
                                 const bool clockwise = false) {
        Primitive primitive;
        primitive.type = PrimitiveType::Ellipse;
        primitive.ellipse.center = center;
        primitive.ellipse.major_axis = major_axis;
        primitive.ellipse.ratio = ratio;
        primitive.ellipse.start_param = start_param;
        primitive.ellipse.end_param = end_param;
        primitive.ellipse.clockwise = clockwise;
        return primitive;
    }

    static Primitive MakePoint(const Point2D& position) {
        Primitive primitive;
        primitive.type = PrimitiveType::Point;
        primitive.point.position = position;
        return primitive;
    }

    static Primitive MakeContour(const std::vector<ContourElement>& elements, const bool closed = false) {
        Primitive primitive;
        primitive.type = PrimitiveType::Contour;
        primitive.contour.elements = elements;
        primitive.contour.closed = closed;
        return primitive;
    }
};

}  // namespace Siligen::ProcessPath::Contracts
