"""Normalize raw recording payloads into observer objects."""

from __future__ import annotations

import json
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping

from ..contracts.observer_types import (
    ArcGeometry,
    AlertObject,
    BBox2D,
    IssueObject,
    ObjectId,
    ObjectType,
    ObserverObjectBase,
    PathPointObject,
    PathSegmentObject,
    Point2D,
    RegionObject,
    SourceRef,
    TimeAnchorKind,
    TimeAnchorObject,
    ensure_display_label,
    ensure_source_refs,
)
from .recording_loader import RawRecordingPayload


@dataclass(frozen=True, slots=True)
class NormalizedObserverData:
    objects_by_id: dict[ObjectId, ObserverObjectBase]
    issues: tuple[IssueObject, ...]
    alerts: tuple[AlertObject, ...]
    time_anchors: tuple[TimeAnchorObject, ...]
    regions: tuple[RegionObject, ...]
    path_segments: tuple[PathSegmentObject, ...]
    path_points: tuple[PathPointObject, ...]
    initial_issue_ids: tuple[ObjectId, ...]
    source_root: RawRecordingPayload


@dataclass(frozen=True, slots=True)
class _PathSegmentSource:
    segment_index: int
    segment_data: Mapping[str, Any]
    source_ref: SourceRef


def normalize_recording(payload: RawRecordingPayload) -> NormalizedObserverData:
    segment_sources = _resolve_path_segment_sources(payload)
    path_segments = _normalize_path_segments(segment_sources)
    path_points = _normalize_path_points(segment_sources, path_segments)
    regions = _normalize_regions(payload)
    issues = _normalize_issues(payload)
    alerts = _normalize_alerts(payload)
    time_anchors = _normalize_time_anchors(payload, alerts)
    objects_by_id = _build_object_index(
        path_segments=path_segments,
        path_points=path_points,
        regions=regions,
        issues=issues,
        alerts=alerts,
        time_anchors=time_anchors,
    )
    initial_issue_ids = _build_initial_issue_ids(issues)
    return NormalizedObserverData(
        objects_by_id=objects_by_id,
        issues=issues,
        alerts=alerts,
        time_anchors=time_anchors,
        regions=regions,
        path_segments=path_segments,
        path_points=path_points,
        initial_issue_ids=initial_issue_ids,
        source_root=payload,
    )


def _resolve_path_segment_sources(payload: RawRecordingPayload) -> tuple[_PathSegmentSource, ...]:
    if not payload.motion_profile:
        return tuple()
    if _motion_profile_looks_like_path_segments(payload.motion_profile):
        return tuple(
            _PathSegmentSource(
                segment_index=_int_value(segment_data, "segment_index", index),
                segment_data=segment_data,
                source_ref=SourceRef(
                    source_name="motion_profile",
                    source_path=f"motion_profile[{index}]",
                    source_index=index,
                ),
            )
            for index, segment_data in enumerate(payload.motion_profile)
        )
    return _load_canonical_path_segment_sources(payload)


def _motion_profile_looks_like_path_segments(motion_profile: tuple[Mapping[str, Any], ...]) -> bool:
    first_segment = motion_profile[0]
    return any(
        key in first_segment
        for key in (
            "segment_index",
            "segment_type",
            "start",
            "end",
            "start_x",
            "start_y",
            "end_x",
            "end_y",
            "points",
        )
    )


def _load_canonical_path_segment_sources(payload: RawRecordingPayload) -> tuple[_PathSegmentSource, ...]:
    canonical_path = _extract_canonical_simulation_input_path(payload)
    if canonical_path is None:
        raise ValueError(
            "Unsupported motion_profile format: missing path segment definitions and no canonical simulation input reference found."
        )
    canonical_root = _load_json_mapping(canonical_path)
    raw_segments = canonical_root.get("segments")
    if not isinstance(raw_segments, list):
        raise ValueError(f"Canonical simulation input is missing a segments list: {canonical_path}")

    segment_sources: list[_PathSegmentSource] = []
    for index, raw_segment in enumerate(raw_segments):
        if not isinstance(raw_segment, Mapping):
            raise ValueError(f"Canonical simulation segment {index} must be a mapping: {canonical_path}")
        segment_sources.append(
            _PathSegmentSource(
                segment_index=index,
                segment_data=_segment_mapping_from_canonical(index, raw_segment),
                source_ref=SourceRef(
                    source_name="canonical_simulation_input",
                    source_path=f"{canonical_path}::segments[{index}]",
                    source_index=index,
                ),
            )
        )
    return tuple(segment_sources)


