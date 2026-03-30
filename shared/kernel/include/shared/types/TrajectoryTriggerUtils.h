#pragma once

#include "shared/types/Point.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>
#include <vector>

namespace Siligen::Shared::Types {

inline constexpr float32 kTriggerMarkerMatchToleranceMm = 1e-3f;
inline constexpr float32 kTriggerMarkerDedupToleranceMm = 1e-4f;
inline constexpr uint16 kDefaultTriggerPulseWidthUs = 2000;

inline void ClearTriggerMarker(TrajectoryPoint& point) {
    point.enable_position_trigger = false;
    point.trigger_position_mm = 0.0f;
    point.trigger_pulse_width_us = 0;
}

inline std::vector<TrajectoryPoint> ClearTriggerMarkers(const std::vector<TrajectoryPoint>& points) {
    std::vector<TrajectoryPoint> sanitized = points;
    for (auto& point : sanitized) {
        ClearTriggerMarker(point);
    }
    return sanitized;
}

inline void ReassignTrajectorySequenceIds(std::vector<TrajectoryPoint>& points) {
    for (size_t index = 0; index < points.size(); ++index) {
        points[index].sequence_id = static_cast<uint32>(index);
    }
}

inline std::vector<float32> BuildTrajectoryCumulativeDistances(const std::vector<TrajectoryPoint>& points) {
    std::vector<float32> cumulative(points.size(), 0.0f);
    for (size_t index = 1; index < points.size(); ++index) {
        cumulative[index] = cumulative[index - 1] + points[index - 1].position.DistanceTo(points[index].position);
    }
    return cumulative;
}

inline void MarkTriggerPoint(TrajectoryPoint& point, float32 trigger_distance_mm, uint16 pulse_width_us) {
    point.enable_position_trigger = true;
    point.trigger_position_mm = trigger_distance_mm;
    point.trigger_pulse_width_us = std::max<uint16>(point.trigger_pulse_width_us, pulse_width_us);
}

inline TrajectoryPoint InterpolateTrajectoryPoint(const TrajectoryPoint& start,
                                                  const TrajectoryPoint& end,
                                                  float32 ratio) {
    const float32 clamped_ratio = std::clamp(ratio, 0.0f, 1.0f);

    TrajectoryPoint point = start;
    point.position = start.position + (end.position - start.position) * clamped_ratio;
    point.velocity = start.velocity + (end.velocity - start.velocity) * clamped_ratio;
    point.acceleration = start.acceleration + (end.acceleration - start.acceleration) * clamped_ratio;
    point.dispensing_time = start.dispensing_time + (end.dispensing_time - start.dispensing_time) * clamped_ratio;
    point.timestamp = start.timestamp + (end.timestamp - start.timestamp) * clamped_ratio;
    point.segment_type = (clamped_ratio >= 0.5f) ? end.segment_type : start.segment_type;
    point.arc_center = (clamped_ratio >= 0.5f) ? end.arc_center : start.arc_center;
    point.arc_radius = (clamped_ratio >= 0.5f) ? end.arc_radius : start.arc_radius;
    ClearTriggerMarker(point);
    return point;
}

enum class TriggerMarkerCandidateKind {
    ExistingPoint,
    SegmentProjection,
};

struct TriggerMarkerCandidate {
    bool found = false;
    TriggerMarkerCandidateKind kind = TriggerMarkerCandidateKind::ExistingPoint;
    size_t index = 0;
    float32 ratio = 0.0f;
    float32 position_error_mm = std::numeric_limits<float32>::max();
    float32 distance_error_mm = std::numeric_limits<float32>::max();
};

inline bool IsBetterTriggerMarkerCandidate(
    const TriggerMarkerCandidate& candidate,
    const TriggerMarkerCandidate& current,
    float32 compare_tolerance_mm = kTriggerMarkerMatchToleranceMm) {
    if (!candidate.found) {
        return false;
    }
    if (!current.found) {
        return true;
    }
    if (candidate.distance_error_mm + compare_tolerance_mm < current.distance_error_mm) {
        return true;
    }
    if (std::fabs(candidate.distance_error_mm - current.distance_error_mm) > compare_tolerance_mm) {
        return false;
    }
    if (candidate.position_error_mm + compare_tolerance_mm < current.position_error_mm) {
        return true;
    }
    if (std::fabs(candidate.position_error_mm - current.position_error_mm) > compare_tolerance_mm) {
        return false;
    }
    return candidate.kind == TriggerMarkerCandidateKind::ExistingPoint &&
           current.kind == TriggerMarkerCandidateKind::SegmentProjection;
}

inline bool InsertTriggerMarkerByDistance(std::vector<TrajectoryPoint>& points,
                                          float32 target_distance_mm,
                                          float32 match_tolerance_mm = kTriggerMarkerMatchToleranceMm,
                                          uint16 pulse_width_us = kDefaultTriggerPulseWidthUs) {
    if (points.size() < 2) {
        return false;
    }

    const auto cumulative = BuildTrajectoryCumulativeDistances(points);
    const float32 total_length = cumulative.back();
    if (target_distance_mm < -match_tolerance_mm || target_distance_mm > total_length + match_tolerance_mm) {
        return false;
    }

    const float32 clamped_target = std::clamp(target_distance_mm, 0.0f, total_length);

    size_t segment_index = 1;
    while (segment_index < cumulative.size() && cumulative[segment_index] + match_tolerance_mm < clamped_target) {
        ++segment_index;
    }
    if (segment_index >= cumulative.size()) {
        return false;
    }

    if (std::fabs(cumulative[segment_index - 1] - clamped_target) <= match_tolerance_mm) {
        MarkTriggerPoint(points[segment_index - 1], clamped_target, pulse_width_us);
        ReassignTrajectorySequenceIds(points);
        return true;
    }
    if (std::fabs(cumulative[segment_index] - clamped_target) <= match_tolerance_mm) {
        MarkTriggerPoint(points[segment_index], clamped_target, pulse_width_us);
        ReassignTrajectorySequenceIds(points);
        return true;
    }

    const float32 segment_length = cumulative[segment_index] - cumulative[segment_index - 1];
    if (segment_length <= match_tolerance_mm) {
        MarkTriggerPoint(points[segment_index], clamped_target, pulse_width_us);
        ReassignTrajectorySequenceIds(points);
        return true;
    }

    const float32 ratio = (clamped_target - cumulative[segment_index - 1]) / segment_length;
    TrajectoryPoint inserted = InterpolateTrajectoryPoint(points[segment_index - 1], points[segment_index], ratio);
    MarkTriggerPoint(inserted, clamped_target, pulse_width_us);
    points.insert(points.begin() + static_cast<std::ptrdiff_t>(segment_index), inserted);
    ReassignTrajectorySequenceIds(points);
    return true;
}

inline bool ApplyTriggerMarkersByDistance(std::vector<TrajectoryPoint>& points,
                                          const std::vector<float32>& trigger_distances_mm,
                                          float32 dedup_tolerance_mm = kTriggerMarkerDedupToleranceMm,
                                          float32 match_tolerance_mm = kTriggerMarkerMatchToleranceMm,
                                          uint16 pulse_width_us = kDefaultTriggerPulseWidthUs) {
    points = ClearTriggerMarkers(points);
    ReassignTrajectorySequenceIds(points);

    if (trigger_distances_mm.empty()) {
        return true;
    }

    std::vector<float32> targets = trigger_distances_mm;
    std::sort(targets.begin(), targets.end());

    bool has_last_target = false;
    float32 last_target = 0.0f;
    for (float32 target : targets) {
        if (has_last_target && std::fabs(target - last_target) <= dedup_tolerance_mm) {
            continue;
        }
        if (!InsertTriggerMarkerByDistance(points, target, match_tolerance_mm, pulse_width_us)) {
            return false;
        }
        has_last_target = true;
        last_target = target;
    }

    return true;
}

inline bool InsertTriggerMarkerByPosition(std::vector<TrajectoryPoint>& points,
                                          const Point2D& target_position,
                                          float32 trigger_distance_mm,
                                          float32 position_tolerance_mm = kTriggerMarkerMatchToleranceMm,
                                          uint16 pulse_width_us = kDefaultTriggerPulseWidthUs) {
    if (points.empty()) {
        return false;
    }

    const auto cumulative = BuildTrajectoryCumulativeDistances(points);
    const bool use_distance_hint = std::isfinite(trigger_distance_mm);
    TriggerMarkerCandidate best_candidate;

    for (size_t index = 0; index < points.size(); ++index) {
        const float32 position_error = points[index].position.DistanceTo(target_position);
        if (position_error > position_tolerance_mm) {
            continue;
        }

        TriggerMarkerCandidate candidate;
        candidate.found = true;
        candidate.kind = TriggerMarkerCandidateKind::ExistingPoint;
        candidate.index = index;
        candidate.position_error_mm = position_error;
        candidate.distance_error_mm =
            use_distance_hint ? std::fabs(cumulative[index] - trigger_distance_mm) : 0.0f;
        if (IsBetterTriggerMarkerCandidate(candidate, best_candidate)) {
            best_candidate = candidate;
        }
    }

    for (size_t index = 1; index < points.size(); ++index) {
        const Point2D start = points[index - 1].position;
        const Point2D end = points[index].position;
        const Point2D delta = end - start;
        const float32 length_sq = delta.x * delta.x + delta.y * delta.y;
        if (length_sq <= kTriggerMarkerMatchToleranceMm * kTriggerMarkerMatchToleranceMm) {
            continue;
        }

        const Point2D offset = target_position - start;
        const float32 raw_ratio = (offset.x * delta.x + offset.y * delta.y) / length_sq;
        const float32 ratio = std::clamp(raw_ratio, 0.0f, 1.0f);
        const Point2D projected = start + delta * ratio;
        const float32 position_error = projected.DistanceTo(target_position);
        if (position_error > position_tolerance_mm) {
            continue;
        }

        TriggerMarkerCandidate candidate;
        candidate.found = true;
        candidate.kind = TriggerMarkerCandidateKind::SegmentProjection;
        candidate.index = index;
        candidate.ratio = ratio;
        candidate.position_error_mm = position_error;
        candidate.distance_error_mm =
            use_distance_hint
                ? std::fabs((cumulative[index - 1] + std::sqrt(length_sq) * ratio) - trigger_distance_mm)
                : 0.0f;
        if (IsBetterTriggerMarkerCandidate(candidate, best_candidate)) {
            best_candidate = candidate;
        }
    }

    if (!best_candidate.found) {
        return false;
    }

    if (best_candidate.kind == TriggerMarkerCandidateKind::ExistingPoint) {
        points[best_candidate.index].position = target_position;
        MarkTriggerPoint(points[best_candidate.index], trigger_distance_mm, pulse_width_us);
        ReassignTrajectorySequenceIds(points);
        return true;
    }

    TrajectoryPoint inserted = InterpolateTrajectoryPoint(
        points[best_candidate.index - 1],
        points[best_candidate.index],
        best_candidate.ratio);
    inserted.position = target_position;
    MarkTriggerPoint(inserted, trigger_distance_mm, pulse_width_us);
    points.insert(points.begin() + static_cast<std::ptrdiff_t>(best_candidate.index), inserted);
    ReassignTrajectorySequenceIds(points);
    return true;
}

inline bool ApplyTriggerMarkersByPosition(std::vector<TrajectoryPoint>& points,
                                          const std::vector<Point2D>& trigger_positions,
                                          const std::vector<float32>& trigger_distances_mm,
                                          float32 dedup_tolerance_mm = kTriggerMarkerDedupToleranceMm,
                                          float32 position_tolerance_mm = kTriggerMarkerMatchToleranceMm,
                                          uint16 pulse_width_us = kDefaultTriggerPulseWidthUs) {
    if (!trigger_distances_mm.empty() && trigger_positions.size() != trigger_distances_mm.size()) {
        return false;
    }

    points = ClearTriggerMarkers(points);
    ReassignTrajectorySequenceIds(points);

    Point2D last_position;
    bool has_last_position = false;
    float32 last_distance_mm = 0.0f;
    bool has_last_distance = false;
    for (size_t index = 0; index < trigger_positions.size(); ++index) {
        const auto& position = trigger_positions[index];
        const float32 trigger_distance_mm =
            trigger_distances_mm.empty() ? 0.0f : trigger_distances_mm[index];
        if (has_last_position && last_position.DistanceTo(position) <= dedup_tolerance_mm) {
            if (trigger_distances_mm.empty() ||
                (has_last_distance && std::fabs(trigger_distance_mm - last_distance_mm) <= dedup_tolerance_mm)) {
                continue;
            }
        }
        if (!InsertTriggerMarkerByPosition(
                points, position, trigger_distance_mm, position_tolerance_mm, pulse_width_us)) {
            return false;
        }
        last_position = position;
        has_last_position = true;
        last_distance_mm = trigger_distance_mm;
        has_last_distance = true;
    }

    return true;
}

inline std::vector<TrajectoryPoint> BuildTriggerPreviewPoints(const std::vector<TrajectoryPoint>& source,
                                                              const std::vector<float32>& trigger_distances_mm,
                                                              float32 dedup_tolerance_mm = kTriggerMarkerDedupToleranceMm,
                                                              float32 match_tolerance_mm = kTriggerMarkerMatchToleranceMm,
                                                              uint16 pulse_width_us = kDefaultTriggerPulseWidthUs) {
    if (source.size() < 2 || trigger_distances_mm.empty()) {
        return {};
    }

    std::vector<TrajectoryPoint> preview = source;
    if (!ApplyTriggerMarkersByDistance(
            preview, trigger_distances_mm, dedup_tolerance_mm, match_tolerance_mm, pulse_width_us)) {
        return {};
    }

    std::vector<TrajectoryPoint> trigger_points;
    trigger_points.reserve(trigger_distances_mm.size());
    for (const auto& point : preview) {
        if (point.enable_position_trigger) {
            trigger_points.push_back(point);
        }
    }
    return trigger_points;
}

inline size_t CountTriggerMarkers(const std::vector<TrajectoryPoint>& points) {
    size_t count = 0;
    for (const auto& point : points) {
        if (point.enable_position_trigger) {
            ++count;
        }
    }
    return count;
}

}  // namespace Siligen::Shared::Types
