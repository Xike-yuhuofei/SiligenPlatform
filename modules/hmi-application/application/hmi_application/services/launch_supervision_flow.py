from __future__ import annotations

import os
from typing import cast
from uuid import uuid4

from ..adapters.launch_supervision_ports import (
    BackendController,
    BackendManagerBasic,
    HardwareProtocolLike,
    RuntimeStatusProbeLike,
    TcpClientLike,
)
from ..contracts.launch_supervision_contract import (
    BackendState,
    FailureCode,
    FailureStage,
    HardwareState,
    RecoveryAction,
    RuntimeIdentity,
    SessionSnapshot,
    SessionStageEvent,
    StageEventType,
    TcpState,
    is_online_ready,
    snapshot_timestamp,
)
from ..domain.launch_supervision_types import (
    BACKEND_READY_STAGE,
    BACKEND_STARTING_STAGE,
    EventCallback,
    HARDWARE_PROBING_STAGE,
    HARDWARE_READY_STAGE,
    ONLINE_READY_STAGE,
    ProgressCallback,
    RUNTIME_CONTRACT_READY_STAGE,
    SnapshotCallback,
    StepResult,
    SupervisorPolicy,
    TCP_CONNECTING_STAGE,
    TCP_READY_STAGE,
)


def _normalize_identity_path(value: str) -> str:
    text = str(value or "").strip()
    if not text:
        return ""
    return os.path.normcase(os.path.normpath(text))


def _coerce_runtime_identity(candidate: object | None) -> RuntimeIdentity | None:
    if candidate is None:
        return None
    executable_path = str(getattr(candidate, "executable_path", "") or "").strip()
    working_directory = str(getattr(candidate, "working_directory", "") or "").strip()
    protocol_version = str(getattr(candidate, "protocol_version", "") or "").strip()
    preview_snapshot_contract = str(getattr(candidate, "preview_snapshot_contract", "") or "").strip()
    if not any((executable_path, working_directory, protocol_version, preview_snapshot_contract)):
        return None
    identity = RuntimeIdentity(
        executable_path=executable_path,
        working_directory=working_directory,
        protocol_version=protocol_version,
        preview_snapshot_contract=preview_snapshot_contract,
    )
    if not identity.is_complete():
        return None
    return identity


def _compare_runtime_identity(actual: RuntimeIdentity | None, expected: RuntimeIdentity) -> str | None:
    if actual is None:
        return "status.runtime_identity is missing"

    mismatches: list[str] = []
    if _normalize_identity_path(actual.executable_path) != _normalize_identity_path(expected.executable_path):
        mismatches.append(
            f"executable_path expected={expected.executable_path} actual={actual.executable_path}"
        )
    if _normalize_identity_path(actual.working_directory) != _normalize_identity_path(expected.working_directory):
        mismatches.append(
            f"working_directory expected={expected.working_directory} actual={actual.working_directory}"
        )
    if actual.protocol_version != expected.protocol_version:
        mismatches.append(
            f"protocol_version expected={expected.protocol_version} actual={actual.protocol_version}"
        )
    if actual.preview_snapshot_contract != expected.preview_snapshot_contract:
        mismatches.append(
            "preview_snapshot_contract "
            f"expected={expected.preview_snapshot_contract} actual={actual.preview_snapshot_contract}"
        )
    if mismatches:
        return "; ".join(mismatches)
    return None


