#include "WorkflowPreviewSnapshotService.h"

#include "application/services/dispensing/PreviewSnapshotService.h"
#include "domain/dispensing/planning/domain-services/CurveFlatteningService.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Application::Services::Dispensing {

using Siligen::Application::UseCases::Dispensing::PreviewSnapshotResponse;

namespace {

constexpr double kPreviewDuplicateEpsilonMm = 1e-4;
constexpr double kPreviewCornerMinTurnDeg = 20.0;
constexpr double kPreviewCornerMinLegMm = 2e-1;

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

double DistanceSquared(
    const Siligen::Shared::Types::Point2D& lhs,
    const Siligen::Shared::Types::Point2D& rhs) {
    const double dx = static_cast<double>(lhs.x) - static_cast<double>(rhs.x);
    const double dy = static_cast<double>(lhs.y) - static_cast<double>(rhs.y);
    return dx * dx + dy * dy;
}

double Distance(
    const Siligen::Shared::Types::Point2D& lhs,
    const Siligen::Shared::Types::Point2D& rhs) {
    return std::sqrt(DistanceSquared(lhs, rhs));
}

bool NearlyEqualPoint(
    const Siligen::Shared::Types::Point2D& lhs,
    const Siligen::Shared::Types::Point2D& rhs,
    double epsilon_mm) {
    const double epsilon_sq = epsilon_mm * epsilon_mm;
    return DistanceSquared(lhs, rhs) <= epsilon_sq;
}

void AppendDistinctPoint(
    std::vector<Siligen::Shared::Types::Point2D>& points,
    const Siligen::Shared::Types::Point2D& candidate) {
    if (points.empty() || !NearlyEqualPoint(points.back(), candidate, kPreviewDuplicateEpsilonMm)) {
        points.push_back(candidate);
    }
}

std::vector<std::size_t> DetectCornerAnchorIndices(
    const std::vector<Siligen::Shared::Types::Point2D>& points) {
    std::vector<std::size_t> anchors;
    if (points.size() < 3U) {
        return anchors;
    }

    const auto find_context_index = [&](std::size_t pivot, int direction) -> std::size_t {
        std::size_t current = pivot;
        double accumulated_length = 0.0;
        while (true) {
            if (direction < 0) {
                if (current == 0U) {
                    break;
                }
                const std::size_t next = current - 1U;
                accumulated_length += Distance(points[current], points[next]);
                current = next;
            } else {
                if (current + 1U >= points.size()) {
                    break;
                }
                const std::size_t next = current + 1U;
                accumulated_length += Distance(points[current], points[next]);
                current = next;
            }

            if (accumulated_length >= kPreviewCornerMinLegMm) {
                break;
            }
        }
        return current;
    };

    anchors.reserve(points.size() / 4U + 2U);
    for (std::size_t i = 1; i + 1 < points.size(); ++i) {
        const auto prev_index = find_context_index(i, -1);
        const auto next_index = find_context_index(i, 1);
        if (prev_index == i || next_index == i) {
            continue;
        }

        const auto& prev = points[prev_index];
        const auto& cur = points[i];
        const auto& next = points[next_index];
        const double leg1 = Distance(prev, cur);
        const double leg2 = Distance(cur, next);
        if (leg1 < kPreviewCornerMinLegMm || leg2 < kPreviewCornerMinLegMm) {
            continue;
        }

        const double vx1 = static_cast<double>(cur.x) - static_cast<double>(prev.x);
        const double vy1 = static_cast<double>(cur.y) - static_cast<double>(prev.y);
        const double vx2 = static_cast<double>(next.x) - static_cast<double>(cur.x);
        const double vy2 = static_cast<double>(next.y) - static_cast<double>(cur.y);
        const double dot = vx1 * vx2 + vy1 * vy2;
        const double cos_theta = std::clamp(dot / (leg1 * leg2), -1.0, 1.0);
        const double turn_deg = std::acos(cos_theta) * 180.0 / 3.14159265358979323846;
        if (turn_deg >= kPreviewCornerMinTurnDeg) {
            anchors.push_back(i);
        }
    }

    return anchors;
}

std::vector<std::size_t> PickUniformIndices(
    const std::vector<std::size_t>& candidates,
    std::size_t target_count) {
    if (target_count == 0U || candidates.empty()) {
        return {};
    }
    if (candidates.size() <= target_count) {
        return candidates;
    }
    if (target_count == 1U) {
        return {candidates.front()};
    }

    std::vector<std::size_t> picked;
    picked.reserve(target_count);
    const std::size_t last_pos = candidates.size() - 1U;
    for (std::size_t i = 0; i < target_count; ++i) {
        const double ratio = static_cast<double>(i) / static_cast<double>(target_count - 1U);
        const std::size_t pos = static_cast<std::size_t>(std::llround(ratio * static_cast<double>(last_pos)));
        const std::size_t value = candidates[pos];
        if (picked.empty() || picked.back() != value) {
            picked.push_back(value);
        }
    }
    if (picked.size() < target_count) {
        for (const auto value : candidates) {
            if (picked.size() >= target_count) {
                break;
            }
            if (std::find(picked.begin(), picked.end(), value) == picked.end()) {
                picked.push_back(value);
            }
        }
        std::sort(picked.begin(), picked.end());
    }
    if (picked.size() > target_count) {
        picked.resize(target_count);
    }
    return picked;
}

std::vector<Siligen::Shared::Types::Point2D> ClampPolylineByMaxPointsPreserveCorners(
    const std::vector<Siligen::Shared::Types::Point2D>& points,
    std::size_t max_points) {
    if (points.empty() || max_points == 0U) {
        return {};
    }

    const std::size_t safe_max_points = std::max<std::size_t>(2U, max_points);
    if (points.size() <= safe_max_points) {
        return points;
    }

    const auto corner_anchors = DetectCornerAnchorIndices(points);
    std::vector<std::size_t> mandatory_indices;
    mandatory_indices.reserve(corner_anchors.size() + 2U);
    mandatory_indices.push_back(0U);
    mandatory_indices.insert(mandatory_indices.end(), corner_anchors.begin(), corner_anchors.end());
    mandatory_indices.push_back(points.size() - 1U);
    std::sort(mandatory_indices.begin(), mandatory_indices.end());
    mandatory_indices.erase(std::unique(mandatory_indices.begin(), mandatory_indices.end()), mandatory_indices.end());

    std::vector<std::size_t> sample_indices;
    if (mandatory_indices.size() >= safe_max_points) {
        sample_indices = PickUniformIndices(mandatory_indices, safe_max_points);
    } else {
        sample_indices = mandatory_indices;
        std::vector<bool> is_mandatory(points.size(), false);
        for (const auto idx : mandatory_indices) {
            if (idx < is_mandatory.size()) {
                is_mandatory[idx] = true;
            }
        }
        std::vector<std::size_t> supplemental_indices;
        supplemental_indices.reserve(points.size() - mandatory_indices.size());
        for (std::size_t idx = 0U; idx < points.size(); ++idx) {
            if (!is_mandatory[idx]) {
                supplemental_indices.push_back(idx);
            }
        }
        const std::size_t remaining = safe_max_points - sample_indices.size();
        const auto picked_supplemental = PickUniformIndices(supplemental_indices, remaining);
        sample_indices.insert(sample_indices.end(), picked_supplemental.begin(), picked_supplemental.end());
        std::sort(sample_indices.begin(), sample_indices.end());
        sample_indices.erase(std::unique(sample_indices.begin(), sample_indices.end()), sample_indices.end());
    }

    std::vector<Siligen::Shared::Types::Point2D> polyline;
    polyline.reserve(sample_indices.size());
    for (const auto idx : sample_indices) {
        if (idx < points.size()) {
            polyline.push_back(points[idx]);
        }
    }
    return polyline;
}

std::vector<Siligen::Shared::Types::Point2D> BuildPointVectorFromTrajectory(
    const std::vector<Siligen::TrajectoryPoint>& trajectory_points) {
    std::vector<Siligen::Shared::Types::Point2D> points;
    points.reserve(trajectory_points.size());
    for (const auto& point : trajectory_points) {
        AppendDistinctPoint(points, point.position);
    }
    return points;
}

std::vector<Siligen::Shared::Types::Point2D> BuildPointVectorFromProcessPath(
    const Siligen::ProcessPath::Contracts::ProcessPath& process_path) {
    std::vector<Siligen::Shared::Types::Point2D> points;
    Siligen::Domain::Dispensing::DomainServices::CurveFlatteningService flattening_service;
    constexpr float32 kProcessPathSplineErrorMm = 0.05f;
    constexpr float32 kProcessPathSampleStepMm = 1.0f;

    for (const auto& process_segment : process_path.segments) {
        const auto& geometry = process_segment.geometry;
        if (geometry.is_point) {
            AppendDistinctPoint(points, geometry.line.start);
            continue;
        }

        const auto flatten_result =
            flattening_service.Flatten(geometry, kProcessPathSplineErrorMm, kProcessPathSampleStepMm);
        if (flatten_result.IsError()) {
            continue;
        }
        for (const auto& point : flatten_result.Value().points) {
            AppendDistinctPoint(points, point);
        }
    }

    return points;
}

void CopyPreviewPolyline(
    const std::vector<Siligen::Application::Services::Dispensing::PreviewSnapshotPoint>& points,
    std::vector<Siligen::Application::UseCases::Dispensing::PreviewSnapshotPoint>& target) {
    target.reserve(points.size());
    for (const auto& point : points) {
        Siligen::Application::UseCases::Dispensing::PreviewSnapshotPoint snapshot_point;
        snapshot_point.x = point.x;
        snapshot_point.y = point.y;
        target.push_back(snapshot_point);
    }
}

void CopyPreviewPolyline(
    const std::vector<Siligen::Shared::Types::Point2D>& points,
    std::vector<Siligen::Application::UseCases::Dispensing::PreviewSnapshotPoint>& target) {
    target.reserve(points.size());
    for (const auto& point : points) {
        Siligen::Application::UseCases::Dispensing::PreviewSnapshotPoint snapshot_point;
        snapshot_point.x = point.x;
        snapshot_point.y = point.y;
        target.push_back(snapshot_point);
    }
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

    if (input.process_path != nullptr && !input.process_path->segments.empty()) {
        const auto process_path_points = BuildPointVectorFromProcessPath(*input.process_path);
        const auto process_path_polyline =
            ClampPolylineByMaxPointsPreserveCorners(process_path_points, max_polyline_points);
        response.motion_preview_source = "process_path_snapshot";
        response.motion_preview_kind = "polyline";
        response.motion_preview_source_point_count = static_cast<std::uint32_t>(process_path_points.size());
        response.motion_preview_point_count = static_cast<std::uint32_t>(process_path_polyline.size());
        response.motion_preview_is_sampled =
            response.motion_preview_source_point_count != response.motion_preview_point_count;
        response.motion_preview_sampling_strategy = response.motion_preview_is_sampled
            ? "process_path_geometry_preserving_clamp"
            : "process_path_geometry_preserving";
        CopyPreviewPolyline(process_path_polyline, response.motion_preview_polyline);
    } else if (input.motion_trajectory_points != nullptr && !input.motion_trajectory_points->empty()) {
        const auto motion_points = BuildPointVectorFromTrajectory(*input.motion_trajectory_points);
        const auto motion_polyline =
            ClampPolylineByMaxPointsPreserveCorners(motion_points, max_polyline_points);
        response.motion_preview_source = "execution_trajectory_snapshot";
        response.motion_preview_kind = "polyline";
        response.motion_preview_source_point_count = static_cast<std::uint32_t>(motion_points.size());
        response.motion_preview_point_count = static_cast<std::uint32_t>(motion_polyline.size());
        response.motion_preview_is_sampled =
            response.motion_preview_source_point_count != response.motion_preview_point_count;
        response.motion_preview_sampling_strategy = response.motion_preview_is_sampled
            ? "execution_trajectory_geometry_preserving_clamp"
            : "execution_trajectory_geometry_preserving";
        CopyPreviewPolyline(motion_polyline, response.motion_preview_polyline);
    }
    return response;
}

}  // namespace Siligen::Application::Services::Dispensing
