#pragma once

#include "shared/types/Error.h"
#include "shared/types/Point2D.h"
#include "shared/types/Result.h"

#include <string>

namespace Siligen::Application::UseCases::Dispensing::DXF {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;

struct DXFContourAugmentConfig {
    float32 cup_offset_far = 20.0f;             // mm, outer offset
    float32 cup_offset_mid = 17.5f;             // mm
    float32 cup_offset_near = 2.5f;             // mm
    float32 mid_path_cut_ratio = 0.34f;         // 0-1 from cup top to cut
    float32 grid_spacing = 6.0f;                // mm
    float32 grid_angle_deg = 55.0f;             // degrees
    float32 grid_u_offset = 5.813678f;          // mm, u-axis offset from inner bbox min
    float32 grid_v_offset = 0.699722f;          // mm, v-axis offset from inner bbox min
    float32 grid_min_distance_to_cups = 20.0f;  // mm
    float32 grid_border_tolerance = 0.8f;       // mm, allow slight spill outside inner
    float32 merge_epsilon = 1e-4f;              // mm, endpoint merge tolerance
    float32 intersect_epsilon = 1e-6f;          // mm, intersection tolerance
    float32 miter_limit = 5.0f;                 // miter limit
    float32 offset_approx_epsilon = 0.1f;       // mm, CGAL offset approximation error bound
    std::string contour_layer = "layer0";       // contour layer
    std::string glue_layer = "0";               // glue path layer
    std::string point_layer = "0";              // glue point layer
    bool output_r12 = true;                     // output R12-compatible POLYLINE/SEQEND
};

// Convert contour DXF into glue paths and points DXF
class DXFContourAugmenter {
public:
    Result<void> ConvertFile(const std::string& input_path,
                             const std::string& output_path,
                             const DXFContourAugmentConfig& config = DXFContourAugmentConfig());
};

}  // namespace Siligen::Application::UseCases::Dispensing::DXF
