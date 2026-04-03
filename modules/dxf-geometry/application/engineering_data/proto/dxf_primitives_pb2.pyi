from __future__ import annotations

from typing import Any, Generic, TypeVar, overload


_T = TypeVar("_T")


class _RepeatedCompositeField(list[_T], Generic[_T]):
    def append(self, value: _T) -> None: ...
    def add(self) -> _T: ...
    @overload
    def __getitem__(self, index: int) -> _T: ...
    @overload
    def __getitem__(self, index: slice) -> list[_T]: ...


class Point2D:
    x: float
    y: float
    def __init__(self, *, x: float = ..., y: float = ...) -> None: ...
    def CopyFrom(self, other: Point2D) -> None: ...


class LinePrimitive:
    start: Point2D
    end: Point2D
    def __init__(self) -> None: ...
    def CopyFrom(self, other: LinePrimitive) -> None: ...


class ArcPrimitive:
    center: Point2D
    radius: float
    start_angle_deg: float
    end_angle_deg: float
    clockwise: bool
    def __init__(self) -> None: ...
    def CopyFrom(self, other: ArcPrimitive) -> None: ...


class SplinePrimitive:
    control_points: _RepeatedCompositeField[Point2D]
    closed: bool
    def __init__(self) -> None: ...
    def CopyFrom(self, other: SplinePrimitive) -> None: ...


class CirclePrimitive:
    center: Point2D
    radius: float
    start_angle_deg: float
    clockwise: bool
    def __init__(self) -> None: ...
    def CopyFrom(self, other: CirclePrimitive) -> None: ...


class EllipsePrimitive:
    center: Point2D
    major_axis: Point2D
    ratio: float
    start_param: float
    end_param: float
    clockwise: bool
    def __init__(self) -> None: ...
    def CopyFrom(self, other: EllipsePrimitive) -> None: ...


class PointPrimitive:
    position: Point2D
    def __init__(self) -> None: ...
    def CopyFrom(self, other: PointPrimitive) -> None: ...


class ContourElement:
    type: int
    line: LinePrimitive
    arc: ArcPrimitive
    spline: SplinePrimitive
    def __init__(self) -> None: ...
    def CopyFrom(self, other: ContourElement) -> None: ...


class ContourPrimitive:
    elements: _RepeatedCompositeField[ContourElement]
    closed: bool
    def __init__(self) -> None: ...
    def CopyFrom(self, other: ContourPrimitive) -> None: ...


class Primitive:
    type: int
    line: LinePrimitive
    arc: ArcPrimitive
    spline: SplinePrimitive
    circle: CirclePrimitive
    ellipse: EllipsePrimitive
    point: PointPrimitive
    contour: ContourPrimitive
    def __init__(self) -> None: ...
    def CopyFrom(self, other: Primitive) -> None: ...


class PrimitiveMeta:
    entity_id: int
    entity_type: int
    entity_segment_index: int
    entity_closed: bool
    layer: str
    color: int
    def __init__(self) -> None: ...
    def CopyFrom(self, other: PrimitiveMeta) -> None: ...


class ToleranceSettings:
    chordal: float
    max_seg: float
    snap: float
    angular: float
    min_seg: float
    def __init__(self) -> None: ...
    def CopyFrom(self, other: ToleranceSettings) -> None: ...


class PathHeader:
    schema_version: int
    source_path: str
    units: str
    unit_scale: float
    tolerance: ToleranceSettings
    def __init__(self) -> None: ...
    def CopyFrom(self, other: PathHeader) -> None: ...


class PathBundle:
    header: PathHeader
    primitives: _RepeatedCompositeField[Primitive]
    metadata: _RepeatedCompositeField[PrimitiveMeta]
    def __init__(self) -> None: ...
    def CopyFrom(self, other: PathBundle) -> None: ...
    def ParseFromString(self, data: bytes) -> int: ...
    def SerializeToString(self) -> bytes: ...


PRIMITIVE_LINE: int
PRIMITIVE_ARC: int
PRIMITIVE_SPLINE: int
PRIMITIVE_CIRCLE: int
PRIMITIVE_ELLIPSE: int
PRIMITIVE_POINT: int
PRIMITIVE_CONTOUR: int

CONTOUR_LINE: int
CONTOUR_ARC: int
CONTOUR_SPLINE: int

DXF_ENTITY_UNKNOWN: int
DXF_ENTITY_LINE: int
DXF_ENTITY_ARC: int
DXF_ENTITY_CIRCLE: int
DXF_ENTITY_LWPOLYLINE: int
DXF_ENTITY_SPLINE: int
DXF_ENTITY_ELLIPSE: int
DXF_ENTITY_POINT: int

DESCRIPTOR: Any
