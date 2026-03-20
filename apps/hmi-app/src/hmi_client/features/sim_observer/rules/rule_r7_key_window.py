"""Execute rule R7: converge or expose the key analysis window."""

from __future__ import annotations

from typing import Any, Mapping

from ..contracts.observer_rules import ContextWindowRef, KeyWindowResult, ensure_reason
from ..contracts.observer_status import ObserverState, REASON_KEY_WINDOW_RESOLVED, REASON_WINDOW_NOT_RESOLVED


def resolve_key_window(
    raw_mapping: Mapping[str, Any],
    *,
    anchor_time: float,
    context_window: ContextWindowRef | None = None,
    default_start: float | None = None,
    default_end: float | None = None,
    default_basis: str = "time_anchor",
) -> KeyWindowResult:
    explicit_window = _explicit_window(raw_mapping)
    if explicit_window is not None:
        key_window_start, key_window_end = explicit_window
        return KeyWindowResult(
            key_window_start=key_window_start,
            key_window_end=key_window_end,
            window_basis="key_window",
            window_state=ObserverState.RESOLVED,
            reason=ensure_reason(
                REASON_KEY_WINDOW_RESOLVED,
                "A stable key window is explicitly available.",
            ),
        )

    if raw_mapping.get("key_window_unresolved") is True:
        return KeyWindowResult(
            key_window_start=None,
            key_window_end=None,
            window_basis="key_window",
            window_state=ObserverState.WINDOW_NOT_RESOLVED,
            reason=ensure_reason(
                REASON_WINDOW_NOT_RESOLVED,
                "The current context window cannot be converged into a key window.",
            ),
        )

    if context_window is not None and context_window.window_state is ObserverState.RESOLVED:
        return KeyWindowResult(
            key_window_start=context_window.window_start,
            key_window_end=context_window.window_end,
            window_basis="context_window",
            window_state=ObserverState.RESOLVED,
            reason=ensure_reason(
                REASON_KEY_WINDOW_RESOLVED,
                "The resolved context window is already compact enough to be used as the key window.",
            ),
        )

    if default_start is not None and default_end is not None and default_end >= default_start:
        return KeyWindowResult(
            key_window_start=default_start,
            key_window_end=default_end,
            window_basis=default_basis,
            window_state=ObserverState.RESOLVED,
            reason=ensure_reason(
                REASON_KEY_WINDOW_RESOLVED,
                "The anchor-local default range is used as the key window.",
            ),
        )

    return KeyWindowResult(
        key_window_start=None,
        key_window_end=None,
        window_basis=default_basis,
        window_state=ObserverState.WINDOW_NOT_RESOLVED,
        reason=ensure_reason(
            REASON_WINDOW_NOT_RESOLVED,
            f"A key window cannot be resolved from anchor_time={anchor_time:.3f}.",
        ),
    )


def _explicit_window(raw_mapping: Mapping[str, Any]) -> tuple[float, float] | None:
    for start_key, end_key in (("key_window_start", "key_window_end"),):
        start_value = raw_mapping.get(start_key)
        end_value = raw_mapping.get(end_key)
        if start_value is None or end_value is None:
            continue
        start = float(start_value)
        end = float(end_value)
        if end >= start:
            return (start, end)
    return None


__all__ = ["resolve_key_window"]
