#include "sim/trajectory/trajectory.h"

#include <numeric>
#include <stdexcept>

namespace sim {

Trajectory::Trajectory(const Trajectory& other) {
    for (const auto& segment_ptr : other.segments_) {
        segments_.push_back(segment_ptr->clone());
    }
}

Trajectory& Trajectory::operator=(const Trajectory& other) {
    if (this == &other) {
        return *this;
    }

    segments_.clear();
    for (const auto& segment_ptr : other.segments_) {
        segments_.push_back(segment_ptr->clone());
    }
    return *this;
}

void Trajectory::addSegment(std::unique_ptr<Segment> segment) {
    if (segment) {
        segments_.push_back(std::move(segment));
    }
}

void Trajectory::clear() {
    segments_.clear();
}

std::size_t Trajectory::segmentCount() const {
    return segments_.size();
}

double Trajectory::totalLength() const {
    return std::accumulate(segments_.begin(),
                           segments_.end(),
                           0.0,
                           [](double total, const std::unique_ptr<Segment>& segment) {
                               return total + segment->length();
                           });
}

const Segment& Trajectory::segment(std::size_t index) const {
    if (index >= segments_.size()) {
        throw std::out_of_range("Trajectory segment index out of range.");
    }
    return *segments_[index];
}

std::vector<double> Trajectory::cumulativeLengths() const {
    std::vector<double> starts;
    starts.reserve(segments_.size());

    double running_length = 0.0;
    for (const auto& segment_ptr : segments_) {
        starts.push_back(running_length);
        running_length += segment_ptr->length();
    }

    return starts;
}

}  // namespace sim
