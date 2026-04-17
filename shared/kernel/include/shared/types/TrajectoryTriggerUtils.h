#pragma once

#include "shared/types/Point.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <unordered_map>
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

struct TriggerMarkerRequest {
    Point2D position;
    float32 trigger_distance_mm = 0.0f;
    uint16 pulse_width_us = kDefaultTriggerPulseWidthUs;
    size_t order_index = 0;
};

struct TriggerMarkerPlan {
    TriggerMarkerCandidateKind kind = TriggerMarkerCandidateKind::ExistingPoint;
    size_t index = 0;
    float32 ratio = 0.0f;
    Point2D position;
    float32 trigger_distance_mm = 0.0f;
    uint16 pulse_width_us = kDefaultTriggerPulseWidthUs;
    size_t order_index = 0;
};

struct TriggerMarkerPointKey {
    int64_t x_bucket = 0;
    int64_t y_bucket = 0;

    bool operator==(const TriggerMarkerPointKey& other) const noexcept {
        return x_bucket == other.x_bucket && y_bucket == other.y_bucket;
    }
};

struct TriggerMarkerPointKeyHash {
    size_t operator()(const TriggerMarkerPointKey& key) const noexcept {
        const auto x_hash = std::hash<int64_t>{}(key.x_bucket);
        const auto y_hash = std::hash<int64_t>{}(key.y_bucket);
        return x_hash ^ (y_hash + 0x9e3779b97f4a7c15ULL + (x_hash << 6) + (x_hash >> 2));
    }
};

using TriggerMarkerPointIndex =
    std::unordered_map<TriggerMarkerPointKey, std::vector<size_t>, TriggerMarkerPointKeyHash>;

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

inline float32 ResolveTriggerMarkerCellSize(float32 position_tolerance_mm) {
    return std::max(position_tolerance_mm, kTriggerMarkerMatchToleranceMm);
}

inline int64_t TriggerMarkerBucketCoord(float32 value, float32 cell_size_mm) {
    return static_cast<int64_t>(std::floor(static_cast<double>(value) / static_cast<double>(cell_size_mm)));
}

inline TriggerMarkerPointKey MakeTriggerMarkerPointKey(const Point2D& position, float32 cell_size_mm) {
    TriggerMarkerPointKey key;
    key.x_bucket = TriggerMarkerBucketCoord(position.x, cell_size_mm);
    key.y_bucket = TriggerMarkerBucketCoord(position.y, cell_size_mm);
    return key;
}

inline TriggerMarkerPointIndex BuildTriggerMarkerPointIndex(const std::vector<TrajectoryPoint>& points,
                                                            float32 position_tolerance_mm) {
    TriggerMarkerPointIndex index;
    const float32 cell_size_mm = ResolveTriggerMarkerCellSize(position_tolerance_mm);
    for (size_t point_index = 0; point_index < points.size(); ++point_index) {
        index[MakeTriggerMarkerPointKey(points[point_index].position, cell_size_mm)].push_back(point_index);
    }
    return index;
}

inline void CollectTriggerMarkerPointCandidates(const TriggerMarkerPointIndex& index,
                                                const Point2D& position,
                                                float32 position_tolerance_mm,
                                                std::vector<size_t>& candidates) {
    candidates.clear();
    const float32 cell_size_mm = ResolveTriggerMarkerCellSize(position_tolerance_mm);
    const auto center_key = MakeTriggerMarkerPointKey(position, cell_size_mm);
    for (int64_t dx = -1; dx <= 1; ++dx) {
        for (int64_t dy = -1; dy <= 1; ++dy) {
            TriggerMarkerPointKey key{center_key.x_bucket + dx, center_key.y_bucket + dy};
            auto it = index.find(key);
            if (it == index.end()) {
                continue;
            }
            candidates.insert(candidates.end(), it->second.begin(), it->second.end());
        }
    }
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
}

