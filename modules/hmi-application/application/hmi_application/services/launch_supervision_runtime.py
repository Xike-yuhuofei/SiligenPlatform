from __future__ import annotations

from dataclasses import replace
from uuid import uuid4

from ..contracts.launch_supervision_contract import (
    BackendState,
    FailureCode,
    FailureStage,
    HardwareState,
    SessionSnapshot,
    SessionStageEvent,
    TcpState,
    is_online_ready,
    snapshot_timestamp,
)
from ..domain.launch_supervision_types import ONLINE_READY_STAGE


def build_runtime_failure_snapshot(
    snapshot: SessionSnapshot,
    *,
    failure_code: FailureCode,
    failure_stage: FailureStage,
    message: str,
    backend_state: BackendState | None = None,
    tcp_state: TcpState | None = None,
    hardware_state: HardwareState | None = None,
    recoverable: bool = True,
) -> SessionSnapshot:
    return replace(
        snapshot,
        session_state="failed",
        backend_state=backend_state or snapshot.backend_state,
        tcp_state=tcp_state or snapshot.tcp_state,
        hardware_state=hardware_state or snapshot.hardware_state,
        failure_code=failure_code,
        failure_stage=failure_stage,
        recoverable=recoverable,
        last_error_message=message,
        updated_at=snapshot_timestamp(),
    )


def detect_runtime_degradation(
    snapshot: SessionSnapshot | None,
    *,
    tcp_connected: bool | None = None,
    hardware_ready: bool | None = None,
    error_message: str | None = None,
) -> SessionSnapshot | None:
    if snapshot is None or not is_online_ready(snapshot):
        return None

    if tcp_connected is False:
        return build_runtime_failure_snapshot(
            snapshot,
            failure_code="SUP_TCP_CONNECT_FAILED",
            failure_stage="tcp_ready",
            message=error_message or "Runtime TCP connection lost.",
            tcp_state="failed",
            hardware_state="unavailable",
            recoverable=True,
        )

    if hardware_ready is False:
        return build_runtime_failure_snapshot(
            snapshot,
            failure_code="SUP_HARDWARE_CONNECT_FAILED",
            failure_stage="hardware_ready",
            message=error_message or "Runtime hardware state unavailable.",
            hardware_state="failed",
            recoverable=True,
        )

    return None


def detect_runtime_degradation_with_event(
    snapshot: SessionSnapshot | None,
    *,
    session_id: str | None = None,
    tcp_connected: bool | None = None,
    hardware_ready: bool | None = None,
    error_message: str | None = None,
) -> tuple[SessionSnapshot, SessionStageEvent] | None:
    degraded = detect_runtime_degradation(
        snapshot,
        tcp_connected=tcp_connected,
        hardware_ready=hardware_ready,
        error_message=error_message,
    )
    if degraded is None:
        return None

    event = SessionStageEvent(
        event_type="stage_failed",
        session_id=session_id or uuid4().hex,
        stage=degraded.failure_stage or ONLINE_READY_STAGE,
        timestamp=snapshot_timestamp(),
        failure_code=degraded.failure_code,
        recoverable=degraded.recoverable,
        message=degraded.last_error_message,
    )
    return degraded, event
