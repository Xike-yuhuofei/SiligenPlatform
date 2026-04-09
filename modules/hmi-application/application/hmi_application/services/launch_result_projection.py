from __future__ import annotations

from ..contracts.launch_supervision_contract import LaunchMode, SessionSnapshot, snapshot_timestamp
from ..domain.launch_result_types import LaunchResult
from ..domain.launch_supervision_types import normalize_launch_mode


def build_launch_result(requested_mode: LaunchMode, snapshot: SessionSnapshot) -> LaunchResult:
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


def build_offline_launch_result(user_message: str | None = None) -> LaunchResult:
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
    return launch_result_from_snapshot(
        requested_mode="offline",
        snapshot=snapshot,
        user_message=user_message or "Offline 模式已启用，本次启动不会尝试连接 gateway。",
    )


__all__ = [
    "build_launch_result",
    "build_offline_launch_result",
    "launch_result_from_snapshot",
]
