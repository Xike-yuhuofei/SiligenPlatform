#pragma once

#include "../types/Point2D.h"

#include <cmath>

namespace Siligen::SVG {

constexpr float32 PI = 3.14159265359f;

inline float32 DegToRad(float32 degrees) {
    return degrees * PI / 180.0f;
}

inline Point2D PointOnArc(const Point2D& center, float32 radius, float32 angle_deg) {
    float32 angle_rad = DegToRad(angle_deg);
    return Point2D(center.x + radius * std::cos(angle_rad), center.y + radius * std::sin(angle_rad));
}

inline void CalcArcFlags(float32 start_angle, float32 end_angle, bool clockwise, int& large_arc, int& sweep) {
    float32 angle_diff = end_angle - start_angle;
    if (clockwise) {
        if (angle_diff < 0) angle_diff += 360.0f;
    } else {
        if (angle_diff > 0) angle_diff -= 360.0f;
    }

    large_arc = (std::abs(angle_diff) > 180.0f) ? 1 : 0;
    sweep = clockwise ? 0 : 1;
}

inline std::string ArrowPath(float32 x, float32 y, float32 angle, float32 size) {
    float32 rad = DegToRad(angle);

    float32 x1 = x + size * std::cos(rad - 2.8f);
    float32 y1 = y + size * std::sin(rad - 2.8f);
    float32 x2 = x + size * std::cos(rad + 2.8f);
    float32 y2 = y + size * std::sin(rad + 2.8f);

    return "<polygon points=\"" + std::to_string(x) + "," + std::to_string(y) + " " + std::to_string(x1) + "," +
           std::to_string(y1) + " " + std::to_string(x2) + "," + std::to_string(y2) + "\" class=\"arrow\"/>\n";
}

}  // namespace Siligen::SVG