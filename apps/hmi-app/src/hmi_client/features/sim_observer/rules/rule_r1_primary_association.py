"""Execute rule R1: resolve the default primary association."""

from __future__ import annotations

from typing import Sequence

from ..adapters.recording_normalizer import NormalizedObserverData
from ..contracts.observer_rules import GroupMinimizationResult, ObserverAnchor, PrimaryAssociationResult, ensure_reason
from ..contracts.observer_status import (
    ObserverState,
    REASON_DIRECT_OBJECT_ANCHOR,
    REASON_FALLBACK_TO_SUMMARY_ONLY,
    REASON_MAPPING_MISSING,
    REASON_MINIMIZED_GROUP_SELECTED,
)
from ..contracts.observer_types import AlertAnchor, ObjectAnchor, ObjectId, RegionAnchor, TimeAnchor


def resolve_primary_association(
    anchor: ObserverAnchor | None,
    data: NormalizedObserverData,
    *,
    candidate_object_ids: Sequence[ObjectId | str] = (),
    group_result: GroupMinimizationResult | None = None,
    primary_anchor_type: str | None = None,
) -> PrimaryAssociationResult:
    context_anchors = (anchor,) if anchor is not None else tuple()
    normalized_candidates = _existing_candidates(candidate_object_ids, data)
    had_candidate_ids = bool(candidate_object_ids)
    direct_object_id = _resolve_direct_object_id(anchor, data)

    if direct_object_id is not None:
        return PrimaryAssociationResult(
            association_level="single_object",
            primary_object_id=direct_object_id,
            primary_anchor_type=primary_anchor_type or _anchor_type_name(anchor),
            context_anchors=context_anchors,
            selection_reason=ensure_reason(
                REASON_DIRECT_OBJECT_ANCHOR,
                "Resolved directly from a stable object anchor.",
            ),
            selection_state=ObserverState.RESOLVED,
        )

    direct_group_id = _resolve_direct_group_id(anchor)
    if direct_group_id is not None:
        return PrimaryAssociationResult(
            association_level="object_group",
            primary_group_id=direct_group_id,
            primary_anchor_type=primary_anchor_type or _anchor_type_name(anchor),
            context_anchors=context_anchors,
            selection_reason=ensure_reason(
                REASON_MINIMIZED_GROUP_SELECTED,
                "Resolved directly from an existing group anchor.",
            ),
            selection_state=ObserverState.RESOLVED,
        )

    if group_result is not None:
        if group_result.state is ObserverState.RESOLVED and group_result.group_cardinality == 1:
            return PrimaryAssociationResult(
                association_level="single_object",
                primary_object_id=group_result.member_object_ids[0],
                primary_anchor_type=primary_anchor_type or "candidate_object",
                context_anchors=context_anchors,
                selection_reason=ensure_reason(
                    REASON_MINIMIZED_GROUP_SELECTED,
                    "A single stable object remained after group minimization.",
                ),
                selection_state=ObserverState.RESOLVED,
            )
        if group_result.state is ObserverState.RESOLVED and group_result.group_id is not None:
            return PrimaryAssociationResult(
                association_level="object_group",
                primary_group_id=group_result.group_id,
                primary_anchor_type=primary_anchor_type or "object_group",
                context_anchors=context_anchors,
                selection_reason=group_result.minimization_reason
                or ensure_reason(
                    REASON_MINIMIZED_GROUP_SELECTED,
                    "Resolved to the minimized object group.",
                ),
                selection_state=ObserverState.RESOLVED,
            )
        return _summary_only_result(
            anchor=anchor,
            selection_state=group_result.state,
            reason_code=(group_result.minimization_reason.reason_code if group_result.minimization_reason else REASON_FALLBACK_TO_SUMMARY_ONLY),
            reason_text=(
                group_result.minimization_reason.reason_text
                if group_result.minimization_reason
                else "A concrete target cannot be selected from the current group result."
            ),
            primary_anchor_type=primary_anchor_type,
        )

    if len(normalized_candidates) == 1:
        return PrimaryAssociationResult(
            association_level="single_object",
            primary_object_id=normalized_candidates[0],
            primary_anchor_type=primary_anchor_type or "candidate_object",
            context_anchors=context_anchors,
            selection_reason=ensure_reason(
                REASON_MINIMIZED_GROUP_SELECTED,
                "A single stable candidate object is available.",
            ),
            selection_state=ObserverState.RESOLVED,
        )

    if len(normalized_candidates) > 1:
        return _summary_only_result(
            anchor=anchor,
            selection_state=ObserverState.GROUP_NOT_MINIMIZED,
            reason_code=REASON_FALLBACK_TO_SUMMARY_ONLY,
            reason_text="Multiple candidate objects remain and require group minimization.",
            primary_anchor_type=primary_anchor_type,
        )

    if had_candidate_ids:
        return _summary_only_result(
            anchor=anchor,
            selection_state=ObserverState.MAPPING_MISSING,
            reason_code=REASON_MAPPING_MISSING,
            reason_text="The candidate object ids do not match any normalized observer object.",
            primary_anchor_type=primary_anchor_type,
        )

    if anchor is None:
        return _summary_only_result(
            anchor=None,
            selection_state=ObserverState.MAPPING_MISSING,
            reason_code=REASON_MAPPING_MISSING,
            reason_text="No anchor or candidate object ids are available.",
            primary_anchor_type=primary_anchor_type,
        )

    return _summary_only_result(
        anchor=anchor,
        selection_state=_fallback_state_for_anchor(anchor),
        reason_code=REASON_FALLBACK_TO_SUMMARY_ONLY,
        reason_text="The current anchor does not contain enough information to resolve a concrete target.",
        primary_anchor_type=primary_anchor_type or _anchor_type_name(anchor),
    )


