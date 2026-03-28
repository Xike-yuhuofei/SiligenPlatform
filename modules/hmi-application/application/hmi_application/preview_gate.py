from __future__ import annotations

from dataclasses import dataclass
from enum import Enum


class PreviewGateState(str, Enum):
    DIRTY = "dirty"
    GENERATING = "generating"
    READY_UNSIGNED = "ready_unsigned"
    READY_SIGNED = "ready_signed"
    STALE = "stale"
    FAILED = "failed"


class StartBlockReason(str, Enum):
    NONE = ""
    NOT_READY = "not_ready"
    PREVIEW_MISSING = "preview_missing"
    PREVIEW_GENERATING = "preview_generating"
    PREVIEW_FAILED = "preview_failed"
    PREVIEW_STALE = "preview_stale"
    CONFIRM_MISSING = "confirm_missing"
    INVALID_SOURCE = "invalid_source"
    HASH_MISMATCH = "hash_mismatch"


@dataclass(frozen=True)
class PreviewSnapshotMeta:
    snapshot_id: str
    snapshot_hash: str
    segment_count: int
    point_count: int
    total_length_mm: float
    estimated_time_s: float
    generated_at: str


@dataclass(frozen=True)
class PreviewGateDecision:
    allowed: bool
    reason: StartBlockReason


class DispensePreviewGate:
    """Single source of truth for preview gate state used by production start."""

    def __init__(self) -> None:
        self._state = PreviewGateState.DIRTY
        self._snapshot: PreviewSnapshotMeta | None = None
        self._confirmed_snapshot_hash = ""
        self._last_error_message = ""

    @property
    def state(self) -> PreviewGateState:
        return self._state

    @property
    def snapshot(self) -> PreviewSnapshotMeta | None:
        return self._snapshot

    @property
    def last_error_message(self) -> str:
        return self._last_error_message

    def reset_for_loaded_dxf(self) -> None:
        self._state = PreviewGateState.DIRTY
        self._snapshot = None
        self._confirmed_snapshot_hash = ""
        self._last_error_message = ""

    def mark_input_changed(self) -> None:
        if self._snapshot is None:
            self._state = PreviewGateState.DIRTY
        else:
            self._state = PreviewGateState.STALE
        self._confirmed_snapshot_hash = ""

    def begin_preview(self) -> None:
        self._state = PreviewGateState.GENERATING
        self._last_error_message = ""
        self._confirmed_snapshot_hash = ""

    def preview_ready(self, snapshot: PreviewSnapshotMeta) -> None:
        self._snapshot = snapshot
        self._state = PreviewGateState.READY_UNSIGNED
        self._confirmed_snapshot_hash = ""
        self._last_error_message = ""

    def preview_failed(self, message: str) -> None:
        self._state = PreviewGateState.FAILED
        self._confirmed_snapshot_hash = ""
        self._last_error_message = message.strip()

    def confirm_current_snapshot(self) -> bool:
        if self._snapshot is None or not self._snapshot.snapshot_hash:
            return False
        self._confirmed_snapshot_hash = self._snapshot.snapshot_hash
        self._state = PreviewGateState.READY_SIGNED
        return True

    def get_confirmed_snapshot_hash(self) -> str:
        return self._confirmed_snapshot_hash

    def validate_online_ready(self, online_ready: bool) -> PreviewGateDecision:
        if not online_ready:
            return PreviewGateDecision(False, StartBlockReason.NOT_READY)
        return PreviewGateDecision(True, StartBlockReason.NONE)

    def validate_preview_source(self, preview_source: str) -> PreviewGateDecision:
        normalized = str(preview_source or "").strip().lower()
        if normalized != "planned_glue_snapshot":
            return PreviewGateDecision(False, StartBlockReason.INVALID_SOURCE)
        return PreviewGateDecision(True, StartBlockReason.NONE)

    def validate_execution_snapshot_hash(self, runtime_hash: str) -> PreviewGateDecision:
        if not self._confirmed_snapshot_hash:
            return PreviewGateDecision(False, StartBlockReason.CONFIRM_MISSING)
        if runtime_hash != self._confirmed_snapshot_hash:
            return PreviewGateDecision(False, StartBlockReason.HASH_MISMATCH)
        return PreviewGateDecision(True, StartBlockReason.NONE)

    def decision_for_start(self) -> PreviewGateDecision:
        if self._state == PreviewGateState.READY_SIGNED:
            return PreviewGateDecision(True, StartBlockReason.NONE)
        if self._state == PreviewGateState.GENERATING:
            return PreviewGateDecision(False, StartBlockReason.PREVIEW_GENERATING)
        if self._state == PreviewGateState.FAILED:
            return PreviewGateDecision(False, StartBlockReason.PREVIEW_FAILED)
        if self._state == PreviewGateState.STALE:
            return PreviewGateDecision(False, StartBlockReason.PREVIEW_STALE)
        if self._state == PreviewGateState.READY_UNSIGNED:
            return PreviewGateDecision(False, StartBlockReason.CONFIRM_MISSING)
        return PreviewGateDecision(False, StartBlockReason.PREVIEW_MISSING)
