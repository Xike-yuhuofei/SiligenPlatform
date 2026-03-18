// Point2D.h - 二维坐标点类型 (2D coordinate point type)
// Task: T014 - Phase 2 基础设施 - 共享类型系统
#pragma once

#include "Error.h"
#include "Types.h"

#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/geometries/register/point.hpp>

#include <cmath>
#include <string>

namespace Siligen {
namespace Shared {
namespace Types {

// 二维坐标点 (2D coordinate point)
struct Point2D {
    float32 x;
    float32 y;

    // 默认构造函数 (Default constructor)
    constexpr Point2D() noexcept : x(0.0f), y(0.0f) {}

    // 带参数构造函数 (Parameterized constructor)
    constexpr Point2D(float32 x_val, float32 y_val) noexcept : x(x_val), y(y_val) {}

    // Phase 3: 转换构造函数模板 - 支持任何具有x,y成员的类型
    template <typename T>
    Point2D(const T& other) : x(static_cast<float32>(other.x)), y(static_cast<float32>(other.y)) {}

    // 加法运算 (Addition operator)
    Point2D operator+(const Point2D& other) const {
        return Point2D(x + other.x, y + other.y);
    }

    // 减法运算 (Subtraction operator)
    Point2D operator-(const Point2D& other) const {
        return Point2D(x - other.x, y - other.y);
    }

    // 标量乘法 (Scalar multiplication)
    Point2D operator*(float32 scalar) const {
        return Point2D(x * scalar, y * scalar);
    }

    // 标量除法 (Scalar division)
    Point2D operator/(float32 scalar) const {
        return Point2D(x / scalar, y / scalar);
    }

    // 复合赋值运算符 (Compound assignment operators)
    Point2D& operator+=(const Point2D& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    // Phase 3: 转换赋值运算符模板 - 支持任何具有x,y成员的类型
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

    // 相等比较 (Equality comparison)
    bool operator==(const Point2D& other) const {
        constexpr float32 epsilon = 0.001f;  // 0.001mm精度
        return std::abs(x - other.x) < epsilon && std::abs(y - other.y) < epsilon;
    }

    // 不等比较 (Inequality comparison)
    bool operator!=(const Point2D& other) const {
        return !(*this == other);
    }

    // 计算到另一点的距离 (Calculate distance to another point)
    float32 DistanceTo(const Point2D& other) const;

    // 计算向量长度 (Calculate vector length)
    float32 Length() const {
        return std::sqrt(x * x + y * y);
    }

    // 归一化 (Normalize)
    Point2D Normalized() const {
        float32 len = Length();
        if (len < 0.001f) {
            return Point2D(0.0f, 0.0f);
        }
        return Point2D(x / len, y / len);
    }

    // 点积 (Dot product)
    float32 Dot(const Point2D& other) const {
        return x * other.x + y * other.y;
    }

    // 叉积 (Cross product) - 返回标量，表示z分量
    float32 Cross(const Point2D& other) const {
        return x * other.y - y * other.x;
    }

    // 旋转 (Rotate) - 绕原点旋转angle弧度
    Point2D Rotated(float32 angle_radians) const {
        float32 cos_a = std::cos(angle_radians);
        float32 sin_a = std::sin(angle_radians);
        return Point2D(x * cos_a - y * sin_a, x * sin_a + y * cos_a);
    }

    // 线性插值 (Linear interpolation)
    Point2D Lerp(const Point2D& target, float32 t) const {
        return Point2D(x + (target.x - x) * t, y + (target.y - y) * t);
    }

    // 转换为字符串 (Convert to string)
    std::string ToString() const {
        return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
    }

    // 验证坐标是否在有效范围内 (Validate if coordinates are within valid range)
    // 范围: -10000.0mm 到 10000.0mm
    bool IsValid() const {
        constexpr float32 max_range = 10000.0f;
        return std::abs(x) <= max_range && std::abs(y) <= max_range;
    }

    // 将坐标限制在有效范围内 (Clamp coordinates to valid range)
    Point2D Clamped() const {
        constexpr float32 max_range = 10000.0f;
        auto clamp = [](float32 v, float32 lo, float32 hi) { return (v < lo) ? lo : (v > hi) ? hi : v; };
        return Point2D(clamp(x, -max_range, max_range), clamp(y, -max_range, max_range));
    }
};

// 标量在前的乘法 (Scalar multiplication with scalar on left)
inline Point2D operator*(float32 scalar, const Point2D& point) {
    return point * scalar;
}

// 常用常量 (Common constants)
namespace Point2DConstants {
inline Point2D Zero() {
    return Point2D(0.0f, 0.0f);
}
inline Point2D One() {
    return Point2D(1.0f, 1.0f);
}
inline Point2D UnitX() {
    return Point2D(1.0f, 0.0f);
}
inline Point2D UnitY() {
    return Point2D(0.0f, 1.0f);
}
}  // namespace Point2DConstants

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen

BOOST_GEOMETRY_REGISTER_POINT_2D(Siligen::Shared::Types::Point2D,
                                 float,
                                 boost::geometry::cs::cartesian,
                                 x,
                                 y)

inline float32 Siligen::Shared::Types::Point2D::DistanceTo(const Point2D& other) const {
    return boost::geometry::distance(*this, other);
}
