#pragma once

#include "Point2D.h"  // 使用统一的Point2D定义
#include "Types.h"

#include <cmath>
#include <string>

namespace Siligen {

// 使用Shared::Types::Point2D作为统一的Point2D类型
using Point2D = Shared::Types::Point2D;

class Point3D : public Point2D {
   public:
    float32 z;
    constexpr Point3D() noexcept : Point2D(), z(0) {}
    constexpr Point3D(float32 x, float32 y, float32 z) noexcept : Point2D(x, y), z(z) {}
    constexpr Point3D(const Point2D& xy, float32 z) noexcept : Point2D(xy), z(z) {}
    constexpr Point3D operator+(const Point3D& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }
    constexpr Point3D operator-(const Point3D& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }
    constexpr Point3D operator*(float32 s) const noexcept {
        return {x * s, y * s, z * s};
    }
    float32 DistanceTo3D(const Point3D& o) const noexcept {
        return std::sqrt((x - o.x) * (x - o.x) + (y - o.y) * (y - o.y) + (z - o.z) * (z - o.z));
    }
    float32 Length3D() const noexcept {
        return std::sqrt(x * x + y * y + z * z);
    }
    std::string ToString3D() const {
        return "(" + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z) + ")";
    }
};

using Point = Point2D;
using DispenserPoint = Point2D;
using Position3D = Point3D;

enum class SegmentType : int8 { LINEAR = 0, ARC_CW = 1, ARC_CCW = 2 };

struct TrajectoryPoint {
    Point2D position;
    float32 velocity = 0, acceleration = 0, dispensing_time = 0, trigger_position_mm = 0, timestamp = 0;
    bool enable_position_trigger = false;
    uint16 trigger_pulse_width_us = 0;
    uint32 sequence_id = 0;
    SegmentType segment_type = SegmentType::LINEAR;
    Point2D arc_center;      // 圆弧圆心
    float32 arc_radius = 0;  // 圆弧半径
    TrajectoryPoint() = default;
    TrajectoryPoint(const Point2D& pos, float32 vel = 0) : position(pos), velocity(vel) {}
    TrajectoryPoint(float32 x, float32 y, float32 vel = 0) : position(x, y), velocity(vel) {}
    bool IsArc() const {
        return segment_type == SegmentType::ARC_CW || segment_type == SegmentType::ARC_CCW;
    }
};

struct MotionSegment {
    Point2D start_point, end_point;
    float32 start_velocity = 0, end_velocity = 0, max_velocity = 0, max_acceleration = 0, length = 0;
    float32 dispensing_start_offset = 0, dispensing_end_offset = 0;
    SegmentType segment_type = SegmentType::LINEAR;
    float32 curvature_radius = 0;
    bool enable_dispensing = false;
    MotionSegment() = default;
    MotionSegment(const Point2D& s, const Point2D& e) : start_point(s), end_point(e), length(s.DistanceTo(e)) {}
};

struct InterpolationConfig {
    float32 max_velocity = 0, max_acceleration = 0, max_jerk = 0;
    float32 position_tolerance = 0.1f, velocity_tolerance = 5, time_step = 0.001f;
    float32 corner_smoothing_radius = 1, adaptive_speed_factor = 0.8f;
    float32 corner_max_deviation_mm = 0.1f;
    float32 corner_min_radius_mm = 0.1f;
    float32 curvature_speed_factor = 0.8f;
    float32 trigger_spacing_mm = 0.0f;
    int32 look_ahead_points = 10;
    bool enable_look_ahead = true, enable_smoothing = true, enable_adaptive_speed = true;
};

inline constexpr Point MakePoint(float32 x, float32 y) noexcept {
    return {x, y};
}
inline constexpr Point3D MakePoint3D(float32 x, float32 y, float32 z) noexcept {
    return {x, y, z};
}

}  // namespace Siligen
