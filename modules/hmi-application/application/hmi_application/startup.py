"""Startup sequence manager - handles launch-mode-aware startup flows."""
from __future__ import annotations

import logging
import os
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Callable, Literal

from PyQt5.QtCore import QThread, pyqtSignal

from .supervisor_contract import (
    RecoveryAction,
    SessionSnapshot,
    SessionStageEvent,
    is_online_ready,
)
from .supervisor_session import SupervisorPolicy, SupervisorSession

if TYPE_CHECKING:
    try:
        from hmi_client.client.backend_manager import BackendManager
        from hmi_client.client.protocol import CommandProtocol
        from hmi_client.client.tcp_client import TcpClient
    except ImportError:  # pragma: no cover - script-mode fallback
        from client.backend_manager import BackendManager  # type: ignore
        from client.protocol import CommandProtocol  # type: ignore
        from client.tcp_client import TcpClient  # type: ignore

LaunchMode = Literal["online", "offline"]

BACKEND_READY_TIMEOUT_ENV = "SILIGEN_SUP_BACKEND_READY_TIMEOUT_S"
TCP_CONNECT_TIMEOUT_ENV = "SILIGEN_SUP_TCP_CONNECT_TIMEOUT_S"
HARDWARE_PROBE_TIMEOUT_ENV = "SILIGEN_SUP_HARDWARE_PROBE_TIMEOUT_S"


_STARTUP_LOGGER = logging.getLogger("hmi.startup")
if not _STARTUP_LOGGER.handlers:
    log_dir = Path("logs")
    log_dir.mkdir(parents=True, exist_ok=True)
    handler = logging.FileHandler(log_dir / "hmi_startup.log", encoding="utf-8")
    formatter = logging.Formatter("%(asctime)s [%(levelname)s] %(message)s")
    handler.setFormatter(formatter)
    _STARTUP_LOGGER.addHandler(handler)
    _STARTUP_LOGGER.setLevel(logging.INFO)
    _STARTUP_LOGGER.propagate = False


def normalize_launch_mode(value: str | None) -> LaunchMode:
    mode = (value or "online").strip().lower()
    if mode not in ("online", "offline"):
        raise ValueError(f"Unsupported launch mode: {value}")
    return mode


def _parse_positive_timeout(env_name: str, default_value: float) -> float:
    raw_value = os.getenv(env_name)
    if raw_value is None:
        return default_value
    text = raw_value.strip()
    if not text:
        return default_value
    try:
        value = float(text)
    except ValueError:
        _STARTUP_LOGGER.warning("Invalid %s=%s; fallback to %.3fs", env_name, text, default_value)
        return default_value
    if value <= 0:
        _STARTUP_LOGGER.warning("Non-positive %s=%s; fallback to %.3fs", env_name, text, default_value)
        return default_value
    return value


def load_supervisor_policy_from_env() -> SupervisorPolicy:
    return SupervisorPolicy(
        backend_ready_timeout_s=_parse_positive_timeout(BACKEND_READY_TIMEOUT_ENV, SupervisorSession.BACKEND_READY_TIMEOUT),
        tcp_connect_timeout_s=_parse_positive_timeout(TCP_CONNECT_TIMEOUT_ENV, SupervisorSession.TCP_CONNECT_TIMEOUT),
        hardware_probe_timeout_s=_parse_positive_timeout(HARDWARE_PROBE_TIMEOUT_ENV, SupervisorSession.HARDWARE_PROBE_TIMEOUT),
    )


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
        self.phase = _phase_from_snapshot(snapshot)
        self.success = is_online_ready(snapshot) or snapshot.mode == "offline"
        self.session_state = snapshot.session_state
        self.backend_started = _backend_started(snapshot)
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
            phase=_phase_from_snapshot(snapshot),
            success=success,
            backend_started=_backend_started(snapshot),
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


def run_launch_sequence(
    launch_mode: str,
    backend: BackendManager,
    client: TcpClient,
    protocol: CommandProtocol,
    progress_callback: Callable[[str, int], None] | None = None,
    snapshot_callback: Callable[[SessionSnapshot], None] | None = None,
    event_callback: Callable[[SessionStageEvent], None] | None = None,
    policy: SupervisorPolicy | None = None,
) -> LaunchResult:
    mode = normalize_launch_mode(launch_mode)
    _STARTUP_LOGGER.info("Launch mode requested: %s", mode)

    session = SupervisorSession(
        backend=backend,
        client=client,
        protocol=protocol,
        launch_mode=mode,
        policy=policy if policy is not None else load_supervisor_policy_from_env(),
    )
    snapshot = session.start(
        progress_callback=progress_callback,
        snapshot_callback=snapshot_callback,
        event_callback=event_callback,
    )
    result = _build_launch_result(mode, snapshot)
    _log_launch_result(result)
    return result


