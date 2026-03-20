"""Execute rule R6: derive the default alert context window."""

from __future__ import annotations

from typing import Any, Mapping

from ..contracts.observer_rules import ContextWindowRef, ensure_reason
from ..contracts.observer_status import ObserverState, REASON_ANCHOR_ONLY_FALLBACK
from ..contracts.observer_types import AlertObject


def resolve_alert_context_window(alert: AlertObject, raw_mapping: Mapping[str, Any]) -> ContextWindowRef:
    explicit_window = _explicit_window(raw_mapping)
    if explicit_window is not None:
        window_start, window_end = explicit_window
        return ContextWindowRef(
            window_start=window_start,
            window_end=window_end,
            anchor_time=alert.alert_time,
            window_basis="alert_context",
            window_state=ObserverState.RESOLVED,
        )

    pre_window_s = _float_value(raw_mapping, "pre_window_s")
    post_window_s = _float_value(raw_mapping, "post_window_s")
    if pre_window_s is not None and post_window_s is not None:
        return ContextWindowRef(
            window_start=max(0.0, alert.alert_time - pre_window_s),
            window_end=alert.alert_time + post_window_s,
            anchor_time=alert.alert_time,
            window_basis="alert_context",
            window_state=ObserverState.RESOLVED,
        )

    pre_window_ms = _float_value(raw_mapping, "pre_window_ms")
    post_window_ms = _float_value(raw_mapping, "post_window_ms")
    if pre_window_ms is not None and post_window_ms is not None:
        return ContextWindowRef(
            window_start=max(0.0, alert.alert_time - (pre_window_ms / 1000.0)),
            window_end=alert.alert_time + (post_window_ms / 1000.0),
            anchor_time=alert.alert_time,
            window_basis="alert_context",
            window_state=ObserverState.RESOLVED,
        )

    return ContextWindowRef(
        window_start=None,
        window_end=None,
        anchor_time=alert.alert_time,
        window_basis="anchor_only_fallback",
        window_state=ObserverState.MAPPING_INSUFFICIENT,
        reason=ensure_reason(
            REASON_ANCHOR_ONLY_FALLBACK,
            "Only the alert anchor time is available; a full context window cannot be derived.",
        ),
        fallback_mode="single_anchor",
    )


def _explicit_window(raw_mapping: Mapping[str, Any]) -> tuple[float, float] | None:
    for start_key, end_key in (("window_start", "window_end"), ("context_window_start", "context_window_end")):
        start_value = _float_value(raw_mapping, start_key)
        end_value = _float_value(raw_mapping, end_key)
        if start_value is not None and end_value is not None and end_value >= start_value:
            return (start_value, end_value)
    return None


def _float_value(raw_mapping: Mapping[str, Any], key: str) -> float | None:
    value = raw_mapping.get(key)
    if value is None:
        return None
    return float(value)


__all__ = ["resolve_alert_context_window"]
