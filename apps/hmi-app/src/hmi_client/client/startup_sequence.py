from __future__ import annotations

try:
    from hmi_client.module_paths import ensure_hmi_application_path
except ImportError:  # pragma: no cover - script-mode fallback
    from module_paths import ensure_hmi_application_path  # type: ignore

ensure_hmi_application_path()

from hmi_application.startup import (
    LaunchMode,
    LaunchResult,
    StartupResult,
    build_offline_launch_result,
    launch_result_from_snapshot,
    load_supervisor_policy_from_env,
    normalize_launch_mode,
    run_launch_sequence,
    run_recovery_action,
)

__all__ = [
    "LaunchMode",
    "LaunchResult",
    "StartupResult",
    "build_offline_launch_result",
    "launch_result_from_snapshot",
    "load_supervisor_policy_from_env",
    "normalize_launch_mode",
    "run_launch_sequence",
    "run_recovery_action",
]
