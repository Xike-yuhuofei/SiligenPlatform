"""Minimal supervisor session orchestrator for startup state machine."""
from __future__ import annotations

from dataclasses import dataclass, replace
from typing import TYPE_CHECKING, Callable
from uuid import uuid4

from .supervisor_contract import (
    FailureCode,
    FailureStage,
    RecoveryAction,
    SessionSnapshot,
    SessionStageEvent,
    is_online_ready,
    snapshot_timestamp,
)

if TYPE_CHECKING:
    try:
        from hmi_client.client.backend_manager import BackendManager
        from hmi_client.client.protocol import CommandProtocol
        from hmi_client.client.tcp_client import TcpClient
    except ImportError:  # pragma: no cover - script-mode fallback
        from client.backend_manager import BackendManager  # type: ignore
        from client.protocol import CommandProtocol  # type: ignore
        from client.tcp_client import TcpClient  # type: ignore

ProgressCallback = Callable[[str, int], None]
SnapshotCallback = Callable[[SessionSnapshot], None]
EventCallback = Callable[[SessionStageEvent], None]

BACKEND_STARTING_STAGE = "backend_starting"
BACKEND_READY_STAGE = "backend_ready"
TCP_CONNECTING_STAGE = "tcp_connecting"
TCP_READY_STAGE = "tcp_ready"
HARDWARE_PROBING_STAGE = "hardware_probing"
HARDWARE_READY_STAGE = "hardware_ready"
ONLINE_READY_STAGE = "online_ready"


@dataclass(frozen=True)
class StepResult:
    ok: bool
    message: str
    failure_code: str | None = None
    recoverable: bool = True


@dataclass(frozen=True)
class SupervisorPolicy:
    """Timeout policy for supervisor startup and recovery orchestration."""

    backend_ready_timeout_s: float = 5.0
    tcp_connect_timeout_s: float = 3.0
    hardware_probe_timeout_s: float = 15.0

    def __post_init__(self) -> None:
        if self.backend_ready_timeout_s <= 0:
            raise ValueError("backend_ready_timeout_s must be > 0")
        if self.tcp_connect_timeout_s <= 0:
            raise ValueError("tcp_connect_timeout_s must be > 0")
        if self.hardware_probe_timeout_s <= 0:
            raise ValueError("hardware_probe_timeout_s must be > 0")