inline void ConsiderTriggerMarkerPointCandidate(const std::vector<TrajectoryPoint>& points,
                                                const std::vector<float32>& cumulative,
                                                const TriggerMarkerRequest& request,
                                                float32 clamped_target,
                                                float32 position_tolerance_mm,
                                                size_t point_index,
                                                TriggerMarkerCandidate& best_candidate) {
    if (point_index >= points.size()) {
        return;
    }

    const float32 position_error = points[point_index].position.DistanceTo(request.position);
    if (position_error > position_tolerance_mm) {
        return;
    }

    TriggerMarkerCandidate candidate;
    candidate.found = true;
    candidate.kind = TriggerMarkerCandidateKind::ExistingPoint;
    candidate.index = point_index;
    candidate.position_error_mm = position_error;
    candidate.distance_error_mm = std::fabs(cumulative[point_index] - clamped_target);
    if (IsBetterTriggerMarkerCandidate(candidate, best_candidate)) {
        best_candidate = candidate;
    }
}

inline void ConsiderTriggerMarkerSegmentCandidate(const std::vector<TrajectoryPoint>& points,
                                                  const std::vector<float32>& cumulative,
                                                  const TriggerMarkerRequest& request,
                                                  float32 clamped_target,
                                                  float32 position_tolerance_mm,
                                                  size_t segment_end_index,
                                                  TriggerMarkerCandidate& best_candidate) {
    if (segment_end_index == 0 || segment_end_index >= points.size()) {
        return;
    }

    const Point2D start = points[segment_end_index - 1].position;
    const Point2D end = points[segment_end_index].position;
    const Point2D delta = end - start;
    const float32 length_sq = delta.x * delta.x + delta.y * delta.y;
    if (length_sq <= kTriggerMarkerMatchToleranceMm * kTriggerMarkerMatchToleranceMm) {
        return;
    }

    const Point2D offset = request.position - start;
    const float32 raw_ratio = (offset.x * delta.x + offset.y * delta.y) / length_sq;
    const float32 ratio = std::clamp(raw_ratio, 0.0f, 1.0f);
    const Point2D projected = start + delta * ratio;
    const float32 position_error = projected.DistanceTo(request.position);
    if (position_error > position_tolerance_mm) {
        return;
    }

    TriggerMarkerCandidate candidate;
    candidate.found = true;
    candidate.kind = TriggerMarkerCandidateKind::SegmentProjection;
    candidate.index = segment_end_index;
    candidate.ratio = ratio;
    candidate.position_error_mm = position_error;
    candidate.distance_error_mm =
        std::fabs((cumulative[segment_end_index - 1] + std::sqrt(length_sq) * ratio) - clamped_target);
    if (IsBetterTriggerMarkerCandidate(candidate, best_candidate)) {
        best_candidate = candidate;
    }
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

inline std::vector<TriggerMarkerRequest> NormalizeTriggerMarkerRequests(
    const std::vector<Point2D>& trigger_positions,
    const std::vector<float32>& trigger_distances_mm,
    float32 dedup_tolerance_mm,
    uint16 pulse_width_us) {
    std::vector<TriggerMarkerRequest> requests;
    requests.reserve(trigger_positions.size());

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

        TriggerMarkerRequest request;
        request.position = position;
        request.trigger_distance_mm = trigger_distance_mm;
        request.pulse_width_us = pulse_width_us;
        request.order_index = requests.size();
        requests.push_back(request);

        last_position = position;
        has_last_position = true;
        last_distance_mm = trigger_distance_mm;
        has_last_distance = true;
    }

    return requests;
}

