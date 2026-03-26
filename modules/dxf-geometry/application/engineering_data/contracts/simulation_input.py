from __future__ import annotations

import csv
import json
import math
from dataclasses import dataclass
from pathlib import Path
from typing import List, Sequence

from engineering_data.proto import dxf_primitives_pb2 as pb


_TWO_PI = math.pi * 2.0
R12_PRIMARY_PRIMITIVE_TYPES = {
    pb.PRIMITIVE_LINE,
    pb.PRIMITIVE_ARC,
    pb.PRIMITIVE_CIRCLE,
    pb.PRIMITIVE_CONTOUR,
    pb.PRIMITIVE_POINT,
}
FALLBACK_PRIMITIVE_TYPES = {
    pb.PRIMITIVE_SPLINE,
    pb.PRIMITIVE_ELLIPSE,
}


@dataclass(frozen=True)
class IoDelay:
    channel: str
    delay_seconds: float


@dataclass(frozen=True)
class ValveConfig:
    open_delay_seconds: float
    close_delay_seconds: float


@dataclass(frozen=True)
class ExportDefaults:
    timestep_seconds: float = 0.001
    max_simulation_time_seconds: float = 600.0
    max_velocity: float = 220.0
    max_acceleration: float = 600.0
    io_delays: tuple[IoDelay, ...] = (
        IoDelay(channel="DO_VALVE", delay_seconds=0.003),
        IoDelay(channel="DO_CAMERA_TRIGGER", delay_seconds=0.001),
    )
    valve: ValveConfig = ValveConfig(
        open_delay_seconds=0.012,
        close_delay_seconds=0.008,
    )


DEFAULT_EXPORTS = ExportDefaults()


@dataclass(frozen=True)
class TriggerPoint:
    x_mm: float
    y_mm: float
    channel: str
    state: bool


def _point_to_dict(point: pb.Point2D) -> dict:
    return {"x": float(point.x), "y": float(point.y)}


def _deg_to_rad(value: float) -> float:
    return math.radians(float(value))


def _normalize_span(start_rad: float, end_rad: float, clockwise: bool) -> tuple[float, float]:
    delta = end_rad - start_rad
    if clockwise:
        if delta >= 0.0:
            end_rad -= _TWO_PI
    elif delta <= 0.0:
        end_rad += _TWO_PI
    return start_rad, end_rad


def _angle_within_span(angle: float, start: float, end: float) -> bool:
    if end >= start:
        while angle < start:
            angle += _TWO_PI
        return angle <= end + 1e-9
    while angle > start:
        angle -= _TWO_PI
    return angle >= end - 1e-9


def _arc_to_segments(center: pb.Point2D,
                     radius: float,
                     start_angle_deg: float,
                     end_angle_deg: float,
                     clockwise: bool) -> List[dict]:
    start_rad, end_rad = _normalize_span(
        _deg_to_rad(start_angle_deg),
        _deg_to_rad(end_angle_deg),
        clockwise,
    )
    return [{
        "type": "ARC",
        "center": _point_to_dict(center),
        "radius": float(radius),
        "start_angle": start_rad,
        "end_angle": end_rad,
    }]


def _circle_to_segments(circle: pb.CirclePrimitive) -> List[dict]:
    start_rad = _deg_to_rad(circle.start_angle_deg)
    half_turn = math.pi
    return [
        {
            "type": "ARC",
            "center": _point_to_dict(circle.center),
            "radius": float(circle.radius),
            "start_angle": start_rad,
            "end_angle": start_rad + half_turn,
        },
        {
            "type": "ARC",
            "center": _point_to_dict(circle.center),
            "radius": float(circle.radius),
            "start_angle": start_rad + half_turn,
            "end_angle": start_rad + (_TWO_PI),
        },
    ]


def _line_to_segments(line: pb.LinePrimitive) -> List[dict]:
    return [{
        "type": "LINE",
        "start": _point_to_dict(line.start),
        "end": _point_to_dict(line.end),
    }]


