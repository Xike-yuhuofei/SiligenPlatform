from __future__ import annotations

from typing import Callable

from ..adapters.launch_supervision_env import load_supervisor_policy_from_env
from ..adapters.launch_supervision_ports import BackendController, HardwareProtocolLike, TcpClientLike
from ..contracts.launch_supervision_contract import RecoveryAction, SessionSnapshot, SessionStageEvent
from ..domain.launch_result_types import LaunchResult
from ..domain.launch_supervision_types import SupervisorPolicy, normalize_launch_mode
from ..launch_supervision_session import SupervisorSession
from .launch_result_projection import build_launch_result


def run_launch_sequence(
    launch_mode: str,
    backend: BackendController,
    client: TcpClientLike,
    protocol: HardwareProtocolLike,
    progress_callback: Callable[[str, int], None] | None = None,
    snapshot_callback: Callable[[SessionSnapshot], None] | None = None,
    event_callback: Callable[[SessionStageEvent], None] | None = None,
    policy: SupervisorPolicy | None = None,
) -> LaunchResult:
    mode = normalize_launch_mode(launch_mode)
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
    return build_launch_result(mode, snapshot)


def run_recovery_action(
    action: RecoveryAction,
    snapshot: SessionSnapshot,
    backend: BackendController,
    client: TcpClientLike,
    protocol: HardwareProtocolLike,
    progress_callback: Callable[[str, int], None] | None = None,
    snapshot_callback: Callable[[SessionSnapshot], None] | None = None,
    event_callback: Callable[[SessionStageEvent], None] | None = None,
    policy: SupervisorPolicy | None = None,
) -> LaunchResult:
    if snapshot.mode != "online":
        raise ValueError("Recovery actions are only supported for online session snapshots.")

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
    return build_launch_result("online", recovered_snapshot)


__all__ = [
    "run_launch_sequence",
    "run_recovery_action",
]