def _extract_canonical_simulation_input_path(payload: RawRecordingPayload) -> Path | None:
    marker = "Loaded canonical simulation input from "
    for entry in payload.timeline:
        if not isinstance(entry, Mapping):
            continue
        message = entry.get("message")
        if not isinstance(message, str):
            continue
        stripped = message.strip()
        if not stripped.startswith(marker):
            continue
        raw_path = stripped[len(marker) :].strip()
        if raw_path.endswith("."):
            raw_path = raw_path[:-1]
        return _resolve_canonical_path(payload, raw_path)
    return None


def _resolve_canonical_path(payload: RawRecordingPayload, raw_path: str) -> Path:
    candidate = Path(raw_path)
    candidates = [candidate]
    if payload.source_path is not None and not candidate.is_absolute():
        candidates.insert(0, payload.source_path.parent / candidate)
    candidates.extend(_fallback_canonical_candidates(payload, candidate))
    for path in candidates:
        if path.exists():
            return path.resolve()
    if payload.source_path is not None and not candidate.is_absolute():
        return (payload.source_path.parent / candidate).resolve()
    return candidate.resolve()


def _fallback_canonical_candidates(payload: RawRecordingPayload, candidate: Path) -> tuple[Path, ...]:
    if payload.source_path is None:
        return tuple()

    file_name = candidate.name
    if not file_name:
        return tuple()

    fallback_paths: list[Path] = []
    seen: set[Path] = set()
    for ancestor in payload.source_path.parents:
        sample_candidate = ancestor / "samples" / "simulation" / file_name
        if sample_candidate not in seen:
            fallback_paths.append(sample_candidate)
            seen.add(sample_candidate)
        if ancestor.name == "samples":
            sibling_candidate = ancestor / "simulation" / file_name
            if sibling_candidate not in seen:
                fallback_paths.append(sibling_candidate)
                seen.add(sibling_candidate)
    return tuple(fallback_paths)


def _load_json_mapping(path: Path) -> Mapping[str, Any]:
    if not path.exists():
        raise ValueError(f"Canonical simulation input referenced by timeline does not exist: {path}")
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise ValueError(f"Failed to read canonical simulation input: {path}") from exc
    except json.JSONDecodeError as exc:
        raise ValueError(f"Canonical simulation input is not valid JSON: {path}") from exc
    if not isinstance(data, Mapping):
        raise ValueError(f"Canonical simulation input root must be a mapping: {path}")
    return data


def _segment_mapping_from_canonical(index: int, raw_segment: Mapping[str, Any]) -> Mapping[str, Any]:
    raw_type = str(raw_segment.get("type", "unknown")).strip().lower()
    segment_type = raw_type or "unknown"
    if segment_type == "line":
        start_point = _read_point(raw_segment, "start", fallback_prefix="start_")
        end_point = _read_point(raw_segment, "end", fallback_prefix="end_")
        return {
            "segment_index": index,
            "segment_type": "line",
            "start": {"x": start_point.x, "y": start_point.y},
            "end": {"x": end_point.x, "y": end_point.y},
        }
    if segment_type == "arc":
        center = _read_point(raw_segment, "center", fallback_prefix="center_")
        radius = float(raw_segment["radius"])
        start_angle = float(raw_segment["start_angle"])
        end_angle = float(raw_segment["end_angle"])
        sampled_points = _sample_arc_points(raw_segment)
        return {
            "segment_index": index,
            "segment_type": "arc",
            "start": {"x": sampled_points[0].x, "y": sampled_points[0].y},
            "end": {"x": sampled_points[-1].x, "y": sampled_points[-1].y},
            "points": [{"x": point.x, "y": point.y} for point in sampled_points],
            "arc_geometry": {
                "center": {"x": center.x, "y": center.y},
                "radius": radius,
                "start_angle_rad": start_angle,
                "end_angle_rad": end_angle,
            },
        }
    raise ValueError(f"Unsupported canonical segment type: {raw_segment.get('type')!r}")


