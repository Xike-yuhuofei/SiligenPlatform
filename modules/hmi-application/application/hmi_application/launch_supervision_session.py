"""Thin launch supervision seam over split flow/runtime services."""
from __future__ import annotations

from .adapters.launch_supervision_ports import (
    BackendController,
    HardwareProtocolLike,
    RuntimeStatusProbeLike,
    TcpClientLike,
)
from .contracts.launch_supervision_contract import (
    BackendState,
    FailureCode,
    FailureStage,
    HardwareState,
    RecoveryAction,
    SessionSnapshot,
    SessionStageEvent,
    TcpState,
)
from .domain.launch_supervision_types import (
    DEFAULT_BACKEND_READY_TIMEOUT_S,
    DEFAULT_HARDWARE_PROBE_TIMEOUT_S,
    DEFAULT_TCP_CONNECT_TIMEOUT_S,
    EventCallback,
    ProgressCallback,
    SnapshotCallback,
    SupervisorPolicy,
    normalize_launch_mode,
)
from .services.launch_supervision_flow import LaunchSupervisionFlow
from .services.launch_supervision_runtime import (
    build_runtime_failure_snapshot,
    detect_runtime_degradation,
    detect_runtime_degradation_with_event,
)


class SupervisorSession:
    BACKEND_READY_TIMEOUT = DEFAULT_BACKEND_READY_TIMEOUT_S
    TCP_CONNECT_TIMEOUT = DEFAULT_TCP_CONNECT_TIMEOUT_S
    HARDWARE_PROBE_TIMEOUT = DEFAULT_HARDWARE_PROBE_TIMEOUT_S

    def __init__(
        self,
        backend: BackendController,
        client: TcpClientLike,
        protocol: HardwareProtocolLike,
        runtime_probe: RuntimeStatusProbeLike,
        launch_mode: str = "online",
        policy: SupervisorPolicy | None = None,
    ) -> None:
        self._mode = normalize_launch_mode(launch_mode)
        self._flow = LaunchSupervisionFlow(
            backend=backend,
            client=client,
            protocol=protocol,
            runtime_probe=runtime_probe,
            policy=policy if policy is not None else SupervisorPolicy(
                backend_ready_timeout_s=self.BACKEND_READY_TIMEOUT,
                tcp_connect_timeout_s=self.TCP_CONNECT_TIMEOUT,
                hardware_probe_timeout_s=self.HARDWARE_PROBE_TIMEOUT,
            ),
        )

    def start(
        self,
        progress_callback: ProgressCallback | None = None,
        snapshot_callback: SnapshotCallback | None = None,
        event_callback: EventCallback | None = None,
    ) -> SessionSnapshot:
        return self._flow.start(
            mode=self._mode,
            progress_callback=progress_callback,
            snapshot_callback=snapshot_callback,
            event_callback=event_callback,
        )

    def invoke_recovery(
        self,
        action: RecoveryAction,
        snapshot: SessionSnapshot,
        progress_callback: ProgressCallback | None = None,
        snapshot_callback: SnapshotCallback | None = None,
        event_callback: EventCallback | None = None,
    ) -> SessionSnapshot:
        return self._flow.invoke_recovery(
            action=action,
            snapshot=snapshot,
            progress_callback=progress_callback,
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
        return self._flow.retry_stage(
            snapshot=snapshot,
            progress_callback=progress_callback,
            snapshot_callback=snapshot_callback,
            event_callback=event_callback,
        )

    def restart_session(
        self,
        snapshot: SessionSnapshot,
        progress_callback: ProgressCallback | None = None,
        snapshot_callback: SnapshotCallback | None = None,
        event_callback: EventCallback | None = None,
    ) -> SessionSnapshot:
        return self._flow.restart_session(
            snapshot=snapshot,
            progress_callback=progress_callback,
            snapshot_callback=snapshot_callback,
            event_callback=event_callback,
        )

    def stop_session(
        self,
        snapshot: SessionSnapshot,
        snapshot_callback: SnapshotCallback | None = None,
        event_callback: EventCallback | None = None,
    ) -> SessionSnapshot:
        return self._flow.stop_session(
            snapshot=snapshot,
            snapshot_callback=snapshot_callback,
            event_callback=event_callback,
        )

    @staticmethod
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
        return build_runtime_failure_snapshot(
            snapshot,
            failure_code=failure_code,
            failure_stage=failure_stage,
            message=message,
            backend_state=backend_state,
            tcp_state=tcp_state,
            hardware_state=hardware_state,
            recoverable=recoverable,
        )

    @staticmethod
    def detect_runtime_degradation(
        snapshot: SessionSnapshot | None,
        *,
        tcp_connected: bool | None = None,
        hardware_ready: bool | None = None,
        error_message: str | None = None,
    ) -> SessionSnapshot | None:
        return detect_runtime_degradation(
            snapshot,
            tcp_connected=tcp_connected,
            hardware_ready=hardware_ready,
            error_message=error_message,
        )

    @staticmethod
    def detect_runtime_degradation_with_event(
        snapshot: SessionSnapshot | None,
        *,
        session_id: str | None = None,
        tcp_connected: bool | None = None,
        hardware_ready: bool | None = None,
        error_message: str | None = None,
    ) -> tuple[SessionSnapshot, SessionStageEvent] | None:
        return detect_runtime_degradation_with_event(
            snapshot,
            session_id=session_id,
            tcp_connected=tcp_connected,
            hardware_ready=hardware_ready,
            error_message=error_message,
        )


__all__ = [
    "BackendController",
    "HardwareProtocolLike",
    "RuntimeStatusProbeLike",
    "SupervisorPolicy",
    "SupervisorSession",
    "TcpClientLike",
]