def _sample_parametric_ellipse(ellipse: pb.EllipsePrimitive, max_seg: float) -> List[tuple[float, float]]:
    major_x = float(ellipse.major_axis.x)
    major_y = float(ellipse.major_axis.y)
    major_len = math.hypot(major_x, major_y)
    if major_len <= 1e-9:
        return []

    minor_x = -major_y * float(ellipse.ratio)
    minor_y = major_x * float(ellipse.ratio)
    start_param = float(ellipse.start_param)
    end_param = float(ellipse.end_param)
    span = end_param - start_param
    if span <= 0.0:
        span += _TWO_PI

    approx_length = max(major_len, major_len * abs(float(ellipse.ratio))) * abs(span)
    segment_count = max(8, int(math.ceil(approx_length / max(max_seg, 0.5))))
    points: List[tuple[float, float]] = []
    for index in range(segment_count + 1):
        t = start_param + (span * index / segment_count)
        x = float(ellipse.center.x) + (major_x * math.cos(t)) + (minor_x * math.sin(t))
        y = float(ellipse.center.y) + (major_y * math.cos(t)) + (minor_y * math.sin(t))
        points.append((x, y))
    return points


def _polyline_points_to_segments(points: Sequence[tuple[float, float]]) -> List[dict]:
    segments: List[dict] = []
    for index in range(1, len(points)):
        start = points[index - 1]
        end = points[index]
        if math.hypot(end[0] - start[0], end[1] - start[1]) <= 1e-9:
            continue
        segments.append({
            "type": "LINE",
            "start": {"x": float(start[0]), "y": float(start[1])},
            "end": {"x": float(end[0]), "y": float(end[1])},
        })
    return segments


def _spline_to_segments(spline: pb.SplinePrimitive) -> List[dict]:
    points = [(float(point.x), float(point.y)) for point in spline.control_points]
    return _polyline_points_to_segments(points)


def _ellipse_to_segments(ellipse: pb.EllipsePrimitive, max_seg: float) -> List[dict]:
    return _polyline_points_to_segments(_sample_parametric_ellipse(ellipse, max_seg))


def _contour_to_segments(contour: pb.ContourPrimitive, max_seg: float) -> List[dict]:
    segments: List[dict] = []
    for element in contour.elements:
        if element.type == pb.CONTOUR_LINE:
            segments.extend(_line_to_segments(element.line))
        elif element.type == pb.CONTOUR_ARC:
            segments.extend(_arc_to_segments(
                center=element.arc.center,
                radius=element.arc.radius,
                start_angle_deg=element.arc.start_angle_deg,
                end_angle_deg=element.arc.end_angle_deg,
                clockwise=bool(element.arc.clockwise),
            ))
        elif element.type == pb.CONTOUR_SPLINE:
            segments.extend(_spline_to_segments(element.spline))
        else:
            raise ValueError(f"Unsupported contour element type: {element.type}")
    return segments


def primitive_to_segments_r12(primitive: pb.Primitive, max_seg: float) -> List[dict] | None:
    if primitive.type == pb.PRIMITIVE_LINE:
        return _line_to_segments(primitive.line)
    if primitive.type == pb.PRIMITIVE_ARC:
        return _arc_to_segments(
            center=primitive.arc.center,
            radius=primitive.arc.radius,
            start_angle_deg=primitive.arc.start_angle_deg,
            end_angle_deg=primitive.arc.end_angle_deg,
            clockwise=bool(primitive.arc.clockwise),
        )
    if primitive.type == pb.PRIMITIVE_CIRCLE:
        return _circle_to_segments(primitive.circle)
    if primitive.type == pb.PRIMITIVE_CONTOUR:
        return _contour_to_segments(primitive.contour, max_seg)
    if primitive.type == pb.PRIMITIVE_POINT:
        return []
    return None


def primitive_to_segments_fallback(primitive: pb.Primitive, max_seg: float) -> List[dict] | None:
    if primitive.type == pb.PRIMITIVE_SPLINE:
        return _spline_to_segments(primitive.spline)
    if primitive.type == pb.PRIMITIVE_ELLIPSE:
        return _ellipse_to_segments(primitive.ellipse, max_seg)
    return None


def primitive_to_segments(primitive: pb.Primitive, max_seg: float) -> List[dict]:
    r12_segments = primitive_to_segments_r12(primitive, max_seg)
    if r12_segments is not None:
        return r12_segments
    fallback_segments = primitive_to_segments_fallback(primitive, max_seg)
    if fallback_segments is not None:
        return fallback_segments
    raise ValueError(f"Unsupported primitive type: {primitive.type}")