def _sample_arc_points(raw_segment: Mapping[str, Any], sample_count: int = 9) -> tuple[Point2D, ...]:
    center = _read_point(raw_segment, "center", fallback_prefix="center_")
    radius = float(raw_segment["radius"])
    if radius < 0.0:
        raise ValueError("Arc radius must be >= 0.")
    start_angle = float(raw_segment["start_angle"])
    end_angle = float(raw_segment["end_angle"])
    if sample_count < 2:
        sample_count = 2
    return tuple(
        Point2D(
            x=center.x + radius * math.cos(start_angle + (end_angle - start_angle) * step / (sample_count - 1)),
            y=center.y + radius * math.sin(start_angle + (end_angle - start_angle) * step / (sample_count - 1)),
        )
        for step in range(sample_count)
    )


def _normalize_path_segments(segment_sources: tuple[_PathSegmentSource, ...]) -> tuple[PathSegmentObject, ...]:
    segments: list[PathSegmentObject] = []
    for segment_source in segment_sources:
        segment_data = segment_source.segment_data
        segment_index = segment_source.segment_index
        segment_type = _str_value(segment_data, "segment_type", "unknown")
        start_point = _read_point(segment_data, "start", fallback_prefix="start_")
        end_point = _read_point(segment_data, "end", fallback_prefix="end_")
        arc_geometry = _arc_geometry_from_segment_data(segment_data)
        bbox = _bbox_from_segment_data(
            segment_data,
            start_point=start_point,
            end_point=end_point,
            arc_geometry=arc_geometry,
        )
        segments.append(
            PathSegmentObject(
                object_id=_make_object_id("segment", segment_index),
                object_type=ObjectType.PATH_SEGMENT,
                display_label=ensure_display_label(f"Segment {segment_index}"),
                source_refs=ensure_source_refs(segment_source.source_ref),
                segment_index=segment_index,
                segment_type=segment_type,
                start_point=start_point,
                end_point=end_point,
                bbox=bbox,
                arc_geometry=arc_geometry,
            )
        )
    return tuple(segments)


def _normalize_path_points(
    segment_sources: tuple[_PathSegmentSource, ...],
    path_segments: tuple[PathSegmentObject, ...],
) -> tuple[PathPointObject, ...]:
    points: list[PathPointObject] = []
    segment_source_by_index = {segment_source.segment_index: segment_source for segment_source in segment_sources}
    for segment in path_segments:
        segment_source = segment_source_by_index.get(segment.segment_index)
        source_mapping = segment_source.segment_data if segment_source is not None else {}
        source_ref = segment_source.source_ref if segment_source is not None else SourceRef(
            source_name="motion_profile",
            source_path=f"motion_profile[{segment.segment_index}]",
            source_index=segment.segment_index,
        )
        raw_points = source_mapping.get("points")
        if isinstance(raw_points, list) and raw_points:
            for point_index, raw_point in enumerate(raw_points):
                if not isinstance(raw_point, Mapping):
                    raise ValueError("Path point entries must be mappings.")
                point = Point2D(x=float(raw_point["x"]), y=float(raw_point["y"]))
                points.append(
                    PathPointObject(
                        object_id=_make_object_id("point", segment.segment_index, point_index),
                        object_type=ObjectType.PATH_POINT,
                        display_label=ensure_display_label(f"Point {segment.segment_index}:{point_index}"),
                        source_refs=ensure_source_refs(
                            SourceRef(
                                source_name=source_ref.source_name,
                                source_path=f"{source_ref.source_path}.points[{point_index}]",
                                source_index=source_ref.source_index,
                                source_subindex=point_index,
                            )
                        ),
                        parent_segment_id=segment.object_id,
                        point_index=point_index,
                        point=point,
                    )
                )
        else:
            for point_index, point in enumerate((segment.start_point, segment.end_point)):
                points.append(
                    PathPointObject(
                        object_id=_make_object_id("point", segment.segment_index, point_index),
                        object_type=ObjectType.PATH_POINT,
                        display_label=ensure_display_label(f"Point {segment.segment_index}:{point_index}"),
                        source_refs=ensure_source_refs(
                            SourceRef(
                                source_name=source_ref.source_name,
                                source_path=source_ref.source_path,
                                source_index=source_ref.source_index,
                            )
                        ),
                        parent_segment_id=segment.object_id,
                        point_index=point_index,
                        point=point,
                    )
                )
    return tuple(points)


