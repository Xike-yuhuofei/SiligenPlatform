#pragma once

#include "sim/trajectory/segment.h"

namespace sim {

class ArcSegment final : public Segment {
public:
    ArcSegment(Point2d center, double radius, double start_angle, double end_angle);

    SegmentType type() const override;
    double length() const override;
    Point2d startPoint() const override;
    Point2d endPoint() const override;
    std::unique_ptr<Segment> clone() const override;

private:
    Point2d center_{};
    double radius_{0.0};
    double start_angle_{0.0};
    double end_angle_{0.0};
};

}  // namespace sim