@dataclass(frozen=True)
class _PathProjectionLine:
    start: tuple[float, float]
    end: tuple[float, float]
    length_offset: float
    length_value: float

    def nearest_position(self, point: tuple[float, float]) -> tuple[float, float]:
        dx = self.end[0] - self.start[0]
        dy = self.end[1] - self.start[1]
        length_sq = (dx * dx) + (dy * dy)
        if length_sq <= 1e-12:
            return self.length_offset, math.hypot(point[0] - self.start[0], point[1] - self.start[1])
        ratio = ((point[0] - self.start[0]) * dx + (point[1] - self.start[1]) * dy) / length_sq
        ratio = max(0.0, min(1.0, ratio))
        proj_x = self.start[0] + (dx * ratio)
        proj_y = self.start[1] + (dy * ratio)
        distance = math.hypot(point[0] - proj_x, point[1] - proj_y)
        return self.length_offset + (self.length_value * ratio), distance


@dataclass(frozen=True)
class _PathProjectionArc:
    center: tuple[float, float]
    radius: float
    start_angle: float
    end_angle: float
    length_offset: float

    def _candidate_angles(self, target_angle: float) -> List[float]:
        angles = [self.start_angle, self.end_angle]
        candidate = target_angle
        if self.end_angle >= self.start_angle:
            while candidate < self.start_angle:
                candidate += _TWO_PI
            while candidate > self.end_angle:
                candidate -= _TWO_PI
        else:
            while candidate > self.start_angle:
                candidate -= _TWO_PI
            while candidate < self.end_angle:
                candidate += _TWO_PI
        if _angle_within_span(candidate, self.start_angle, self.end_angle):
            angles.append(candidate)
        return angles

    def nearest_position(self, point: tuple[float, float]) -> tuple[float, float]:
        dx = point[0] - self.center[0]
        dy = point[1] - self.center[1]
        target_angle = math.atan2(dy, dx)
        best_position = self.length_offset
        best_distance = float("inf")
        total_length = abs(self.end_angle - self.start_angle) * self.radius

        for candidate in self._candidate_angles(target_angle):
            radial_x = self.center[0] + (self.radius * math.cos(candidate))
            radial_y = self.center[1] + (self.radius * math.sin(candidate))
            distance = math.hypot(point[0] - radial_x, point[1] - radial_y)
            travelled = abs(candidate - self.start_angle) * self.radius
            travelled = min(max(travelled, 0.0), total_length)
            if distance < best_distance:
                best_distance = distance
                best_position = self.length_offset + travelled
        return best_position, best_distance


def _build_projection_segments(segments: Sequence[dict]) -> List[object]:
    projection_segments: List[object] = []
    offset = 0.0
    for segment in segments:
        if segment["type"] == "LINE":
            start = (float(segment["start"]["x"]), float(segment["start"]["y"]))
            end = (float(segment["end"]["x"]), float(segment["end"]["y"]))
            length_value = math.hypot(end[0] - start[0], end[1] - start[1])
            if length_value <= 1e-9:
                continue
            projection_segments.append(_PathProjectionLine(
                start=start,
                end=end,
                length_offset=offset,
                length_value=length_value,
            ))
            offset += length_value
            continue

        center = (float(segment["center"]["x"]), float(segment["center"]["y"]))
        radius = float(segment["radius"])
        start_angle = float(segment["start_angle"])
        end_angle = float(segment["end_angle"])
        if radius <= 1e-9 or abs(end_angle - start_angle) <= 1e-9:
            continue
        projection_segments.append(_PathProjectionArc(
            center=center,
            radius=radius,
            start_angle=start_angle,
            end_angle=end_angle,
            length_offset=offset,
        ))
        offset += abs(end_angle - start_angle) * radius
    return projection_segments


def project_trigger_points(trigger_points: Sequence[TriggerPoint], segments: Sequence[dict]) -> List[dict]:
    if not trigger_points:
        return []

    projection_segments = _build_projection_segments(segments)
    if not projection_segments:
        return []

    projected: List[dict] = []
    for trigger in trigger_points:
        best_position = 0.0
        best_distance = float("inf")
        trigger_xy = (trigger.x_mm, trigger.y_mm)
        for segment in projection_segments:
            position, distance = segment.nearest_position(trigger_xy)
            if distance < best_distance:
                best_distance = distance
                best_position = position
        projected.append({
            "channel": trigger.channel,
            "trigger_position": best_position,
            "state": bool(trigger.state),
        })

    projected.sort(key=lambda item: (float(item["trigger_position"]), str(item["channel"]), bool(item["state"])))
    return projected