inline bool ResolveDistanceGuidedTriggerMarkerPlan(
    const std::vector<TrajectoryPoint>& points,
    const std::vector<float32>& cumulative,
    const TriggerMarkerRequest& request,
    float32 position_tolerance_mm,
    TriggerMarkerPlan& plan) {
    if (points.empty() || cumulative.empty() || !std::isfinite(request.trigger_distance_mm)) {
        return false;
    }

    const float32 total_length = cumulative.back();
    if (request.trigger_distance_mm < -position_tolerance_mm ||
        request.trigger_distance_mm > total_length + position_tolerance_mm) {
        return false;
    }

    const float32 clamped_target = std::clamp(request.trigger_distance_mm, 0.0f, total_length);
    const auto lower = std::lower_bound(cumulative.begin(), cumulative.end(), clamped_target);
    size_t right_index = static_cast<size_t>(std::distance(cumulative.begin(), lower));
    if (right_index == 0) {
        right_index = std::min<size_t>(1, points.size() - 1);
    } else if (right_index >= points.size()) {
        right_index = points.size() - 1;
    }

    TriggerMarkerCandidate best_candidate;
    ConsiderTriggerMarkerPointCandidate(
        points, cumulative, request, clamped_target, position_tolerance_mm, right_index, best_candidate);
    ConsiderTriggerMarkerPointCandidate(
        points, cumulative, request, clamped_target, position_tolerance_mm, right_index - 1, best_candidate);
    ConsiderTriggerMarkerSegmentCandidate(
        points, cumulative, request, clamped_target, position_tolerance_mm, right_index, best_candidate);

    if (!best_candidate.found) {
        return false;
    }

    plan.kind = best_candidate.kind;
    plan.index = best_candidate.index;
    plan.ratio = best_candidate.ratio;
    plan.position = request.position;
    plan.trigger_distance_mm = request.trigger_distance_mm;
    plan.pulse_width_us = request.pulse_width_us;
    plan.order_index = request.order_index;
    return true;
}

inline bool ResolveIndexedPointTriggerMarkerPlan(const std::vector<TrajectoryPoint>& points,
                                                 const std::vector<float32>& cumulative,
                                                 const TriggerMarkerPointIndex& point_index,
                                                 const TriggerMarkerRequest& request,
                                                 float32 position_tolerance_mm,
                                                 size_t preferred_start_index,
                                                 TriggerMarkerPlan& plan,
                                                 bool allow_wraparound = true) {
    if (points.empty() || cumulative.empty()) {
        return false;
    }

    std::vector<size_t> candidates;
    CollectTriggerMarkerPointCandidates(point_index, request.position, position_tolerance_mm, candidates);
    if (candidates.empty()) {
        return false;
    }

    const float32 total_length = cumulative.back();
    const float32 clamped_target = std::clamp(request.trigger_distance_mm, 0.0f, total_length);
    TriggerMarkerCandidate best_candidate;

    auto consider_candidates = [&](bool prefer_forward) {
        for (size_t point_index_value : candidates) {
            if (prefer_forward && point_index_value < preferred_start_index) {
                continue;
            }
            if (!prefer_forward && point_index_value >= preferred_start_index) {
                continue;
            }
            ConsiderTriggerMarkerPointCandidate(
                points,
                cumulative,
                request,
                clamped_target,
                position_tolerance_mm,
                point_index_value,
                best_candidate);
        }
    };

    consider_candidates(true);
    if (!best_candidate.found && allow_wraparound) {
        consider_candidates(false);
    }
    if (!best_candidate.found) {
        return false;
    }

    plan.kind = best_candidate.kind;
    plan.index = best_candidate.index;
    plan.ratio = best_candidate.ratio;
    plan.position = request.position;
    plan.trigger_distance_mm = request.trigger_distance_mm;
    plan.pulse_width_us = request.pulse_width_us;
    plan.order_index = request.order_index;
    return true;
}