def _normalize_regions(payload: RawRecordingPayload) -> tuple[RegionObject, ...]:
    raw_regions = payload.raw_root.get("regions") or payload.summary.get("regions") or []
    if not isinstance(raw_regions, list):
        return tuple()
    regions: list[RegionObject] = []
    for index, raw_region in enumerate(raw_regions):
        if not isinstance(raw_region, Mapping):
            raise ValueError("Region entries must be mappings.")
        geometry_refs = tuple(str(value) for value in raw_region.get("geometry_refs", ()))
        bbox = None
        raw_bbox = raw_region.get("bbox")
        if isinstance(raw_bbox, Mapping):
            bbox = BBox2D(
                min_x=float(raw_bbox["min_x"]),
                min_y=float(raw_bbox["min_y"]),
                max_x=float(raw_bbox["max_x"]),
                max_y=float(raw_bbox["max_y"]),
            )
        region_key = raw_region.get("region_id") or raw_region.get("name") or index
        regions.append(
            RegionObject(
                object_id=_make_object_id("region", region_key),
                object_type=ObjectType.REGION,
                display_label=ensure_display_label(str(raw_region.get("label") or raw_region.get("name") or f"Region {index}")),
                source_refs=ensure_source_refs(
                    SourceRef(
                        source_name="regions",
                        source_path=f"regions[{index}]",
                        source_index=index,
                    )
                ),
                region_type=str(raw_region.get("region_type", "unknown")),
                geometry_refs=geometry_refs,
                rule_source=str(raw_region.get("rule_source", "unknown")),
                bbox=bbox,
            )
        )
    return tuple(regions)


def _normalize_issues(payload: RawRecordingPayload) -> tuple[IssueObject, ...]:
    raw_issues = payload.summary.get("issues", [])
    if not isinstance(raw_issues, list):
        raise ValueError("summary.issues must be a list when present.")
    issues: list[IssueObject] = []
    for index, raw_issue in enumerate(raw_issues):
        if not isinstance(raw_issue, Mapping):
            raise ValueError("Issue entries must be mappings.")
        issue_key = raw_issue.get("issue_id") or raw_issue.get("issue_category") or index
        issues.append(
            IssueObject(
                object_id=_make_object_id("issue", issue_key, index),
                object_type=ObjectType.ISSUE,
                display_label=ensure_display_label(str(raw_issue.get("title") or raw_issue.get("label") or f"Issue {index}")),
                source_refs=ensure_source_refs(
                    SourceRef(
                        source_name="summary",
                        source_path=f"summary.issues[{index}]",
                        source_index=index,
                    )
                ),
                issue_category=str(raw_issue.get("issue_category", "unknown")),
                issue_level=str(raw_issue.get("issue_level", "unknown")),
                primary_anchor_type=_optional_str(raw_issue.get("primary_anchor_type")),
                issue_state=_optional_str(raw_issue.get("issue_state")),
            )
        )
    return tuple(issues)


def _normalize_alerts(payload: RawRecordingPayload) -> tuple[AlertObject, ...]:
    alerts: list[AlertObject] = []
    explicit_alerts = payload.summary.get("alerts", [])
    if isinstance(explicit_alerts, list):
        for index, raw_alert in enumerate(explicit_alerts):
            if not isinstance(raw_alert, Mapping):
                raise ValueError("Alert entries must be mappings.")
            alert_key = raw_alert.get("alert_id") or raw_alert.get("code") or index
            alerts.append(
                AlertObject(
                    object_id=_make_object_id("alert", "summary", alert_key, index),
                    object_type=ObjectType.ALERT,
                    display_label=ensure_display_label(str(raw_alert.get("label") or raw_alert.get("message") or f"Alert {index}")),
                    source_refs=ensure_source_refs(
                        SourceRef(
                            source_name="summary",
                            source_path=f"summary.alerts[{index}]",
                            source_index=index,
                        )
                    ),
                    alert_time=float(raw_alert.get("alert_time", 0.0)),
                    window_state=_optional_str(raw_alert.get("window_state")),
                )
            )
    for index, entry in enumerate(payload.timeline):
        if not isinstance(entry, Mapping):
            raise ValueError("Timeline entries must be mappings.")
        kind = str(entry.get("kind", "")).lower()
        source = str(entry.get("source", "")).lower()
        if kind != "alert" and source != "alert":
            continue
        tick = entry.get("tick", {})
        time_ns = tick.get("time_ns", entry.get("time_ns", 0))
        alert_key = entry.get("code") or entry.get("message") or index
        alerts.append(
            AlertObject(
                object_id=_make_object_id("alert", "timeline", alert_key, index),
                object_type=ObjectType.ALERT,
                display_label=ensure_display_label(str(entry.get("message") or f"Alert {index}")),
                source_refs=ensure_source_refs(
                    SourceRef(
                        source_name="timeline",
                        source_path=f"timeline[{index}]",
                        source_index=index,
                    )
                ),
                alert_time=float(time_ns) / 1_000_000_000.0,
                window_state=None,
            )
        )
    return tuple(alerts)


