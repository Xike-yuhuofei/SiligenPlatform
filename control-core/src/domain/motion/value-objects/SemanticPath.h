#pragma once

#include "shared/types/Point2D.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Motion::ValueObjects {

using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::float32;

enum class SegmentType {
    Line,
    Arc
};

struct LinePrimitive {
    Point2D start{};
    Point2D end{};
};

struct ArcPrimitive {
    Point2D center{};
    float32 radius = 0.0f;
    float32 start_angle_deg = 0.0f;
    float32 end_angle_deg = 0.0f;
    bool clockwise = false;
};

struct Segment {
    SegmentType type = SegmentType::Line;
    LinePrimitive line{};
    ArcPrimitive arc{};
    float32 length = 0.0f;
};

enum class SemanticTag {
    Normal,
    Start,
    End,
    Corner,
    Rapid
};

struct SemanticSegment {
    Segment geometry{};
    bool dispense_on = true;
    float32 flow_rate = 0.0f;
    SemanticTag tag = SemanticTag::Normal;
};

struct SemanticPath {
    std::vector<SemanticSegment> segments;
};

}  // namespace Siligen::Domain::Motion::ValueObjects
