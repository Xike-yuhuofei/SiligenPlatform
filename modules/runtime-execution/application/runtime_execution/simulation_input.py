from __future__ import annotations

import csv
import json
import math
from pathlib import Path
from typing import List, Sequence

from runtime_execution.contracts import DEFAULT_EXPORTS, ExportDefaults, TriggerPoint

from engineering_data.processing.simulation_geometry import build_simulation_segments


_TWO_PI = math.pi * 2.0


class _PathProjectionLine:
    def __init__(self, start: tuple[float, float], end: tuple[float, float], length_offset: float, length_value: float) -> None:
        self.start = start
        self.end = end
        self.length_offset = length_offset
        self.length_value = length_value

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


class _PathProjectionArc:
    def __init__(self, center: tuple[float, float], radius: float, start_angle: float, end_angle: float, length_offset: float) -> None:
        self.center = center
        self.radius = radius
        self.start_angle = start_angle
        self.end_angle = end_angle
        self.length_offset = length_offset

    def _angle_within_span(self, angle: float) -> bool:
        start = self.start_angle
        end = self.end_angle
        if end >= start:
            while angle < start:
                angle += _TWO_PI
            return angle <= end + 1e-9
        while angle > start:
            angle -= _TWO_PI
        return angle >= end - 1e-9

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
        if self._angle_within_span(candidate):
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


ProjectionSegment = _PathProjectionLine | _PathProjectionArc


def _build_projection_segments(segments: Sequence[dict]) -> List[ProjectionSegment]:
    projection_segments: List[ProjectionSegment] = []
    offset = 0.0
    for segment in segments:
        if segment["type"] == "LINE":
            start = (float(segment["start"]["x"]), float(segment["start"]["y"]))
            end = (float(segment["end"]["x"]), float(segment["end"]["y"]))
            length_value = math.hypot(end[0] - start[0], end[1] - start[1])
            if length_value <= 1e-9:
                continue
            projection_segments.append(
                _PathProjectionLine(
                    start=start,
                    end=end,
                    length_offset=offset,
                    length_value=length_value,
                )
            )
            offset += length_value
            continue

        center = (float(segment["center"]["x"]), float(segment["center"]["y"]))
        radius = float(segment["radius"])
        start_angle = float(segment["start_angle"])
        end_angle = float(segment["end_angle"])
        if radius <= 1e-9 or abs(end_angle - start_angle) <= 1e-9:
            continue
        projection_segments.append(
            _PathProjectionArc(
                center=center,
                radius=radius,
                start_angle=start_angle,
                end_angle=end_angle,
                length_offset=offset,
            )
        )
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
        projected.append(
            {
                "channel": trigger.channel,
                "trigger_position": best_position,
                "state": bool(trigger.state),
            }
        )

    projected.sort(key=lambda item: (float(item["trigger_position"]), str(item["channel"]), bool(item["state"])))
    return projected


def load_trigger_points_csv(path: Path, default_channel: str, default_state: bool) -> List[TriggerPoint]:
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
            trigger_points.append(
                TriggerPoint(
                    x_mm=float(row["x_mm"]),
                    y_mm=float(row["y_mm"]),
                    channel=channel,
                    state=state,
                )
            )
    return trigger_points


def bundle_to_simulation_payload(bundle, defaults: ExportDefaults = DEFAULT_EXPORTS, trigger_points: Sequence[TriggerPoint] | None = None, ellipse_max_seg: float = 1.0) -> dict:
    segments = build_simulation_segments(bundle=bundle, ellipse_max_seg=ellipse_max_seg)
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