class LaunchSupervisionFlow:
    def __init__(
        self,
        *,
        backend: BackendController,
        client: TcpClientLike,
        protocol: HardwareProtocolLike,
        runtime_probe: RuntimeStatusProbeLike,
        policy: SupervisorPolicy,
    ) -> None:
        self._backend = backend
        self._client = client
        self._protocol = protocol
        self._runtime_probe = runtime_probe
        self._policy = policy
        self._active_session_id: str | None = None
        self._last_snapshot: SessionSnapshot | None = None

    def start(
        self,
        *,
        mode: str,
        progress_callback: ProgressCallback | None = None,
        snapshot_callback: SnapshotCallback | None = None,
        event_callback: EventCallback | None = None,
    ) -> SessionSnapshot:
        self._active_session_id = uuid4().hex
        session_id = self._active_session_id

        if mode == "offline":
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
        *,
        action: RecoveryAction,
        snapshot: SessionSnapshot,
        progress_callback: ProgressCallback | None = None,
        snapshot_callback: SnapshotCallback | None = None,
        event_callback: EventCallback | None = None,
    ) -> SessionSnapshot:
        if action == "retry_stage":
            return self.retry_stage(
                snapshot=snapshot,
                progress_callback=progress_callback,
                snapshot_callback=snapshot_callback,
                event_callback=event_callback,
            )
        if action == "restart_session":
            return self.restart_session(
                snapshot=snapshot,
                progress_callback=progress_callback,
                snapshot_callback=snapshot_callback,
                event_callback=event_callback,
            )
        return self.stop_session(
            snapshot=snapshot,
            snapshot_callback=snapshot_callback,
            event_callback=event_callback,
        )

    def retry_stage(
        self,
        *,
        snapshot: SessionSnapshot,
        progress_callback: ProgressCallback | None = None,
        snapshot_callback: SnapshotCallback | None = None,
        event_callback: EventCallback | None = None,
    ) -> SessionSnapshot:
        _assert_recovery_allowed("retry_stage", snapshot)

        session_id = self._active_session_id or uuid4().hex
        stage = snapshot.failure_stage or BACKEND_STARTING_STAGE
        self._emit_stage_event(
            event_callback=event_callback,
            session_id=session_id,
            event_type="recovery_invoked",
            stage=stage,
            failure_code=snapshot.failure_code,
            recoverable=snapshot.recoverable,
            message="retry_stage",
        )

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
        if stage in (
            RUNTIME_CONTRACT_READY_STAGE,
            HARDWARE_PROBING_STAGE,
            HARDWARE_READY_STAGE,
            ONLINE_READY_STAGE,
        ):
            return self._run_online_flow(
                start_stage=RUNTIME_CONTRACT_READY_STAGE,
                progress_callback=progress_callback,
                snapshot_callback=snapshot_callback,
                event_callback=event_callback,
                session_id=session_id,
            )
        return snapshot

    def restart_session(
        self,
        *,
        snapshot: SessionSnapshot,
        progress_callback: ProgressCallback | None = None,
        snapshot_callback: SnapshotCallback | None = None,
        event_callback: EventCallback | None = None,
    ) -> SessionSnapshot:
        _assert_recovery_allowed("restart_session", snapshot)
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
        *,
        snapshot: SessionSnapshot,
        snapshot_callback: SnapshotCallback | None = None,
        event_callback: EventCallback | None = None,
    ) -> SessionSnapshot:
        _assert_recovery_allowed("stop_session", snapshot)
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
            runtime_contract_verified=snapshot.runtime_contract_verified,
            runtime_identity=snapshot.runtime_identity,
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
            runtime_contract_verified=False,
            runtime_identity=None,
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
        states = _initial_states_for(start_stage)
        runtime_contract_verified = False
        runtime_identity: RuntimeIdentity | None = None

        if start_stage == BACKEND_STARTING_STAGE:
            self._emit_stage_event(event_callback, session_id, "stage_entered", BACKEND_STARTING_STAGE)
            self._emit_progress(progress_callback, "Starting backend...", 10)
            self._emit_starting_snapshot(
                snapshot_callback=snapshot_callback,
                message="Starting backend...",
                backend_state="starting",
                tcp_state=cast(TcpState, states["tcp_state"]),
                hardware_state=cast(HardwareState, states["hardware_state"]),
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
                    runtime_contract_verified=runtime_contract_verified,
                    runtime_identity=runtime_identity,
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
                    runtime_contract_verified=runtime_contract_verified,
                    runtime_identity=runtime_identity,
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
                    runtime_contract_verified=runtime_contract_verified,
                    runtime_identity=runtime_identity,
                    snapshot_callback=snapshot_callback,
                    event_callback=event_callback,
                    session_id=session_id,
                )
            self._emit_stage_event(event_callback, session_id, "stage_succeeded", TCP_CONNECTING_STAGE)
            self._emit_progress(progress_callback, "TCP connected", 60)
            states["tcp_state"] = "ready"

            self._emit_stage_event(event_callback, session_id, "stage_entered", TCP_READY_STAGE)
            is_connected = getattr(self._client, "is_connected", None)
            connected = bool(is_connected()) if callable(is_connected) else True
            if not connected:
                return self._failed(
                    stage=TCP_READY_STAGE,
                    code="SUP_TCP_CONNECT_FAILED",
                    message="TCP connected then disconnected immediately.",
                    recoverable=True,
                    backend_state="ready",
                    tcp_state="failed",
                    hardware_state="unavailable",
                    runtime_contract_verified=runtime_contract_verified,
                    runtime_identity=runtime_identity,
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
            RUNTIME_CONTRACT_READY_STAGE,
        ):
            self._emit_stage_event(event_callback, session_id, "stage_entered", RUNTIME_CONTRACT_READY_STAGE)
            self._emit_progress(progress_callback, "Verifying runtime contract...", 70)
            self._emit_starting_snapshot(
                snapshot_callback=snapshot_callback,
                message="Verifying runtime contract...",
                backend_state="ready",
                tcp_state="ready",
                hardware_state="unavailable",
                runtime_contract_verified=False,
                runtime_identity=None,
            )
            runtime_res, actual_runtime_identity = self._runtime_contract_ready()
            runtime_identity = actual_runtime_identity
            if not runtime_res.ok:
                return self._failed(
                    stage=RUNTIME_CONTRACT_READY_STAGE,
                    code=runtime_res.failure_code,
                    message=runtime_res.message,
                    recoverable=runtime_res.recoverable,
                    backend_state="ready",
                    tcp_state="ready",
                    hardware_state="unavailable",
                    runtime_contract_verified=False,
                    runtime_identity=runtime_identity,
                    snapshot_callback=snapshot_callback,
                    event_callback=event_callback,
                    session_id=session_id,
                )
            runtime_contract_verified = True
            self._emit_stage_event(event_callback, session_id, "stage_succeeded", RUNTIME_CONTRACT_READY_STAGE)

        if start_stage in (
            BACKEND_STARTING_STAGE,
            BACKEND_READY_STAGE,
            TCP_CONNECTING_STAGE,
            TCP_READY_STAGE,
            RUNTIME_CONTRACT_READY_STAGE,
            HARDWARE_PROBING_STAGE,
            HARDWARE_READY_STAGE,
            ONLINE_READY_STAGE,
        ):
            self._emit_stage_event(event_callback, session_id, "stage_entered", HARDWARE_PROBING_STAGE)
            self._emit_progress(progress_callback, "Initializing hardware...", 80)
            self._emit_starting_snapshot(
                snapshot_callback=snapshot_callback,
                message="Initializing hardware...",
                backend_state="ready",
                tcp_state="ready",
                hardware_state="probing",
                runtime_contract_verified=runtime_contract_verified,
                runtime_identity=runtime_identity,
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
                    runtime_contract_verified=runtime_contract_verified,
                    runtime_identity=runtime_identity,
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
                runtime_contract_verified=runtime_contract_verified,
                runtime_identity=runtime_identity,
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
            runtime_contract_verified=runtime_contract_verified,
            runtime_identity=runtime_identity,
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
                runtime_contract_verified=ready_snapshot.runtime_contract_verified,
                runtime_identity=ready_snapshot.runtime_identity,
                snapshot_callback=snapshot_callback,
                event_callback=event_callback,
                session_id=session_id,
            )
        self._emit_stage_event(event_callback, session_id, "stage_succeeded", ONLINE_READY_STAGE)
        return self._emit_snapshot(ready_snapshot, snapshot_callback)

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
        backend_state: BackendState,
        tcp_state: TcpState,
        hardware_state: HardwareState,
        runtime_contract_verified: bool = False,
        runtime_identity: RuntimeIdentity | None = None,
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
                runtime_contract_verified=runtime_contract_verified,
                runtime_identity=runtime_identity,
            ),
            snapshot_callback,
        )

    def _emit_stage_event(
        self,
        event_callback: EventCallback | None,
        session_id: str,
        event_type: StageEventType,
        stage: FailureStage,
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
        stage: FailureStage,
        code: FailureCode | None,
        message: str,
        recoverable: bool,
        backend_state: BackendState,
        tcp_state: TcpState,
        hardware_state: HardwareState,
        runtime_contract_verified: bool,
        runtime_identity: RuntimeIdentity | None,
        snapshot_callback: SnapshotCallback | None,
        event_callback: EventCallback | None,
        session_id: str,
    ) -> SessionSnapshot:
        failure_code: FailureCode = code or "SUP_UNKNOWN"
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
            runtime_contract_verified=runtime_contract_verified,
            runtime_identity=runtime_identity,
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
        start_detailed = getattr(self._backend, "start_detailed", None)
        if callable(start_detailed):
            result = start_detailed()
            return StepResult(
                ok=bool(getattr(result, "ok", False)),
                message=str(getattr(result, "message", "")),
                failure_code=getattr(result, "failure_code", None),
                recoverable=bool(getattr(result, "recoverable", True)),
            )
        basic_backend = cast(BackendManagerBasic, self._backend)
        ok, msg = basic_backend.start()
        return StepResult(
            ok=ok,
            message=msg,
            failure_code=None if ok else "SUP_BACKEND_START_FAILED",
            recoverable=True,
        )

    def _backend_wait_ready(self) -> StepResult:
        wait_ready_detailed = getattr(self._backend, "wait_ready_detailed", None)
        if callable(wait_ready_detailed):
            try:
                result = wait_ready_detailed(timeout=self._policy.backend_ready_timeout_s)
            except TypeError:
                result = wait_ready_detailed()
            return StepResult(
                ok=bool(getattr(result, "ok", False)),
                message=str(getattr(result, "message", "")),
                failure_code=getattr(result, "failure_code", None),
                recoverable=bool(getattr(result, "recoverable", True)),
            )
        basic_backend = cast(BackendManagerBasic, self._backend)
        try:
            ok, msg = basic_backend.wait_ready(timeout=self._policy.backend_ready_timeout_s)
        except TypeError:
            ok, msg = basic_backend.wait_ready()
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

    def _resolve_expected_runtime_identity(self) -> tuple[RuntimeIdentity | None, str | None]:
        executable_path = str(getattr(self._backend, "exe_path", "") or "").strip()
        working_directory = str(getattr(self._backend, "working_directory", "") or "").strip()
        if not executable_path:
            return None, "Local launch contract missing expected executable_path"
        if not working_directory:
            return None, "Local launch contract missing expected working_directory"
        return (
            RuntimeIdentity(
                executable_path=executable_path,
                working_directory=working_directory,
            ),
            None,
        )

    def _runtime_contract_ready(self) -> tuple[StepResult, RuntimeIdentity | None]:
        get_status_detailed = getattr(self._runtime_probe, "get_status_detailed", None)
        if not callable(get_status_detailed):
            return (
                StepResult(
                    ok=False,
                    message="Runtime status probe unavailable",
                    failure_code="SUP_RUNTIME_STATUS_FAILED",
                    recoverable=True,
                ),
                None,
            )

        try:
            status_result = get_status_detailed(timeout=5.0)
        except TypeError:
            status_result = get_status_detailed()

        if not bool(getattr(status_result, "ok", False)):
            message = str(getattr(status_result, "error_message", "") or "status query failed")
            return (
                StepResult(
                    ok=False,
                    message=message,
                    failure_code="SUP_RUNTIME_STATUS_FAILED",
                    recoverable=True,
                ),
                None,
            )

        status = getattr(status_result, "status", None)
        actual_runtime_identity = _coerce_runtime_identity(getattr(status, "runtime_identity", None))
        expected_runtime_identity, contract_error = self._resolve_expected_runtime_identity()
        if expected_runtime_identity is None:
            return (
                StepResult(
                    ok=False,
                    message=contract_error or "Local launch contract unavailable",
                    failure_code="SUP_RUNTIME_CONTRACT_MISMATCH",
                    recoverable=True,
                ),
                actual_runtime_identity,
            )

        mismatch = _compare_runtime_identity(actual_runtime_identity, expected_runtime_identity)
        if mismatch is not None:
            return (
                StepResult(
                    ok=False,
                    message=f"Runtime contract mismatch: {mismatch}",
                    failure_code="SUP_RUNTIME_CONTRACT_MISMATCH",
                    recoverable=True,
                ),
                actual_runtime_identity,
            )

        return StepResult(ok=True, message="Runtime contract verified"), actual_runtime_identity


