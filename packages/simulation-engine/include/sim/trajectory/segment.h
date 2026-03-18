#pragma once

#include <cmath>
#include <memory>

namespace sim {

struct Point2d {
    double x{0.0};
    double y{0.0};
};

inline Point2d operator-(const Point2d& lhs, const Point2d& rhs) {
    return Point2d{lhs.x - rhs.x, lhs.y - rhs.y};
}

inline double norm(const Point2d& point) {
    return std::sqrt((point.x * point.x) + (point.y * point.y));
}

enum class SegmentType {
    Line,
    Arc
};

class Segment {
public:
    virtual ~Segment() = default;

    virtual SegmentType type() const = 0;
    virtual double length() const = 0;
    virtual Point2d startPoint() const = 0;
    virtual Point2d endPoint() const = 0;
    virtual std::unique_ptr<Segment> clone() const = 0;
};

}  // namespace sim
