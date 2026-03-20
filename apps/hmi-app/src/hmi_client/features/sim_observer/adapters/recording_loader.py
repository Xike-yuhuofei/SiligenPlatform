"""Minimal loader for simulation recording payloads."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping


class RecordingLoadError(Exception):
    """Base error for recording load failures."""


class RecordingSchemaError(RecordingLoadError):
    """Raised when the minimal recording schema is invalid."""


class RecordingSourceError(RecordingLoadError):
    """Raised when the recording source cannot be read."""


@dataclass(frozen=True, slots=True)
class RawRecordingPayload:
    summary: Mapping[str, Any]
    snapshots: tuple[Mapping[str, Any], ...]
    timeline: tuple[Mapping[str, Any], ...]
    trace: tuple[Mapping[str, Any], ...]
    motion_profile: tuple[Mapping[str, Any], ...]
    raw_root: Mapping[str, Any]
    source_path: Path | None = None


def load_recording_from_file(path: str | Path) -> RawRecordingPayload:
    file_path = Path(path)
    if not file_path.exists():
        raise RecordingSourceError(f"Recording file does not exist: {file_path}")
    try:
        data = json.loads(file_path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise RecordingSourceError(f"Failed to read recording file: {file_path}") from exc
    except json.JSONDecodeError as exc:
        raise RecordingSourceError(f"Invalid JSON in recording file: {file_path}") from exc
    if not isinstance(data, Mapping):
        raise RecordingSchemaError("Recording root must be a mapping.")
    payload = load_recording_from_mapping(data)
    return RawRecordingPayload(
        summary=payload.summary,
        snapshots=payload.snapshots,
        timeline=payload.timeline,
        trace=payload.trace,
        motion_profile=payload.motion_profile,
        raw_root=payload.raw_root,
        source_path=file_path,
    )


def load_recording_from_mapping(data: Mapping[str, Any]) -> RawRecordingPayload:
    payload_root = _extract_payload_root(data)
    _validate_minimal_schema(payload_root)
    return RawRecordingPayload(
        summary=payload_root["summary"],
        snapshots=tuple(payload_root["snapshots"]),
        timeline=tuple(payload_root["timeline"]),
        trace=tuple(payload_root["trace"]),
        motion_profile=_normalize_motion_profile(payload_root["motion_profile"]),
        raw_root=payload_root,
    )


def _extract_payload_root(data: Mapping[str, Any]) -> Mapping[str, Any]:
    recording = data.get("recording")
    if isinstance(recording, Mapping):
        return recording
    return data


def _validate_minimal_schema(data: Mapping[str, Any]) -> None:
    required_keys = {
        "summary": Mapping,
        "snapshots": list,
        "timeline": list,
        "trace": list,
    }
    for key, expected in required_keys.items():
        if key not in data:
            raise RecordingSchemaError(f"Missing required key: {key}")
        if not isinstance(data[key], expected):
            raise RecordingSchemaError(f"Key {key} must be of type {expected.__name__}.")
    if "motion_profile" not in data:
        raise RecordingSchemaError("Missing required key: motion_profile")
    motion_profile = data["motion_profile"]
    if not isinstance(motion_profile, (list, Mapping)):
        raise RecordingSchemaError("Key motion_profile must be a list or mapping.")


def _normalize_motion_profile(value: Any) -> tuple[Mapping[str, Any], ...]:
    if isinstance(value, list):
        if not all(isinstance(item, Mapping) for item in value):
            raise RecordingSchemaError("motion_profile list must contain mappings.")
        return tuple(value)
    if isinstance(value, Mapping):
        segments = value.get("segments")
        if isinstance(segments, list):
            if not all(isinstance(item, Mapping) for item in segments):
                raise RecordingSchemaError("motion_profile.segments must contain mappings.")
            return tuple(segments)
        raise RecordingSchemaError("motion_profile mapping must contain a segments list.")
    raise RecordingSchemaError("Unsupported motion_profile value.")


__all__ = [
    "RawRecordingPayload",
    "RecordingLoadError",
    "RecordingSchemaError",
    "RecordingSourceError",
    "load_recording_from_file",
    "load_recording_from_mapping",
]
