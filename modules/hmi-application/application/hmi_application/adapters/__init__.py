from __future__ import annotations

from importlib import import_module

_EXPORTS = {
    "DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S": (".qt_workers", "DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S"),
    "PreviewSnapshotWorker": (".qt_workers", "PreviewSnapshotWorker"),
    "RecoveryWorker": (".startup_qt_workers", "RecoveryWorker"),
    "StartupWorker": (".startup_qt_workers", "StartupWorker"),
    "get_hmi_application_file_logger": (".logging_support", "get_hmi_application_file_logger"),
    "load_supervisor_policy_from_env": (".launch_supervision_env", "load_supervisor_policy_from_env"),
}

__all__ = list(_EXPORTS)


def __getattr__(name: str):
    try:
        module_name, attr_name = _EXPORTS[name]
    except KeyError as exc:
        raise AttributeError(f"module {__name__!r} has no attribute {name!r}") from exc

    value = getattr(import_module(module_name, __name__), attr_name)
    globals()[name] = value
    return value
