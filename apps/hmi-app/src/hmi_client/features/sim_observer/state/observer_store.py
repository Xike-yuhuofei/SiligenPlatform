"""Shared state entry point for the simulation observer feature."""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Mapping, Sequence

from ..adapters.recording_loader import RawRecordingPayload, load_recording_from_file, load_recording_from_mapping
from ..adapters.recording_normalizer import NormalizedObserverData, normalize_recording
from ..contracts.observer_rules import (
    ContextWindowRef,
    GroupMinimizationResult,
    KeyWindowResult,
    ObserverAnchor,
    PrimaryAssociationResult,
    SequenceRangeRef,
    TimeMappingResult,
)
from ..contracts.observer_status import FailureReason, ObserverState
from ..contracts.observer_types import (
    AlertAnchor,
    AlertObject,
    GroupBasis,
    IssueObject,
    ObjectAnchor,
    ObjectId,
    ObserverObjectBase,
    ObjectType,
    PathSegmentObject,
    RegionAnchor,
    TimeAnchor,
    TimeAnchorKind,
    TimeAnchorObject,
)
from ..rules.rule_r1_primary_association import resolve_primary_association
from ..rules.rule_r2_minimize_group import minimize_candidate_group
from ..rules.rule_r5_time_mapping import resolve_time_mapping
from ..rules.rule_r6_alert_context import resolve_alert_context_window
from ..rules.rule_r7_key_window import resolve_key_window
from .time_mapping_hints import with_time_mapping_hint


_SOURCE_TOKEN_RE = re.compile(r"([^\.\[\]]+)|\[(\d+)\]")


@dataclass(frozen=True, slots=True)
class ObserverSelection:
    selected_kind: str
    selected_id: ObjectId
    selected_anchor: ObserverAnchor | None
    primary_association: PrimaryAssociationResult
    group_result: GroupMinimizationResult | None
    failure_reason: FailureReason | None


@dataclass(frozen=True, slots=True)
class P1Backlink:
    backlink_level: str
    object_id: ObjectId | None = None
    group_id: ObjectId | None = None
    group_member_ids: tuple[ObjectId, ...] = field(default_factory=tuple)
    sequence_range_ref: SequenceRangeRef | None = None
    reason_text: str = ""

    def __post_init__(self) -> None:
        allowed_levels = {"single_object", "object_group", "sequence_range", "unavailable"}
        if self.backlink_level not in allowed_levels:
            raise ValueError(f"Unsupported backlink_level: {self.backlink_level}")
        if self.backlink_level == "single_object" and self.object_id is None:
            raise ValueError("single_object backlink requires object_id.")
        if self.backlink_level == "object_group" and self.group_id is None:
            raise ValueError("object_group backlink requires group_id.")
        if self.backlink_level == "sequence_range" and self.sequence_range_ref is None:
            raise ValueError("sequence_range backlink requires sequence_range_ref.")


@dataclass(frozen=True, slots=True)
class P1Selection:
    selected_kind: str
    selected_id: ObjectId
    selected_object: AlertObject | TimeAnchorObject
    time_mapping: TimeMappingResult
    context_window: ContextWindowRef | None
    key_window: KeyWindowResult | None
    backlink: P1Backlink
    failure_reason: FailureReason | None
    candidate_object_ids: tuple[ObjectId, ...] = field(default_factory=tuple)


