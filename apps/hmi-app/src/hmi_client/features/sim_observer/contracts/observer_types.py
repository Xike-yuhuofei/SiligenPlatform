"""Core observer object and anchor types.

V1 policy:
- Some business-facing descriptive fields remain plain strings for now.
- If those fields drift across modules, they should be promoted to enums later.
- This module defines static data structures only and must stay free of UI and
  rule-execution logic.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from enum import Enum
from typing import NewType, Optional, Sequence, Tuple


ObjectId = NewType("ObjectId", str)
DisplayLabel = NewType("DisplayLabel", str)


class ObjectType(str, Enum):
    PATH_SEGMENT = "path_segment"
    PATH_POINT = "path_point"
    REGION = "region"
    ISSUE = "issue"
    ALERT = "alert"
    TIME_ANCHOR = "time_anchor"
    OBJECT_GROUP = "object_group"


@dataclass(frozen=True, slots=True)
class SourceRef:
    source_name: str
    source_path: str
    source_index: Optional[int] = None
    source_subindex: Optional[int] = None

    def __post_init__(self) -> None:
        self.validate()

    def validate(self) -> None:
        if not self.source_name:
            raise ValueError("source_name must not be empty.")
        if not self.source_path:
            raise ValueError("source_path must not be empty.")


SourceRefs = Tuple[SourceRef, ...]


@dataclass(frozen=True, slots=True)
class Point2D:
    x: float
    y: float


@dataclass(frozen=True, slots=True)
class ArcGeometry:
    center: Point2D
    radius: float
    start_angle_rad: float
    end_angle_rad: float

    def __post_init__(self) -> None:
        self.validate()

    def validate(self) -> None:
        if not math.isfinite(self.radius) or self.radius <= 0.0:
            raise ValueError("arc radius must be > 0.")
        if not math.isfinite(self.start_angle_rad):
            raise ValueError("arc start_angle_rad must be finite.")
        if not math.isfinite(self.end_angle_rad):
            raise ValueError("arc end_angle_rad must be finite.")


@dataclass(frozen=True, slots=True)
class BBox2D:
    min_x: float
    min_y: float
    max_x: float
    max_y: float

    def __post_init__(self) -> None:
        self.validate()

    def validate(self) -> None:
        if not self.is_valid():
            raise ValueError("Invalid bounding box coordinates.")

    def is_valid(self) -> bool:
        return self.min_x <= self.max_x and self.min_y <= self.max_y


class GroupBasis(str, Enum):
    REGION_DERIVED = "region_derived"
    ISSUE_RELATED = "issue_related"
    TIME_MAPPED = "time_mapped"
    ALERT_CONTEXT = "alert_context"


class RegionRelationType(str, Enum):
    IN_REGION = "in_region"
    INTERSECTS_REGION = "intersects_region"
    BOUNDARY_TOUCH = "boundary_touch"


class TimeAnchorKind(str, Enum):
    TIME_POINT = "time_point"
    TIME_RANGE = "time_range"


@dataclass(frozen=True, slots=True, kw_only=True)
class ObserverObjectBase:
    object_id: ObjectId
    object_type: ObjectType
    display_label: DisplayLabel
    source_refs: SourceRefs = field(default_factory=tuple)

    def __post_init__(self) -> None:
        self.validate()

    def validate(self) -> None:
        if not str(self.object_id):
            raise ValueError("object_id must not be empty.")
        if not isinstance(self.object_type, ObjectType):
            raise ValueError("object_type must be an ObjectType.")
        if not str(self.display_label):
            raise ValueError("display_label must not be empty.")
        ensure_source_refs(self.source_refs)


@dataclass(frozen=True, slots=True, kw_only=True)
class PathSegmentObject(ObserverObjectBase):
    segment_index: int
    segment_type: str
    start_point: Point2D
    end_point: Point2D
    bbox: BBox2D
    arc_geometry: Optional[ArcGeometry] = None

    def validate(self) -> None:
        ObserverObjectBase.validate(self)
        if self.object_type is not ObjectType.PATH_SEGMENT:
            raise ValueError("PathSegmentObject must use PATH_SEGMENT type.")
        if self.segment_index < 0:
            raise ValueError("segment_index must be >= 0.")
        if not self.segment_type:
            raise ValueError("segment_type must not be empty.")
        self.bbox.validate()
        if self.arc_geometry is not None:
            if self.segment_type.strip().lower() != "arc":
                raise ValueError("arc_geometry is only valid when segment_type is 'arc'.")
            self.arc_geometry.validate()


@dataclass(frozen=True, slots=True, kw_only=True)
class PathPointObject(ObserverObjectBase):
    parent_segment_id: ObjectId
    point_index: int
    point: Point2D

    def validate(self) -> None:
        ObserverObjectBase.validate(self)
        if self.object_type is not ObjectType.PATH_POINT:
            raise ValueError("PathPointObject must use PATH_POINT type.")
        if not str(self.parent_segment_id):
            raise ValueError("parent_segment_id must not be empty.")
        if self.point_index < 0:
            raise ValueError("point_index must be >= 0.")


@dataclass(frozen=True, slots=True, kw_only=True)
class RegionObject(ObserverObjectBase):
    region_type: str
    geometry_refs: Tuple[str, ...]
    rule_source: str
    bbox: Optional[BBox2D] = None

    def validate(self) -> None:
        ObserverObjectBase.validate(self)
        if self.object_type is not ObjectType.REGION:
            raise ValueError("RegionObject must use REGION type.")
        if not self.region_type:
            raise ValueError("region_type must not be empty.")
        if not self.rule_source:
            raise ValueError("rule_source must not be empty.")
        for geometry_ref in self.geometry_refs:
            if not geometry_ref:
                raise ValueError("geometry_refs must not contain empty values.")
        if self.bbox is not None:
            self.bbox.validate()


@dataclass(frozen=True, slots=True, kw_only=True)
class IssueObject(ObserverObjectBase):
    issue_category: str
    issue_level: str
    primary_anchor_type: Optional[str] = None
    issue_state: Optional[str] = None

    def validate(self) -> None:
        ObserverObjectBase.validate(self)
        if self.object_type is not ObjectType.ISSUE:
            raise ValueError("IssueObject must use ISSUE type.")
        if not self.issue_category:
            raise ValueError("issue_category must not be empty.")
        if not self.issue_level:
            raise ValueError("issue_level must not be empty.")


@dataclass(frozen=True, slots=True, kw_only=True)
class AlertObject(ObserverObjectBase):
    alert_time: float
    window_state: Optional[str] = None

    def validate(self) -> None:
        ObserverObjectBase.validate(self)
        if self.object_type is not ObjectType.ALERT:
            raise ValueError("AlertObject must use ALERT type.")
        if self.alert_time < 0:
            raise ValueError("alert_time must be >= 0.")


@dataclass(frozen=True, slots=True, kw_only=True)
class TimeAnchorObject(ObserverObjectBase):
    anchor_kind: TimeAnchorKind
    time_start: float
    time_end: Optional[float] = None
    source_kind: Optional[str] = None

    def validate(self) -> None:
        ObserverObjectBase.validate(self)
        if self.object_type is not ObjectType.TIME_ANCHOR:
            raise ValueError("TimeAnchorObject must use TIME_ANCHOR type.")
        if self.time_start < 0:
            raise ValueError("time_start must be >= 0.")
        if self.anchor_kind is TimeAnchorKind.TIME_POINT:
            if self.time_end is not None and self.time_end < self.time_start:
                raise ValueError("time_end must be >= time_start when present.")
        elif self.anchor_kind is TimeAnchorKind.TIME_RANGE:
            if self.time_end is None:
                raise ValueError("TIME_RANGE anchors require time_end.")
            if self.time_end < self.time_start:
                raise ValueError("time_end must be >= time_start.")


@dataclass(frozen=True, slots=True, kw_only=True)
class ObjectGroup(ObserverObjectBase):
    member_object_ids: Tuple[ObjectId, ...]
    group_source: str
    group_basis: GroupBasis
    group_cardinality: int
    is_minimized: bool

    def validate(self) -> None:
        ObserverObjectBase.validate(self)
        if self.object_type is not ObjectType.OBJECT_GROUP:
            raise ValueError("ObjectGroup must use OBJECT_GROUP type.")
        if not self.group_source:
            raise ValueError("group_source must not be empty.")
        if self.group_cardinality != len(self.member_object_ids):
            raise ValueError("group_cardinality must match member count.")
        if self.group_cardinality < 0:
            raise ValueError("group_cardinality must be >= 0.")
        for member_id in self.member_object_ids:
            if not str(member_id):
                raise ValueError("member_object_ids must not contain empty ids.")


@dataclass(frozen=True, slots=True)
class ObjectAnchor:
    object_id: Optional[ObjectId] = None
    group_id: Optional[ObjectId] = None

    def __post_init__(self) -> None:
        self.validate()

    def validate(self) -> None:
        if self.object_id is None and self.group_id is None:
            raise ValueError("ObjectAnchor requires object_id or group_id.")


@dataclass(frozen=True, slots=True)
class RegionAnchor:
    region_id: ObjectId

    def __post_init__(self) -> None:
        if not str(self.region_id):
            raise ValueError("region_id must not be empty.")


@dataclass(frozen=True, slots=True)
class TimeAnchor:
    time_anchor_id: ObjectId

    def __post_init__(self) -> None:
        if not str(self.time_anchor_id):
            raise ValueError("time_anchor_id must not be empty.")


@dataclass(frozen=True, slots=True)
class AlertAnchor:
    alert_id: ObjectId

    def __post_init__(self) -> None:
        if not str(self.alert_id):
            raise ValueError("alert_id must not be empty.")


def ensure_source_refs(value: Optional[Sequence[SourceRef] | SourceRef]) -> SourceRefs:
    if value is None:
        return tuple()
    if isinstance(value, SourceRef):
        return (value,)
    normalized: list[SourceRef] = []
    for item in value:
        if not isinstance(item, SourceRef):
            raise ValueError("All source refs must be SourceRef instances.")
        normalized.append(item)
    return tuple(normalized)


def ensure_display_label(value: str) -> DisplayLabel:
    text = value.strip()
    if not text:
        raise ValueError("display label must not be empty.")
    return DisplayLabel(text)


__all__ = [
    "ArcGeometry",
    "AlertAnchor",
    "AlertObject",
    "BBox2D",
    "DisplayLabel",
    "GroupBasis",
    "IssueObject",
    "ObjectAnchor",
    "ObjectGroup",
    "ObjectId",
    "ObjectType",
    "ObserverObjectBase",
    "PathPointObject",
    "PathSegmentObject",
    "Point2D",
    "RegionAnchor",
    "RegionObject",
    "RegionRelationType",
    "SourceRef",
    "SourceRefs",
    "TimeAnchor",
    "TimeAnchorKind",
    "TimeAnchorObject",
    "ensure_display_label",
    "ensure_source_refs",
]
