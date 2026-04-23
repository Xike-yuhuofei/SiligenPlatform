#include "application/services/dispensing/PreviewSnapshotService.h"

#include "domain/dispensing/value-objects/AuthorityTriggerLayout.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Application::Services::Dispensing {

using Siligen::Shared::Types::Point2D;

namespace {

constexpr double kPreviewGluePointSpacingMm = 3.0;
constexpr double kPreviewDuplicateEpsilonMm = 1e-4;
constexpr double kPreviewTailPositionEpsilonMm = 2e-2;
constexpr double kPreviewTailLegMaxMm = 4e-1;
constexpr double kPreviewCornerMinTurnDeg = 20.0;
constexpr double kPreviewCornerMinLegMm = 2e-1;

bool HasAuthoritativeGluePoints(const PreviewSnapshotInput& input) {
    return !input.authority_layout_id.empty() &&
           input.binding_ready &&
           input.validation_classification != "fail" &&
           input.glue_points != nullptr &&
           !input.glue_points->empty();
}

double DistanceSquared(const Point2D& lhs, const Point2D& rhs) {
    const double dx = static_cast<double>(lhs.x) - static_cast<double>(rhs.x);
    const double dy = static_cast<double>(lhs.y) - static_cast<double>(rhs.y);
    return dx * dx + dy * dy;
}

double Distance(const Point2D& lhs, const Point2D& rhs) {
    return std::sqrt(DistanceSquared(lhs, rhs));
}

bool NearlyEqualPoint(const Point2D& lhs, const Point2D& rhs, double epsilon_mm) {
    const double epsilon_sq = epsilon_mm * epsilon_mm;
    return DistanceSquared(lhs, rhs) <= epsilon_sq;
}

void AppendDistinctPoint(
    std::vector<Point2D>& points,
    const Point2D& candidate) {
    if (points.empty() || !NearlyEqualPoint(points.back(), candidate, kPreviewDuplicateEpsilonMm)) {
        points.push_back(candidate);
    }
}

std::vector<Point2D> ToPointVector(const std::vector<Siligen::TrajectoryPoint>& points) {
    std::vector<Point2D> converted;
    converted.reserve(points.size());
    for (const auto& trajectory_point : points) {
        Point2D point;
        point.x = trajectory_point.position.x;
        point.y = trajectory_point.position.y;
        converted.push_back(point);
    }
    return converted;
}

std::vector<Point2D> RemoveConsecutiveNearDuplicates(
    const std::vector<Point2D>& points,
    double epsilon_mm = kPreviewDuplicateEpsilonMm) {
    std::vector<Point2D> deduped;
    if (points.empty()) {
        return deduped;
    }
    deduped.reserve(points.size());
    deduped.push_back(points.front());
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (!NearlyEqualPoint(deduped.back(), points[i], epsilon_mm)) {
            deduped.push_back(points[i]);
        }
    }
    return deduped;
}

std::vector<Point2D> RemoveShortABABacktracks(const std::vector<Point2D>& points) {
    std::vector<Point2D> reduced;
    reduced.reserve(points.size());
    for (const auto& point : points) {
        reduced.push_back(point);
        while (reduced.size() >= 3U) {
            const auto& a = reduced[reduced.size() - 3U];
            const auto& b = reduced[reduced.size() - 2U];
            const auto& c = reduced[reduced.size() - 1U];
            const bool is_backtrack =
                NearlyEqualPoint(a, c, kPreviewTailPositionEpsilonMm) &&
                Distance(a, b) <= kPreviewTailLegMaxMm &&
                Distance(b, c) <= kPreviewTailLegMaxMm;
            if (!is_backtrack) {
                break;
            }
            reduced.pop_back();
            reduced.pop_back();
        }
    }
    return reduced;
}

