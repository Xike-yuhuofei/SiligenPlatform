#pragma once

#include "process_path/contracts/Primitive.h"

#include <vector>

namespace Siligen::ProcessPath::Contracts {

using Siligen::Shared::Types::float32;

enum class SegmentType {
    Line,
    Arc,
    Spline
};

struct Segment {
    SegmentType type = SegmentType::Line;
    LinePrimitive line{};
    ArcPrimitive arc{};
    SplinePrimitive spline{};
    float32 length = 0.0f;
    float32 curvature_radius = 0.0f;
    bool is_spline_approx = false;
    bool is_point = false;
};

struct Path {
    std::vector<Segment> segments;
    bool closed = false;
};

}  // namespace Siligen::ProcessPath::Contracts
