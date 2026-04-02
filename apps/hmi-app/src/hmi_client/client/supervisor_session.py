from __future__ import annotations

try:
    from hmi_client.module_paths import ensure_hmi_application_path
except ImportError:  # pragma: no cover - script-mode fallback
    from module_paths import ensure_hmi_application_path  # type: ignore

ensure_hmi_application_path()

from hmi_application.launch_supervision_session import *  # noqa: F401,F403