def _normalize_time_anchors(
    payload: RawRecordingPayload,
    alerts: tuple[AlertObject, ...],
) -> tuple[TimeAnchorObject, ...]:
    anchors: list[TimeAnchorObject] = []
    for index, entry in enumerate(payload.trace):
        if not isinstance(entry, Mapping):
            raise ValueError("Trace entries must be mappings.")
        tick = entry.get("tick", {})
        time_ns = tick.get("time_ns", entry.get("time_ns"))
        if time_ns is None:
            continue
        anchors.append(
            TimeAnchorObject(
                object_id=_make_object_id("time_anchor", "trace", index),
                object_type=ObjectType.TIME_ANCHOR,
                display_label=ensure_display_label(str(entry.get("field") or f"Trace {index}")),
                source_refs=ensure_source_refs(
                    SourceRef(
                        source_name="trace",
                        source_path=f"trace[{index}]",
                        source_index=index,
                    )
                ),
                anchor_kind=TimeAnchorKind.TIME_POINT,
                time_start=float(time_ns) / 1_000_000_000.0,
                time_end=None,
                source_kind="trace",
            )
        )
    for index, alert in enumerate(alerts):
        anchors.append(
            TimeAnchorObject(
                object_id=_make_object_id("time_anchor", "alert", index),
                object_type=ObjectType.TIME_ANCHOR,
                display_label=ensure_display_label(f"Alert Time {index}"),
                source_refs=alert.source_refs,
                anchor_kind=TimeAnchorKind.TIME_POINT,
                time_start=alert.alert_time,
                time_end=None,
                source_kind="alert",
            )
        )
    return tuple(anchors)


def _build_object_index(
    *,
    path_segments: tuple[PathSegmentObject, ...],
    path_points: tuple[PathPointObject, ...],
    regions: tuple[RegionObject, ...],
    issues: tuple[IssueObject, ...],
    alerts: tuple[AlertObject, ...],
    time_anchors: tuple[TimeAnchorObject, ...],
) -> dict[ObjectId, ObserverObjectBase]:
    objects_by_id: dict[ObjectId, ObserverObjectBase] = {}
    for obj in (*path_segments, *path_points, *regions, *issues, *alerts, *time_anchors):
        if obj.object_id in objects_by_id:
            raise ValueError(f"Duplicate ObjectId detected: {obj.object_id}")
        objects_by_id[obj.object_id] = obj
    return objects_by_id


def _build_initial_issue_ids(issues: tuple[IssueObject, ...]) -> tuple[ObjectId, ...]:
    return tuple(issue.object_id for issue in issues)


def _make_object_id(prefix: str, *parts: object) -> ObjectId:
    normalized_parts = ":".join(str(part) for part in parts)
    return ObjectId(f"{prefix}:{normalized_parts}")


def _read_point(segment_data: Mapping[str, Any], key: str, *, fallback_prefix: str) -> Point2D:
    raw_point = segment_data.get(key)
    if isinstance(raw_point, Mapping):
        return Point2D(x=float(raw_point["x"]), y=float(raw_point["y"]))
    x_key = f"{fallback_prefix}x"
    y_key = f"{fallback_prefix}y"
    if x_key in segment_data and y_key in segment_data:
        return Point2D(x=float(segment_data[x_key]), y=float(segment_data[y_key]))
    raise ValueError(f"Missing point definition for {key}.")


def _bbox_from_segment_data(
    segment_data: Mapping[str, Any],
    *,
    start_point: Point2D,
    end_point: Point2D,
    arc_geometry: ArcGeometry | None,
) -> BBox2D:
    if arc_geometry is not None:
        return _bbox_from_arc_geometry(arc_geometry)
    raw_points = segment_data.get("points")
    if isinstance(raw_points, list) and raw_points:
        sampled_points: list[Point2D] = []
        for raw_point in raw_points:
            if not isinstance(raw_point, Mapping):
                raise ValueError("Path point entries must be mappings.")
            sampled_points.append(Point2D(x=float(raw_point["x"]), y=float(raw_point["y"])))
        return _bbox_from_points(*sampled_points)
    return _bbox_from_points(start_point, end_point)


