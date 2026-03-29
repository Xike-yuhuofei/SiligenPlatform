#include "WorkflowPreviewSnapshotService.h"

#include "application/services/dispensing/PreviewSnapshotService.h"

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

}  // namespace

PreviewSnapshotResponse WorkflowPreviewSnapshotService::BuildResponse(
    const WorkflowPreviewSnapshotInput& input,
    std::size_t max_points) const {
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
    const auto payload = owner_service.BuildPayload(owner_input, max_points);

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
    response.total_length_mm = payload.total_length_mm;
    response.estimated_time_s = payload.estimated_time_s;
    response.generated_at = payload.generated_at;
    if (HasAuthoritativeGluePoints(input)) {
        response.glue_point_count = static_cast<std::uint32_t>(input.glue_points->size());
        response.point_count = response.glue_point_count;
        response.glue_points.reserve(input.glue_points->size());
        for (const auto& point : *input.glue_points) {
            Siligen::Application::UseCases::Dispensing::PreviewSnapshotPoint snapshot_point;
            snapshot_point.x = point.x;
            snapshot_point.y = point.y;
            response.glue_points.push_back(snapshot_point);
        }
    }
    response.execution_polyline.reserve(payload.trajectory_polyline.size());
    for (const auto& point : payload.trajectory_polyline) {
        Siligen::Application::UseCases::Dispensing::PreviewSnapshotPoint snapshot_point;
        snapshot_point.x = point.x;
        snapshot_point.y = point.y;
        response.execution_polyline.push_back(snapshot_point);
    }
    return response;
}

}  // namespace Siligen::Application::Services::Dispensing
