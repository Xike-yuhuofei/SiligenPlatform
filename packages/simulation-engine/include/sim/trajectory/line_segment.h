#pragma once

#include "sim/trajectory/segment.h"

namespace sim {

class LineSegment final : public Segment {
public:
    LineSegment(Point2d start_point, Point2d end_point);

    SegmentType type() const override;
    double length() const override;
    Point2d startPoint() const override;
    Point2d endPoint() const override;
    std::unique_ptr<Segment> clone() const override;

private:
    Point2d start_point_{};
    Point2d end_point_{};
};

}  // namespace sim
