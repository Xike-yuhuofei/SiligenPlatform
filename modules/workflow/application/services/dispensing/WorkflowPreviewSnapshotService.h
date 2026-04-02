#pragma once

#include "application/usecases/dispensing/DispensingWorkflowUseCase.h"

#include <cstddef>
#include <string>

namespace Siligen::Application::Services::Dispensing {

struct WorkflowPreviewSnapshotInput {
    std::string snapshot_id;
    std::string snapshot_hash;
    std::string plan_id;
    std::string preview_state;
    std::string confirmed_at;
    std::uint32_t segment_count = 0;
    std::uint32_t execution_point_count = 0;
    float32 total_length_mm = 0.0f;
    float32 estimated_time_s = 0.0f;
    std::string generated_at;
    const std::vector<Siligen::TrajectoryPoint>* execution_trajectory_points = nullptr;
    const std::vector<Siligen::Shared::Types::Point2D>* glue_points = nullptr;
    std::string authority_layout_id;
    bool binding_ready = false;
    std::string validation_classification;
    std::string exception_reason;
};

class WorkflowPreviewSnapshotService {
public:
    UseCases::Dispensing::PreviewSnapshotResponse BuildResponse(
        const WorkflowPreviewSnapshotInput& input,
        std::size_t max_polyline_points,
        std::size_t max_glue_points) const;
};

}  // namespace Siligen::Application::Services::Dispensing

