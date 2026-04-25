from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Iterable as _Iterable, Mapping as _Mapping, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class PrimitiveType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    PRIMITIVE_LINE: _ClassVar[PrimitiveType]
    PRIMITIVE_ARC: _ClassVar[PrimitiveType]
    PRIMITIVE_SPLINE: _ClassVar[PrimitiveType]
    PRIMITIVE_CIRCLE: _ClassVar[PrimitiveType]
    PRIMITIVE_ELLIPSE: _ClassVar[PrimitiveType]
    PRIMITIVE_POINT: _ClassVar[PrimitiveType]
    PRIMITIVE_CONTOUR: _ClassVar[PrimitiveType]

class ContourElementType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    CONTOUR_LINE: _ClassVar[ContourElementType]
    CONTOUR_ARC: _ClassVar[ContourElementType]
    CONTOUR_SPLINE: _ClassVar[ContourElementType]

class DxfEntityType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    DXF_ENTITY_UNKNOWN: _ClassVar[DxfEntityType]
    DXF_ENTITY_LINE: _ClassVar[DxfEntityType]
    DXF_ENTITY_ARC: _ClassVar[DxfEntityType]
    DXF_ENTITY_CIRCLE: _ClassVar[DxfEntityType]
    DXF_ENTITY_LWPOLYLINE: _ClassVar[DxfEntityType]
    DXF_ENTITY_SPLINE: _ClassVar[DxfEntityType]
    DXF_ENTITY_ELLIPSE: _ClassVar[DxfEntityType]
    DXF_ENTITY_POINT: _ClassVar[DxfEntityType]
PRIMITIVE_LINE: PrimitiveType
PRIMITIVE_ARC: PrimitiveType
PRIMITIVE_SPLINE: PrimitiveType
PRIMITIVE_CIRCLE: PrimitiveType
PRIMITIVE_ELLIPSE: PrimitiveType
PRIMITIVE_POINT: PrimitiveType
PRIMITIVE_CONTOUR: PrimitiveType
CONTOUR_LINE: ContourElementType
CONTOUR_ARC: ContourElementType
CONTOUR_SPLINE: ContourElementType
DXF_ENTITY_UNKNOWN: DxfEntityType
DXF_ENTITY_LINE: DxfEntityType
DXF_ENTITY_ARC: DxfEntityType
DXF_ENTITY_CIRCLE: DxfEntityType
DXF_ENTITY_LWPOLYLINE: DxfEntityType
DXF_ENTITY_SPLINE: DxfEntityType
DXF_ENTITY_ELLIPSE: DxfEntityType
DXF_ENTITY_POINT: DxfEntityType

class Point2D(_message.Message):
    __slots__ = ("x", "y")
    X_FIELD_NUMBER: _ClassVar[int]
    Y_FIELD_NUMBER: _ClassVar[int]
    x: float
    y: float
    def __init__(self, x: _Optional[float] = ..., y: _Optional[float] = ...) -> None: ...

class LinePrimitive(_message.Message):
    __slots__ = ("start", "end")
    START_FIELD_NUMBER: _ClassVar[int]
    END_FIELD_NUMBER: _ClassVar[int]
    start: Point2D
    end: Point2D
    def __init__(self, start: _Optional[_Union[Point2D, _Mapping]] = ..., end: _Optional[_Union[Point2D, _Mapping]] = ...) -> None: ...

class ArcPrimitive(_message.Message):
    __slots__ = ("center", "radius", "start_angle_deg", "end_angle_deg", "clockwise")
    CENTER_FIELD_NUMBER: _ClassVar[int]
    RADIUS_FIELD_NUMBER: _ClassVar[int]
    START_ANGLE_DEG_FIELD_NUMBER: _ClassVar[int]
    END_ANGLE_DEG_FIELD_NUMBER: _ClassVar[int]
    CLOCKWISE_FIELD_NUMBER: _ClassVar[int]
    center: Point2D
    radius: float
    start_angle_deg: float
    end_angle_deg: float
    clockwise: bool
    def __init__(self, center: _Optional[_Union[Point2D, _Mapping]] = ..., radius: _Optional[float] = ..., start_angle_deg: _Optional[float] = ..., end_angle_deg: _Optional[float] = ..., clockwise: bool = ...) -> None: ...

