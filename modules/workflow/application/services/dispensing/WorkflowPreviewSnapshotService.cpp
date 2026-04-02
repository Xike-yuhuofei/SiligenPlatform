#include "WorkflowPreviewSnapshotService.h"

#include "application/services/dispensing/PreviewSnapshotService.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Application::Services::Dispensing {

using Siligen::Application::UseCases::Dispensing::PreviewSnapshotResponse;

namespace {

bool HasAuthoritativeGluePoints(const WorkflowPreviewSnapshotInput& input) {
    return !input.authority_layout_id.empty() &&
           input.binding_ready &&
           input.validation_classification != "fail" &&
           input.glue_points != nullptr &&
           !input.glue_points->empty();
}

std::vector<Siligen::Shared::Types::Point2D> SampleGluePoints(
    const std::vector<Siligen::Shared::Types::Point2D>& points,
    std::size_t max_points) {
    if (points.empty()) {
        return {};
    }
    const std::size_t safe_max_points = std::max<std::size_t>(1, max_points);
    if (points.size() <= safe_max_points) {
        return points;
    }

    std::vector<Siligen::Shared::Types::Point2D> sampled;
    sampled.reserve(safe_max_points);
    sampled.push_back(points.front());
    if (safe_max_points == 1U) {
        return sampled;
    }

    const std::size_t interior_budget = safe_max_points - 2U;
    if (interior_budget > 0U) {
        const double step =
            static_cast<double>(points.size() - 1U) / static_cast<double>(interior_budget + 1U);
        for (std::size_t index = 1; index <= interior_budget; ++index) {
            const auto sampled_index = static_cast<std::size_t>(std::llround(step * static_cast<double>(index)));
            sampled.push_back(points[std::min<std::size_t>(points.size() - 1U, sampled_index)]);
        }
    }
    sampled.push_back(points.back());
    return sampled;
}

}  // namespace

PreviewSnapshotResponse WorkflowPreviewSnapshotService::BuildResponse(
    const WorkflowPreviewSnapshotInput& input,
    std::size_t max_polyline_points,
    std::size_t max_glue_points) const {
    PreviewSnapshotInput owner_input;
    owner_input.snapshot_id = input.snapshot_id;
    owner_input.snapshot_hash = input.snapshot_hash;
    owner_input.plan_id = input.plan_id;
    owner_input.preview_state = input.preview_state;
    owner_input.confirmed_at = input.confirmed_at;
    owner_input.segment_count = input.segment_count;
    owner_input.point_count = input.execution_point_count;
    owner_input.total_length_mm = input.total_length_mm;
    owner_input.estimated_time_s = input.estimated_time_s;
    owner_input.generated_at = input.generated_at;
    owner_input.trajectory_points = input.execution_trajectory_points;

    PreviewSnapshotService owner_service;
    const auto payload = owner_service.BuildPayload(owner_input, max_polyline_points);

    PreviewSnapshotResponse response;
    response.snapshot_id = payload.snapshot_id;
    response.snapshot_hash = payload.snapshot_hash;
    response.plan_id = payload.plan_id;
    response.preview_state = payload.preview_state;
    response.preview_source = "planned_glue_snapshot";
    response.preview_kind = "glue_points";
    response.confirmed_at = payload.confirmed_at;
    response.segment_count = payload.segment_count;
    response.execution_point_count = payload.point_count;
    response.execution_polyline_source_point_count = payload.polyline_source_point_count;
    response.execution_polyline_point_count = payload.polyline_point_count;
    response.motion_preview.source = "execution_trajectory_snapshot";
    response.motion_preview.kind = "polyline";
    response.motion_preview.source_point_count = payload.polyline_source_point_count;
    response.motion_preview.point_count = payload.polyline_point_count;
    response.motion_preview.is_sampled = payload.polyline_point_count < payload.polyline_source_point_count;
    response.motion_preview.sampling_strategy = "fixed_spacing_corner_preserving";
    response.total_length_mm = payload.total_length_mm;
    response.estimated_time_s = payload.estimated_time_s;
    response.generated_at = payload.generated_at;
    if (HasAuthoritativeGluePoints(input)) {
        response.glue_point_count = static_cast<std::uint32_t>(input.glue_points->size());
        response.point_count = response.glue_point_count;
        const auto display_glue_points = SampleGluePoints(*input.glue_points, max_glue_points);
        response.glue_points.reserve(display_glue_points.size());
        for (const auto& point : display_glue_points) {
            Siligen::Application::UseCases::Dispensing::PreviewSnapshotPoint snapshot_point;
            snapshot_point.x = point.x;
            snapshot_point.y = point.y;
            response.glue_points.push_back(snapshot_point);
        }
    }
    response.motion_preview.polyline.reserve(payload.trajectory_polyline.size());
    for (const auto& point : payload.trajectory_polyline) {
        Siligen::Application::UseCases::Dispensing::PreviewSnapshotPoint snapshot_point;
        snapshot_point.x = point.x;
        snapshot_point.y = point.y;
        response.motion_preview.polyline.push_back(snapshot_point);
    }
    response.execution_polyline = response.motion_preview.polyline;
    return response;
}

}  // namespace Siligen::Application::Services::Dispensing
