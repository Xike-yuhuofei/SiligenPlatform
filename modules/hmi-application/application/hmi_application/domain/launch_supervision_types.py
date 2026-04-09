from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, cast

from ..contracts.launch_supervision_contract import FailureCode, FailureStage, LaunchMode, SessionSnapshot, SessionStageEvent

ProgressCallback = Callable[[str, int], None]
SnapshotCallback = Callable[[SessionSnapshot], None]
EventCallback = Callable[[SessionStageEvent], None]

DEFAULT_BACKEND_READY_TIMEOUT_S = 5.0
DEFAULT_TCP_CONNECT_TIMEOUT_S = 3.0
DEFAULT_HARDWARE_PROBE_TIMEOUT_S = 15.0

BACKEND_STARTING_STAGE: FailureStage = "backend_starting"
BACKEND_READY_STAGE: FailureStage = "backend_ready"
TCP_CONNECTING_STAGE: FailureStage = "tcp_connecting"
TCP_READY_STAGE: FailureStage = "tcp_ready"
HARDWARE_PROBING_STAGE: FailureStage = "hardware_probing"
HARDWARE_READY_STAGE: FailureStage = "hardware_ready"
ONLINE_READY_STAGE: FailureStage = "online_ready"


@dataclass(frozen=True)
class StepResult:
    ok: bool
    message: str
    failure_code: FailureCode | None = None
    recoverable: bool = True


@dataclass(frozen=True)
class SupervisorPolicy:
    """Timeout policy for supervisor startup and recovery orchestration."""

    backend_ready_timeout_s: float = DEFAULT_BACKEND_READY_TIMEOUT_S
    tcp_connect_timeout_s: float = DEFAULT_TCP_CONNECT_TIMEOUT_S
    hardware_probe_timeout_s: float = DEFAULT_HARDWARE_PROBE_TIMEOUT_S

    def __post_init__(self) -> None:
        if self.backend_ready_timeout_s <= 0:
            raise ValueError("backend_ready_timeout_s must be > 0")
        if self.tcp_connect_timeout_s <= 0:
            raise ValueError("tcp_connect_timeout_s must be > 0")
        if self.hardware_probe_timeout_s <= 0:
            raise ValueError("hardware_probe_timeout_s must be > 0")


def normalize_launch_mode(value: str | None) -> LaunchMode:
    mode = (value or "online").strip().lower()
    if mode not in ("online", "offline"):
        raise ValueError(f"Unsupported launch mode: {value}")
    return cast(LaunchMode, mode)