def _summary_only_result(
    *,
    anchor: ObserverAnchor | None,
    selection_state: ObserverState,
    reason_code: str,
    reason_text: str,
    primary_anchor_type: str | None,
) -> PrimaryAssociationResult:
    context_anchors = (anchor,) if anchor is not None else tuple()
    return PrimaryAssociationResult(
        association_level="summary_only",
        primary_anchor_type=primary_anchor_type or _anchor_type_name(anchor),
        context_anchors=context_anchors,
        selection_reason=ensure_reason(reason_code, reason_text),
        selection_state=selection_state,
    )


def _existing_candidates(
    candidate_object_ids: Sequence[ObjectId | str],
    data: NormalizedObserverData,
) -> tuple[ObjectId, ...]:
    normalized: list[ObjectId] = []
    seen: set[str] = set()
    for value in candidate_object_ids:
        text = str(value).strip()
        if not text or text in seen:
            continue
        if ObjectId(text) not in data.objects_by_id:
            continue
        seen.add(text)
        normalized.append(ObjectId(text))
    return tuple(normalized)


def _resolve_direct_object_id(anchor: ObserverAnchor | None, data: NormalizedObserverData) -> ObjectId | None:
    if isinstance(anchor, ObjectAnchor) and anchor.object_id is not None:
        if anchor.object_id in data.objects_by_id:
            return anchor.object_id
        return None
    if isinstance(anchor, RegionAnchor) and anchor.region_id in data.objects_by_id:
        return anchor.region_id
    return None


def _resolve_direct_group_id(anchor: ObserverAnchor | None) -> ObjectId | None:
    if isinstance(anchor, ObjectAnchor) and anchor.object_id is None:
        return anchor.group_id
    return None


def _fallback_state_for_anchor(anchor: ObserverAnchor) -> ObserverState:
    if isinstance(anchor, (AlertAnchor, TimeAnchor)):
        return ObserverState.MAPPING_INSUFFICIENT
    if isinstance(anchor, RegionAnchor):
        return ObserverState.MAPPING_MISSING
    if isinstance(anchor, ObjectAnchor):
        return ObserverState.MAPPING_MISSING
    return ObserverState.MAPPING_INSUFFICIENT


def _anchor_type_name(anchor: ObserverAnchor | None) -> str | None:
    if anchor is None:
        return None
    if isinstance(anchor, ObjectAnchor):
        return "object"
    if isinstance(anchor, RegionAnchor):
        return "region"
    if isinstance(anchor, TimeAnchor):
        return "time_anchor"
    if isinstance(anchor, AlertAnchor):
        return "alert"
    return None


__all__ = ["resolve_primary_association"]