std::vector<std::size_t> DetectCornerAnchorIndices(const std::vector<Point2D>& points) {
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

std::vector<std::size_t> PickUniformIndices(const std::vector<std::size_t>& candidates, std::size_t target_count) {
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

std::vector<Point2D> BuildUniformStepSample(const std::vector<Point2D>& points, std::size_t max_points) {
    std::vector<Point2D> polyline;
    if (points.empty() || max_points == 0U) {
        return polyline;
    }

    const std::size_t safe_max_points = std::max<std::size_t>(2U, max_points);
    const std::size_t source_point_count = points.size();
    std::size_t step = 1U;
    if (source_point_count > safe_max_points) {
        const std::size_t numerator = source_point_count - 1U;
        const std::size_t denominator = safe_max_points - 1U;
        step = (numerator + denominator - 1U) / denominator;
        step = std::max<std::size_t>(1U, step);
    }

    polyline.reserve(std::min(source_point_count, safe_max_points));
    for (std::size_t i = 0U; i < source_point_count; i += step) {
        polyline.push_back(points[i]);
    }

    const auto& last_source = points.back();
    if (polyline.empty() || !NearlyEqualPoint(polyline.back(), last_source, kPreviewDuplicateEpsilonMm)) {
        polyline.push_back(last_source);
    }
    return polyline;
}

Point2D InterpolatePoint(const Point2D& start, const Point2D& end, double ratio) {
    Point2D point;
    point.x = static_cast<float32>(
        static_cast<double>(start.x) + (static_cast<double>(end.x) - static_cast<double>(start.x)) * ratio);
    point.y = static_cast<float32>(
        static_cast<double>(start.y) + (static_cast<double>(end.y) - static_cast<double>(start.y)) * ratio);
    return point;
}

std::vector<Point2D> BuildFixedSpacingSample(
    const std::vector<Point2D>& points,
    double spacing_mm) {
    std::vector<Point2D> sampled;
    if (points.empty()) {
        return sampled;
    }

    sampled.reserve(points.size());
    sampled.push_back(points.front());
    if (points.size() == 1U) {
        return sampled;
    }

    const double safe_spacing = std::max(spacing_mm, kPreviewDuplicateEpsilonMm);
    double distance_since_last_emit = 0.0;

    for (std::size_t seg_idx = 1U; seg_idx < points.size(); ++seg_idx) {
        const auto& seg_start = points[seg_idx - 1U];
        const auto& seg_end = points[seg_idx];
        const double segment_length = Distance(seg_start, seg_end);
        if (segment_length <= kPreviewDuplicateEpsilonMm) {
            continue;
        }

        double traveled = 0.0;
        while (traveled + kPreviewDuplicateEpsilonMm < segment_length) {
            const double remaining_to_emit = std::max(safe_spacing - distance_since_last_emit, kPreviewDuplicateEpsilonMm);
            const double remaining_in_segment = segment_length - traveled;
            if (remaining_in_segment + kPreviewDuplicateEpsilonMm < remaining_to_emit) {
                distance_since_last_emit += remaining_in_segment;
                traveled = segment_length;
                break;
            }

            traveled += remaining_to_emit;
            const double ratio = std::clamp(traveled / segment_length, 0.0, 1.0);
            const auto interpolated = InterpolatePoint(seg_start, seg_end, ratio);
            if (!NearlyEqualPoint(sampled.back(), interpolated, kPreviewDuplicateEpsilonMm)) {
                sampled.push_back(interpolated);
            }
            distance_since_last_emit = 0.0;
        }
    }

    if (!NearlyEqualPoint(sampled.back(), points.back(), kPreviewDuplicateEpsilonMm)) {
        sampled.push_back(points.back());
    }
    return sampled;
}

std::vector<Point2D> ClampPolylineByMaxPointsPreserveCorners(
    const std::vector<Point2D>& points,
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
    for (const auto idx : corner_anchors) {
        if (idx + 1U < points.size()) {
            mandatory_indices.push_back(idx);
        }
    }
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

    std::vector<Point2D> polyline;
    polyline.reserve(sample_indices.size());
    for (const auto idx : sample_indices) {
        if (idx < points.size()) {
            polyline.push_back(points[idx]);
        }
    }
    if (polyline.size() < 2U) {
        return BuildUniformStepSample(points, safe_max_points);
    }
    return polyline;
}

std::vector<Point2D> BuildPreviewPolyline(
    const std::vector<Siligen::TrajectoryPoint>& trajectory_points,
    std::size_t max_points) {
    const auto raw_points = ToPointVector(trajectory_points);
    auto fallback_polyline = BuildUniformStepSample(raw_points, max_points);
    if (raw_points.empty()) {
        return fallback_polyline;
    }

    auto deduped_points = RemoveConsecutiveNearDuplicates(raw_points);
    if (deduped_points.size() < 2U) {
        return fallback_polyline;
    }

    auto reduced_points = RemoveShortABABacktracks(deduped_points);
    if (reduced_points.size() < 2U) {
        reduced_points = deduped_points;
    } else if (deduped_points.size() >= 20U && reduced_points.size() * 5U < deduped_points.size()) {
        reduced_points = deduped_points;
    }
    reduced_points = RemoveConsecutiveNearDuplicates(reduced_points);

    auto spacing_sample = BuildFixedSpacingSample(reduced_points, kPreviewGluePointSpacingMm);
    if (spacing_sample.size() < 2U) {
        spacing_sample = BuildUniformStepSample(reduced_points, max_points);
    }
    if (spacing_sample.size() < 2U) {
        return fallback_polyline;
    }

    auto polyline = ClampPolylineByMaxPointsPreserveCorners(spacing_sample, max_points);
    if (polyline.size() < 2U) {
        return fallback_polyline;
    }
    return polyline;
}

std::vector<std::size_t> SampleGluePointIndices(std::size_t point_count, std::size_t max_points) {
    if (point_count == 0U) {
        return {};
    }
    const std::size_t safe_max_points = std::max<std::size_t>(1, max_points);
    if (point_count <= safe_max_points) {
        std::vector<std::size_t> indices(point_count);
        for (std::size_t index = 0; index < point_count; ++index) {
            indices[index] = index;
        }
        return indices;
    }

    std::vector<std::size_t> indices;
    indices.reserve(safe_max_points);
    indices.push_back(0U);
    if (safe_max_points == 1U) {
        return indices;
    }

    const std::size_t interior_budget = safe_max_points - 2U;
    if (interior_budget > 0U) {
        const double step = static_cast<double>(point_count - 1U) / static_cast<double>(interior_budget + 1U);
        for (std::size_t index = 1; index <= interior_budget; ++index) {
            const auto sampled_index = static_cast<std::size_t>(std::llround(step * static_cast<double>(index)));
            indices.push_back(std::min<std::size_t>(point_count - 1U, sampled_index));
        }
    }
    indices.push_back(point_count - 1U);
    return indices;
}

std::vector<float32> BuildFallbackRevealLengthsFromGluePoints(
    const std::vector<Point2D>& glue_points) {
    std::vector<float32> reveal_lengths;
    if (glue_points.empty()) {
        return reveal_lengths;
    }
    reveal_lengths.reserve(glue_points.size());
    float32 cumulative_length_mm = 0.0f;
    reveal_lengths.push_back(cumulative_length_mm);
    for (std::size_t index = 1; index < glue_points.size(); ++index) {
        cumulative_length_mm += static_cast<float32>(glue_points[index - 1U].DistanceTo(glue_points[index]));
        reveal_lengths.push_back(cumulative_length_mm);
    }
    return reveal_lengths;
}

std::vector<float32> BuildAuthorityRevealLengths(
    const PreviewSnapshotInput& input,
    const std::vector<Point2D>& glue_points) {
    if (input.authority_trigger_layout != nullptr &&
        input.authority_trigger_layout->trigger_points.size() == glue_points.size()) {
        std::vector<float32> reveal_lengths;
        reveal_lengths.reserve(input.authority_trigger_layout->trigger_points.size());
        float32 previous_length_mm = 0.0f;
        for (const auto& trigger : input.authority_trigger_layout->trigger_points) {
            const float32 clamped_length_mm = std::max(previous_length_mm, trigger.distance_mm_global);
            reveal_lengths.push_back(clamped_length_mm);
            previous_length_mm = clamped_length_mm;
        }
        return reveal_lengths;
    }
    return BuildFallbackRevealLengthsFromGluePoints(glue_points);
}

std::vector<float32> BuildPolylineCumulativeLengths(const std::vector<Point2D>& polyline) {
    std::vector<float32> cumulative_lengths;
    if (polyline.empty()) {
        return cumulative_lengths;
    }

    cumulative_lengths.reserve(polyline.size());
    cumulative_lengths.push_back(0.0f);
    float32 total_length_mm = 0.0f;
    for (std::size_t index = 1; index < polyline.size(); ++index) {
        total_length_mm += static_cast<float32>(polyline[index - 1U].DistanceTo(polyline[index]));
        cumulative_lengths.push_back(total_length_mm);
    }
    return cumulative_lengths;
}

bool ProjectPointOntoPolylineInOrder(
    const Point2D& point,
    const std::vector<Point2D>& polyline,
    const std::vector<float32>& cumulative_lengths,
    std::size_t start_segment_index,
    float32 minimum_length_mm,
    float32* projected_length_mm,
    std::size_t* matched_segment_index) {
    if (projected_length_mm == nullptr || matched_segment_index == nullptr) {
        return false;
    }
    if (polyline.size() < 2U || cumulative_lengths.size() != polyline.size()) {
        return false;
    }

    bool found = false;
    double best_distance_sq = 0.0;
    float32 best_length_mm = minimum_length_mm;
    std::size_t best_segment_index = start_segment_index;
    for (std::size_t segment_index = start_segment_index; segment_index + 1U < polyline.size(); ++segment_index) {
        const auto& start = polyline[segment_index];
        const auto& end = polyline[segment_index + 1U];
        const double dx = static_cast<double>(end.x) - static_cast<double>(start.x);
        const double dy = static_cast<double>(end.y) - static_cast<double>(start.y);
        const double segment_length_sq = (dx * dx) + (dy * dy);
        if (segment_length_sq <= 1e-9) {
            continue;
        }

        const double point_dx = static_cast<double>(point.x) - static_cast<double>(start.x);
        const double point_dy = static_cast<double>(point.y) - static_cast<double>(start.y);
        const double ratio = std::clamp(((point_dx * dx) + (point_dy * dy)) / segment_length_sq, 0.0, 1.0);
        const Point2D projected = InterpolatePoint(start, end, ratio);
        const double distance_sq = DistanceSquared(point, projected);
        const float32 segment_length_mm =
            static_cast<float32>(std::sqrt(segment_length_sq));
        const float32 segment_start_length_mm = cumulative_lengths[segment_index];
        const float32 candidate_length_mm = std::max(
            minimum_length_mm,
            segment_start_length_mm + static_cast<float32>(segment_length_mm * ratio));

        if (!found || distance_sq < best_distance_sq) {
            found = true;
            best_distance_sq = distance_sq;
            best_length_mm = candidate_length_mm;
            best_segment_index = segment_index;
        }
    }

    if (!found) {
        return false;
    }

    *projected_length_mm = best_length_mm;
    *matched_segment_index = best_segment_index;
    return true;
}

PreviewBindingSnapshot BuildPreviewBindingSnapshot(
    const PreviewSnapshotInput& input,
    const std::vector<std::size_t>& sampled_glue_indices,
    const std::vector<Point2D>& motion_polyline) {
    PreviewBindingSnapshot binding_snapshot;
    binding_snapshot.source = "runtime_authority_preview_binding";
    binding_snapshot.status = "unavailable";
    binding_snapshot.layout_id = input.authority_layout_id;
    binding_snapshot.glue_point_count = static_cast<std::uint32_t>(sampled_glue_indices.size());
    binding_snapshot.binding_basis = "execution_binding_to_motion_preview_polyline";
    binding_snapshot.diagnostic_code = input.diagnostic_code;

    if (input.authority_layout_id.empty()) {
        binding_snapshot.failure_reason = "preview authority layout id unavailable";
        return binding_snapshot;
    }
    if (!input.binding_ready || input.authority_trigger_layout == nullptr) {
        binding_snapshot.failure_reason = "preview binding unavailable";
        return binding_snapshot;
    }
    if (motion_polyline.size() < 2U) {
        binding_snapshot.failure_reason = "preview motion preview unavailable";
        return binding_snapshot;
    }

    const auto cumulative_lengths = BuildPolylineCumulativeLengths(motion_polyline);
    if (cumulative_lengths.empty()) {
        binding_snapshot.failure_reason = "preview motion preview unavailable";
        return binding_snapshot;
    }
    binding_snapshot.display_path_length_mm = cumulative_lengths.back();

    const auto& layout = *input.authority_trigger_layout;
    if (!layout.binding_ready || layout.bindings.size() != layout.trigger_points.size()) {
        binding_snapshot.failure_reason = "preview binding unavailable";
        return binding_snapshot;
    }

    binding_snapshot.source_trigger_indices.reserve(sampled_glue_indices.size());
    binding_snapshot.display_reveal_lengths_mm.reserve(sampled_glue_indices.size());
    std::size_t search_segment_index = 0U;
    float32 previous_length_mm = 0.0f;
    for (const auto sampled_index : sampled_glue_indices) {
        binding_snapshot.source_trigger_indices.push_back(static_cast<std::uint32_t>(sampled_index));
        if (sampled_index >= layout.bindings.size() || !layout.bindings[sampled_index].bound) {
            binding_snapshot.failure_reason = "preview binding unavailable";
            binding_snapshot.display_reveal_lengths_mm.clear();
            return binding_snapshot;
        }

        float32 projected_length_mm = previous_length_mm;
        std::size_t matched_segment_index = search_segment_index;
        if (!ProjectPointOntoPolylineInOrder(
                layout.bindings[sampled_index].execution_position,
                motion_polyline,
                cumulative_lengths,
                search_segment_index,
                previous_length_mm,
                &projected_length_mm,
                &matched_segment_index)) {
            binding_snapshot.failure_reason = "preview binding projection unavailable";
            binding_snapshot.display_reveal_lengths_mm.clear();
            return binding_snapshot;
        }

        binding_snapshot.display_reveal_lengths_mm.push_back(projected_length_mm);
        previous_length_mm = projected_length_mm;
        search_segment_index = matched_segment_index;
    }

    binding_snapshot.status = "ready";
    binding_snapshot.failure_reason.clear();
    return binding_snapshot;
}

std::vector<Point2D> BuildPointVectorFromTrajectory(
    const std::vector<Siligen::TrajectoryPoint>& trajectory_points) {
    std::vector<Point2D> points;
    points.reserve(trajectory_points.size());
    for (const auto& point : trajectory_points) {
        AppendDistinctPoint(points, point.position);
    }
    return points;
}

void CopyPreviewPolyline(
    const std::vector<Point2D>& points,
    std::vector<PreviewSnapshotPoint>& target) {
    target.reserve(points.size());
    for (const auto& point : points) {
        PreviewSnapshotPoint snapshot_point;
        snapshot_point.x = point.x;
        snapshot_point.y = point.y;
        target.push_back(snapshot_point);
    }
}

}  // namespace

PreviewSnapshotResponse PreviewSnapshotService::BuildResponse(
    const PreviewSnapshotInput& input,
    std::size_t max_polyline_points,
    std::size_t max_glue_points) const {
    PreviewSnapshotResponse response;
    response.snapshot_id = input.snapshot_id;
    response.snapshot_hash = input.snapshot_hash;
    response.plan_id = input.plan_id;
    response.preview_state = input.preview_state;
    response.preview_source = "planned_glue_snapshot";
    response.preview_kind = "glue_points";
    response.confirmed_at = input.confirmed_at;
    response.segment_count = input.segment_count;
    response.execution_point_count = input.point_count;
    response.total_length_mm = input.total_length_mm;
    response.estimated_time_s = input.estimated_time_s;
    response.preview_validation_classification = input.validation_classification;
    response.preview_exception_reason = input.exception_reason;
    response.preview_failure_reason = input.failure_reason;
    response.preview_diagnostic_code = input.diagnostic_code;
    response.generated_at = input.generated_at;
    response.preview_binding.source = "runtime_authority_preview_binding";
    response.preview_binding.status = "unavailable";
    response.preview_binding.layout_id = input.authority_layout_id;
    response.preview_binding.binding_basis = "execution_binding_to_motion_preview_polyline";
    response.preview_binding.diagnostic_code = input.diagnostic_code;

    std::vector<std::size_t> sampled_glue_indices;
    if (HasAuthoritativeGluePoints(input)) {
        response.glue_point_count = static_cast<std::uint32_t>(input.glue_points->size());
        response.point_count = response.glue_point_count;
        const auto authority_reveal_lengths = BuildAuthorityRevealLengths(input, *input.glue_points);
        sampled_glue_indices = SampleGluePointIndices(input.glue_points->size(), max_glue_points);
        response.glue_points.reserve(sampled_glue_indices.size());
        response.glue_reveal_lengths_mm.reserve(sampled_glue_indices.size());
        for (const auto sampled_index : sampled_glue_indices) {
            const auto& point = input.glue_points->at(sampled_index);
            PreviewSnapshotPoint snapshot_point;
            snapshot_point.x = point.x;
            snapshot_point.y = point.y;
            response.glue_points.push_back(snapshot_point);
            if (sampled_index < authority_reveal_lengths.size()) {
                response.glue_reveal_lengths_mm.push_back(authority_reveal_lengths[sampled_index]);
            }
        }
    }

    std::vector<Point2D> motion_points;
    std::vector<Point2D> motion_polyline;
    if (input.motion_trajectory_points != nullptr && !input.motion_trajectory_points->empty()) {
        motion_points = BuildPointVectorFromTrajectory(*input.motion_trajectory_points);
        response.motion_preview_source = "execution_trajectory_snapshot";
        response.motion_preview_kind = "polyline";
        motion_polyline = ClampPolylineByMaxPointsPreserveCorners(motion_points, max_polyline_points);
        response.motion_preview_source_point_count = static_cast<std::uint32_t>(motion_points.size());
        response.motion_preview_point_count = static_cast<std::uint32_t>(motion_polyline.size());
        response.motion_preview_is_sampled =
            response.motion_preview_source_point_count != response.motion_preview_point_count;
        response.motion_preview_sampling_strategy = response.motion_preview_is_sampled
            ? "execution_trajectory_geometry_preserving_clamp"
            : "execution_trajectory_geometry_preserving";
        CopyPreviewPolyline(motion_polyline, response.motion_preview_polyline);
    }
    response.preview_binding = BuildPreviewBindingSnapshot(input, sampled_glue_indices, motion_polyline);

    return response;
}

}  // namespace Siligen::Application::Services::Dispensing
