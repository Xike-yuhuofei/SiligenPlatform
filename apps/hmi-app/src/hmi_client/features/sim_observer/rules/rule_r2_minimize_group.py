"""Execute rule R2: minimize a candidate object group."""

from __future__ import annotations

from typing import Iterable, Sequence

from ..contracts.observer_rules import GroupMinimizationResult, ensure_reason
from ..contracts.observer_status import (
    ObserverState,
    REASON_GROUP_CONTAINS_REMOVABLE_MEMBERS,
    REASON_GROUP_MINIMIZED,
    REASON_MAPPING_MISSING,
)
from ..contracts.observer_types import GroupBasis, ObjectId


def minimize_candidate_group(
    candidate_object_ids: Sequence[ObjectId | str],
    group_basis: GroupBasis,
    *,
    group_id: ObjectId | None = None,
    removable_object_ids: Iterable[ObjectId | str] = (),
) -> GroupMinimizationResult:
    members = _normalize_object_ids(candidate_object_ids)
    removable_members = tuple(
        member_id for member_id in _normalize_object_ids(removable_object_ids) if member_id in members
    )

    if not members:
        return GroupMinimizationResult(
            group_id=None,
            group_basis=group_basis,
            member_object_ids=tuple(),
            group_cardinality=0,
            is_minimized=False,
            minimization_reason=ensure_reason(
                REASON_MAPPING_MISSING,
                "No candidate objects are available for group minimization.",
            ),
            state=ObserverState.MAPPING_MISSING,
        )

    resolved_group_id = group_id or _make_group_id(group_basis, members)
    if removable_members:
        return GroupMinimizationResult(
            group_id=resolved_group_id,
            group_basis=group_basis,
            member_object_ids=members,
            group_cardinality=len(members),
            is_minimized=False,
            minimization_reason=ensure_reason(
                REASON_GROUP_CONTAINS_REMOVABLE_MEMBERS,
                "The candidate set still contains removable members and is not minimal.",
            ),
            state=ObserverState.GROUP_NOT_MINIMIZED,
        )

    return GroupMinimizationResult(
        group_id=resolved_group_id,
        group_basis=group_basis,
        member_object_ids=members,
        group_cardinality=len(members),
        is_minimized=True,
        minimization_reason=ensure_reason(
            REASON_GROUP_MINIMIZED,
            "The candidate set is already the minimal explainable group.",
        ),
        state=ObserverState.RESOLVED,
    )


def _normalize_object_ids(values: Iterable[ObjectId | str]) -> tuple[ObjectId, ...]:
    normalized: list[ObjectId] = []
    seen: set[str] = set()
    for value in values:
        text = str(value).strip()
        if not text:
            raise ValueError("Object ids used for group minimization must not be empty.")
        if text in seen:
            continue
        seen.add(text)
        normalized.append(ObjectId(text))
    return tuple(normalized)


def _make_group_id(group_basis: GroupBasis, members: tuple[ObjectId, ...]) -> ObjectId:
    serialized_members = ",".join(str(member_id) for member_id in members)
    return ObjectId(f"group:{group_basis.value}:{serialized_members}")


__all__ = ["minimize_candidate_group"]
