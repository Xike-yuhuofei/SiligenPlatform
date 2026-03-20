#pragma once

#include "siligen/shared/axis_types.h"
#include "siligen/shared/error.h"
#include "siligen/shared/numeric_types.h"

#include <cmath>
#include <string>

namespace Siligen::SharedKernel {

struct Point2D {
    float32 x = 0.0f;
    float32 y = 0.0f;

    constexpr Point2D() noexcept = default;
    constexpr Point2D(float32 x_val, float32 y_val) noexcept : x(x_val), y(y_val) {}

    template <typename T>
    Point2D(const T& other) : x(static_cast<float32>(other.x)), y(static_cast<float32>(other.y)) {}

    [[nodiscard]] Point2D operator+(const Point2D& other) const { return {x + other.x, y + other.y}; }
    [[nodiscard]] Point2D operator-(const Point2D& other) const { return {x - other.x, y - other.y}; }
    [[nodiscard]] Point2D operator*(float32 scalar) const { return {x * scalar, y * scalar}; }
    [[nodiscard]] Point2D operator/(float32 scalar) const { return {x / scalar, y / scalar}; }

    Point2D& operator+=(const Point2D& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    template <typename T>
    Point2D& operator=(const T& other) {
        x = static_cast<float32>(other.x);
        y = static_cast<float32>(other.y);
        return *this;
    }

    Point2D& operator-=(const Point2D& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    Point2D& operator*=(float32 scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    Point2D& operator/=(float32 scalar) {
        x /= scalar;
        y /= scalar;
        return *this;
    }

    [[nodiscard]] bool operator==(const Point2D& other) const {
        constexpr float32 epsilon = 0.001f;
        return std::abs(x - other.x) < epsilon && std::abs(y - other.y) < epsilon;
    }

    [[nodiscard]] bool operator!=(const Point2D& other) const { return !(*this == other); }
    [[nodiscard]] float32 DistanceTo(const Point2D& other) const;
    [[nodiscard]] float32 Length() const { return std::sqrt(x * x + y * y); }

    [[nodiscard]] Point2D Normalized() const {
        const float32 len = Length();
        if (len < 0.001f) {
            return {0.0f, 0.0f};
        }
        return {x / len, y / len};
    }

    [[nodiscard]] float32 Dot(const Point2D& other) const { return x * other.x + y * other.y; }
    [[nodiscard]] float32 Cross(const Point2D& other) const { return x * other.y - y * other.x; }

    [[nodiscard]] Point2D Rotated(float32 angle_radians) const {
        const float32 cos_a = std::cos(angle_radians);
        const float32 sin_a = std::sin(angle_radians);
        return {x * cos_a - y * sin_a, x * sin_a + y * cos_a};
    }

    [[nodiscard]] Point2D Lerp(const Point2D& target, float32 t) const {
        return {x + (target.x - x) * t, y + (target.y - y) * t};
    }

    [[nodiscard]] std::string ToString() const {
        return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
    }

    [[nodiscard]] bool IsValid() const {
        constexpr float32 max_range = 10000.0f;
        return std::abs(x) <= max_range && std::abs(y) <= max_range;
    }

    [[nodiscard]] Point2D Clamped() const {
        constexpr float32 max_range = 10000.0f;
        const auto clamp = [](float32 v, float32 lo, float32 hi) {
            return (v < lo) ? lo : (v > hi) ? hi : v;
        };
        return {clamp(x, -max_range, max_range), clamp(y, -max_range, max_range)};
    }
};

inline Point2D operator*(float32 scalar, const Point2D& point) {
    return point * scalar;
}

namespace Point2DConstants {
inline Point2D Zero() { return {0.0f, 0.0f}; }
inline Point2D One() { return {1.0f, 1.0f}; }
inline Point2D UnitX() { return {1.0f, 0.0f}; }
inline Point2D UnitY() { return {0.0f, 1.0f}; }
}  // namespace Point2DConstants

}  // namespace Siligen::SharedKernel

inline Siligen::SharedKernel::float32 Siligen::SharedKernel::Point2D::DistanceTo(const Point2D& other) const {
    const auto dx = static_cast<double>(x) - static_cast<double>(other.x);
    const auto dy = static_cast<double>(y) - static_cast<double>(other.y);
    return static_cast<float32>(std::sqrt((dx * dx) + (dy * dy)));
}