class ObserverStore:
    def __init__(self, normalized_data: NormalizedObserverData) -> None:
        self._normalized_data = normalized_data
        self._current_selection: ObserverSelection | None = None
        self._current_p1_selection: P1Selection | None = None

    @classmethod
    def from_file(cls, path: str | Path) -> "ObserverStore":
        payload = load_recording_from_file(path)
        return cls(normalize_recording(payload))

    @classmethod
    def from_mapping(cls, data: Mapping[str, Any]) -> "ObserverStore":
        payload = load_recording_from_mapping(data)
        return cls(normalize_recording(payload))

    @classmethod
    def from_payload(cls, payload: RawRecordingPayload) -> "ObserverStore":
        return cls(normalize_recording(payload))

    @property
    def normalized_data(self) -> NormalizedObserverData:
        return self._normalized_data

    @property
    def issue_ids(self) -> tuple[ObjectId, ...]:
        return self._normalized_data.initial_issue_ids

    @property
    def alert_ids(self) -> tuple[ObjectId, ...]:
        return tuple(alert.object_id for alert in self._normalized_data.alerts)

    @property
    def time_anchor_ids(self) -> tuple[ObjectId, ...]:
        return tuple(anchor.object_id for anchor in self._normalized_data.time_anchors)

    @property
    def current_selection(self) -> ObserverSelection | None:
        return self._current_selection

    @property
    def current_p1_selection(self) -> P1Selection | None:
        return self._current_p1_selection

    @property
    def current_issue(self) -> IssueObject | None:
        if self._current_selection is None or self._current_selection.selected_kind != "issue":
            return None
        return self._require_issue(self._current_selection.selected_id)

    @property
    def current_alert(self) -> AlertObject | None:
        if self._current_p1_selection is None or self._current_p1_selection.selected_kind != "alert":
            return None
        return self._require_alert(self._current_p1_selection.selected_id)

    @property
    def current_time_anchor(self) -> TimeAnchorObject | None:
        if self._current_p1_selection is None or self._current_p1_selection.selected_kind != "time_anchor":
            return None
        return self._require_time_anchor(self._current_p1_selection.selected_id)

    @property
    def current_primary_object(self) -> ObserverObjectBase | None:
        if self._current_selection is None:
            return None
        object_id = self._current_selection.primary_association.primary_object_id
        if object_id is None:
            return None
        return self._normalized_data.objects_by_id.get(object_id)

    @property
    def current_primary_object_id(self) -> ObjectId | None:
        if self._current_selection is None:
            return None
        return self._current_selection.primary_association.primary_object_id

    @property
    def current_group_members(self) -> tuple[ObserverObjectBase, ...]:
        if self._current_selection is None or self._current_selection.group_result is None:
            return tuple()
        return tuple(
            self._normalized_data.objects_by_id[member_id]
            for member_id in self._current_selection.group_result.member_object_ids
            if member_id in self._normalized_data.objects_by_id
        )

    @property
    def current_group_member_ids(self) -> tuple[ObjectId, ...]:
        if self._current_selection is None or self._current_selection.group_result is None:
            return tuple()
        return self._current_selection.group_result.member_object_ids

    @property
    def current_failure_reason(self) -> FailureReason | None:
        if self._current_selection is None:
            return None
        return self._current_selection.failure_reason

    @property
    def current_time_mapping(self) -> TimeMappingResult | None:
        if self._current_p1_selection is None:
            return None
        return self._current_p1_selection.time_mapping

    @property
    def current_context_window(self) -> ContextWindowRef | None:
        if self._current_p1_selection is None:
            return None
        return self._current_p1_selection.context_window

    @property
    def current_key_window(self) -> KeyWindowResult | None:
        if self._current_p1_selection is None:
            return None
        return self._current_p1_selection.key_window

    @property
    def current_sequence_range(self) -> SequenceRangeRef | None:
        if self._current_p1_selection is None:
            return None
        return self._current_p1_selection.time_mapping.sequence_range_ref

    @property
    def current_p1_backlink(self) -> P1Backlink | None:
        if self._current_p1_selection is None:
            return None
        return self._current_p1_selection.backlink

    @property
    def default_issue_id(self) -> ObjectId | None:
        first_issue_id = self.issue_ids[0] if self.issue_ids else None
        fallback_issue_id = None
        for issue in self._normalized_data.issues:
            selection = self._selection_for_issue(issue)
            if selection.primary_association.selection_state is ObserverState.RESOLVED:
                return issue.object_id
            if selection.failure_reason is not FailureReason.ANCHOR_MISSING and fallback_issue_id is None:
                fallback_issue_id = issue.object_id
        return fallback_issue_id or first_issue_id

    @property
    def default_time_anchor_id(self) -> ObjectId | None:
        trace_anchor_id = next(
            (anchor.object_id for anchor in self._normalized_data.time_anchors if anchor.source_kind == "trace"),
            None,
        )
        return trace_anchor_id or (self.time_anchor_ids[0] if self.time_anchor_ids else None)

    @property
    def default_alert_id(self) -> ObjectId | None:
        return self.alert_ids[0] if self.alert_ids else None

    def current_path_segments_for_structure(self) -> tuple[PathSegmentObject, ...]:
        return self._normalized_data.path_segments

    def clear_selection(self) -> None:
        self._current_selection = None

    def clear_p1_selection(self) -> None:
        self._current_p1_selection = None

    def select_default_issue(self) -> ObserverSelection | None:
        issue_id = self.default_issue_id
        if issue_id is None:
            self.clear_selection()
            return None
        return self.select_issue(issue_id)

    def select_default_time_anchor(self) -> P1Selection | None:
        time_anchor_id = self.default_time_anchor_id
        if time_anchor_id is None:
            self.clear_p1_selection()
            return None
        return self.select_p1_time_anchor(time_anchor_id)

    def select_default_alert(self) -> P1Selection | None:
        alert_id = self.default_alert_id
        if alert_id is None:
            self.clear_p1_selection()
            return None
        return self.select_p1_alert(alert_id)

    def select_issue(self, issue_id: ObjectId | str) -> ObserverSelection:
        issue = self._require_issue(issue_id)
        selection = self._selection_for_issue(issue)
        self._current_selection = selection
        return selection

    def reselect_issue(self, issue_id: ObjectId | str) -> ObserverSelection:
        return self.select_issue(issue_id)

    def select_alert(self, alert_id: ObjectId | str) -> ObserverSelection:
        alert = self._require_alert(alert_id)
        selection = self._resolve_selection(
            selected_kind="alert",
            selected_id=alert.object_id,
            selected_object=alert,
            group_basis=GroupBasis.ALERT_CONTEXT,
        )
        self._current_selection = selection
        return selection

    def select_time_anchor(self, time_anchor_id: ObjectId | str) -> ObserverSelection:
        time_anchor = self._require_time_anchor(time_anchor_id)
        selection = self._resolve_selection(
            selected_kind="time_anchor",
            selected_id=time_anchor.object_id,
            selected_object=time_anchor,
            group_basis=GroupBasis.TIME_MAPPED,
        )
        self._current_selection = selection
        return selection

    def select_p1_alert(self, alert_id: ObjectId | str) -> P1Selection:
        alert = self._require_alert(alert_id)
        selection = self._resolve_p1_selection(selected_kind="alert", selected_object=alert)
        self._current_p1_selection = selection
        return selection

    def select_p1_time_anchor(self, time_anchor_id: ObjectId | str) -> P1Selection:
        time_anchor = self._require_time_anchor(time_anchor_id)
        selection = self._resolve_p1_selection(selected_kind="time_anchor", selected_object=time_anchor)
        self._current_p1_selection = selection
        return selection

    def find_issue_ids_for_object(self, object_id: ObjectId | str) -> tuple[ObjectId, ...]:
        normalized_object_id = ObjectId(str(object_id))
        matched_issue_ids: list[ObjectId] = []
        for issue in self._normalized_data.issues:
            raw_mapping = self._raw_mapping_for(issue)
            selection = self._selection_for_issue(issue)
            if self._selection_hits_object(selection, raw_mapping, normalized_object_id):
                matched_issue_ids.append(issue.object_id)
        return tuple(matched_issue_ids)

    def find_issue_ids_for_objects(self, object_ids: Sequence[ObjectId | str]) -> tuple[ObjectId, ...]:
        normalized_ids = tuple(_unique_ids(ObjectId(str(object_id)) for object_id in object_ids))
        if not normalized_ids:
            return tuple()
        candidate_sets = [set(self.find_issue_ids_for_object(object_id)) for object_id in normalized_ids]
        if not candidate_sets:
            return tuple()
        shared_issue_ids = set.intersection(*candidate_sets)
        return tuple(issue_id for issue_id in self.issue_ids if issue_id in shared_issue_ids)

    def _selection_for_issue(self, issue: IssueObject) -> ObserverSelection:
        return self._resolve_selection(
            selected_kind="issue",
            selected_id=issue.object_id,
            selected_object=issue,
            group_basis=GroupBasis.ISSUE_RELATED,
        )

    def _resolve_selection(
        self,
        *,
        selected_kind: str,
        selected_id: ObjectId,
        selected_object: IssueObject | AlertObject | TimeAnchorObject,
        group_basis: GroupBasis,
    ) -> ObserverSelection:
        raw_mapping = self._raw_mapping_for(selected_object)
        selected_anchor = self._derive_anchor(selected_object, raw_mapping)
        candidate_object_ids = self._candidate_object_ids(raw_mapping)
        removable_object_ids = self._removable_object_ids(raw_mapping)

        group_result = None
        if len(candidate_object_ids) > 1:
            group_result = minimize_candidate_group(
                candidate_object_ids,
                self._group_basis(raw_mapping, default_basis=group_basis),
                removable_object_ids=removable_object_ids,
            )

        primary_association = resolve_primary_association(
            selected_anchor,
            self._normalized_data,
            candidate_object_ids=candidate_object_ids,
            group_result=group_result,
            primary_anchor_type=self._primary_anchor_type(selected_object, raw_mapping),
        )
        failure_reason = self._failure_reason_for(
            selected_object=selected_object,
            raw_mapping=raw_mapping,
            selected_anchor=selected_anchor,
            candidate_object_ids=candidate_object_ids,
            primary_association=primary_association,
        )
        return ObserverSelection(
            selected_kind=selected_kind,
            selected_id=selected_id,
            selected_anchor=selected_anchor,
            primary_association=primary_association,
            group_result=group_result,
            failure_reason=failure_reason,
        )

    def _resolve_p1_selection(
        self,
        *,
        selected_kind: str,
        selected_object: AlertObject | TimeAnchorObject,
    ) -> P1Selection:
        raw_mapping = self._raw_mapping_for(selected_object)
        if isinstance(selected_object, AlertObject):
            candidate_object_ids = self._candidate_object_ids(raw_mapping)
            time_anchor = _synthetic_time_anchor_for_alert(selected_object)
            time_mapping = resolve_time_mapping(
                time_anchor,
                raw_mapping,
                self._normalized_data,
                default_group_basis=GroupBasis.ALERT_CONTEXT,
            )
            context_window = resolve_alert_context_window(selected_object, raw_mapping)
            key_window = resolve_key_window(
                raw_mapping,
                anchor_time=selected_object.alert_time,
                context_window=context_window,
            )
        else:
            raw_mapping = with_time_mapping_hint(selected_object, raw_mapping, self._normalized_data)
            candidate_object_ids = self._candidate_object_ids(raw_mapping)
            time_mapping = resolve_time_mapping(
                selected_object,
                raw_mapping,
                self._normalized_data,
                default_group_basis=GroupBasis.TIME_MAPPED,
            )
            context_window = None
            key_window = resolve_key_window(
                raw_mapping,
                anchor_time=selected_object.time_start,
                default_start=selected_object.time_start,
                default_end=selected_object.time_end if selected_object.time_end is not None else selected_object.time_start,
                default_basis="time_anchor",
            )
        backlink = self._build_p1_backlink(time_mapping, candidate_object_ids)
        return P1Selection(
            selected_kind=selected_kind,
            selected_id=selected_object.object_id,
            selected_object=selected_object,
            time_mapping=time_mapping,
            context_window=context_window,
            key_window=key_window,
            backlink=backlink,
            failure_reason=self._p1_failure_reason_for(time_mapping, context_window, key_window),
            candidate_object_ids=candidate_object_ids,
        )

    def _build_p1_backlink(
        self,
        time_mapping: TimeMappingResult,
        candidate_object_ids: Sequence[ObjectId],
    ) -> P1Backlink:
        reason_text = time_mapping.reason.reason_text if time_mapping.reason is not None else "P1 当前结果不可回接到 P0。"
        if time_mapping.mapping_state is not ObserverState.RESOLVED:
            return P1Backlink(
                backlink_level="unavailable",
                reason_text=reason_text,
            )
        if time_mapping.association_level == "single_object" and time_mapping.object_id is not None:
            return P1Backlink(
                backlink_level="single_object",
                object_id=time_mapping.object_id,
                reason_text=reason_text,
            )
        if time_mapping.association_level == "object_group" and time_mapping.group_id is not None:
            return P1Backlink(
                backlink_level="object_group",
                group_id=time_mapping.group_id,
                group_member_ids=tuple(candidate_object_ids),
                reason_text=reason_text,
            )
        if time_mapping.association_level == "sequence_range" and time_mapping.sequence_range_ref is not None:
            return P1Backlink(
                backlink_level="sequence_range",
                sequence_range_ref=time_mapping.sequence_range_ref,
                reason_text=reason_text,
            )
        return P1Backlink(backlink_level="unavailable", reason_text=reason_text)

    def _p1_failure_reason_for(
        self,
        time_mapping: TimeMappingResult,
        context_window: ContextWindowRef | None,
        key_window: KeyWindowResult | None,
    ) -> FailureReason | None:
        if key_window is not None and key_window.window_state is ObserverState.WINDOW_NOT_RESOLVED:
            return FailureReason.WINDOW_NOT_RESOLVED
        if context_window is not None and context_window.window_state is not ObserverState.RESOLVED:
            return FailureReason.WINDOW_NOT_RESOLVED
        if time_mapping.mapping_state is ObserverState.RESOLVED:
            return None
        if time_mapping.mapping_state is ObserverState.EMPTY_BUT_EXPLAINED:
            return FailureReason.EMPTY_STATE_REASON
        if time_mapping.mapping_state is ObserverState.MAPPING_INSUFFICIENT:
            return FailureReason.MAPPING_INSUFFICIENT
        if time_mapping.mapping_state is ObserverState.MAPPING_MISSING:
            return FailureReason.MAPPING_MISSING
        return None

    def _selection_hits_object(
        self,
        selection: ObserverSelection,
        raw_mapping: Mapping[str, Any],
        object_id: ObjectId,
    ) -> bool:
        if selection.primary_association.primary_object_id == object_id:
            return True
        if selection.group_result is not None and object_id in selection.group_result.member_object_ids:
            return True
        if object_id in self._candidate_object_ids(raw_mapping):
            return True
        if isinstance(selection.selected_anchor, ObjectAnchor) and selection.selected_anchor.object_id == object_id:
            return True
        return False

    def _require_issue(self, issue_id: ObjectId | str) -> IssueObject:
        normalized_id = ObjectId(str(issue_id))
        for issue in self._normalized_data.issues:
            if issue.object_id == normalized_id:
                return issue
        raise KeyError(f"Unknown issue id: {normalized_id}")

    def _require_alert(self, alert_id: ObjectId | str) -> AlertObject:
        normalized_id = ObjectId(str(alert_id))
        for alert in self._normalized_data.alerts:
            if alert.object_id == normalized_id:
                return alert
        raise KeyError(f"Unknown alert id: {normalized_id}")

    def _require_time_anchor(self, time_anchor_id: ObjectId | str) -> TimeAnchorObject:
        normalized_id = ObjectId(str(time_anchor_id))
        for time_anchor in self._normalized_data.time_anchors:
            if time_anchor.object_id == normalized_id:
                return time_anchor
        raise KeyError(f"Unknown time anchor id: {normalized_id}")

    def _raw_mapping_for(self, obj: IssueObject | AlertObject | TimeAnchorObject) -> Mapping[str, Any]:
        for source_ref in obj.source_refs:
            value = _resolve_source_path(self._normalized_data.source_root.raw_root, source_ref.source_path)
            if isinstance(value, Mapping):
                return value
        return {}

    def _derive_anchor(
        self,
        obj: IssueObject | AlertObject | TimeAnchorObject,
        raw_mapping: Mapping[str, Any],
    ) -> ObserverAnchor | None:
        if isinstance(obj, AlertObject):
            return AlertAnchor(alert_id=obj.object_id)
        if isinstance(obj, TimeAnchorObject):
            return TimeAnchor(time_anchor_id=obj.object_id)

        direct_object_id = _first_text_value(raw_mapping, ("primary_object_id", "object_id"))
        if direct_object_id is not None:
            return ObjectAnchor(object_id=ObjectId(direct_object_id))

        direct_group_id = _first_text_value(raw_mapping, ("primary_group_id", "group_id"))
        if direct_group_id is not None:
            return ObjectAnchor(group_id=ObjectId(direct_group_id))

        if obj.primary_anchor_type in {"segment", "path_segment"}:
            segment_index = raw_mapping.get("segment_index")
            if segment_index is not None:
                return ObjectAnchor(object_id=ObjectId(f"segment:{int(segment_index)}"))

        region_id = _first_text_value(raw_mapping, ("region_id", "primary_region_id"))
        if obj.primary_anchor_type == "region" and region_id is not None:
            return RegionAnchor(region_id=ObjectId(f"region:{region_id}"))

        return None

    def _candidate_object_ids(self, raw_mapping: Mapping[str, Any]) -> tuple[ObjectId, ...]:
        candidate_ids: list[ObjectId] = []
        candidate_ids.extend(_list_text_values(raw_mapping, ("candidate_object_ids", "object_ids")))
        candidate_ids.extend(ObjectId(f"segment:{index}") for index in _list_int_values(raw_mapping, ("candidate_segment_indices", "segment_indices")))
        candidate_ids.extend(ObjectId(f"region:{region_id}") for region_id in _list_text_values(raw_mapping, ("candidate_region_ids", "region_ids")))
        return _unique_ids(candidate_ids)

    def _removable_object_ids(self, raw_mapping: Mapping[str, Any]) -> tuple[ObjectId, ...]:
        removable_ids: list[ObjectId] = []
        removable_ids.extend(_list_text_values(raw_mapping, ("removable_object_ids",)))
        removable_ids.extend(ObjectId(f"segment:{index}") for index in _list_int_values(raw_mapping, ("removable_segment_indices",)))
        removable_ids.extend(ObjectId(f"region:{region_id}") for region_id in _list_text_values(raw_mapping, ("removable_region_ids",)))
        return _unique_ids(removable_ids)

    def _primary_anchor_type(
        self,
        obj: IssueObject | AlertObject | TimeAnchorObject,
        raw_mapping: Mapping[str, Any],
    ) -> str | None:
        explicit = _first_text_value(raw_mapping, ("primary_anchor_type",))
        if explicit is not None:
            return explicit
        if isinstance(obj, IssueObject):
            return obj.primary_anchor_type
        if isinstance(obj, AlertObject):
            return "alert"
        if isinstance(obj, TimeAnchorObject):
            return "time_anchor"
        return None

    def _group_basis(self, raw_mapping: Mapping[str, Any], *, default_basis: GroupBasis) -> GroupBasis:
        raw_basis = _first_text_value(raw_mapping, ("group_basis",))
        if raw_basis is None:
            return default_basis
        try:
            return GroupBasis(raw_basis)
        except ValueError:
            return default_basis

    def _failure_reason_for(
        self,
        *,
        selected_object: IssueObject | AlertObject | TimeAnchorObject,
        raw_mapping: Mapping[str, Any],
        selected_anchor: ObserverAnchor | None,
        candidate_object_ids: Sequence[ObjectId],
        primary_association: PrimaryAssociationResult,
    ) -> FailureReason | None:
        state = primary_association.selection_state
        if state is ObserverState.RESOLVED:
            return None
        if state is ObserverState.EMPTY_BUT_EXPLAINED:
            return FailureReason.EMPTY_STATE_REASON
        if state is ObserverState.GROUP_NOT_MINIMIZED:
            return FailureReason.GROUP_NOT_MINIMIZED
        if state is ObserverState.WINDOW_NOT_RESOLVED:
            return FailureReason.WINDOW_NOT_RESOLVED
        if state is ObserverState.MAPPING_INSUFFICIENT:
            return FailureReason.MAPPING_INSUFFICIENT
        if state is ObserverState.MAPPING_MISSING:
            if isinstance(selected_object, IssueObject) and selected_anchor is None and not candidate_object_ids:
                if _looks_anchor_related(raw_mapping) or selected_object.primary_anchor_type:
                    return FailureReason.MAPPING_MISSING
                return FailureReason.ANCHOR_MISSING
            return FailureReason.MAPPING_MISSING
        return None


