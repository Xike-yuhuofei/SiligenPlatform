#pragma once

#include "shared/types/Types.h"
#include "shared/types/VisualizationTypes.h"

#include <vector>

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::Shared::Types::DXFSegment;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::float32;

struct RegularizerConfig {
    float32 collinear_angle_tolerance_deg = 0.5f;
    float32 collinear_distance_tolerance = 0.05f;
    float32 overlap_tolerance = 0.05f;
    float32 gap_tolerance = 0.05f;
    float32 connectivity_tolerance = 0.1f;
    float32 min_line_length = 0.0f;
    float32 arc_center_tolerance = 0.05f;
    float32 arc_radius_tolerance = 0.05f;
    float32 arc_gap_tolerance_deg = 0.5f;
    bool auto_scale_tolerance = true;
    float32 auto_scale_ratio = 1e-6f;
    float32 auto_scale_min_mm = 0.01f;
    bool enable_bucket_acceleration = true;
    bool enable_line_fit = true;
    bool enable_arc_fit = true;
};

struct Contour {
    std::vector<DXFSegment> segments;
    bool closed = false;
};

struct RegularizationReport {
    size_t line_groups = 0;
    size_t line_merged = 0;
    size_t line_removed = 0;
    size_t arc_groups = 0;
    size_t arc_merged = 0;
    size_t arc_removed = 0;
    size_t contour_count = 0;
    size_t reordered_segments = 0;
    size_t disconnected_chains = 0;
};

struct RegularizedPath {
    std::vector<Contour> contours;
    RegularizationReport report;
};

class PathRegularizer {
   public:
    PathRegularizer() = default;
    ~PathRegularizer() = default;

    RegularizedPath Regularize(const std::vector<DXFSegment>& segments,
                               const RegularizerConfig& config) const;
};

}  // namespace Siligen::Domain::Trajectory::DomainServices