def _bbox_from_points(*points: Point2D) -> BBox2D:
    return BBox2D(
        min_x=min(point.x for point in points),
        min_y=min(point.y for point in points),
        max_x=max(point.x for point in points),
        max_y=max(point.y for point in points),
    )


def _bbox_from_arc_geometry(arc_geometry: ArcGeometry) -> BBox2D:
    candidate_angles = [0.0, math.pi * 0.5, math.pi, math.pi * 1.5]
    sweep_rad = arc_geometry.end_angle_rad - arc_geometry.start_angle_rad
    points = [
        _point_on_arc(arc_geometry, arc_geometry.start_angle_rad),
        _point_on_arc(arc_geometry, arc_geometry.end_angle_rad),
    ]
    for angle in candidate_angles:
        if _angle_lies_on_arc(angle, arc_geometry.start_angle_rad, sweep_rad):
            points.append(_point_on_arc(arc_geometry, angle))
    return _bbox_from_points(*points)


def _point_on_arc(arc_geometry: ArcGeometry, angle_rad: float) -> Point2D:
    return Point2D(
        x=arc_geometry.center.x + arc_geometry.radius * math.cos(angle_rad),
        y=arc_geometry.center.y + arc_geometry.radius * math.sin(angle_rad),
    )


def _angle_lies_on_arc(angle_rad: float, start_angle_rad: float, sweep_rad: float) -> bool:
    full_turn = math.tau
    if abs(sweep_rad) >= full_turn:
        return True
    if math.isclose(sweep_rad, 0.0, abs_tol=1e-9):
        return math.isclose(_normalize_angle(angle_rad), _normalize_angle(start_angle_rad), abs_tol=1e-9)

    start = _normalize_angle(start_angle_rad)
    target = _normalize_angle(angle_rad)
    if sweep_rad > 0.0:
        delta = (target - start) % full_turn
        return delta <= sweep_rad + 1e-9
    delta = (start - target) % full_turn
    return delta <= abs(sweep_rad) + 1e-9


def _normalize_angle(angle_rad: float) -> float:
    return angle_rad % math.tau


def _arc_geometry_from_segment_data(segment_data: Mapping[str, Any]) -> ArcGeometry | None:
    raw_arc_geometry = segment_data.get("arc_geometry")
    if isinstance(raw_arc_geometry, Mapping):
        return _arc_geometry_from_mapping(raw_arc_geometry)

    has_direct_arc_keys = any(
        key in segment_data
        for key in (
            "center",
            "center_x",
            "center_y",
            "radius",
            "start_angle",
            "end_angle",
            "start_angle_rad",
            "end_angle_rad",
        )
    )
    if not has_direct_arc_keys:
        return None
    return _arc_geometry_from_mapping(segment_data)


def _arc_geometry_from_mapping(data: Mapping[str, Any]) -> ArcGeometry:
    center = _read_point(data, "center", fallback_prefix="center_")
    radius = float(data["radius"])
    start_angle = _optional_float(data, "start_angle_rad")
    if start_angle is None:
        start_angle = _optional_float(data, "start_angle")
    end_angle = _optional_float(data, "end_angle_rad")
    if end_angle is None:
        end_angle = _optional_float(data, "end_angle")
    if start_angle is None or end_angle is None:
        raise ValueError("Arc geometry requires start/end angles.")
    return ArcGeometry(
        center=center,
        radius=radius,
        start_angle_rad=start_angle,
        end_angle_rad=end_angle,
    )


def _int_value(data: Mapping[str, Any], key: str, default: int) -> int:
    return int(data.get(key, default))


def _str_value(data: Mapping[str, Any], key: str, default: str) -> str:
    text = str(data.get(key, default))
    return text or default


def _optional_str(value: Any) -> str | None:
    if value is None:
        return None
    text = str(value)
    return text or None


def _optional_float(data: Mapping[str, Any], key: str) -> float | None:
    value = data.get(key)
    if value is None:
        return None
    return float(value)


__all__ = [
    "NormalizedObserverData",
    "normalize_recording",
]
