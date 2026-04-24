"""Launch supervision contract for HMI startup truth source."""
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
    "runtime_contract_ready",
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
    "SUP_RUNTIME_STATUS_FAILED",
    "SUP_RUNTIME_CONTRACT_MISMATCH",
    "SUP_HARDWARE_CONNECT_FAILED",
    "SUP_RUNTIME_HARDWARE_STATE_FAILED",
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
    "runtime_contract_ready",
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
    "SUP_RUNTIME_STATUS_FAILED",
    "SUP_RUNTIME_CONTRACT_MISMATCH",
    "SUP_HARDWARE_CONNECT_FAILED",
    "SUP_RUNTIME_HARDWARE_STATE_FAILED",
    "SUP_BACKEND_EXITED",
    "SUP_UNKNOWN",
}
VALID_STAGE_EVENT_TYPES = {
    "stage_entered",
    "stage_succeeded",
    "stage_failed",
    "recovery_invoked",
}

RUNTIME_PROTOCOL_VERSION = "siligen.application/1.0"
PREVIEW_SNAPSHOT_CONTRACT = "planned_glue_snapshot.glue_points+execution_trajectory_snapshot.polyline"


def snapshot_timestamp() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


@dataclass(frozen=True)
class RuntimeIdentity:
    executable_path: str
    working_directory: str
    protocol_version: str = RUNTIME_PROTOCOL_VERSION
    preview_snapshot_contract: str = PREVIEW_SNAPSHOT_CONTRACT

    def is_complete(self) -> bool:
        return all(
            (
                self.executable_path.strip(),
                self.working_directory.strip(),
                self.protocol_version.strip(),
                self.preview_snapshot_contract.strip(),
            )
        )


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
    runtime_contract_verified: bool = False
    runtime_identity: RuntimeIdentity | None = None

    def __post_init__(self) -> None:
        if self.session_state not in VALID_SESSION_STATES:
            raise ValueError(f"Invalid session_state: {self.session_state}")
        if self.failure_stage is not None and self.failure_stage not in VALID_FAILURE_STAGES:
            raise ValueError(f"Invalid failure_stage: {self.failure_stage}")
        if self.failure_code is not None and self.failure_code not in VALID_FAILURE_CODES:
            raise ValueError(f"Invalid failure_code: {self.failure_code}")
        if self.mode != "online":
            if self.runtime_contract_verified:
                raise ValueError("offline snapshot must not set runtime_contract_verified")
            if self.runtime_identity is not None:
                raise ValueError("offline snapshot must not carry runtime_identity")
        if self.runtime_contract_verified and self.runtime_identity is None:
            raise ValueError("runtime_contract_verified requires runtime_identity")
        if self.runtime_identity is not None and not self.runtime_identity.is_complete():
            raise ValueError("runtime_identity must be complete when present")
        if self.session_state == "failed":
            if self.failure_code is None or self.failure_stage is None:
                raise ValueError("failed snapshot requires failure_code and failure_stage")
        elif self.failure_code is not None or self.failure_stage is not None:
            raise ValueError("non-failed snapshot must not carry failure_code/failure_stage")
        if self.mode == "online" and self.session_state == "ready" and not self.runtime_contract_verified:
            raise ValueError("ready online snapshot requires runtime_contract_verified")

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
        and snapshot.runtime_contract_verified
        and snapshot.runtime_identity is not None
        and snapshot.failure_code is None
    )
