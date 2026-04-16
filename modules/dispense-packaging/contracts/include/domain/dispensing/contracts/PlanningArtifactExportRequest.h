#pragma once

#include "process_path/contracts/ProcessPath.h"
#include "shared/types/Point.h"
#include "shared/types/TrajectoryTypes.h"

#include <cstddef>
#include <string>
#include <vector>

namespace Siligen::Domain::Dispensing::Contracts {

struct PlanningArtifactExportGluePointMetadata {
    std::string span_ref;
    std::size_t component_index = 0;
    std::size_t span_order_index = 0;
    std::size_t sequence_index_global = 0;
    std::size_t sequence_index_span = 0;
    std::size_t source_segment_index = 0;
    bool span_closed = false;
    float distance_mm_span = 0.0f;
    float span_total_length_mm = 0.0f;
    float span_actual_spacing_mm = 0.0f;
    float span_phase_mm = 0.0f;
    std::string span_dispatch_type;
    std::string span_split_reason;
    std::string source_kind;
};

struct PlanningArtifactExportExecutionTriggerMetadata {
    std::size_t trajectory_index = 0;
    std::size_t authority_trigger_index = 0;
    std::size_t component_index = 0;
    std::size_t span_order_index = 0;
    std::size_t source_segment_index = 0;
    float authority_distance_mm = 0.0f;
    float binding_match_error_mm = 0.0f;
    bool binding_monotonic = true;
    std::string authority_trigger_ref;
    std::string span_ref;
};

struct PlanningArtifactExportRequest {
    std::string source_path;
    std::string dxf_filename;
    Siligen::ProcessPath::Contracts::ProcessPath process_path;
    std::vector<Siligen::TrajectoryPoint> execution_trajectory_points;
    std::vector<Siligen::TrajectoryPoint> interpolation_trajectory_points;
    std::vector<Siligen::TrajectoryPoint> motion_trajectory_points;
    std::vector<Siligen::Shared::Types::Point2D> glue_points;
    std::vector<float> glue_distances_mm;
    std::vector<PlanningArtifactExportGluePointMetadata> glue_point_metadata;
    std::vector<PlanningArtifactExportExecutionTriggerMetadata> execution_trigger_metadata;
};

}  // namespace Siligen::Domain::Dispensing::Contracts
