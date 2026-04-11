#pragma once

namespace Siligen::ProcessPath::Contracts {

struct PathTopologyDiagnostics {
    bool repair_requested = false;
    bool repair_applied = false;
    bool metadata_valid = false;
    bool fragmentation_suspected = false;
    int original_primitive_count = 0;
    int repaired_primitive_count = 0;
    int contour_count = 0;
    int discontinuity_before = 0;
    int discontinuity_after = 0;
    int intersection_pairs = 0;
    int collinear_pairs = 0;
    int pre_shape_rapid_segment_count = 0;
    int pre_shape_dispense_fragment_count = 0;
    int rapid_segment_count = 0;
    int dispense_fragment_count = 0;
    int reordered_contours = 0;
    int reversed_contours = 0;
    int rotated_contours = 0;
};

}  // namespace Siligen::ProcessPath::Contracts
