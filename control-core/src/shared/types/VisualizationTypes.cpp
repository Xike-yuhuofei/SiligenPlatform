// VisualizationTypes.cpp - 可视化类型实现
#include "VisualizationTypes.h"

namespace Siligen {
namespace Shared {
namespace Types {

// DXFSegment::ToMotionSegment 实现
MotionSegment DXFSegment::ToMotionSegment() const {
    MotionSegment seg(start_point, end_point);
    seg.length = length;
    if (type == DXFSegmentType::ARC) {
        seg.segment_type = clockwise ? SegmentType::ARC_CW : SegmentType::ARC_CCW;
        seg.curvature_radius = radius;
    } else {
        seg.segment_type = SegmentType::LINEAR;
        seg.curvature_radius = 0.0f;
    }
    return seg;
}

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen
