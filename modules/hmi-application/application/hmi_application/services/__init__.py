from __future__ import annotations

from importlib import import_module

_EXPORTS = {
    "LaunchSupervisionFlow": (".launch_supervision_flow", "LaunchSupervisionFlow"),
    "PreviewPlaybackController": (".preview_playback", "PreviewPlaybackController"),
    "PreviewPlaybackModel": (".preview_playback", "PreviewPlaybackModel"),
    "PreviewPlaybackState": (".preview_playback", "PreviewPlaybackState"),
    "PreviewPlaybackStatus": (".preview_playback", "PreviewPlaybackStatus"),
    "PreviewPayloadAuthorityService": (".preview_payload_authority", "PreviewPayloadAuthorityService"),
    "PreviewPreflightService": (".preview_preflight", "PreviewPreflightService"),
    "PreviewStatusProjectionService": (".preview_status_projection", "PreviewStatusProjectionService"),
    "build_launch_result": (".launch_result_projection", "build_launch_result"),
    "build_offline_launch_result": (".launch_result_projection", "build_offline_launch_result"),
    "build_runtime_failure_snapshot": (".launch_supervision_runtime", "build_runtime_failure_snapshot"),
    "detect_runtime_degradation": (".launch_supervision_runtime", "detect_runtime_degradation"),
    "detect_runtime_degradation_with_event": (".launch_supervision_runtime", "detect_runtime_degradation_with_event"),
    "launch_result_from_snapshot": (".launch_result_projection", "launch_result_from_snapshot"),
    "run_launch_sequence": (".startup_sequence_service", "run_launch_sequence"),
    "run_recovery_action": (".startup_sequence_service", "run_recovery_action"),
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