class SplinePrimitive(_message.Message):
    __slots__ = ("control_points", "closed")
    CONTROL_POINTS_FIELD_NUMBER: _ClassVar[int]
    CLOSED_FIELD_NUMBER: _ClassVar[int]
    control_points: _containers.RepeatedCompositeFieldContainer[Point2D]
    closed: bool
    def __init__(self, control_points: _Optional[_Iterable[_Union[Point2D, _Mapping]]] = ..., closed: bool = ...) -> None: ...

class CirclePrimitive(_message.Message):
    __slots__ = ("center", "radius", "start_angle_deg", "clockwise")
    CENTER_FIELD_NUMBER: _ClassVar[int]
    RADIUS_FIELD_NUMBER: _ClassVar[int]
    START_ANGLE_DEG_FIELD_NUMBER: _ClassVar[int]
    CLOCKWISE_FIELD_NUMBER: _ClassVar[int]
    center: Point2D
    radius: float
    start_angle_deg: float
    clockwise: bool
    def __init__(self, center: _Optional[_Union[Point2D, _Mapping]] = ..., radius: _Optional[float] = ..., start_angle_deg: _Optional[float] = ..., clockwise: bool = ...) -> None: ...

class EllipsePrimitive(_message.Message):
    __slots__ = ("center", "major_axis", "ratio", "start_param", "end_param", "clockwise")
    CENTER_FIELD_NUMBER: _ClassVar[int]
    MAJOR_AXIS_FIELD_NUMBER: _ClassVar[int]
    RATIO_FIELD_NUMBER: _ClassVar[int]
    START_PARAM_FIELD_NUMBER: _ClassVar[int]
    END_PARAM_FIELD_NUMBER: _ClassVar[int]
    CLOCKWISE_FIELD_NUMBER: _ClassVar[int]
    center: Point2D
    major_axis: Point2D
    ratio: float
    start_param: float
    end_param: float
    clockwise: bool
    def __init__(self, center: _Optional[_Union[Point2D, _Mapping]] = ..., major_axis: _Optional[_Union[Point2D, _Mapping]] = ..., ratio: _Optional[float] = ..., start_param: _Optional[float] = ..., end_param: _Optional[float] = ..., clockwise: bool = ...) -> None: ...

class PointPrimitive(_message.Message):
    __slots__ = ("position",)
    POSITION_FIELD_NUMBER: _ClassVar[int]
    position: Point2D
    def __init__(self, position: _Optional[_Union[Point2D, _Mapping]] = ...) -> None: ...

class ContourElement(_message.Message):
    __slots__ = ("type", "line", "arc", "spline")
    TYPE_FIELD_NUMBER: _ClassVar[int]
    LINE_FIELD_NUMBER: _ClassVar[int]
    ARC_FIELD_NUMBER: _ClassVar[int]
    SPLINE_FIELD_NUMBER: _ClassVar[int]
    type: ContourElementType
    line: LinePrimitive
    arc: ArcPrimitive
    spline: SplinePrimitive
    def __init__(self, type: _Optional[_Union[ContourElementType, str]] = ..., line: _Optional[_Union[LinePrimitive, _Mapping]] = ..., arc: _Optional[_Union[ArcPrimitive, _Mapping]] = ..., spline: _Optional[_Union[SplinePrimitive, _Mapping]] = ...) -> None: ...

class ContourPrimitive(_message.Message):
    __slots__ = ("elements", "closed")
    ELEMENTS_FIELD_NUMBER: _ClassVar[int]
    CLOSED_FIELD_NUMBER: _ClassVar[int]
    elements: _containers.RepeatedCompositeFieldContainer[ContourElement]
    closed: bool
    def __init__(self, elements: _Optional[_Iterable[_Union[ContourElement, _Mapping]]] = ..., closed: bool = ...) -> None: ...

