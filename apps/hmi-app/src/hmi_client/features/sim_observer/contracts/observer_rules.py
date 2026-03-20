"""Result shells for simulation observer rules R1-R7.

V1 policy:
- All reason codes used here must come from observer_status.py.
- Some business-facing descriptive fields remain plain strings for now.
- This module defines result carriers only and must stay free of rule logic.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional, Tuple

from .observer_status import ObserverState
from .observer_types import (
    AlertAnchor,
    BBox2D,
    GroupBasis,
    ObjectAnchor,
    ObjectId,
    RegionAnchor,
    TimeAnchor,
)


ObserverAnchor = ObjectAnchor | RegionAnchor | TimeAnchor | AlertAnchor


@dataclass(frozen=True, slots=True)
class ReasonInfo:
    reason_code: str
    reason_text: str

    def __post_init__(self) -> None:
        self.validate()

    def validate(self) -> None:
        if not self.reason_code:
            raise ValueError("reason_code must not be empty.")
        if not self.reason_text:
            raise ValueError("reason_text must not be empty.")


SelectionReason = ReasonInfo


@dataclass(frozen=True, slots=True)
class PrimaryAssociationResult:
    association_level: str
    primary_object_id: Optional[ObjectId] = None
    primary_group_id: Optional[ObjectId] = None
    primary_anchor_type: Optional[str] = None
    context_anchors: Tuple[ObserverAnchor, ...] = field(default_factory=tuple)
    selection_reason: Optional[SelectionReason] = None
    selection_state: ObserverState = ObserverState.RESOLVED

    def __post_init__(self) -> None:
        self.validate()

    def validate(self) -> None:
        allowed_levels = {"single_object", "object_group", "summary_only"}
        if self.association_level not in allowed_levels:
            raise ValueError(f"Unsupported association_level: {self.association_level}")
        if self.association_level == "single_object":
            if self.primary_object_id is None:
                raise ValueError("single_object requires primary_object_id.")
            if self.primary_group_id is not None:
                raise ValueError("single_object must not set primary_group_id.")
        elif self.association_level == "object_group":
            if self.primary_group_id is None:
                raise ValueError("object_group requires primary_group_id.")
            if self.primary_object_id is not None:
                raise ValueError("object_group must not set primary_object_id.")
        else:
            if self.primary_object_id is not None or self.primary_group_id is not None:
                raise ValueError("summary_only must not expose object or group ids.")
        if self.selection_state is not ObserverState.RESOLVED and self.association_level != "summary_only":
            raise ValueError("Non-resolved selection must not pretend to be a concrete target.")


@dataclass(frozen=True, slots=True)
class GroupMinimizationResult:
    group_id: Optional[ObjectId]
    group_basis: GroupBasis
    member_object_ids: Tuple[ObjectId, ...] = field(default_factory=tuple)
    group_cardinality: int = 0
    is_minimized: bool = False
    minimization_reason: Optional[ReasonInfo] = None
    state: ObserverState = ObserverState.GROUP_NOT_MINIMIZED

    def __post_init__(self) -> None:
        self.validate()

    def validate(self) -> None:
        if self.group_cardinality != len(self.member_object_ids):
            raise ValueError("group_cardinality must match member count.")
        if self.group_cardinality < 0:
            raise ValueError("group_cardinality must be >= 0.")
        if self.is_minimized and self.state is ObserverState.GROUP_NOT_MINIMIZED:
            raise ValueError("Minimized group must not stay in GROUP_NOT_MINIMIZED state.")
        if self.state is ObserverState.RESOLVED:
            if not self.member_object_ids:
                raise ValueError("Resolved group result requires members.")
            if self.group_id is None:
                raise ValueError("Resolved group result requires group_id.")


@dataclass(frozen=True, slots=True)
class SpatialRef:
    geometry_refs: Tuple[str, ...] = field(default_factory=tuple)
    bbox: Optional[BBox2D] = None
    mapping_state: ObserverState = ObserverState.MAPPING_MISSING
    mapping_reason: Optional[ReasonInfo] = None
    mapped_object_ids: Tuple[ObjectId, ...] = field(default_factory=tuple)
    unmapped_object_ids: Tuple[ObjectId, ...] = field(default_factory=tuple)

    def __post_init__(self) -> None:
        self.validate()

    def validate(self) -> None:
        if self.mapping_state is ObserverState.RESOLVED:
            if not self.geometry_refs and self.bbox is None:
                raise ValueError("Resolved SpatialRef requires geometry_refs or bbox.")
        if self.mapping_state is ObserverState.MAPPING_INSUFFICIENT and not self.unmapped_object_ids:
            raise ValueError("Mapping-insufficient SpatialRef should preserve unmapped object ids.")
        if self.mapping_state is ObserverState.MAPPING_MISSING and self.mapped_object_ids:
            raise ValueError("Mapping-missing SpatialRef must not expose mapped_object_ids.")


@dataclass(frozen=True, slots=True)
class SequenceRangeRef:
    start_segment_id: ObjectId
    end_segment_id: ObjectId
    member_segment_ids: Tuple[ObjectId, ...]
    continuity_state: str

    def __post_init__(self) -> None:
        self.validate()

    def validate(self) -> None:
        if not self.member_segment_ids:
            raise ValueError("member_segment_ids must not be empty.")
        if self.start_segment_id not in self.member_segment_ids:
            raise ValueError("start_segment_id must be part of member_segment_ids.")
        if self.end_segment_id not in self.member_segment_ids:
            raise ValueError("end_segment_id must be part of member_segment_ids.")


@dataclass(frozen=True, slots=True)
class TimeMappingResult:
    association_level: str
    object_id: Optional[ObjectId] = None
    group_id: Optional[ObjectId] = None
    sequence_range_ref: Optional[SequenceRangeRef] = None
    reason: Optional[ReasonInfo] = None
    mapping_state: ObserverState = ObserverState.MAPPING_INSUFFICIENT

    def __post_init__(self) -> None:
        self.validate()

    def validate(self) -> None:
        allowed_levels = {
            "single_object",
            "object_group",
            "sequence_range",
            "mapping_insufficient",
            "mapping_missing",
        }
        if self.association_level not in allowed_levels:
            raise ValueError(f"Unsupported association_level: {self.association_level}")
        if self.association_level == "single_object":
            if self.object_id is None:
                raise ValueError("single_object requires object_id.")
            if self.mapping_state is not ObserverState.RESOLVED:
                raise ValueError("single_object mapping must be resolved.")
        elif self.association_level == "object_group":
            if self.group_id is None:
                raise ValueError("object_group requires group_id.")
            if self.mapping_state is not ObserverState.RESOLVED:
                raise ValueError("object_group mapping must be resolved.")
        elif self.association_level == "sequence_range":
            if self.sequence_range_ref is None:
                raise ValueError("sequence_range requires sequence_range_ref.")
            if self.mapping_state is not ObserverState.RESOLVED:
                raise ValueError("sequence_range mapping must be resolved.")
        else:
            if any((self.object_id, self.group_id, self.sequence_range_ref)):
                raise ValueError("Mapping failures must not pretend to resolve targets.")


@dataclass(frozen=True, slots=True)
class ContextWindowRef:
    window_start: Optional[float]
    window_end: Optional[float]
    anchor_time: float
    window_basis: str
    window_state: ObserverState
    reason: Optional[ReasonInfo] = None
    fallback_mode: Optional[str] = None

    def __post_init__(self) -> None:
        self.validate()

    def validate(self) -> None:
        if self.anchor_time < 0:
            raise ValueError("anchor_time must be >= 0.")
        if self.window_state is ObserverState.RESOLVED:
            if self.window_start is None or self.window_end is None:
                raise ValueError("Resolved context window requires start and end.")
            if self.window_end < self.window_start:
                raise ValueError("window_end must be >= window_start.")
        if self.window_basis == "anchor_only_fallback" or self.fallback_mode == "single_anchor":
            if self.window_state is ObserverState.RESOLVED:
                raise ValueError("Anchor-only fallback must not use resolved state.")


@dataclass(frozen=True, slots=True)
class KeyWindowResult:
    key_window_start: Optional[float]
    key_window_end: Optional[float]
    window_basis: str
    window_state: ObserverState
    reason: Optional[ReasonInfo] = None

    def __post_init__(self) -> None:
        self.validate()

    def validate(self) -> None:
        if self.window_state is ObserverState.RESOLVED:
            if self.key_window_start is None or self.key_window_end is None:
                raise ValueError("Resolved key window requires start and end.")
            if self.key_window_end < self.key_window_start:
                raise ValueError("key_window_end must be >= key_window_start.")


def ensure_reason(reason_code: str, reason_text: str) -> ReasonInfo:
    return ReasonInfo(reason_code=reason_code, reason_text=reason_text)


__all__ = [
    "ContextWindowRef",
    "GroupMinimizationResult",
    "KeyWindowResult",
    "ObserverAnchor",
    "PrimaryAssociationResult",
    "ReasonInfo",
    "SelectionReason",
    "SequenceRangeRef",
    "SpatialRef",
    "TimeMappingResult",
    "ensure_reason",
]
