#pragma once

#include "shared/types/Point.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Siligen::ProcessPath::Contracts {
struct ProcessPath;
}

namespace Siligen::Domain::Dispensing::ValueObjects {
struct AuthorityTriggerLayout;
}

namespace Siligen::Application::Services::Dispensing {

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::Point2D;

struct PreviewSnapshotRequest {
    std::string plan_id;
    std::size_t max_polyline_points = 4000;
    std::size_t max_glue_points = 5000;
};

struct PreviewSnapshotPoint {
    float32 x = 0.0f;
    float32 y = 0.0f;
};

struct PreviewSnapshotResponse {
    std::string snapshot_id;
    std::string snapshot_hash;
    std::string plan_id;
    std::string preview_state;
    std::string preview_source;
    std::string preview_kind;
    std::string confirmed_at;
    std::uint32_t segment_count = 0;
    std::uint32_t point_count = 0;
    std::uint32_t glue_point_count = 0;
    std::uint32_t execution_point_count = 0;
    std::uint32_t execution_polyline_source_point_count = 0;
    std::uint32_t execution_polyline_point_count = 0;
    std::string motion_preview_source;
    std::string motion_preview_kind;
    std::uint32_t motion_preview_source_point_count = 0;
    std::uint32_t motion_preview_point_count = 0;
    bool motion_preview_is_sampled = false;
    std::string motion_preview_sampling_strategy;
    std::vector<PreviewSnapshotPoint> glue_points;
    std::vector<float32> glue_reveal_lengths_mm;
    std::vector<PreviewSnapshotPoint> execution_polyline;
    std::vector<PreviewSnapshotPoint> motion_preview_polyline;
    float32 total_length_mm = 0.0f;
    float32 estimated_time_s = 0.0f;
    std::string preview_validation_classification;
    std::string preview_exception_reason;
    std::string preview_failure_reason;
    std::string preview_diagnostic_code;
    std::string generated_at;
};

struct PreviewSnapshotInput {
    std::string snapshot_id;
    std::string snapshot_hash;
    std::string plan_id;
    std::string preview_state;
    std::string confirmed_at;
    std::uint32_t segment_count = 0;
    std::uint32_t point_count = 0;
    float32 total_length_mm = 0.0f;
    float32 estimated_time_s = 0.0f;
    std::string generated_at;
    const std::vector<Siligen::TrajectoryPoint>* trajectory_points = nullptr;
    const Siligen::ProcessPath::Contracts::ProcessPath* process_path = nullptr;
    const std::vector<Siligen::TrajectoryPoint>* motion_trajectory_points = nullptr;
    const std::vector<Point2D>* glue_points = nullptr;
    const Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout* authority_trigger_layout = nullptr;
    std::string authority_layout_id;
    bool binding_ready = false;
    std::string validation_classification;
    std::string exception_reason;
    std::string failure_reason;
    std::string diagnostic_code;
};

class PreviewSnapshotService {
public:
    PreviewSnapshotResponse BuildResponse(
        const PreviewSnapshotInput& input,
        std::size_t max_polyline_points,
        std::size_t max_glue_points) const;
};

}  // namespace Siligen::Application::Services::Dispensing