inline bool ResolveSequentialSegmentTriggerMarkerPlan(const std::vector<TrajectoryPoint>& points,
                                                      const std::vector<float32>& cumulative,
                                                      const TriggerMarkerRequest& request,
                                                      float32 position_tolerance_mm,
                                                      size_t preferred_start_index,
                                                      TriggerMarkerPlan& plan,
                                                      bool allow_wraparound = true) {
    if (points.size() < 2 || cumulative.empty()) {
        return false;
    }

    const float32 total_length = cumulative.back();
    const float32 clamped_target = std::clamp(request.trigger_distance_mm, 0.0f, total_length);

    auto try_scan_range = [&](size_t start_index, size_t end_index_inclusive) -> bool {
        if (start_index > end_index_inclusive || end_index_inclusive >= points.size()) {
            return false;
        }
        for (size_t segment_end_index = std::max<size_t>(1, start_index);
             segment_end_index <= end_index_inclusive;
             ++segment_end_index) {
            TriggerMarkerCandidate candidate;
            ConsiderTriggerMarkerSegmentCandidate(
                points,
                cumulative,
                request,
                clamped_target,
                position_tolerance_mm,
                segment_end_index,
                candidate);
            if (!candidate.found) {
                continue;
            }

            if (candidate.ratio <= kTriggerMarkerMatchToleranceMm) {
                plan.kind = TriggerMarkerCandidateKind::ExistingPoint;
                plan.index = segment_end_index - 1;
                plan.ratio = 0.0f;
            } else if (candidate.ratio >= 1.0f - kTriggerMarkerMatchToleranceMm) {
                plan.kind = TriggerMarkerCandidateKind::ExistingPoint;
                plan.index = segment_end_index;
                plan.ratio = 0.0f;
            } else {
                plan.kind = candidate.kind;
                plan.index = candidate.index;
                plan.ratio = candidate.ratio;
            }
            plan.position = request.position;
            plan.trigger_distance_mm = request.trigger_distance_mm;
            plan.pulse_width_us = request.pulse_width_us;
            plan.order_index = request.order_index;
            return true;
        }
        return false;
    };

    const size_t forward_start = std::min(preferred_start_index, points.size() - 1);
    if (try_scan_range(forward_start, points.size() - 1)) {
        return true;
    }
    return allow_wraparound && forward_start > 1 && try_scan_range(1, forward_start - 1);
}

inline bool ResolveDistanceOnlyTriggerMarkerPlan(const std::vector<TrajectoryPoint>& points,
                                                 const std::vector<float32>& cumulative,
                                                 const TriggerMarkerRequest& request,
                                                 float32 compare_tolerance_mm,
                                                 TriggerMarkerPlan& plan) {
    if (points.size() < 2 || cumulative.empty() || !std::isfinite(request.trigger_distance_mm)) {
        return false;
    }

    const float32 total_length = cumulative.back();
    const float32 clamped_target = std::clamp(request.trigger_distance_mm, 0.0f, total_length);
    const auto lower = std::lower_bound(cumulative.begin(), cumulative.end(), clamped_target);
    size_t right_index = static_cast<size_t>(std::distance(cumulative.begin(), lower));
    if (right_index == 0) {
        right_index = std::min<size_t>(1, points.size() - 1);
    } else if (right_index >= points.size()) {
        right_index = points.size() - 1;
    }

    if (std::fabs(cumulative[right_index - 1] - clamped_target) <= compare_tolerance_mm) {
        plan.kind = TriggerMarkerCandidateKind::ExistingPoint;
        plan.index = right_index - 1;
        plan.ratio = 0.0f;
        plan.position = request.position;
        plan.trigger_distance_mm = request.trigger_distance_mm;
        plan.pulse_width_us = request.pulse_width_us;
        plan.order_index = request.order_index;
        return true;
    }

    if (std::fabs(cumulative[right_index] - clamped_target) <= compare_tolerance_mm) {
        plan.kind = TriggerMarkerCandidateKind::ExistingPoint;
        plan.index = right_index;
        plan.ratio = 0.0f;
        plan.position = request.position;
        plan.trigger_distance_mm = request.trigger_distance_mm;
        plan.pulse_width_us = request.pulse_width_us;
        plan.order_index = request.order_index;
        return true;
    }

    const float32 segment_length = cumulative[right_index] - cumulative[right_index - 1];
    if (segment_length <= compare_tolerance_mm) {
        plan.kind = TriggerMarkerCandidateKind::ExistingPoint;
        plan.index = right_index;
        plan.ratio = 0.0f;
        plan.position = request.position;
        plan.trigger_distance_mm = request.trigger_distance_mm;
        plan.pulse_width_us = request.pulse_width_us;
        plan.order_index = request.order_index;
        return true;
    }

    plan.kind = TriggerMarkerCandidateKind::SegmentProjection;
    plan.index = right_index;
    plan.ratio = std::clamp(
        (clamped_target - cumulative[right_index - 1]) / segment_length,
        0.0f,
        1.0f);
    plan.position = request.position;
    plan.trigger_distance_mm = request.trigger_distance_mm;
    plan.pulse_width_us = request.pulse_width_us;
    plan.order_index = request.order_index;
    return true;
}

