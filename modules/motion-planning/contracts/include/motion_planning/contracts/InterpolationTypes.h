#pragma once

namespace Siligen::MotionPlanning::Contracts {

enum class InterpolationAlgorithm {
    LINEAR,
    ARC,
    SPLINE,
    TRANSITION,
    CMP_COORDINATED,
    SEAL_BEAD,
    GRID_FILLING,
    CIRCULAR_ARRAY
};

}  // namespace Siligen::MotionPlanning::Contracts
