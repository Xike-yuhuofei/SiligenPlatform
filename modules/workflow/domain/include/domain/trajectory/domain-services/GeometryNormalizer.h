#pragma once

#include "../value-objects/Path.h"
#include "../value-objects/Primitive.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::Domain::Trajectory::ValueObjects::Path;
using Siligen::Domain::Trajectory::ValueObjects::Primitive;
using Siligen::Shared::Types::float32;

struct NormalizationConfig {
    float32 unit_scale = 1.0f;
    float32 continuity_tolerance = 0.1f;
    bool approximate_splines = false;
    float32 spline_max_step_mm = 0.0f;
    float32 spline_max_error_mm = 0.0f;
};

struct NormalizationReport {
    int discontinuity_count = 0;
    bool closed = false;
    bool invalid_unit_scale = false;
    int skipped_spline_count = 0;
    int point_primitive_count = 0;
    int consumable_segment_count = 0;
};

struct NormalizedPath {
    Path path;
    NormalizationReport report;
};

class GeometryNormalizer {
   public:
    GeometryNormalizer() = default;
    ~GeometryNormalizer() = default;

    NormalizedPath Normalize(const std::vector<Primitive>& primitives,
                             const NormalizationConfig& config) const noexcept;
};

}  // namespace Siligen::Domain::Trajectory::DomainServices