inline bool SupportsTriggerMarkerCyclicWraparound(const std::vector<TrajectoryPoint>& points,
                                                  float32 position_tolerance_mm) {
    if (points.size() < 3) {
        return false;
    }

    const float32 closure_tolerance_mm =
        std::max(position_tolerance_mm, kTriggerMarkerMatchToleranceMm);
    return points.front().position.DistanceTo(points.back().position) <= closure_tolerance_mm;
}

inline std::vector<TrajectoryPoint> MaterializeTriggerMarkerPlans(
    const std::vector<TrajectoryPoint>& base_points,
    std::vector<TriggerMarkerPlan> plans) {
    std::vector<std::vector<TriggerMarkerPlan>> point_plans(base_points.size());
    std::vector<std::vector<TriggerMarkerPlan>> segment_plans(base_points.size());
    for (const auto& plan : plans) {
        if (plan.kind == TriggerMarkerCandidateKind::ExistingPoint) {
            point_plans[plan.index].push_back(plan);
        } else {
            segment_plans[plan.index].push_back(plan);
        }
    }

    auto sort_by_order = [](auto& bucket) {
        std::stable_sort(bucket.begin(), bucket.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.order_index < rhs.order_index;
        });
    };
    auto sort_segments = [](auto& bucket) {
        std::stable_sort(bucket.begin(), bucket.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.ratio != rhs.ratio) {
                return lhs.ratio < rhs.ratio;
            }
            return lhs.order_index < rhs.order_index;
        });
    };

    for (auto& bucket : point_plans) {
        sort_by_order(bucket);
    }
    for (auto& bucket : segment_plans) {
        sort_segments(bucket);
    }

    std::vector<TrajectoryPoint> result;
    result.reserve(base_points.size() + plans.size());
    for (size_t point_index = 0; point_index < base_points.size(); ++point_index) {
        if (point_index > 0) {
            for (const auto& plan : segment_plans[point_index]) {
                TrajectoryPoint inserted = InterpolateTrajectoryPoint(
                    base_points[point_index - 1],
                    base_points[point_index],
                    plan.ratio);
                inserted.position = plan.position;
                MarkTriggerPoint(inserted, plan.trigger_distance_mm, plan.pulse_width_us);
                result.push_back(inserted);
            }
        }

        if (point_plans[point_index].empty()) {
            result.push_back(base_points[point_index]);
            continue;
        }

        for (const auto& plan : point_plans[point_index]) {
            TrajectoryPoint marked = base_points[point_index];
            marked.position = plan.position;
            MarkTriggerPoint(marked, plan.trigger_distance_mm, plan.pulse_width_us);
            result.push_back(marked);
        }
    }

    ReassignTrajectorySequenceIds(result);
    return result;
}

