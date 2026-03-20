"""Internal semantic hints for conservative time-anchor mapping."""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Any, Mapping, Sequence

from ..adapters.recording_normalizer import NormalizedObserverData
from ..contracts.observer_status import (
    REASON_TIME_MAPPING_FROM_SNAPSHOT_POSITION,
    REASON_TIME_MAPPING_FROM_SNAPSHOT_SEQUENCE,
    REASON_TIME_MAPPING_SNAPSHOT_AMBIGUOUS,
)
from ..contracts.observer_types import ArcGeometry, ObjectId, PathPointObject, PathSegmentObject, Point2D, TimeAnchorObject

_MAX_SNAPSHOT_TIME_DELTA_NS = 1_000_000
_SNAPSHOT_MATCH_DISTANCE_MM = 0.5
_UNIQUE_DISTANCE_MARGIN_MM = 0.5
_MAX_SEQUENCE_CANDIDATE_COUNT = 3


@dataclass(frozen=True, slots=True)
class _PositionHint:
    segment_ids: tuple[ObjectId, ...]
    min_distance: float
    reason_code: str
    reason_text: str
    concurrent_candidates_present: bool = False


def with_time_mapping_hint(
    time_anchor: TimeAnchorObject,
    raw_mapping: Mapping[str, Any],
    data: NormalizedObserverData,
) -> Mapping[str, Any]:
    if _contains_mapping_keys(raw_mapping):
        return raw_mapping
    if time_anchor.source_kind != "trace":
        return raw_mapping

    tick_time_ns = _tick_time_ns(raw_mapping, time_anchor)
    if tick_time_ns is None:
        return raw_mapping
    axis_position = _axis_position_from_snapshots(data, tick_time_ns)
    if axis_position is None:
        return raw_mapping

    hint = _position_hint(axis_position, data)
    if hint is None:
        return raw_mapping

    merged = dict(raw_mapping)
    merged["time_mapping_hint_source"] = "snapshot_position"
    merged["time_mapping_hint_reason_code"] = hint.reason_code
    merged["time_mapping_hint_reason_text"] = hint.reason_text
    merged["time_mapping_hint_distance_mm"] = hint.min_distance
    if len(hint.segment_ids) == 1:
        merged["primary_object_id"] = str(hint.segment_ids[0])
        return merged
    merged["segment_member_ids"] = [str(segment_id) for segment_id in hint.segment_ids]
    merged["candidate_segment_indices"] = [int(str(segment_id).split(":")[-1]) for segment_id in hint.segment_ids]
    if hint.concurrent_candidates_present:
        merged["concurrent_candidates_present"] = True
    return merged


def _position_hint(
    axis_position: tuple[float, float],
    data: NormalizedObserverData,
) -> _PositionHint | None:
    if not data.path_segments:
        return None
    path_points_by_segment = _path_points_by_segment(data.path_points)
    distance_items: list[tuple[float, PathSegmentObject]] = []
    for segment in data.path_segments:
        render_points = _render_points_for_segment(segment, path_points_by_segment)
        distance_items.append((_distance_to_segment(axis_position, segment, render_points), segment))
    distance_items.sort(key=lambda item: (item[0], item[1].segment_index))
    best_distance = distance_items[0][0]
    if best_distance > _SNAPSHOT_MATCH_DISTANCE_MM:
        return None

    if len(distance_items) == 1 or distance_items[1][0] - best_distance >= _UNIQUE_DISTANCE_MARGIN_MM:
        best_segment = distance_items[0][1]
        return _PositionHint(
            segment_ids=(best_segment.object_id,),
            min_distance=best_distance,
            reason_code=REASON_TIME_MAPPING_FROM_SNAPSHOT_POSITION,
            reason_text=(
                "Snapshot axis position is uniquely closest to a path segment; "
                f"distance={best_distance:.3f}mm."
            ),
        )

    candidate_segments = tuple(
        segment
        for distance, segment in distance_items
        if distance <= _SNAPSHOT_MATCH_DISTANCE_MM
    )
    candidate_ids = tuple(segment.object_id for segment in candidate_segments)
    candidate_indexes = tuple(segment.segment_index for segment in candidate_segments)
    if (
        1 < len(candidate_segments) <= _MAX_SEQUENCE_CANDIDATE_COUNT
        and _is_contiguous(candidate_indexes)
    ):
        return _PositionHint(
            segment_ids=candidate_ids,
            min_distance=best_distance,
            reason_code=REASON_TIME_MAPPING_FROM_SNAPSHOT_SEQUENCE,
            reason_text=(
                "Snapshot axis position is shared by a contiguous segment range; "
                f"distance={best_distance:.3f}mm."
            ),
        )

    return _PositionHint(
        segment_ids=candidate_ids,
        min_distance=best_distance,
        reason_code=REASON_TIME_MAPPING_SNAPSHOT_AMBIGUOUS,
        reason_text=(
            "Snapshot axis position matches multiple non-contiguous segments; "
            f"distance={best_distance:.3f}mm."
        ),
        concurrent_candidates_present=True,
    )