class SupervisorSession:
    BACKEND_READY_TIMEOUT = 5.0
    TCP_CONNECT_TIMEOUT = 3.0
    HARDWARE_PROBE_TIMEOUT = 15.0

    def __init__(
        self,
        backend: BackendManager,
        client: TcpClient,
        protocol: CommandProtocol,
        launch_mode: str = "online",
        policy: SupervisorPolicy | None = None,
    ) -> None:
        self._backend = backend
        self._client = client
        self._protocol = protocol
        self._mode = self._normalize_mode(launch_mode)
        self._policy = policy if policy is not None else SupervisorPolicy(
            backend_ready_timeout_s=self.BACKEND_READY_TIMEOUT,
            tcp_connect_timeout_s=self.TCP_CONNECT_TIMEOUT,
            hardware_probe_timeout_s=self.HARDWARE_PROBE_TIMEOUT,
        )
        self._active_session_id: str | None = None
        self._last_snapshot: SessionSnapshot | None = None

    @staticmethod
    def _normalize_mode(value: str | None) -> str:
        mode = (value or "online").strip().lower()
        if mode not in ("online", "offline"):
            raise ValueError(f"Unsupported launch mode: {value}")
        return mode

    def start(
        self,
        progress_callback: ProgressCallback | None = None,
        snapshot_callback: SnapshotCallback | None = None,
        event_callback: EventCallback | None = None,
    ) -> SessionSnapshot:
        self._active_session_id = uuid4().hex
        session_id = self._active_session_id

        if self._mode == "offline":
            self._emit_progress(progress_callback, "Offline mode: startup skipped", 100)
            snapshot = SessionSnapshot(
                mode="offline",
                session_state="idle",
                backend_state="stopped",
                tcp_state="disconnected",
                hardware_state="unavailable",
                failure_code=None,
                failure_stage=None,
                recoverable=True,
                last_error_message="Offline mode active: startup sequence skipped.",
                updated_at=snapshot_timestamp(),
            )
            return self._emit_snapshot(snapshot, snapshot_callback)

        try:
            return self._run_online_flow(
                start_stage=BACKEND_STARTING_STAGE,
                progress_callback=progress_callback,
                snapshot_callback=snapshot_callback,
                event_callback=event_callback,
                session_id=session_id,
            )
        except Exception as exc:  # pragma: no cover - defensive path
            return self._failed(
                stage=ONLINE_READY_STAGE,
                code="SUP_UNKNOWN",
                message=f"Unexpected supervisor failure: {exc}",
                recoverable=False,
                backend_state="failed",
                tcp_state="failed",
                hardware_state="failed",
                snapshot_callback=snapshot_callback,
                event_callback=event_callback,
                session_id=session_id,
            )

    def invoke_recovery(
        self,
        action: RecoveryAction,
        snapshot: SessionSnapshot,
        progress_callback: ProgressCallback | None = None,
        snapshot_callback: SnapshotCallback | None = None,
        event_callback: EventCallback | None = None,
    ) -> SessionSnapshot:
        if action == "retry_stage":
            return self.retry_stage(
                snapshot,
                progress_callback=progress_callback,
                snapshot_callback=snapshot_callback,
                event_callback=event_callback,
            )
        if action == "restart_session":
            return self.restart_session(
                snapshot,
                progress_callback=progress_callback,
                snapshot_callback=snapshot_callback,
                event_callback=event_callback,
            )
        return self.stop_session(
            snapshot,
            snapshot_callback=snapshot_callback,
            event_callback=event_callback,
        )

    def retry_stage(
        self,
        snapshot: SessionSnapshot,
        progress_callback: ProgressCallback | None = None,
        snapshot_callback: SnapshotCallback | None = None,
        event_callback: EventCallback | None = None,
    ) -> SessionSnapshot:
        if (
            snapshot.mode != "online"
            or snapshot.session_state != "failed"
            or not snapshot.recoverable
            or snapshot.failure_stage is None
        ):
            return snapshot

        session_id = self._active_session_id or uuid4().hex
        self._emit_stage_event(
            event_callback=event_callback,
            session_id=session_id,
            event_type="recovery_invoked",
            stage=snapshot.failure_stage,
            failure_code=snapshot.failure_code,
            recoverable=snapshot.recoverable,
            message="retry_stage",
        )

        stage = snapshot.failure_stage
        if stage == BACKEND_STARTING_STAGE:
            return self._run_online_flow(
                start_stage=BACKEND_STARTING_STAGE,
                progress_callback=progress_callback,
                snapshot_callback=snapshot_callback,
                event_callback=event_callback,
                session_id=session_id,
            )
        if stage == BACKEND_READY_STAGE:
            return self._run_online_flow(
                start_stage=BACKEND_READY_STAGE,
                progress_callback=progress_callback,
                snapshot_callback=snapshot_callback,
                event_callback=event_callback,
                session_id=session_id,
            )
        if stage in (TCP_CONNECTING_STAGE, TCP_READY_STAGE):
            self._client.disconnect()
            return self._run_online_flow(
                start_stage=TCP_CONNECTING_STAGE,
                progress_callback=progress_callback,
                snapshot_callback=snapshot_callback,
                event_callback=event_callback,
                session_id=session_id,
            )
        if stage in (HARDWARE_PROBING_STAGE, HARDWARE_READY_STAGE, ONLINE_READY_STAGE):
            return self._run_online_flow(
                start_stage=HARDWARE_PROBING_STAGE,
                progress_callback=progress_callback,
                snapshot_callback=snapshot_callback,
                event_callback=event_callback,
                session_id=session_id,
            )
        return snapshot

    def restart_session(
        self,
        snapshot: SessionSnapshot,
        progress_callback: ProgressCallback | None = None,
        snapshot_callback: SnapshotCallback | None = None,
        event_callback: EventCallback | None = None,
    ) -> SessionSnapshot:
        session_id = self._active_session_id or uuid4().hex
        stage = snapshot.failure_stage or BACKEND_STARTING_STAGE
        self._emit_stage_event(
            event_callback=event_callback,
            session_id=session_id,
            event_type="recovery_invoked",
            stage=stage,
            failure_code=snapshot.failure_code,
            recoverable=snapshot.recoverable,
            message="restart_session",
        )
        self._client.disconnect()
        self._backend.stop()
        return self._run_online_flow(
            start_stage=BACKEND_STARTING_STAGE,
            progress_callback=progress_callback,
            snapshot_callback=snapshot_callback,
            event_callback=event_callback,
            session_id=session_id,
        )

    def stop_session(
        self,
        snapshot: SessionSnapshot,
        snapshot_callback: SnapshotCallback | None = None,
        event_callback: EventCallback | None = None,
    ) -> SessionSnapshot:
        mode = snapshot.mode
        session_id = self._active_session_id or uuid4().hex
        stage = snapshot.failure_stage or ONLINE_READY_STAGE
        self._emit_stage_event(
            event_callback=event_callback,
            session_id=session_id,
            event_type="recovery_invoked",
            stage=stage,
            failure_code=snapshot.failure_code,
            recoverable=snapshot.recoverable,
            message="stop_session",
        )

        stopping = SessionSnapshot(
            mode=mode,
            session_state="stopping",
            backend_state=snapshot.backend_state,
            tcp_state=snapshot.tcp_state,
            hardware_state=snapshot.hardware_state,
            failure_code=None,
            failure_stage=None,
            recoverable=True,
            last_error_message="Stopping session...",
            updated_at=snapshot_timestamp(),
        )
        self._emit_snapshot(stopping, snapshot_callback)

        self._client.disconnect()
        self._backend.stop()
        idle = SessionSnapshot(
            mode=mode,
            session_state="idle",
            backend_state="stopped",
            tcp_state="disconnected",
            hardware_state="unavailable",
            failure_code=None,
            failure_stage=None,
            recoverable=True,
            last_error_message="Session stopped.",
            updated_at=snapshot_timestamp(),
        )
        return self._emit_snapshot(idle, snapshot_callback)

    def _run_online_flow(
        self,
        *,
        start_stage: str,
        progress_callback: ProgressCallback | None,
        snapshot_callback: SnapshotCallback | None,
        event_callback: EventCallback | None,
        session_id: str,
    ) -> SessionSnapshot:
        states = self._initial_states_for(start_stage)

        if start_stage == BACKEND_STARTING_STAGE:
            self._emit_stage_event(event_callback, session_id, "stage_entered", BACKEND_STARTING_STAGE)
            self._emit_progress(progress_callback, "Starting backend...", 10)
            self._emit_starting_snapshot(
                snapshot_callback=snapshot_callback,
                message="Starting backend...",
                backend_state="starting",
                tcp_state=states["tcp_state"],
                hardware_state=states["hardware_state"],
            )
            start_res = self._backend_start()
            if not start_res.ok:
                return self._failed(
                    stage=BACKEND_STARTING_STAGE,
                    code=start_res.failure_code,
                    message=start_res.message,
                    recoverable=start_res.recoverable,
                    backend_state="failed",
                    tcp_state="disconnected",
                    hardware_state="unavailable",
                    snapshot_callback=snapshot_callback,
                    event_callback=event_callback,
                    session_id=session_id,
                )
            self._emit_stage_event(event_callback, session_id, "stage_succeeded", BACKEND_STARTING_STAGE)
            states["backend_state"] = "starting"

        if start_stage in (BACKEND_STARTING_STAGE, BACKEND_READY_STAGE):
            self._emit_stage_event(event_callback, session_id, "stage_entered", BACKEND_READY_STAGE)
            self._emit_progress(progress_callback, "Waiting for backend...", 20)
            self._emit_starting_snapshot(
                snapshot_callback=snapshot_callback,
                message="Waiting for backend...",
                backend_state="starting",
                tcp_state="disconnected",
                hardware_state="unavailable",
            )
            ready_res = self._backend_wait_ready()
            if not ready_res.ok:
                return self._failed(
                    stage=BACKEND_READY_STAGE,
                    code=ready_res.failure_code,
                    message=ready_res.message,
                    recoverable=ready_res.recoverable,
                    backend_state="failed",
                    tcp_state="disconnected",
                    hardware_state="unavailable",
                    snapshot_callback=snapshot_callback,
                    event_callback=event_callback,
                    session_id=session_id,
                )
            self._emit_stage_event(event_callback, session_id, "stage_succeeded", BACKEND_READY_STAGE)
            self._emit_progress(progress_callback, "Backend ready", 30)
            states["backend_state"] = "ready"

        if start_stage in (BACKEND_STARTING_STAGE, BACKEND_READY_STAGE, TCP_CONNECTING_STAGE, TCP_READY_STAGE):
            self._emit_stage_event(event_callback, session_id, "stage_entered", TCP_CONNECTING_STAGE)
            self._emit_progress(progress_callback, "Connecting TCP...", 40)
            self._emit_starting_snapshot(
                snapshot_callback=snapshot_callback,
                message="Connecting TCP...",
                backend_state="ready",
                tcp_state="connecting",
                hardware_state="unavailable",
            )
            tcp_res = self._tcp_connect()
            if not tcp_res.ok:
                return self._failed(
                    stage=TCP_CONNECTING_STAGE,
                    code=tcp_res.failure_code,
                    message=tcp_res.message,
                    recoverable=tcp_res.recoverable,
                    backend_state="ready",
                    tcp_state="failed",
                    hardware_state="unavailable",
                    snapshot_callback=snapshot_callback,
                    event_callback=event_callback,
                    session_id=session_id,
                )
            self._emit_stage_event(event_callback, session_id, "stage_succeeded", TCP_CONNECTING_STAGE)
            self._emit_progress(progress_callback, "TCP connected", 60)
            states["tcp_state"] = "ready"

            self._emit_stage_event(event_callback, session_id, "stage_entered", TCP_READY_STAGE)
            if hasattr(self._client, "is_connected"):
                connected = bool(self._client.is_connected())
            else:
                connected = True
            if not connected:
                return self._failed(
                    stage=TCP_READY_STAGE,
                    code="SUP_TCP_CONNECT_FAILED",
                    message="TCP connected then disconnected immediately.",
                    recoverable=True,
                    backend_state="ready",
                    tcp_state="failed",
                    hardware_state="unavailable",
                    snapshot_callback=snapshot_callback,
                    event_callback=event_callback,
                    session_id=session_id,
                )
            self._emit_stage_event(event_callback, session_id, "stage_succeeded", TCP_READY_STAGE)

        if start_stage in (
            BACKEND_STARTING_STAGE,
            BACKEND_READY_STAGE,
            TCP_CONNECTING_STAGE,
            TCP_READY_STAGE,
            HARDWARE_PROBING_STAGE,
            HARDWARE_READY_STAGE,
            ONLINE_READY_STAGE,
        ):
            self._emit_stage_event(event_callback, session_id, "stage_entered", HARDWARE_PROBING_STAGE)
            self._emit_progress(progress_callback, "Initializing hardware...", 70)
            self._emit_starting_snapshot(
                snapshot_callback=snapshot_callback,
                message="Initializing hardware...",
                backend_state="ready",
                tcp_state="ready",
                hardware_state="probing",
            )
            hw_res = self._hardware_connect()
            if not hw_res.ok:
                return self._failed(
                    stage=HARDWARE_PROBING_STAGE,
                    code=hw_res.failure_code,
                    message=hw_res.message,
                    recoverable=hw_res.recoverable,
                    backend_state="ready",
                    tcp_state="ready",
                    hardware_state="failed",
                    snapshot_callback=snapshot_callback,
                    event_callback=event_callback,
                    session_id=session_id,
                )
            self._emit_stage_event(event_callback, session_id, "stage_succeeded", HARDWARE_PROBING_STAGE)
            self._emit_stage_event(event_callback, session_id, "stage_entered", HARDWARE_READY_STAGE)
            self._emit_starting_snapshot(
                snapshot_callback=snapshot_callback,
                message="Hardware ready",
                backend_state="ready",
                tcp_state="ready",
                hardware_state="ready",
            )
            self._emit_stage_event(event_callback, session_id, "stage_succeeded", HARDWARE_READY_STAGE)

        self._emit_stage_event(event_callback, session_id, "stage_entered", ONLINE_READY_STAGE)
        self._emit_progress(progress_callback, "System ready", 100)
        ready_snapshot = SessionSnapshot(
            mode="online",
            session_state="ready",
            backend_state="ready",
            tcp_state="ready",
            hardware_state="ready",
            failure_code=None,
            failure_stage=None,
            recoverable=True,
            last_error_message="System ready",
            updated_at=snapshot_timestamp(),
        )
        if not is_online_ready(ready_snapshot):
            return self._failed(
                stage=ONLINE_READY_STAGE,
                code="SUP_UNKNOWN",
                message="online_ready predicate not satisfied",
                recoverable=False,
                backend_state=ready_snapshot.backend_state,
                tcp_state=ready_snapshot.tcp_state,
                hardware_state=ready_snapshot.hardware_state,
                snapshot_callback=snapshot_callback,
                event_callback=event_callback,
                session_id=session_id,
            )
        self._emit_stage_event(event_callback, session_id, "stage_succeeded", ONLINE_READY_STAGE)
        return self._emit_snapshot(ready_snapshot, snapshot_callback)

    @staticmethod
    def _initial_states_for(start_stage: str) -> dict[str, str]:
        states = {
            "backend_state": "stopped",
            "tcp_state": "disconnected",
            "hardware_state": "unavailable",
        }
        if start_stage in (BACKEND_READY_STAGE,):
            states["backend_state"] = "starting"
        if start_stage in (
            TCP_CONNECTING_STAGE,
            TCP_READY_STAGE,
            HARDWARE_PROBING_STAGE,
            HARDWARE_READY_STAGE,
            ONLINE_READY_STAGE,
        ):
            states["backend_state"] = "ready"
        if start_stage in (HARDWARE_PROBING_STAGE, HARDWARE_READY_STAGE, ONLINE_READY_STAGE):
            states["tcp_state"] = "ready"
        if start_stage == ONLINE_READY_STAGE:
            states["hardware_state"] = "ready"
        return states

    @staticmethod
    def _emit_progress(progress_callback: ProgressCallback | None, message: str, percent: int) -> None:
        if progress_callback is not None:
            progress_callback(message, percent)

    def _emit_snapshot(
        self,
        snapshot: SessionSnapshot,
        snapshot_callback: SnapshotCallback | None,
    ) -> SessionSnapshot:
        self._last_snapshot = snapshot
        if snapshot_callback is not None:
            snapshot_callback(snapshot)
        return snapshot

    def _emit_starting_snapshot(
        self,
        *,
        snapshot_callback: SnapshotCallback | None,
        message: str,
        backend_state: str,
        tcp_state: str,
        hardware_state: str,
    ) -> SessionSnapshot:
        return self._emit_snapshot(
            SessionSnapshot(
                mode="online",
                session_state="starting",
                backend_state=backend_state,
                tcp_state=tcp_state,
                hardware_state=hardware_state,
                failure_code=None,
                failure_stage=None,
                recoverable=True,
                last_error_message=message,
                updated_at=snapshot_timestamp(),
            ),
            snapshot_callback,
        )

    def _emit_stage_event(
        self,
        event_callback: EventCallback | None,
        session_id: str,
        event_type: str,
        stage: str,
        failure_code: FailureCode | None = None,
        recoverable: bool | None = None,
        message: str | None = None,
    ) -> None:
        if event_callback is None:
            return
        event_callback(
            SessionStageEvent(
                event_type=event_type,
                session_id=session_id,
                stage=stage,
                timestamp=snapshot_timestamp(),
                failure_code=failure_code,
                recoverable=recoverable,
                message=message,
            )
        )

    def _failed(
        self,
        *,
        stage: str,
        code: str | None,
        message: str,
        recoverable: bool,
        backend_state: str,
        tcp_state: str,
        hardware_state: str,
        snapshot_callback: SnapshotCallback | None,
        event_callback: EventCallback | None,
        session_id: str,
    ) -> SessionSnapshot:
        failure_code = code or "SUP_UNKNOWN"
        snapshot = SessionSnapshot(
            mode="online",
            session_state="failed",
            backend_state=backend_state,
            tcp_state=tcp_state,
            hardware_state=hardware_state,
            failure_code=failure_code,
            failure_stage=stage,
            recoverable=recoverable,
            last_error_message=message,
            updated_at=snapshot_timestamp(),
        )
        self._emit_stage_event(
            event_callback=event_callback,
            session_id=session_id,
            event_type="stage_failed",
            stage=stage,
            failure_code=snapshot.failure_code,
            recoverable=snapshot.recoverable,
            message=snapshot.last_error_message,
        )
        return self._emit_snapshot(snapshot, snapshot_callback)

    def _backend_start(self) -> StepResult:
        if hasattr(self._backend, "start_detailed"):
            result = self._backend.start_detailed()
            return StepResult(
                ok=bool(getattr(result, "ok", False)),
                message=str(getattr(result, "message", "")),
                failure_code=getattr(result, "failure_code", None),
                recoverable=bool(getattr(result, "recoverable", True)),
            )
        ok, msg = self._backend.start()
        return StepResult(
            ok=ok,
            message=msg,
            failure_code=None if ok else "SUP_BACKEND_START_FAILED",
            recoverable=True,
        )

    def _backend_wait_ready(self) -> StepResult:
        if hasattr(self._backend, "wait_ready_detailed"):
            try:
                result = self._backend.wait_ready_detailed(timeout=self._policy.backend_ready_timeout_s)
            except TypeError:
                result = self._backend.wait_ready_detailed()
            return StepResult(
                ok=bool(getattr(result, "ok", False)),
                message=str(getattr(result, "message", "")),
                failure_code=getattr(result, "failure_code", None),
                recoverable=bool(getattr(result, "recoverable", True)),
            )
        try:
            ok, msg = self._backend.wait_ready(timeout=self._policy.backend_ready_timeout_s)
        except TypeError:
            ok, msg = self._backend.wait_ready()
        return StepResult(
            ok=ok,
            message=msg,
            failure_code=None if ok else "SUP_BACKEND_READY_TIMEOUT",
            recoverable=True,
        )

    def _tcp_connect(self) -> StepResult:
        try:
            connected = self._client.connect(timeout=self._policy.tcp_connect_timeout_s)
        except TypeError:
            connected = self._client.connect()
        if connected:
            return StepResult(ok=True, message="TCP connected")
        message = getattr(self._client, "last_connect_error", "") or "TCP connection failed"
        return StepResult(
            ok=False,
            message=message,
            failure_code="SUP_TCP_CONNECT_FAILED",
            recoverable=True,
        )

    def _hardware_connect(self) -> StepResult:
        try:
            ok, msg = self._protocol.connect_hardware(timeout=self._policy.hardware_probe_timeout_s)
        except TypeError:
            ok, msg = self._protocol.connect_hardware()
        if ok:
            return StepResult(ok=True, message=msg or "Hardware ready")
        return StepResult(
            ok=False,
            message=msg or "Hardware initialization failed",
            failure_code="SUP_HARDWARE_CONNECT_FAILED",
            recoverable=True,
        )

    @staticmethod
    def build_runtime_failure_snapshot(
        snapshot: SessionSnapshot,
        *,
        failure_code: FailureCode,
        failure_stage: FailureStage,
        message: str,
        backend_state: str | None = None,
        tcp_state: str | None = None,
        hardware_state: str | None = None,
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

    @staticmethod
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
            return SupervisorSession.build_runtime_failure_snapshot(
                snapshot,
                failure_code="SUP_TCP_CONNECT_FAILED",
                failure_stage="tcp_ready",
                message=error_message or "Runtime TCP connection lost.",
                tcp_state="failed",
                hardware_state="unavailable",
                recoverable=True,
            )

        if hardware_ready is False:
            return SupervisorSession.build_runtime_failure_snapshot(
                snapshot,
                failure_code="SUP_HARDWARE_CONNECT_FAILED",
                failure_stage="hardware_ready",
                message=error_message or "Runtime hardware state unavailable.",
                hardware_state="failed",
                recoverable=True,
            )

        return None

    @staticmethod
    def detect_runtime_degradation_with_event(
        snapshot: SessionSnapshot | None,
        *,
        session_id: str | None = None,
        tcp_connected: bool | None = None,
        hardware_ready: bool | None = None,
        error_message: str | None = None,
    ) -> tuple[SessionSnapshot, SessionStageEvent] | None:
        degraded = SupervisorSession.detect_runtime_degradation(
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
            stage=degraded.failure_stage,
            timestamp=snapshot_timestamp(),
            failure_code=degraded.failure_code,
            recoverable=degraded.recoverable,
            message=degraded.last_error_message,
        )
        return degraded, event
