#include "sim/trajectory/line_segment.h"

namespace sim {

LineSegment::LineSegment(Point2d start_point, Point2d end_point)
    : start_point_(start_point), end_point_(end_point) {}

SegmentType LineSegment::type() const {
    return SegmentType::Line;
}

double LineSegment::length() const {
    return norm(end_point_ - start_point_);
}

Point2d LineSegment::startPoint() const {
    return start_point_;
}

Point2d LineSegment::endPoint() const {
    return end_point_;
}

std::unique_ptr<Segment> LineSegment::clone() const {
    return std::make_unique<LineSegment>(*this);
}

}  // namespace sim
