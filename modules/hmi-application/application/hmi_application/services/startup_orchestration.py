from __future__ import annotations

from typing import Callable

from ..adapters.logging_support import get_hmi_application_file_logger
from ..adapters.launch_supervision_ports import BackendController, HardwareProtocolLike, RuntimeStatusProbeLike, TcpClientLike
from ..contracts.launch_supervision_contract import (
    RecoveryAction,
    SessionSnapshot,
    SessionStageEvent,
)
from ..domain.launch_result_types import LaunchResult
from ..domain.launch_supervision_types import SupervisorPolicy, normalize_launch_mode
from .launch_result_projection import build_launch_result, build_offline_launch_result, launch_result_from_snapshot
from .startup_sequence_service import (
    run_launch_sequence as _run_launch_sequence_impl,
    run_recovery_action as _run_recovery_action_impl,
)

_STARTUP_LOGGER = get_hmi_application_file_logger("hmi.startup", "hmi_startup.log")


def run_launch_sequence(
    launch_mode: str,
    backend: BackendController,
    client: TcpClientLike,
    protocol: HardwareProtocolLike,
    runtime_probe: RuntimeStatusProbeLike,
    progress_callback: Callable[[str, int], None] | None = None,
    snapshot_callback: Callable[[SessionSnapshot], None] | None = None,
    event_callback: Callable[[SessionStageEvent], None] | None = None,
    policy: SupervisorPolicy | None = None,
) -> LaunchResult:
    mode = normalize_launch_mode(launch_mode)
    _STARTUP_LOGGER.info("Launch mode requested: %s", mode)
    result = _run_launch_sequence_impl(
        launch_mode=mode,
        backend=backend,
        client=client,
        protocol=protocol,
        runtime_probe=runtime_probe,
        progress_callback=progress_callback,
        snapshot_callback=snapshot_callback,
        event_callback=event_callback,
        policy=policy,
    )
    log_launch_result(result)
    return result


def run_recovery_action(
    action: RecoveryAction,
    snapshot: SessionSnapshot,
    backend: BackendController,
    client: TcpClientLike,
    protocol: HardwareProtocolLike,
    runtime_probe: RuntimeStatusProbeLike,
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
    result = _run_recovery_action_impl(
        action=action,
        snapshot=snapshot,
        backend=backend,
        client=client,
        protocol=protocol,
        runtime_probe=runtime_probe,
        progress_callback=progress_callback,
        snapshot_callback=snapshot_callback,
        event_callback=event_callback,
        policy=policy,
    )
    log_launch_result(result)
    return result


def log_launch_result(result: LaunchResult) -> None:
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


__all__ = [
    "build_launch_result",
    "build_offline_launch_result",
    "launch_result_from_snapshot",
    "log_launch_result",
    "run_launch_sequence",
    "run_recovery_action",
]