def _resolve_source_path(root: Mapping[str, Any], source_path: str) -> Any:
    current: Any = root
    for match in _SOURCE_TOKEN_RE.finditer(source_path):
        key = match.group(1)
        index = match.group(2)
        if key is not None:
            if not isinstance(current, Mapping) or key not in current:
                return None
            current = current[key]
            continue
        if index is not None:
            if not isinstance(current, Sequence) or isinstance(current, (str, bytes)):
                return None
            position = int(index)
            if position >= len(current):
                return None
            current = current[position]
    return current


def _first_text_value(data: Mapping[str, Any], keys: Sequence[str]) -> str | None:
    for key in keys:
        value = data.get(key)
        if value is None:
            continue
        text = str(value).strip()
        if text:
            return text
    return None


def _list_text_values(data: Mapping[str, Any], keys: Sequence[str]) -> tuple[ObjectId, ...]:
    values: list[ObjectId] = []
    for key in keys:
        raw_value = data.get(key)
        if not isinstance(raw_value, Sequence) or isinstance(raw_value, (str, bytes)):
            continue
        for item in raw_value:
            text = str(item).strip()
            if text:
                values.append(ObjectId(text))
    return tuple(values)


def _list_int_values(data: Mapping[str, Any], keys: Sequence[str]) -> tuple[int, ...]:
    values: list[int] = []
    for key in keys:
        raw_value = data.get(key)
        if not isinstance(raw_value, Sequence) or isinstance(raw_value, (str, bytes)):
            continue
        for item in raw_value:
            values.append(int(item))
    return tuple(values)


