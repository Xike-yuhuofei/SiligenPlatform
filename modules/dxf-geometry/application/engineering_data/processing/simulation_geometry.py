from __future__ import annotations

import math
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


def _arc_to_segments(center: pb.Point2D, radius: float, start_angle_deg: float, end_angle_deg: float, clockwise: bool) -> List[dict]:
    start_rad, end_rad = _normalize_span(
        _deg_to_rad(start_angle_deg),
        _deg_to_rad(end_angle_deg),
        clockwise,
    )
    return [
        {
            "type": "ARC",
            "center": _point_to_dict(center),
            "radius": float(radius),
            "start_angle": start_rad,
            "end_angle": end_rad,
        }
    ]


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
            "end_angle": start_rad + _TWO_PI,
        },
    ]


def _line_to_segments(line: pb.LinePrimitive) -> List[dict]:
    return [
        {
            "type": "LINE",
            "start": _point_to_dict(line.start),
            "end": _point_to_dict(line.end),
        }
    ]


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
        segments.append(
            {
                "type": "LINE",
                "start": {"x": float(start[0]), "y": float(start[1])},
                "end": {"x": float(end[0]), "y": float(end[1])},
            }
        )
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
            segments.extend(
                _arc_to_segments(
                    center=element.arc.center,
                    radius=element.arc.radius,
                    start_angle_deg=element.arc.start_angle_deg,
                    end_angle_deg=element.arc.end_angle_deg,
                    clockwise=bool(element.arc.clockwise),
                )
            )
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


def load_path_bundle(path: Path) -> pb.PathBundle:
    bundle = pb.PathBundle()
    bundle.ParseFromString(path.read_bytes())
    return bundle


def build_simulation_segments(bundle: pb.PathBundle, ellipse_max_seg: float = 1.0) -> List[dict]:
    segments: List[dict] = []
    for primitive in bundle.primitives:
        segments.extend(primitive_to_segments(primitive, max_seg=ellipse_max_seg))
    return segments


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
        notes.append("Skipped %d point primitives because the runtime simulation path only accepts path segments." % skipped_points)
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
