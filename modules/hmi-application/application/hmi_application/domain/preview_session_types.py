from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Protocol

from ..preview_gate import DispensePreviewGate, PreviewSnapshotMeta


class PreviewStatusLike(Protocol):
    connected: bool

    def gate_estop_known(self) -> bool: ...

    def gate_door_known(self) -> bool: ...

    def gate_estop_active(self) -> bool: ...

    def gate_door_active(self) -> bool: ...


@dataclass
class PreviewSessionState:
    gate: DispensePreviewGate
    preview_source: str = ""
    preview_kind: str = ""
    glue_point_count: int = 0
    motion_preview_source: str = ""
    motion_preview_kind: str = ""
    motion_preview_point_count: int = 0
    motion_preview_source_point_count: int = 0
    motion_preview_sampling_strategy: str = ""
    motion_preview_is_sampled: bool = False
    preview_validation_classification: str = ""
    preview_exception_reason: str = ""
    preview_failure_reason: str = ""
    preview_diagnostic_code: str = ""
    path_quality_verdict: str = ""
    path_quality_blocking: bool = False
    path_quality_reason_codes: tuple[str, ...] = ()
    path_quality_summary: str = ""
    path_quality_source_plan_id: str = ""
    current_plan_id: str = ""
    current_plan_fingerprint: str = ""
    preview_plan_dry_run: bool | None = None
    preview_refresh_inflight: bool = False
    preview_state_resync_pending: bool = False
    last_preview_resync_attempt_ts: float = 0.0
    dxf_segment_count: int = 0
    dxf_total_length_mm: float = 0.0
    dxf_estimated_time_text: str = "-"
    local_playback_state: str = "idle"
    local_playback_speed_ratio: float = 1.0
    local_playback_progress: float = 0.0
    local_playback_elapsed_s: float = 0.0
    local_playback_duration_s: float = 0.0
    local_playback_point_count: int = 0


class PreflightBlockReason(str, Enum):
    NONE = ""
    NOT_CONNECTED = "not_connected"
    HARDWARE_NOT_READY = "hardware_not_ready"
    RUNTIME_STATUS_FAULT = "runtime_status_fault"
    PREVIEW_GENERATING = "preview_generating"
    STATUS_UNAVAILABLE = "status_unavailable"
    INTERLOCK_UNKNOWN = "interlock_unknown"
    ESTOP_ACTIVE = "estop_active"
    DOOR_ACTIVE = "door_active"
    PREVIEW_PLAN_MISSING = "preview_plan_missing"
    PREVIEW_MODE_UNKNOWN = "preview_mode_unknown"
    PREVIEW_MODE_MISMATCH = "preview_mode_mismatch"
    INVALID_SOURCE = "invalid_source"
    INVALID_KIND = "invalid_kind"
    EMPTY_GLUE_POINTS = "empty_glue_points"
    PREVIEW_CONFIRM_REQUIRED = "preview_confirm_required"
    PRODUCTION_BLOCKED = "production_blocked"
    PREVIEW_FAILED = "preview_failed"
    PREVIEW_STALE = "preview_stale"
    HASH_MISMATCH = "hash_mismatch"
    NOT_READY = "not_ready"


@dataclass(frozen=True)
class PreflightDecision:
    allowed: bool
    reason: PreflightBlockReason = PreflightBlockReason.NONE
    message: str = ""
    requires_confirmation: bool = False


@dataclass(frozen=True)
class PreviewConfirmResult:
    ok: bool
    message: str = ""


@dataclass(frozen=True)
class MotionPreviewMeta:
    source: str = ""
    kind: str = ""
    point_count: int = 0
    source_point_count: int = 0
    is_sampled: bool = False
    sampling_strategy: str = ""


@dataclass(frozen=True)
class PreviewDiagnosticNotice:
    title: str = ""
    detail: str = ""


@dataclass(frozen=True)
class PreviewBindingMeta:
    source: str = ""
    status: str = ""
    layout_id: str = ""
    glue_point_count: int = 0
    binding_basis: str = ""
    display_path_length_mm: float = 0.0
    source_trigger_indices: tuple[int, ...] = ()
    diagnostic_code: str = ""
    failure_reason: str = ""


@dataclass(frozen=True)
class PreviewPayloadResult:
    ok: bool
    title: str
    detail: str
    status_message: str
    is_error: bool
    should_render: bool = False
    snapshot: PreviewSnapshotMeta | None = None
    glue_points: tuple[tuple[float, float], ...] = ()
    glue_reveal_lengths_mm: tuple[float, ...] = ()
    display_reveal_lengths_mm: tuple[float, ...] = ()
    motion_preview: tuple[tuple[float, float], ...] = ()
    motion_preview_meta: MotionPreviewMeta | None = None
    preview_binding_meta: PreviewBindingMeta | None = None
    preview_source: str = ""
    preview_kind: str = "glue_points"
    dry_run: bool = False
    preview_warning: str = ""
    preview_diagnostic_notice: PreviewDiagnosticNotice | None = None
    motion_preview_warning: str = ""


__all__ = [
    "MotionPreviewMeta",
    "PreflightBlockReason",
    "PreflightDecision",
    "PreviewBindingMeta",
    "PreviewConfirmResult",
    "PreviewDiagnosticNotice",
    "PreviewPayloadResult",
    "PreviewSessionState",
    "PreviewStatusLike",
]