def _unique_ids(values: Sequence[ObjectId] | Sequence[str]) -> tuple[ObjectId, ...]:
    seen: set[str] = set()
    normalized: list[ObjectId] = []
    for value in values:
        text = str(value).strip()
        if not text or text in seen:
            continue
        seen.add(text)
        normalized.append(ObjectId(text))
    return tuple(normalized)


def _looks_anchor_related(raw_mapping: Mapping[str, Any]) -> bool:
    for key in (
        "primary_object_id",
        "object_id",
        "primary_group_id",
        "group_id",
        "segment_index",
        "region_id",
        "candidate_object_ids",
        "object_ids",
        "candidate_segment_indices",
        "segment_indices",
    ):
        if key in raw_mapping:
            return True
    return False


def _synthetic_time_anchor_for_alert(alert: AlertObject) -> TimeAnchorObject:
    return TimeAnchorObject(
        object_id=ObjectId(f"time_anchor:synthetic:{alert.object_id}"),
        object_type=ObjectType.TIME_ANCHOR,
        display_label=alert.display_label,
        source_refs=alert.source_refs,
        anchor_kind=TimeAnchorKind.TIME_POINT,
        time_start=alert.alert_time,
        time_end=None,
        source_kind="alert",
    )


__all__ = [
    "ObserverSelection",
    "ObserverStore",
    "P1Backlink",
    "P1Selection",
]
