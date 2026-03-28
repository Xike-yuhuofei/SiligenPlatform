#pragma once

#include "shared/types/Point.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Siligen::Application::Services::Dispensing {

using Siligen::Shared::Types::float32;

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
};

struct PreviewSnapshotPoint {
    float32 x = 0.0f;
    float32 y = 0.0f;
};

struct PreviewSnapshotPayload {
    std::string snapshot_id;
    std::string snapshot_hash;
    std::string plan_id;
    std::string preview_state;
    std::string preview_source;
    std::string confirmed_at;
    std::uint32_t segment_count = 0;
    std::uint32_t point_count = 0;
    std::uint32_t polyline_source_point_count = 0;
    std::uint32_t polyline_point_count = 0;
    std::vector<PreviewSnapshotPoint> trajectory_polyline;
    float32 total_length_mm = 0.0f;
    float32 estimated_time_s = 0.0f;
    std::string generated_at;
};

class PreviewSnapshotService {
public:
    PreviewSnapshotPayload BuildPayload(
        const PreviewSnapshotInput& input,
        std::size_t max_points) const;
};

}  // namespace Siligen::Application::Services::Dispensing