def _contains_mapping_keys(raw_mapping: Mapping[str, Any]) -> bool:
    for key in (
        "primary_object_id",
        "object_id",
        "primary_segment_index",
        "segment_index",
        "start_segment_id",
        "end_segment_id",
        "segment_member_ids",
        "candidate_object_ids",
        "object_ids",
        "candidate_segment_indices",
        "segment_indices",
    ):
        if key in raw_mapping:
            return True
    return False


def _tick_time_ns(raw_mapping: Mapping[str, Any], time_anchor: TimeAnchorObject) -> int | None:
    tick = raw_mapping.get("tick")
    if isinstance(tick, Mapping):
        time_ns = tick.get("time_ns")
        if time_ns is not None:
            return int(time_ns)
    return int(round(time_anchor.time_start * 1_000_000_000))


def _axis_position_from_snapshots(
    data: NormalizedObserverData,
    tick_time_ns: int,
) -> tuple[float, float] | None:
    best_position: tuple[float, float] | None = None
    best_delta: int | None = None
    for snapshot in data.source_root.snapshots:
        if not isinstance(snapshot, Mapping):
            continue
        tick = snapshot.get("tick")
        if not isinstance(tick, Mapping) or tick.get("time_ns") is None:
            continue
        delta = abs(int(tick["time_ns"]) - tick_time_ns)
        if delta > _MAX_SNAPSHOT_TIME_DELTA_NS:
            continue
        position = _axis_position_from_snapshot(snapshot)
        if position is None:
            continue
        if best_delta is None or delta < best_delta:
            best_delta = delta
            best_position = position
            if delta == 0:
                break
    return best_position


def _axis_position_from_snapshot(snapshot: Mapping[str, Any]) -> tuple[float, float] | None:
    axes = snapshot.get("axes")
    if not isinstance(axes, Sequence) or isinstance(axes, (str, bytes)):
        return None
    x_value: float | None = None
    y_value: float | None = None
    for axis in axes:
        if not isinstance(axis, Mapping):
            continue
        axis_name = str(axis.get("axis", "")).upper()
        position = axis.get("position_mm")
        if position is None:
            continue
        if axis_name == "X":
            x_value = float(position)
        elif axis_name == "Y":
            y_value = float(position)
    if x_value is None or y_value is None:
        return None
    return (x_value, y_value)


def _path_points_by_segment(
    path_points: tuple[PathPointObject, ...],
) -> dict[ObjectId, tuple[Point2D, ...]]:
    grouped: dict[ObjectId, list[PathPointObject]] = {}
    for path_point in path_points:
        grouped.setdefault(path_point.parent_segment_id, []).append(path_point)
    return {
        segment_id: tuple(
            path_point.point
            for path_point in sorted(items, key=lambda item: item.point_index)
        )
        for segment_id, items in grouped.items()
    }