class Primitive(_message.Message):
    __slots__ = ("type", "line", "arc", "spline", "circle", "ellipse", "point", "contour")
    TYPE_FIELD_NUMBER: _ClassVar[int]
    LINE_FIELD_NUMBER: _ClassVar[int]
    ARC_FIELD_NUMBER: _ClassVar[int]
    SPLINE_FIELD_NUMBER: _ClassVar[int]
    CIRCLE_FIELD_NUMBER: _ClassVar[int]
    ELLIPSE_FIELD_NUMBER: _ClassVar[int]
    POINT_FIELD_NUMBER: _ClassVar[int]
    CONTOUR_FIELD_NUMBER: _ClassVar[int]
    type: PrimitiveType
    line: LinePrimitive
    arc: ArcPrimitive
    spline: SplinePrimitive
    circle: CirclePrimitive
    ellipse: EllipsePrimitive
    point: PointPrimitive
    contour: ContourPrimitive
    def __init__(self, type: _Optional[_Union[PrimitiveType, str]] = ..., line: _Optional[_Union[LinePrimitive, _Mapping]] = ..., arc: _Optional[_Union[ArcPrimitive, _Mapping]] = ..., spline: _Optional[_Union[SplinePrimitive, _Mapping]] = ..., circle: _Optional[_Union[CirclePrimitive, _Mapping]] = ..., ellipse: _Optional[_Union[EllipsePrimitive, _Mapping]] = ..., point: _Optional[_Union[PointPrimitive, _Mapping]] = ..., contour: _Optional[_Union[ContourPrimitive, _Mapping]] = ...) -> None: ...

class PrimitiveMeta(_message.Message):
    __slots__ = ("entity_id", "entity_type", "entity_segment_index", "entity_closed", "layer", "color")
    ENTITY_ID_FIELD_NUMBER: _ClassVar[int]
    ENTITY_TYPE_FIELD_NUMBER: _ClassVar[int]
    ENTITY_SEGMENT_INDEX_FIELD_NUMBER: _ClassVar[int]
    ENTITY_CLOSED_FIELD_NUMBER: _ClassVar[int]
    LAYER_FIELD_NUMBER: _ClassVar[int]
    COLOR_FIELD_NUMBER: _ClassVar[int]
    entity_id: int
    entity_type: DxfEntityType
    entity_segment_index: int
    entity_closed: bool
    layer: str
    color: int
    def __init__(self, entity_id: _Optional[int] = ..., entity_type: _Optional[_Union[DxfEntityType, str]] = ..., entity_segment_index: _Optional[int] = ..., entity_closed: bool = ..., layer: _Optional[str] = ..., color: _Optional[int] = ...) -> None: ...

class ToleranceSettings(_message.Message):
    __slots__ = ("chordal", "max_seg", "snap", "angular", "min_seg")
    CHORDAL_FIELD_NUMBER: _ClassVar[int]
    MAX_SEG_FIELD_NUMBER: _ClassVar[int]
    SNAP_FIELD_NUMBER: _ClassVar[int]
    ANGULAR_FIELD_NUMBER: _ClassVar[int]
    MIN_SEG_FIELD_NUMBER: _ClassVar[int]
    chordal: float
    max_seg: float
    snap: float
    angular: float
    min_seg: float
    def __init__(self, chordal: _Optional[float] = ..., max_seg: _Optional[float] = ..., snap: _Optional[float] = ..., angular: _Optional[float] = ..., min_seg: _Optional[float] = ...) -> None: ...

class PathHeader(_message.Message):
    __slots__ = ("schema_version", "source_path", "units", "unit_scale", "tolerance")
    SCHEMA_VERSION_FIELD_NUMBER: _ClassVar[int]
    SOURCE_PATH_FIELD_NUMBER: _ClassVar[int]
    UNITS_FIELD_NUMBER: _ClassVar[int]
    UNIT_SCALE_FIELD_NUMBER: _ClassVar[int]
    TOLERANCE_FIELD_NUMBER: _ClassVar[int]
    schema_version: int
    source_path: str
    units: str
    unit_scale: float
    tolerance: ToleranceSettings
    def __init__(self, schema_version: _Optional[int] = ..., source_path: _Optional[str] = ..., units: _Optional[str] = ..., unit_scale: _Optional[float] = ..., tolerance: _Optional[_Union[ToleranceSettings, _Mapping]] = ...) -> None: ...

class PathBundle(_message.Message):
    __slots__ = ("header", "primitives", "metadata")
    HEADER_FIELD_NUMBER: _ClassVar[int]
    PRIMITIVES_FIELD_NUMBER: _ClassVar[int]
    METADATA_FIELD_NUMBER: _ClassVar[int]
    header: PathHeader
    primitives: _containers.RepeatedCompositeFieldContainer[Primitive]
    metadata: _containers.RepeatedCompositeFieldContainer[PrimitiveMeta]
    def __init__(self, header: _Optional[_Union[PathHeader, _Mapping]] = ..., primitives: _Optional[_Iterable[_Union[Primitive, _Mapping]]] = ..., metadata: _Optional[_Iterable[_Union[PrimitiveMeta, _Mapping]]] = ...) -> None: ...