def load_trigger_points_csv(path: Path,
                            default_channel: str,
                            default_state: bool) -> List[TriggerPoint]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        required = {"x_mm", "y_mm"}
        missing = required.difference(reader.fieldnames or [])
        if missing:
            missing_list = ", ".join(sorted(missing))
            raise ValueError(f"Trigger CSV missing required columns: {missing_list}")

        trigger_points: List[TriggerPoint] = []
        for row in reader:
            if row.get("x_mm") in (None, "") or row.get("y_mm") in (None, ""):
                continue
            channel = (row.get("channel") or default_channel).strip() or default_channel
            state_raw = row.get("state")
            if state_raw is None or state_raw == "":
                state = default_state
            else:
                state = state_raw.strip().lower() in {"1", "true", "on", "open", "yes"}
            trigger_points.append(TriggerPoint(
                x_mm=float(row["x_mm"]),
                y_mm=float(row["y_mm"]),
                channel=channel,
                state=state,
            ))
    return trigger_points


def load_path_bundle(path: Path) -> pb.PathBundle:
    bundle = pb.PathBundle()
    bundle.ParseFromString(path.read_bytes())
    return bundle


def bundle_to_simulation_payload(bundle: pb.PathBundle,
                                 defaults: ExportDefaults = DEFAULT_EXPORTS,
                                 trigger_points: Sequence[TriggerPoint] | None = None,
                                 ellipse_max_seg: float = 1.0) -> dict:
    segments: List[dict] = []
    for primitive in bundle.primitives:
        segments.extend(primitive_to_segments(primitive, max_seg=ellipse_max_seg))

    if not segments:
        raise ValueError("No exportable path segments found in PathBundle.")

    triggers = project_trigger_points(trigger_points or [], segments)

    return {
        "timestep_seconds": defaults.timestep_seconds,
        "max_simulation_time_seconds": defaults.max_simulation_time_seconds,
        "max_velocity": defaults.max_velocity,
        "max_acceleration": defaults.max_acceleration,
        "segments": segments,
        "io_delays": [
            {"channel": item.channel, "delay_seconds": item.delay_seconds}
            for item in defaults.io_delays
        ],
        "triggers": triggers,
        "valve": {
            "open_delay_seconds": defaults.valve.open_delay_seconds,
            "close_delay_seconds": defaults.valve.close_delay_seconds,
        },
    }


def write_simulation_payload(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=True, indent=2), encoding="utf-8")


def count_skipped_points(bundle: pb.PathBundle) -> int:
    return sum(1 for primitive in bundle.primitives if primitive.type == pb.PRIMITIVE_POINT)


def count_fallback_primitives(bundle: pb.PathBundle) -> dict[str, int]:
    counts: dict[str, int] = {}
    type_names = {
        pb.PRIMITIVE_SPLINE: "SPLINE",
        pb.PRIMITIVE_ELLIPSE: "ELLIPSE",
    }
    for primitive in bundle.primitives:
        if primitive.type not in FALLBACK_PRIMITIVE_TYPES:
            continue
        name = type_names.get(int(primitive.type), f"TYPE_{primitive.type}")
        counts[name] = counts.get(name, 0) + 1
    return counts


def bundle_contains_fallback_primitives(bundle: pb.PathBundle) -> bool:
    return any(primitive.type in FALLBACK_PRIMITIVE_TYPES for primitive in bundle.primitives)


def collect_export_notes(bundle: pb.PathBundle) -> List[str]:
    notes: List[str] = []
    skipped_points = count_skipped_points(bundle)
    if skipped_points:
        notes.append(f"Skipped {skipped_points} point primitives because the runtime simulation path only accepts path segments.")
    fallback_counts = count_fallback_primitives(bundle)
    if fallback_counts:
        summary = ", ".join(f"{name}={fallback_counts[name]}" for name in sorted(fallback_counts))
        notes.append(f"Used non-R12 fallback primitive conversions for: {summary}.")
    if any(primitive.type == pb.PRIMITIVE_SPLINE for primitive in bundle.primitives):
        notes.append("Spline primitives were linearized into LINE segments.")
    if any(primitive.type == pb.PRIMITIVE_ELLIPSE for primitive in bundle.primitives):
        notes.append("Ellipse primitives were linearized into LINE segments.")
    if any(primitive.type == pb.PRIMITIVE_CIRCLE for primitive in bundle.primitives):
        notes.append("Circle primitives were split into two ARC segments.")
    return notes
