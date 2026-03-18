#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "sim/trajectory/segment.h"

namespace sim {

class Trajectory {
public:
    Trajectory() = default;
    Trajectory(const Trajectory& other);
    Trajectory& operator=(const Trajectory& other);
    Trajectory(Trajectory&&) noexcept = default;
    Trajectory& operator=(Trajectory&&) noexcept = default;
    ~Trajectory() = default;

    void addSegment(std::unique_ptr<Segment> segment);
    void clear();

    std::size_t segmentCount() const;
    double totalLength() const;
    const Segment& segment(std::size_t index) const;
    std::vector<double> cumulativeLengths() const;

private:
    std::vector<std::unique_ptr<Segment>> segments_{};
};

}  // namespace sim
