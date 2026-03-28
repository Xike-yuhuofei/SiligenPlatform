"""Supervisor session contract for HMI startup truth source."""
from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Literal

LaunchMode = Literal["online", "offline"]
SessionState = Literal["idle", "starting", "ready", "degraded", "failed", "stopping"]
BackendState = Literal["stopped", "starting", "ready", "failed"]
TcpState = Literal["disconnected", "connecting", "ready", "failed"]
HardwareState = Literal["unavailable", "probing", "ready", "failed"]
FailureStage = Literal[
    "backend_starting",
    "backend_ready",
    "tcp_connecting",
    "tcp_ready",
    "hardware_probing",
    "hardware_ready",
    "online_ready",
]
FailureCode = Literal[
    "SUP_BACKEND_AUTOSTART_DISABLED",
    "SUP_BACKEND_SPEC_MISSING",
    "SUP_BACKEND_EXE_NOT_FOUND",
    "SUP_BACKEND_START_FAILED",
    "SUP_BACKEND_READY_TIMEOUT",
    "SUP_TCP_CONNECT_FAILED",
    "SUP_HARDWARE_CONNECT_FAILED",
    "SUP_BACKEND_EXITED",
    "SUP_UNKNOWN",
]
RecoveryAction = Literal["retry_stage", "restart_session", "stop_session"]
StageEventType = Literal["stage_entered", "stage_succeeded", "stage_failed", "recovery_invoked"]

VALID_SESSION_STATES = {"idle", "starting", "ready", "degraded", "failed", "stopping"}
VALID_FAILURE_STAGES = {
    "backend_starting",
    "backend_ready",
    "tcp_connecting",
    "tcp_ready",
    "hardware_probing",
    "hardware_ready",
    "online_ready",
}
VALID_FAILURE_CODES = {
    "SUP_BACKEND_AUTOSTART_DISABLED",
    "SUP_BACKEND_SPEC_MISSING",
    "SUP_BACKEND_EXE_NOT_FOUND",
    "SUP_BACKEND_START_FAILED",
    "SUP_BACKEND_READY_TIMEOUT",
    "SUP_TCP_CONNECT_FAILED",
    "SUP_HARDWARE_CONNECT_FAILED",
    "SUP_BACKEND_EXITED",
    "SUP_UNKNOWN",
}
VALID_STAGE_EVENT_TYPES = {
    "stage_entered",
    "stage_succeeded",
    "stage_failed",
    "recovery_invoked",
}


def snapshot_timestamp() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


@dataclass(frozen=True)
class SessionSnapshot:
    mode: LaunchMode
    session_state: SessionState
    backend_state: BackendState
    tcp_state: TcpState
    hardware_state: HardwareState
    failure_code: FailureCode | None
    failure_stage: FailureStage | None
    recoverable: bool
    last_error_message: str | None
    updated_at: str

    def __post_init__(self) -> None:
        if self.session_state not in VALID_SESSION_STATES:
            raise ValueError(f"Invalid session_state: {self.session_state}")
        if self.failure_stage is not None and self.failure_stage not in VALID_FAILURE_STAGES:
            raise ValueError(f"Invalid failure_stage: {self.failure_stage}")
        if self.failure_code is not None and self.failure_code not in VALID_FAILURE_CODES:
            raise ValueError(f"Invalid failure_code: {self.failure_code}")
        if self.session_state == "failed":
            if self.failure_code is None or self.failure_stage is None:
                raise ValueError("failed snapshot requires failure_code and failure_stage")
        elif self.failure_code is not None or self.failure_stage is not None:
            raise ValueError("non-failed snapshot must not carry failure_code/failure_stage")

    @property
    def online_ready(self) -> bool:
        return is_online_ready(self)


@dataclass(frozen=True)
class SessionStageEvent:
    event_type: StageEventType
    session_id: str
    stage: FailureStage
    timestamp: str
    failure_code: FailureCode | None = None
    recoverable: bool | None = None
    message: str | None = None

    def __post_init__(self) -> None:
        if self.event_type not in VALID_STAGE_EVENT_TYPES:
            raise ValueError(f"Invalid event_type: {self.event_type}")
        if self.stage not in VALID_FAILURE_STAGES:
            raise ValueError(f"Invalid stage: {self.stage}")
        if self.failure_code is not None and self.failure_code not in VALID_FAILURE_CODES:
            raise ValueError(f"Invalid failure_code: {self.failure_code}")


def is_online_ready(snapshot: SessionSnapshot | None) -> bool:
    if snapshot is None:
        return False
    return (
        snapshot.mode == "online"
        and snapshot.session_state == "ready"
        and snapshot.backend_state == "ready"
        and snapshot.tcp_state == "ready"
        and snapshot.hardware_state == "ready"
        and snapshot.failure_code is None
    )
