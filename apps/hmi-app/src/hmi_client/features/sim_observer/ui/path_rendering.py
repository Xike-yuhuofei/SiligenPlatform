"""Pure helpers for turning normalized path data into renderable geometry."""

from __future__ import annotations

import math
from dataclasses import dataclass

from ..contracts.observer_types import PathPointObject, PathSegmentObject, Point2D


def segment_render_points(
    segment: PathSegmentObject,
    path_points: tuple[PathPointObject, ...],
) -> tuple[Point2D, ...]:
    ordered_points = tuple(
        path_point.point
        for path_point in sorted(
            (item for item in path_points if item.parent_segment_id == segment.object_id),
            key=lambda item: item.point_index,
        )
    )
    if len(ordered_points) >= 2:
        return ordered_points
    return (segment.start_point, segment.end_point)


@dataclass(frozen=True, slots=True)
class PolylineRenderInstruction:
    points: tuple[Point2D, ...]
    mode: str = "polyline_fallback"


@dataclass(frozen=True, slots=True)
class ArcRenderInstruction:
    center: Point2D
    radius: float
    start_angle_rad: float
    sweep_angle_rad: float
    mode: str = "arc_primitive"


def segment_render_instruction(
    segment: PathSegmentObject,
    path_points: tuple[PathPointObject, ...],
) -> PolylineRenderInstruction | ArcRenderInstruction:
    arc_geometry = segment.arc_geometry
    if arc_geometry is not None and segment.segment_type.strip().lower() == "arc":
        sweep_angle_rad = arc_geometry.end_angle_rad - arc_geometry.start_angle_rad
        if not math.isclose(sweep_angle_rad, 0.0, abs_tol=1e-9):
            return ArcRenderInstruction(
                center=arc_geometry.center,
                radius=arc_geometry.radius,
                start_angle_rad=arc_geometry.start_angle_rad,
                sweep_angle_rad=sweep_angle_rad,
            )
    return PolylineRenderInstruction(points=segment_render_points(segment, path_points))


__all__ = [
    "ArcRenderInstruction",
    "PolylineRenderInstruction",
    "segment_render_instruction",
    "segment_render_points",
]
