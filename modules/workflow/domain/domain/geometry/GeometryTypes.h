#ifndef SILIGEN_DOMAIN_GEOMETRY_TYPES_H
#define SILIGEN_DOMAIN_GEOMETRY_TYPES_H

#include "shared/types/MathConstants.h"

#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/geometries/register/point.hpp>

#include <cmath>
#include <limits>

namespace Siligen {
namespace Domain {
namespace Geometry {

/**
 * @brief 2D点值对象
 *
 * 纯数据结构,无状态,不可变
 */
struct Point2D {
    float x;
    float y;

    constexpr Point2D() noexcept : x(0.0f), y(0.0f) {}
    constexpr Point2D(float x_, float y_) noexcept : x(x_), y(y_) {}

    /**
     * @brief 计算到另一点的欧几里得距离
     */
    [[nodiscard]] float DistanceTo(const Point2D& other) const noexcept;

    /**
     * @brief 相等比较(浮点容差)
     *
     * @param other 要比较的点
     * @param tolerance 浮点容差,默认 1e-6
     * @return true 如果两点在容差范围内相等
     */
    [[nodiscard]] bool Equals(const Point2D& other, float tolerance = 1e-6f) const noexcept {
        return std::fabs(x - other.x) < tolerance &&
               std::fabs(y - other.y) < tolerance;
    }

    /**
     * @brief 相等比较(精确比较 IEEE 754标准)
     *
     * 注意: operator== 进行精确浮点比较,用于精确相等性检验。
     *       对于近似比较,请使用 Equals(other, tolerance)。
     */
    [[nodiscard]] bool operator==(const Point2D& other) const noexcept {
        return x == other.x && y == other.y;
    }

    [[nodiscard]] bool operator!=(const Point2D& other) const noexcept {
        return !(*this == other);
    }
};

/**
 * @brief LINE段值对象
 *
 * 表示DXF文件中的LINE实体
 */
struct LineSegment {
    Point2D start;
    Point2D end;

    constexpr LineSegment() noexcept = default;
    constexpr LineSegment(Point2D s, Point2D e) noexcept
        : start(s), end(e) {}

    /**
     * @brief 验证线段有效性
     *
     * @return 起点与终点不重合则有效
     */
    [[nodiscard]] bool IsValid() const noexcept {
        return start != end;
    }

    /**
     * @brief 计算线段长度
     */
    [[nodiscard]] float Length() const noexcept {
        return start.DistanceTo(end);
    }
};

/**
 * @brief ARC段值对象
 *
 * 表示DXF文件中的ARC实体
 */
struct ArcSegment {
    Point2D start;              ///< 起点
    Point2D end;                ///< 终点
    Point2D center;             ///< 圆心
    float radius;               ///< 半径
    bool counterclockwise;      ///< true=逆时针, false=顺时针

    ArcSegment() noexcept
        : start(), end(), center(), radius(0.0f), counterclockwise(true) {}

    ArcSegment(Point2D s, Point2D e, Point2D c, float r, bool ccw) noexcept
        : start(s), end(e), center(c), radius(r), counterclockwise(ccw) {}

    /**
     * @brief 验证弧段有效性
     *
     * @return 检查:
     *         1. 半径>0
     *         2. 起点到圆心距离 ≈ 半径
     *         3. 终点到圆心距离 ≈ 半径
     */
    [[nodiscard]] bool IsValid() const noexcept {
        constexpr float epsilon = 1e-3f; // DXF精度容差

        if (radius <= 0.0f) {
            return false;
        }

        const float dist_start = start.DistanceTo(center);
        const float dist_end = end.DistanceTo(center);

        const bool start_on_circle = std::fabs(dist_start - radius) < epsilon;
        const bool end_on_circle = std::fabs(dist_end - radius) < epsilon;

        return start_on_circle && end_on_circle;
    }

    /**
     * @brief 计算扫描角度(弧度)
     *
     * @return 从起点到终点的角度差,考虑旋转方向
     */
    [[nodiscard]] float SweepAngle() const noexcept {
        // 计算起点和终点相对圆心的角度
        const float angle_start = std::atan2(start.y - center.y, start.x - center.x);
        const float angle_end = std::atan2(end.y - center.y, end.x - center.x);

        float sweep = angle_end - angle_start;

        // 处理角度跨越±π的情况
        if (counterclockwise) {
            if (sweep < 0.0f) {
                sweep += 2.0f * Siligen::Shared::Types::kPi;
            }
        } else {
            if (sweep > 0.0f) {
                sweep -= 2.0f * Siligen::Shared::Types::kPi;
            }
        }

        return sweep;
    }

    /**
     * @brief 计算弧长
     *
     * @return 弧长 = 半径 × 扫描角度
     */
    [[nodiscard]] float Length() const noexcept {
        return radius * SweepAngle();
    }
};

/**
 * @brief 几何类型枚举
 */
enum class GeometryType {
    LINE,
    ARC
};

/**
 * @brief 几何段联合体(类std::variant实现)
 *
 * 由于Domain层禁止STL容器,使用tagged union手动实现
 */
struct GeometrySegment {
    GeometryType type;
    LineSegment line;  // 仅当type==LINE时有效
    ArcSegment arc;    // 仅当type==ARC时有效

    GeometrySegment() noexcept : type(GeometryType::LINE), line(), arc() {}

    GeometrySegment(GeometryType t, LineSegment l, ArcSegment a) noexcept
        : type(t), line(l), arc(a) {}

    /**
     * @brief 验证当前激活段的有效性
     */
    [[nodiscard]] bool IsValid() const noexcept {
        switch (type) {
            case GeometryType::LINE:
                return line.IsValid();
            case GeometryType::ARC:
                return arc.IsValid();
            default:
                return false;
        }
    }

    /**
     * @brief 计算段长度
     */
    [[nodiscard]] float Length() const noexcept {
        switch (type) {
            case GeometryType::LINE:
                return line.Length();
            case GeometryType::ARC:
                return arc.Length();
            default:
                return 0.0f;
        }
    }

    /**
     * @brief 获取起点
     */
    [[nodiscard]] Point2D GetStart() const noexcept {
        switch (type) {
            case GeometryType::LINE:
                return line.start;
            case GeometryType::ARC:
                return arc.start;
            default:
                return Point2D{};
        }
    }

    /**
     * @brief 获取终点
     */
    [[nodiscard]] Point2D GetEnd() const noexcept {
        switch (type) {
            case GeometryType::LINE:
                return line.end;
            case GeometryType::ARC:
                return arc.end;
            default:
                return Point2D{};
        }
    }
};

} // namespace Geometry
} // namespace Domain
} // namespace Siligen

BOOST_GEOMETRY_REGISTER_POINT_2D(Siligen::Domain::Geometry::Point2D,
                                 float,
                                 boost::geometry::cs::cartesian,
                                 x,
                                 y)

inline float Siligen::Domain::Geometry::Point2D::DistanceTo(const Point2D& other) const noexcept {
    return boost::geometry::distance(*this, other);
}

#endif // SILIGEN_DOMAIN_GEOMETRY_TYPES_H