def _render_points_for_segment(
    segment: PathSegmentObject,
    path_points_by_segment: Mapping[ObjectId, tuple[Point2D, ...]],
) -> tuple[Point2D, ...]:
    ordered_points = path_points_by_segment.get(segment.object_id, tuple())
    if len(ordered_points) >= 2:
        return ordered_points
    return (segment.start_point, segment.end_point)


def _distance_to_polyline(
    axis_position: tuple[float, float],
    render_points: tuple[Point2D, ...],
) -> float:
    if len(render_points) < 2:
        return float("inf")
    px, py = axis_position
    return min(
        _distance_to_line_segment(px, py, start_point, end_point)
        for start_point, end_point in zip(render_points, render_points[1:])
    )


def _distance_to_segment(
    axis_position: tuple[float, float],
    segment: PathSegmentObject,
    render_points: tuple[Point2D, ...],
) -> float:
    distance = _distance_to_polyline(axis_position, render_points)
    arc_geometry = segment.arc_geometry
    if arc_geometry is not None and segment.segment_type.strip().lower() == "arc":
        distance = min(distance, _distance_to_arc(axis_position, arc_geometry))
    return distance


def _distance_to_arc(axis_position: tuple[float, float], arc_geometry: ArcGeometry) -> float:
    px, py = axis_position
    cx = arc_geometry.center.x
    cy = arc_geometry.center.y
    radius = arc_geometry.radius

    start_angle = arc_geometry.start_angle_rad
    sweep_angle = arc_geometry.end_angle_rad - arc_geometry.start_angle_rad
    start_point = _point_on_arc(cx, cy, radius, start_angle)
    end_point = _point_on_arc(cx, cy, radius, start_angle + sweep_angle)
    endpoint_distance = min(
        math.hypot(px - start_point.x, py - start_point.y),
        math.hypot(px - end_point.x, py - end_point.y),
    )

    if math.isclose(sweep_angle, 0.0, abs_tol=1e-9):
        return endpoint_distance
    if abs(sweep_angle) >= math.tau:
        return abs(math.hypot(px - cx, py - cy) - radius)

    candidate_angle = math.atan2(py - cy, px - cx)
    if _angle_on_arc(candidate_angle, start_angle, sweep_angle):
        return abs(math.hypot(px - cx, py - cy) - radius)
    return endpoint_distance


def _point_on_arc(cx: float, cy: float, radius: float, angle: float) -> Point2D:
    return Point2D(
        x=cx + radius * math.cos(angle),
        y=cy + radius * math.sin(angle),
    )


def _angle_on_arc(angle: float, start_angle: float, sweep_angle: float) -> bool:
    if abs(sweep_angle) >= math.tau:
        return True
    normalized_start = _normalize_angle(start_angle)
    normalized_angle = _normalize_angle(angle)
    if sweep_angle > 0.0:
        delta = (normalized_angle - normalized_start) % math.tau
        return delta <= sweep_angle + 1e-9
    delta = (normalized_start - normalized_angle) % math.tau
    return delta <= abs(sweep_angle) + 1e-9


def _normalize_angle(angle: float) -> float:
    return angle % math.tau


def _distance_to_line_segment(
    px: float,
    py: float,
    start_point: Point2D,
    end_point: Point2D,
) -> float:
    ax = start_point.x
    ay = start_point.y
    bx = end_point.x
    by = end_point.y
    vx = bx - ax
    vy = by - ay
    wx = px - ax
    wy = py - ay
    c1 = vx * wx + vy * wy
    if c1 <= 0.0:
        return math.hypot(px - ax, py - ay)
    c2 = vx * vx + vy * vy
    if c2 <= c1:
        return math.hypot(px - bx, py - by)
    ratio = c1 / c2
    qx = ax + ratio * vx
    qy = ay + ratio * vy
    return math.hypot(px - qx, py - qy)


def _is_contiguous(indexes: Sequence[int]) -> bool:
    if not indexes:
        return False
    sorted_indexes = sorted(set(indexes))
    return sorted_indexes == list(range(sorted_indexes[0], sorted_indexes[-1] + 1))


__all__ = ["with_time_mapping_hint"]