def _assert_recovery_allowed(action: RecoveryAction, snapshot: SessionSnapshot) -> None:
    if snapshot.mode != "online":
        raise ValueError("Recovery actions are only supported for online session snapshots.")
    if action == "stop_session":
        if snapshot.session_state not in ("ready", "failed"):
            raise ValueError("stop_session requires a ready or failed online session snapshot.")
        return
    if snapshot.session_state != "failed":
        raise ValueError(f"{action} requires a failed online session snapshot.")
    if not snapshot.recoverable:
        raise ValueError(f"{action} requires a recoverable failed session snapshot.")
    if snapshot.failure_stage is None:
        raise ValueError(f"{action} requires failure_stage on the failed session snapshot.")


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
        RUNTIME_CONTRACT_READY_STAGE,
        HARDWARE_PROBING_STAGE,
        HARDWARE_READY_STAGE,
        ONLINE_READY_STAGE,
    ):
        states["backend_state"] = "ready"
    if start_stage in (
        RUNTIME_CONTRACT_READY_STAGE,
        HARDWARE_PROBING_STAGE,
        HARDWARE_READY_STAGE,
        ONLINE_READY_STAGE,
    ):
        states["tcp_state"] = "ready"
    if start_stage == ONLINE_READY_STAGE:
        states["hardware_state"] = "ready"
    return states
