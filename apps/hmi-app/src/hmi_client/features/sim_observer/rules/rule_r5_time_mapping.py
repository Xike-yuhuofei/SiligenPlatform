"""Execute rule R5: map a time anchor to an object, object group, or sequence range."""

from __future__ import annotations

from typing import Any, Mapping, Sequence

from ..adapters.recording_normalizer import NormalizedObserverData
from ..contracts.observer_rules import SequenceRangeRef, TimeMappingResult, ensure_reason
from ..contracts.observer_status import (
    ObserverState,
    REASON_CONCURRENT_CANDIDATES_PRESENT,
    REASON_DIRECT_OBJECT_ANCHOR,
    REASON_MAPPING_MISSING,
    REASON_MINIMIZED_GROUP_SELECTED,
    REASON_SEQUENCE_RANGE_RESOLVED,
    REASON_TIME_MAPPING_INSUFFICIENT,
)
from ..contracts.observer_types import GroupBasis, ObjectId, PathSegmentObject, TimeAnchorObject


def resolve_time_mapping(
    time_anchor: TimeAnchorObject,
    raw_mapping: Mapping[str, Any],
    data: NormalizedObserverData,
    *,
    default_group_basis: GroupBasis = GroupBasis.TIME_MAPPED,
) -> TimeMappingResult:
    direct_object_id = _direct_object_id(raw_mapping, data)
    if direct_object_id is not None:
        return TimeMappingResult(
            association_level="single_object",
            object_id=direct_object_id,
            reason=_reason_for(
                raw_mapping,
                REASON_DIRECT_OBJECT_ANCHOR,
                "The time anchor resolves directly to a stable object.",
            ),
            mapping_state=ObserverState.RESOLVED,
        )

    sequence_range = _sequence_range(raw_mapping, data)
    if sequence_range is not None:
        return TimeMappingResult(
            association_level="sequence_range",
            sequence_range_ref=sequence_range,
            reason=_reason_for(
                raw_mapping,
                REASON_SEQUENCE_RANGE_RESOLVED,
                "The time anchor resolves to a stable contiguous sequence range.",
            ),
            mapping_state=ObserverState.RESOLVED,
        )

    candidate_object_ids = _candidate_object_ids(raw_mapping, data)
    if len(candidate_object_ids) == 1:
        return TimeMappingResult(
            association_level="single_object",
            object_id=candidate_object_ids[0],
            reason=ensure_reason(
                REASON_MINIMIZED_GROUP_SELECTED,
                "The time anchor has a single stable candidate object.",
            ),
            mapping_state=ObserverState.RESOLVED,
        )

    if len(candidate_object_ids) > 1:
        if _has_concurrent_candidates(raw_mapping):
            return TimeMappingResult(
                association_level="mapping_insufficient",
                reason=_reason_for(
                    raw_mapping,
                    REASON_CONCURRENT_CANDIDATES_PRESENT,
                    "Concurrent candidates prevent a stable time-to-object mapping.",
                ),
                mapping_state=ObserverState.MAPPING_INSUFFICIENT,
            )
        return TimeMappingResult(
            association_level="object_group",
            group_id=_make_group_id(default_group_basis, candidate_object_ids),
            reason=ensure_reason(
                REASON_MINIMIZED_GROUP_SELECTED,
                "The time anchor resolves to an explainable object group.",
            ),
            mapping_state=ObserverState.RESOLVED,
        )

    if _looks_mapping_related(raw_mapping):
        return TimeMappingResult(
            association_level="mapping_missing",
            reason=_reason_for(
                raw_mapping,
                REASON_MAPPING_MISSING,
                "The time anchor exposes mapping hints, but no normalized targets were found.",
            ),
            mapping_state=ObserverState.MAPPING_MISSING,
        )

    return TimeMappingResult(
        association_level="mapping_insufficient",
        reason=_reason_for(
            raw_mapping,
            REASON_TIME_MAPPING_INSUFFICIENT,
            "The time anchor exposes process facts, but object mapping remains insufficient.",
        ),
        mapping_state=ObserverState.MAPPING_INSUFFICIENT,
    )


def _direct_object_id(raw_mapping: Mapping[str, Any], data: NormalizedObserverData) -> ObjectId | None:
    direct_object_id = _first_text_value(raw_mapping, ("primary_object_id", "object_id"))
    if direct_object_id is not None:
        object_id = ObjectId(direct_object_id)
        if object_id in data.objects_by_id:
            return object_id

    for key in ("primary_segment_index", "segment_index"):
        segment_index = raw_mapping.get(key)
        if segment_index is None:
            continue
        object_id = ObjectId(f"segment:{int(segment_index)}")
        if object_id in data.objects_by_id:
            return object_id
    return None


def _sequence_range(
    raw_mapping: Mapping[str, Any],
    data: NormalizedObserverData,
) -> SequenceRangeRef | None:
    explicit_member_ids = _segment_member_ids(raw_mapping, data)
    if explicit_member_ids:
        return _build_sequence_range(explicit_member_ids)

    candidate_segment_ids = tuple(
        object_id for object_id in _candidate_object_ids(raw_mapping, data) if _segment_object(object_id, data) is not None
    )
    if len(candidate_segment_ids) > 1:
        return _build_sequence_range(candidate_segment_ids)
    return None