inline bool ApplyTriggerMarkersByPositionLegacy(std::vector<TrajectoryPoint>& points,
                                                const std::vector<Point2D>& trigger_positions,
                                                const std::vector<float32>& trigger_distances_mm,
                                                float32 dedup_tolerance_mm = kTriggerMarkerDedupToleranceMm,
                                                float32 position_tolerance_mm = kTriggerMarkerMatchToleranceMm,
                                                uint16 pulse_width_us = kDefaultTriggerPulseWidthUs) {
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

inline bool ApplyTriggerMarkersByPosition(std::vector<TrajectoryPoint>& points,
                                          const std::vector<Point2D>& trigger_positions,
                                          const std::vector<float32>& trigger_distances_mm,
                                          float32 dedup_tolerance_mm = kTriggerMarkerDedupToleranceMm,
                                          float32 position_tolerance_mm = kTriggerMarkerMatchToleranceMm,
                                          uint16 pulse_width_us = kDefaultTriggerPulseWidthUs) {
    if (!trigger_distances_mm.empty() && trigger_positions.size() != trigger_distances_mm.size()) {
        return false;
    }

    if (!trigger_distances_mm.empty()) {
        auto base_points = ClearTriggerMarkers(points);
        ReassignTrajectorySequenceIds(base_points);
        if (trigger_positions.empty()) {
            points = std::move(base_points);
            return true;
        }
        if (!base_points.empty()) {
            const auto requests = NormalizeTriggerMarkerRequests(
                trigger_positions,
                trigger_distances_mm,
                dedup_tolerance_mm,
                pulse_width_us);
            const auto cumulative = BuildTrajectoryCumulativeDistances(base_points);
            const auto point_index = BuildTriggerMarkerPointIndex(base_points, position_tolerance_mm);
            const bool allow_cyclic_wraparound =
                SupportsTriggerMarkerCyclicWraparound(base_points, position_tolerance_mm);

            std::vector<TriggerMarkerPlan> plans;
            plans.reserve(requests.size());
            bool batch_resolved = true;
            size_t preferred_start_index = 0;
            for (const auto& request : requests) {
                TriggerMarkerPlan plan;
                TriggerMarkerPlan indexed_point_plan;
                const bool has_indexed_point_plan = ResolveIndexedPointTriggerMarkerPlan(
                    base_points,
                    cumulative,
                    point_index,
                    request,
                    position_tolerance_mm,
                    preferred_start_index,
                    indexed_point_plan,
                    allow_cyclic_wraparound);
                TriggerMarkerPlan sequential_segment_plan;
                const bool has_sequential_segment_plan = ResolveSequentialSegmentTriggerMarkerPlan(
                    base_points,
                    cumulative,
                    request,
                    position_tolerance_mm,
                    preferred_start_index,
                    sequential_segment_plan,
                    allow_cyclic_wraparound);

                if (has_sequential_segment_plan &&
                    (!has_indexed_point_plan || sequential_segment_plan.index < indexed_point_plan.index)) {
                    plan = sequential_segment_plan;
                } else if (has_indexed_point_plan) {
                    plan = indexed_point_plan;
                } else if (
                    !ResolveDistanceGuidedTriggerMarkerPlan(
                        base_points,
                        cumulative,
                        request,
                        position_tolerance_mm,
                        plan) &&
                    !ResolveDistanceOnlyTriggerMarkerPlan(
                        base_points,
                        cumulative,
                        request,
                        position_tolerance_mm,
                        plan)) {
                    batch_resolved = false;
                    break;
                }
                plans.push_back(plan);
                preferred_start_index = plan.index;
                if (plan.kind == TriggerMarkerCandidateKind::ExistingPoint &&
                    preferred_start_index < base_points.size()) {
                    ++preferred_start_index;
                }
            }

            if (batch_resolved) {
                points = MaterializeTriggerMarkerPlans(base_points, std::move(plans));
                return true;
            }
        }
    }

    return ApplyTriggerMarkersByPositionLegacy(
        points,
        trigger_positions,
        trigger_distances_mm,
        dedup_tolerance_mm,
        position_tolerance_mm,
        pulse_width_us);
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
