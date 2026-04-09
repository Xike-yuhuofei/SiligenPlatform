from __future__ import annotations

from dataclasses import dataclass

from ..contracts.launch_supervision_contract import LaunchMode, SessionSnapshot, is_online_ready


@dataclass
class LaunchResult:
    """Unified launch outcome consumed by the UI layer."""

    requested_mode: LaunchMode
    effective_mode: LaunchMode
    phase: str
    success: bool
    backend_started: bool
    tcp_connected: bool
    hardware_ready: bool
    user_message: str
    failure_code: str | None = None
    failure_stage: str | None = None
    recoverable: bool = True
    session_state: str = "idle"
    session_snapshot: SessionSnapshot | None = None

    def __post_init__(self) -> None:
        if self.session_snapshot is None:
            return
        snapshot = self.session_snapshot
        self.effective_mode = snapshot.mode
        self.phase = phase_from_snapshot(snapshot)
        self.success = is_online_ready(snapshot) or snapshot.mode == "offline"
        self.session_state = snapshot.session_state
        self.backend_started = backend_started_from_snapshot(snapshot)
        self.tcp_connected = snapshot.tcp_state == "ready"
        self.hardware_ready = snapshot.hardware_state == "ready"
        self.failure_code = snapshot.failure_code
        self.failure_stage = snapshot.failure_stage
        self.recoverable = snapshot.recoverable
        if not self.user_message:
            if snapshot.last_error_message:
                self.user_message = snapshot.last_error_message
            elif is_online_ready(snapshot):
                self.user_message = "System ready"
            else:
                self.user_message = "Startup failed"

    @classmethod
    def from_snapshot(
        cls,
        requested_mode: LaunchMode,
        snapshot: SessionSnapshot,
        user_message: str | None = None,
    ) -> "LaunchResult":
        success = is_online_ready(snapshot) or snapshot.mode == "offline"
        message = user_message or snapshot.last_error_message or ("System ready" if success else "Startup failed")
        return cls(
            requested_mode=requested_mode,
            effective_mode=snapshot.mode,
            phase=phase_from_snapshot(snapshot),
            success=success,
            backend_started=backend_started_from_snapshot(snapshot),
            tcp_connected=snapshot.tcp_state == "ready",
            hardware_ready=snapshot.hardware_state == "ready",
            user_message=message,
            failure_code=snapshot.failure_code,
            failure_stage=snapshot.failure_stage,
            recoverable=snapshot.recoverable,
            session_state=snapshot.session_state,
            session_snapshot=snapshot,
        )

    @property
    def degraded(self) -> bool:
        return self.requested_mode != self.effective_mode

    @property
    def online_ready(self) -> bool:
        if self.session_snapshot is not None:
            return is_online_ready(self.session_snapshot)
        return self.success and self.effective_mode == "online"


StartupResult = LaunchResult


def backend_started_from_snapshot(snapshot: SessionSnapshot) -> bool:
    if snapshot.mode != "online":
        return False
    if snapshot.failure_stage == "backend_starting":
        return False
    return snapshot.backend_state in ("starting", "ready", "failed")


def phase_from_snapshot(snapshot: SessionSnapshot) -> str:
    if snapshot.mode == "offline":
        return "offline"
    if is_online_ready(snapshot):
        return "ready"
    stage = snapshot.failure_stage or ""
    if stage.startswith("backend"):
        return "backend"
    if stage.startswith("tcp"):
        return "tcp"
    if stage.startswith("hardware"):
        return "hardware"
    if stage == "online_ready":
        return "hardware"
    return "backend"


__all__ = [
    "LaunchResult",
    "StartupResult",
    "backend_started_from_snapshot",
    "phase_from_snapshot",
]
