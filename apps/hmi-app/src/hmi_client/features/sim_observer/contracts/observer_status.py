"""Unified observer states, failure reasons, and reason codes."""

from __future__ import annotations

from enum import Enum
from typing import Final


class ObserverState(str, Enum):
    RESOLVED = "resolved"
    EMPTY_BUT_EXPLAINED = "empty_but_explained"
    MAPPING_MISSING = "mapping_missing"
    MAPPING_INSUFFICIENT = "mapping_insufficient"
    GROUP_NOT_MINIMIZED = "group_not_minimized"
    WINDOW_NOT_RESOLVED = "window_not_resolved"


class FailureReason(str, Enum):
    ANCHOR_MISSING = "anchor_missing"
    MAPPING_MISSING = "mapping_missing"
    MAPPING_INSUFFICIENT = "mapping_insufficient"
    GROUP_NOT_MINIMIZED = "group_not_minimized"
    WINDOW_NOT_RESOLVED = "window_not_resolved"
    EMPTY_STATE_REASON = "empty_state_reason"


REASON_NO_DIRECT_RELATION: Final[str] = "no_direct_relation"
REASON_DIRECT_OBJECT_ANCHOR: Final[str] = "direct_object_anchor"
REASON_MINIMIZED_GROUP_SELECTED: Final[str] = "minimized_group_selected"
REASON_FALLBACK_TO_SUMMARY_ONLY: Final[str] = "fallback_to_summary_only"
REASON_GROUP_NOT_MINIMIZED: Final[str] = "group_not_minimized"
REASON_GROUP_MINIMIZED: Final[str] = "group_minimized"
REASON_GROUP_CONTAINS_REMOVABLE_MEMBERS: Final[str] = "group_contains_removable_members"
REASON_REGION_GEOMETRY_INCOMPLETE: Final[str] = "region_geometry_incomplete"
REASON_SPATIAL_MAPPING_MISSING: Final[str] = "spatial_mapping_missing"
REASON_PARTIAL_SPATIAL_MAPPING: Final[str] = "partial_spatial_mapping"
REASON_TIME_MAPPING_INSUFFICIENT: Final[str] = "time_mapping_insufficient"
REASON_CONCURRENT_CANDIDATES_PRESENT: Final[str] = "concurrent_candidates_present"
REASON_ANCHOR_ONLY_FALLBACK: Final[str] = "anchor_only_fallback"
REASON_WINDOW_NOT_RESOLVED: Final[str] = "window_not_resolved"
REASON_TIME_MAPPING_FROM_SNAPSHOT_POSITION: Final[str] = "time_mapping_from_snapshot_position"
REASON_TIME_MAPPING_FROM_SNAPSHOT_SEQUENCE: Final[str] = "time_mapping_from_snapshot_sequence"
REASON_TIME_MAPPING_SNAPSHOT_AMBIGUOUS: Final[str] = "time_mapping_snapshot_ambiguous"
REASON_KEY_WINDOW_RESOLVED: Final[str] = "key_window_resolved"
REASON_SEQUENCE_RANGE_RESOLVED: Final[str] = "sequence_range_resolved"
REASON_MAPPING_MISSING: Final[str] = "mapping_missing"


def is_failure_state(state: ObserverState) -> bool:
    return state in {
        ObserverState.MAPPING_MISSING,
        ObserverState.MAPPING_INSUFFICIENT,
        ObserverState.GROUP_NOT_MINIMIZED,
        ObserverState.WINDOW_NOT_RESOLVED,
    }


def is_terminal_state(state: ObserverState) -> bool:
    return state in {
        ObserverState.RESOLVED,
        ObserverState.EMPTY_BUT_EXPLAINED,
    }


def is_explained_empty(state: ObserverState) -> bool:
    return state is ObserverState.EMPTY_BUT_EXPLAINED


__all__ = [
    "FailureReason",
    "ObserverState",
    "REASON_ANCHOR_ONLY_FALLBACK",
    "REASON_CONCURRENT_CANDIDATES_PRESENT",
    "REASON_DIRECT_OBJECT_ANCHOR",
    "REASON_FALLBACK_TO_SUMMARY_ONLY",
    "REASON_GROUP_CONTAINS_REMOVABLE_MEMBERS",
    "REASON_GROUP_MINIMIZED",
    "REASON_GROUP_NOT_MINIMIZED",
    "REASON_KEY_WINDOW_RESOLVED",
    "REASON_MAPPING_MISSING",
    "REASON_MINIMIZED_GROUP_SELECTED",
    "REASON_NO_DIRECT_RELATION",
    "REASON_PARTIAL_SPATIAL_MAPPING",
    "REASON_REGION_GEOMETRY_INCOMPLETE",
    "REASON_SEQUENCE_RANGE_RESOLVED",
    "REASON_SPATIAL_MAPPING_MISSING",
    "REASON_TIME_MAPPING_INSUFFICIENT",
    "REASON_TIME_MAPPING_FROM_SNAPSHOT_POSITION",
    "REASON_TIME_MAPPING_FROM_SNAPSHOT_SEQUENCE",
    "REASON_TIME_MAPPING_SNAPSHOT_AMBIGUOUS",
    "REASON_WINDOW_NOT_RESOLVED",
    "is_explained_empty",
    "is_failure_state",
    "is_terminal_state",
]