def _segment_member_ids(raw_mapping: Mapping[str, Any], data: NormalizedObserverData) -> tuple[ObjectId, ...]:
    segment_ids: list[ObjectId] = []

    start_segment_id = _segment_id_from_value(raw_mapping.get("start_segment_id"), data)
    end_segment_id = _segment_id_from_value(raw_mapping.get("end_segment_id"), data)
    if start_segment_id is not None and end_segment_id is not None:
        start_segment = _segment_object(start_segment_id, data)
        end_segment = _segment_object(end_segment_id, data)
        if start_segment is not None and end_segment is not None:
            lower = min(start_segment.segment_index, end_segment.segment_index)
            upper = max(start_segment.segment_index, end_segment.segment_index)
            for segment in data.path_segments:
                if lower <= segment.segment_index <= upper:
                    segment_ids.append(segment.object_id)

    if not segment_ids:
        for key in ("segment_member_ids",):
            raw_value = raw_mapping.get(key)
            if not isinstance(raw_value, Sequence) or isinstance(raw_value, (str, bytes)):
                continue
            for item in raw_value:
                segment_id = _segment_id_from_value(item, data)
                if segment_id is not None:
                    segment_ids.append(segment_id)

    if not segment_ids:
        for key in ("candidate_segment_indices", "segment_indices"):
            raw_value = raw_mapping.get(key)
            if not isinstance(raw_value, Sequence) or isinstance(raw_value, (str, bytes)):
                continue
            for item in raw_value:
                segment_id = ObjectId(f"segment:{int(item)}")
                if segment_id in data.objects_by_id:
                    segment_ids.append(segment_id)

    return _unique_ids(segment_ids)


def _build_sequence_range(member_segment_ids: Sequence[ObjectId]) -> SequenceRangeRef | None:
    normalized_ids = _unique_ids(member_segment_ids)
    if len(normalized_ids) < 2:
        return None
    segment_indexes = [int(str(member_id).split(":")[-1]) for member_id in normalized_ids]
    if sorted(segment_indexes) != list(range(min(segment_indexes), max(segment_indexes) + 1)):
        return None
    sorted_member_ids = tuple(
        member_id for _, member_id in sorted(zip(segment_indexes, normalized_ids, strict=False), key=lambda pair: pair[0])
    )
    return SequenceRangeRef(
        start_segment_id=sorted_member_ids[0],
        end_segment_id=sorted_member_ids[-1],
        member_segment_ids=sorted_member_ids,
        continuity_state="contiguous",
    )


def _candidate_object_ids(
    raw_mapping: Mapping[str, Any],
    data: NormalizedObserverData,
) -> tuple[ObjectId, ...]:
    candidate_ids: list[ObjectId] = []
    for key in ("candidate_object_ids", "object_ids"):
        raw_value = raw_mapping.get(key)
        if not isinstance(raw_value, Sequence) or isinstance(raw_value, (str, bytes)):
            continue
        for item in raw_value:
            object_id = ObjectId(str(item))
            if object_id in data.objects_by_id:
                candidate_ids.append(object_id)
    for key in ("candidate_segment_indices", "segment_indices"):
        raw_value = raw_mapping.get(key)
        if not isinstance(raw_value, Sequence) or isinstance(raw_value, (str, bytes)):
            continue
        for item in raw_value:
            object_id = ObjectId(f"segment:{int(item)}")
            if object_id in data.objects_by_id:
                candidate_ids.append(object_id)
    return _unique_ids(candidate_ids)


def _segment_id_from_value(value: object, data: NormalizedObserverData) -> ObjectId | None:
    if value is None:
        return None
    text = str(value).strip()
    if not text:
        return None
    object_id = ObjectId(text if text.startswith("segment:") else f"segment:{text}")
    if object_id in data.objects_by_id:
        return object_id
    return None


def _segment_object(object_id: ObjectId, data: NormalizedObserverData) -> PathSegmentObject | None:
    obj = data.objects_by_id.get(object_id)
    if isinstance(obj, PathSegmentObject):
        return obj
    return None


def _has_concurrent_candidates(raw_mapping: Mapping[str, Any]) -> bool:
    raw_flag = raw_mapping.get("concurrent_candidates_present")
    if isinstance(raw_flag, bool):
        return raw_flag
    raw_candidates = raw_mapping.get("concurrent_candidate_ids")
    return isinstance(raw_candidates, Sequence) and not isinstance(raw_candidates, (str, bytes)) and bool(raw_candidates)


def _looks_mapping_related(raw_mapping: Mapping[str, Any]) -> bool:
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


def _reason_for(raw_mapping: Mapping[str, Any], default_code: str, default_text: str):
    hint_code = _first_text_value(raw_mapping, ("time_mapping_hint_reason_code",))
    hint_text = _first_text_value(raw_mapping, ("time_mapping_hint_reason_text",))
    if hint_code is not None and hint_text is not None:
        return ensure_reason(hint_code, hint_text)
    return ensure_reason(default_code, default_text)


def _make_group_id(group_basis: GroupBasis, members: Sequence[ObjectId]) -> ObjectId:
    serialized_members = ",".join(str(member_id) for member_id in members)
    return ObjectId(f"group:{group_basis.value}:{serialized_members}")


def _first_text_value(data: Mapping[str, Any], keys: Sequence[str]) -> str | None:
    for key in keys:
        value = data.get(key)
        if value is None:
            continue
        text = str(value).strip()
        if text:
            return text
    return None


def _unique_ids(values: Sequence[ObjectId]) -> tuple[ObjectId, ...]:
    seen: set[str] = set()
    normalized: list[ObjectId] = []
    for value in values:
        text = str(value).strip()
        if not text or text in seen:
            continue
        seen.add(text)
        normalized.append(ObjectId(text))
    return tuple(normalized)


__all__ = ["resolve_time_mapping"]