def run_recovery_action(
    action: RecoveryAction,
    snapshot: SessionSnapshot,
    backend: BackendManager,
    client: TcpClient,
    protocol: CommandProtocol,
    progress_callback: Callable[[str, int], None] | None = None,
    snapshot_callback: Callable[[SessionSnapshot], None] | None = None,
    event_callback: Callable[[SessionStageEvent], None] | None = None,
    policy: SupervisorPolicy | None = None,
) -> LaunchResult:
    if snapshot.mode != "online":
        raise ValueError("Recovery actions are only supported for online session snapshots.")

    _STARTUP_LOGGER.info(
        "Recovery action requested: %s; session_state=%s; failure_code=%s; failure_stage=%s",
        action,
        snapshot.session_state,
        snapshot.failure_code,
        snapshot.failure_stage,
    )
    session = SupervisorSession(
        backend=backend,
        client=client,
        protocol=protocol,
        launch_mode="online",
        policy=policy if policy is not None else load_supervisor_policy_from_env(),
    )
    recovered_snapshot = session.invoke_recovery(
        action=action,
        snapshot=snapshot,
        progress_callback=progress_callback,
        snapshot_callback=snapshot_callback,
        event_callback=event_callback,
    )
    result = _build_launch_result("online", recovered_snapshot)
    _log_launch_result(result)
    return result


def _log_launch_result(result: LaunchResult) -> None:
    _STARTUP_LOGGER.info(
        "Launch mode effective: %s; phase: %s; session_state=%s; success=%s; "
        "failure_code=%s; message=%s",
        result.effective_mode,
        result.phase,
        result.session_state,
        result.success,
        result.failure_code,
        result.user_message,
    )


def _build_launch_result(requested_mode: LaunchMode, snapshot: SessionSnapshot) -> LaunchResult:
    return LaunchResult.from_snapshot(requested_mode=requested_mode, snapshot=snapshot)


def launch_result_from_snapshot(
    requested_mode: str,
    snapshot: SessionSnapshot,
    user_message: str | None = None,
) -> LaunchResult:
    return LaunchResult.from_snapshot(
        requested_mode=normalize_launch_mode(requested_mode),
        snapshot=snapshot,
        user_message=user_message,
    )


def _backend_started(snapshot: SessionSnapshot) -> bool:
    if snapshot.mode != "online":
        return False
    if snapshot.failure_stage == "backend_starting":
        return False
    return snapshot.backend_state in ("starting", "ready", "failed")


def _phase_from_snapshot(snapshot: SessionSnapshot) -> str:
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


class StartupWorker(QThread):
    """Background worker thread for startup sequence."""

    progress = pyqtSignal(str, int)
    snapshot = pyqtSignal(object)
    stage_event = pyqtSignal(object)
    finished = pyqtSignal(object)

    def __init__(
        self,
        backend: BackendManager,
        client: TcpClient,
        protocol: CommandProtocol,
        launch_mode: str = "online",
        policy: SupervisorPolicy | None = None,
    ):
        super().__init__()
        self._backend = backend
        self._client = client
        self._protocol = protocol
        self._launch_mode = normalize_launch_mode(launch_mode)
        self._policy = policy if policy is not None else load_supervisor_policy_from_env()

    def run(self):
        self.finished.emit(
            run_launch_sequence(
                self._launch_mode,
                self._backend,
                self._client,
                self._protocol,
                progress_callback=self.progress.emit,
                snapshot_callback=self.snapshot.emit,
                event_callback=self.stage_event.emit,
                policy=self._policy,
            )
        )


class RecoveryWorker(QThread):
    """Background worker for supervisor recovery actions."""

    progress = pyqtSignal(str, int)
    snapshot = pyqtSignal(object)
    stage_event = pyqtSignal(object)
    finished = pyqtSignal(object)

    def __init__(
        self,
        action: RecoveryAction,
        recovery_snapshot: SessionSnapshot,
        backend: BackendManager,
        client: TcpClient,
        protocol: CommandProtocol,
        policy: SupervisorPolicy | None = None,
    ):
        super().__init__()
        self._action = action
        self._recovery_snapshot = recovery_snapshot
        self._backend = backend
        self._client = client
        self._protocol = protocol
        self._policy = policy if policy is not None else load_supervisor_policy_from_env()

    def run(self):
        self.finished.emit(
            run_recovery_action(
                action=self._action,
                snapshot=self._recovery_snapshot,
                backend=self._backend,
                client=self._client,
                protocol=self._protocol,
                progress_callback=self.progress.emit,
                snapshot_callback=self.snapshot.emit,
                event_callback=self.stage_event.emit,
                policy=self._policy,
            )
        )
