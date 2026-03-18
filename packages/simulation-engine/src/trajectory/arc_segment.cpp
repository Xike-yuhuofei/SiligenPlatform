#include "sim/trajectory/arc_segment.h"

#include <cmath>

namespace sim {

ArcSegment::ArcSegment(Point2d center, double radius, double start_angle, double end_angle)
    : center_(center), radius_(radius), start_angle_(start_angle), end_angle_(end_angle) {}

SegmentType ArcSegment::type() const {
    return SegmentType::Arc;
}

double ArcSegment::length() const {
    return std::abs(end_angle_ - start_angle_) * radius_;
}

Point2d ArcSegment::startPoint() const {
    return Point2d{
        center_.x + (radius_ * std::cos(start_angle_)),
        center_.y + (radius_ * std::sin(start_angle_))
    };
}

Point2d ArcSegment::endPoint() const {
    return Point2d{
        center_.x + (radius_ * std::cos(end_angle_)),
        center_.y + (radius_ * std::sin(end_angle_))
    };
}

std::unique_ptr<Segment> ArcSegment::clone() const {
    return std::make_unique<ArcSegment>(*this);
}

}  // namespace sim
