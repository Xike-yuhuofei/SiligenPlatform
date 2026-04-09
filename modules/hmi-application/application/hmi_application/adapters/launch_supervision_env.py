from __future__ import annotations

import os

from .logging_support import get_hmi_application_file_logger
from ..domain.launch_supervision_types import (
    DEFAULT_BACKEND_READY_TIMEOUT_S,
    DEFAULT_HARDWARE_PROBE_TIMEOUT_S,
    DEFAULT_TCP_CONNECT_TIMEOUT_S,
    SupervisorPolicy,
)

BACKEND_READY_TIMEOUT_ENV = "SILIGEN_SUP_BACKEND_READY_TIMEOUT_S"
TCP_CONNECT_TIMEOUT_ENV = "SILIGEN_SUP_TCP_CONNECT_TIMEOUT_S"
HARDWARE_PROBE_TIMEOUT_ENV = "SILIGEN_SUP_HARDWARE_PROBE_TIMEOUT_S"


_STARTUP_LOGGER = get_hmi_application_file_logger("hmi.startup", "hmi_startup.log")


def parse_positive_timeout(env_name: str, default_value: float) -> float:
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
        backend_ready_timeout_s=parse_positive_timeout(BACKEND_READY_TIMEOUT_ENV, DEFAULT_BACKEND_READY_TIMEOUT_S),
        tcp_connect_timeout_s=parse_positive_timeout(TCP_CONNECT_TIMEOUT_ENV, DEFAULT_TCP_CONNECT_TIMEOUT_S),
        hardware_probe_timeout_s=parse_positive_timeout(HARDWARE_PROBE_TIMEOUT_ENV, DEFAULT_HARDWARE_PROBE_TIMEOUT_S),
    )


__all__ = [
    "BACKEND_READY_TIMEOUT_ENV",
    "HARDWARE_PROBE_TIMEOUT_ENV",
    "TCP_CONNECT_TIMEOUT_ENV",
    "load_supervisor_policy_from_env",
    "parse_positive_timeout",
]
