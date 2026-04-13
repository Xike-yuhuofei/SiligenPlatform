from __future__ import annotations

import argparse
import base64
import configparser
import json
import os
import socket
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[3]
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
if str(HIL_DIR) not in sys.path:
    sys.path.insert(0, str(HIL_DIR))

from runtime_gateway_harness import (  # noqa: E402
    KNOWN_FAILURE_EXIT_CODE,
    TcpJsonClient,
    build_process_env,
    read_process_output,
    resolve_default_exe,
    truncate_json,
    wait_gateway_ready,
)


DEFAULT_DXF_FILE = ROOT / "samples" / "dxf" / "rect_diag.dxf"
DEFAULT_CONFIG_PATH = ROOT / "config" / "machine" / "machine_config.ini"
DEFAULT_REPORT_ROOT = ROOT / "tests" / "reports" / "adhoc" / "real-dxf-machine-dryrun-canonical"
DEFAULT_COORD_SYS = 1
DEFAULT_CONTRADICTION_PROGRESS_THRESHOLD_PERCENT = 95.0
DEFAULT_CONTRADICTION_CONSECUTIVE_SAMPLES = 5
DEFAULT_CONTRADICTION_GRACE_SECONDS = 0.5
DEFAULT_POSITION_EPSILON_MM = 0.001
DEFAULT_VELOCITY_EPSILON_MM_S = 0.001
DEFAULT_JOB_TIMEOUT_SCALE = 2.0
DEFAULT_JOB_TIMEOUT_BUFFER_SECONDS = 15.0
CRDSYS_STATUS_PROG_RUN = 0x00000001
CRDSYS_STATUS_PROG_STOP = 0x00000002
CRDSYS_STATUS_PROG_ESTOP = 0x00000004
CRDSYS_STATUS_FIFO_FINISH_0 = 0x00000010
CRDSYS_STATUS_FIFO_FINISH_1 = 0x00000020
CRDSYS_STATUS_ALARM = 0x00000040
CRDSYS_STATUS_ACTIVE_MOTION_MASK = CRDSYS_STATUS_PROG_RUN
CRDSYS_STATUS_IDLE_ALLOWED_MASK = (
    CRDSYS_STATUS_PROG_STOP
    | CRDSYS_STATUS_FIFO_FINISH_0
    | CRDSYS_STATUS_FIFO_FINISH_1
)

SUPPORTED_DRYRUN_PHASES = (
    "prepare",
    "binding",
    "start",
    "coord_running",
    "axis_motion",
    "terminal",
)
SUPPORTED_DRYRUN_VERDICTS = (
    "completed",
    "canonical_step_failed",
    "motion_timeout_unclassified",
    "state_contradiction",
)
ENABLED_DRYRUN_VERDICTS = (
    "completed",
    "canonical_step_failed",
    "motion_timeout_unclassified",
)
STATE_CONTRADICTION_SIGNAL_FIELDS = (
    "job.state",
    "job.current_segment",
    "job.total_segments",
    "job.cycle_progress_percent",
    "job.overall_progress_percent",
    "coord.state",
    "coord.is_moving",
    "coord.remaining_segments",
    "coord.current_velocity",
    "coord.raw_status_word",
    "coord.raw_segment",
    "coord.position.x",
    "coord.position.y",
    "coord.axes.X.position",
    "coord.axes.X.velocity",
    "coord.axes.Y.position",
    "coord.axes.Y.velocity",
)
SUPPORTED_MOCK_IO_KEYS = (
    "estop",
    "door",
    "limit_x_pos",
    "limit_x_neg",
    "limit_y_pos",
    "limit_y_neg",
)
MOCK_IO_TO_SAFETY_FIELD = {
    "estop": "estop",
    "door": "door_open",
    "limit_x_pos": "limit_x_pos",
    "limit_x_neg": "home_boundary_x_active",
    "limit_y_pos": "limit_y_pos",
    "limit_y_neg": "home_boundary_y_active",
}


@dataclass
class Step:
    name: str
    status: str
    note: str
    payload: dict[str, Any] | None = None
    timestamp: str = ""


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run canonical real DXF + machine dry-run smoke.")
    parser.add_argument("--gateway-exe", type=Path, default=resolve_default_exe("siligen_runtime_gateway.exe"))
    parser.add_argument("--config-path", type=Path, default=DEFAULT_CONFIG_PATH)
    parser.add_argument("--dxf-file", type=Path, default=DEFAULT_DXF_FILE)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--gateway-ready-timeout", type=float, default=8.0)
    parser.add_argument("--connect-timeout", type=float, default=15.0)
    parser.add_argument("--home-timeout", type=float, default=60.0)
    parser.add_argument("--job-timeout", type=float, default=120.0)
    parser.add_argument("--job-timeout-scale", type=float, default=DEFAULT_JOB_TIMEOUT_SCALE)
    parser.add_argument("--job-timeout-buffer-seconds", type=float, default=DEFAULT_JOB_TIMEOUT_BUFFER_SECONDS)
    parser.add_argument("--report-root", type=Path, default=DEFAULT_REPORT_ROOT)
    parser.add_argument("--dispensing-speed-mm-s", type=float, default=10.0)
    parser.add_argument("--dry-run-speed-mm-s", type=float, default=10.0)
    parser.add_argument("--rapid-speed-mm-s", type=float, default=20.0)
    parser.add_argument("--velocity-trace-interval-ms", type=int, default=50)
    parser.add_argument(
        "--contradiction-progress-threshold-percent",
        type=float,
        default=DEFAULT_CONTRADICTION_PROGRESS_THRESHOLD_PERCENT,
    )
    parser.add_argument(
        "--contradiction-consecutive-samples",
        type=int,
        default=DEFAULT_CONTRADICTION_CONSECUTIVE_SAMPLES,
    )
    parser.add_argument(
        "--contradiction-grace-seconds",
        type=float,
        default=DEFAULT_CONTRADICTION_GRACE_SECONDS,
    )
    parser.add_argument("--position-epsilon-mm", type=float, default=DEFAULT_POSITION_EPSILON_MM)
    parser.add_argument("--velocity-epsilon-mm-s", type=float, default=DEFAULT_VELOCITY_EPSILON_MM_S)
    parser.add_argument("--mock-io-json", default="")
    parser.add_argument("--mock-io-settle-timeout-seconds", type=float, default=5.0)
    return parser.parse_args()


def add_step(
    steps: list[Step],
    name: str,
    status: str,
    note: str,
    payload: dict[str, Any] | None = None,
) -> None:
    steps.append(
        Step(
            name=name,
            status=status,
            note=note,
            payload=payload,
            timestamp=utc_now(),
        )
    )


def ensure_exists(path: Path, label: str) -> None:
    if not path.exists():
        raise FileNotFoundError(f"{label} missing: {path}")


def resolve_listen_port(host: str, requested_port: int) -> int:
    if requested_port > 0:
        return requested_port

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((host, 0))
        return int(sock.getsockname()[1])


def parse_mock_io_json(raw_text: str) -> dict[str, bool]:
    payload_text = str(raw_text or "").strip()
    if not payload_text:
        return {}

    try:
        payload = json.loads(payload_text)
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid --mock-io-json: {exc}") from exc

    if not isinstance(payload, dict):
        raise ValueError("--mock-io-json must decode to an object")

    normalized: dict[str, bool] = {}
    for raw_key, raw_value in payload.items():
        key = str(raw_key).strip()
        if key not in SUPPORTED_MOCK_IO_KEYS:
            raise ValueError(
                "--mock-io-json contains unsupported key: "
                f"{key}; supported={','.join(SUPPORTED_MOCK_IO_KEYS)}"
            )
        normalized[key] = bool(raw_value)
    return normalized


def load_connection_params(config_path: Path) -> dict[str, str]:
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    with config_path.open("r", encoding="utf-8") as handle:
        parser.read_file(handle)

    card_ip = parser.get("Network", "control_card_ip", fallback=parser.get("Network", "card_ip", fallback="")).strip()
    local_ip = parser.get("Network", "local_ip", fallback="").strip()

    params: dict[str, str] = {}
    if card_ip:
        params["card_ip"] = card_ip
    if local_ip:
        params["local_ip"] = local_ip
    return params


def status_result(payload: dict[str, Any]) -> dict[str, Any]:
    result = payload.get("result", {})
    return result if isinstance(result, dict) else {}


def parse_float(value: Any) -> float | None:
    if value in (None, ""):
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def parse_int(value: Any) -> int | None:
    if value in (None, ""):
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def abs_or_zero(value: float | None) -> float:
    return abs(value) if value is not None else 0.0


def parse_timestamp(value: Any) -> datetime | None:
    text = str(value or "").strip()
    if not text:
        return None
    normalized = text.replace("Z", "+00:00")
    try:
        return datetime.fromisoformat(normalized)
    except ValueError:
        return None


def normalize_axis_snapshot(axis_payload: dict[str, Any]) -> dict[str, Any]:
    return {
        "position": parse_float(axis_payload.get("position")),
        "velocity": parse_float(axis_payload.get("velocity")),
        "state": parse_int(axis_payload.get("state")),
        "enabled": bool(axis_payload.get("enabled", False)),
        "homed": bool(axis_payload.get("homed", False)),
        "in_position": bool(axis_payload.get("in_position", False)),
        "has_error": bool(axis_payload.get("has_error", axis_payload.get("alarm", False))),
        "error_code": parse_int(axis_payload.get("error_code")),
        "servo_alarm": bool(axis_payload.get("servo_alarm", axis_payload.get("alarm", False))),
        "following_error": bool(axis_payload.get("following_error", False)),
        "home_failed": bool(axis_payload.get("home_failed", False)),
        "hard_limit_positive": bool(axis_payload.get("hard_limit_positive", False)),
        "hard_limit_negative": bool(axis_payload.get("hard_limit_negative", False)),
        "soft_limit_positive": bool(axis_payload.get("soft_limit_positive", False)),
        "soft_limit_negative": bool(axis_payload.get("soft_limit_negative", False)),
        "home_signal": bool(axis_payload.get("home_signal", False)),
        "index_signal": bool(axis_payload.get("index_signal", False)),
        "selected_feedback_source": str(axis_payload.get("selected_feedback_source", "")).strip(),
        "prf_position_mm": parse_float(axis_payload.get("prf_position_mm")),
        "enc_position_mm": parse_float(axis_payload.get("enc_position_mm")),
        "prf_velocity_mm_s": parse_float(axis_payload.get("prf_velocity_mm_s")),
        "enc_velocity_mm_s": parse_float(axis_payload.get("enc_velocity_mm_s")),
        "prf_position_ret": parse_int(axis_payload.get("prf_position_ret")),
        "enc_position_ret": parse_int(axis_payload.get("enc_position_ret")),
        "prf_velocity_ret": parse_int(axis_payload.get("prf_velocity_ret")),
        "enc_velocity_ret": parse_int(axis_payload.get("enc_velocity_ret")),
    }


def normalize_machine_status_sample(
    status_payload: dict[str, Any],
    *,
    sampled_at: str,
    poll_index: int,
) -> dict[str, Any]:
    result = status_result(status_payload)
    io_payload = result.get("io", {})
    interlocks = result.get("effective_interlocks", {})
    axes_payload = result.get("axes", {})
    if not isinstance(io_payload, dict):
        io_payload = {}
    if not isinstance(interlocks, dict):
        interlocks = {}
    if not isinstance(axes_payload, dict):
        axes_payload = {}

    axes: dict[str, Any] = {}
    for axis_name in ("X", "Y"):
        axis_payload = axes_payload.get(axis_name)
        if isinstance(axis_payload, dict):
            axes[axis_name] = normalize_axis_snapshot(axis_payload)

    return {
        "sampled_at": sampled_at,
        "poll_index": poll_index,
        "machine_state": str(result.get("machine_state", "")).strip(),
        "machine_state_reason": str(result.get("machine_state_reason", "")).strip(),
        "connected": bool(result.get("connected", False)),
        "io": {
            "estop": bool(io_payload.get("estop", False)),
            "door_open": bool(io_payload.get("door", False)),
            "door_known": bool(io_payload.get("door_known", False)),
            "limit_x_pos": bool(io_payload.get("limit_x_pos", False)),
            "limit_x_neg": bool(io_payload.get("limit_x_neg", False)),
            "limit_y_pos": bool(io_payload.get("limit_y_pos", False)),
            "limit_y_neg": bool(io_payload.get("limit_y_neg", False)),
            "home_boundary_x_active": bool(interlocks.get("home_boundary_x_active", False)),
            "home_boundary_y_active": bool(interlocks.get("home_boundary_y_active", False)),
        },
        "axes": axes,
    }


def normalize_job_status_sample(
    job_payload: dict[str, Any],
    *,
    sampled_at: str,
    poll_index: int,
) -> dict[str, Any]:
    result = dict(status_result(job_payload))
    result["sampled_at"] = sampled_at
    result["poll_index"] = poll_index
    return result


def normalize_coord_status_sample(
    coord_payload: dict[str, Any],
    *,
    sampled_at: str,
    poll_index: int,
    coord_sys: int,
) -> dict[str, Any]:
    result = status_result(coord_payload)
    position_payload = result.get("position", {})
    axes_payload = result.get("axes", {})
    if not isinstance(position_payload, dict):
        position_payload = {}
    if not isinstance(axes_payload, dict):
        axes_payload = {}

    axes: dict[str, Any] = {}
    for axis_name in ("X", "Y"):
        axis_payload = axes_payload.get(axis_name)
        if isinstance(axis_payload, dict):
            axes[axis_name] = normalize_axis_snapshot(axis_payload)

    return {
        "sampled_at": sampled_at,
        "poll_index": poll_index,
        "coord_sys": coord_sys,
        "state": parse_int(result.get("state")),
        "is_moving": bool(result.get("is_moving", False)),
        "remaining_segments": parse_int(result.get("remaining_segments")),
        "current_velocity": parse_float(result.get("current_velocity")),
        "raw_status_word": parse_int(result.get("raw_status_word")),
        "raw_segment": parse_int(result.get("raw_segment")),
        "mc_status_ret": parse_int(result.get("mc_status_ret")),
        "buffer_space": parse_int(result.get("buffer_space")),
        "lookahead_space": parse_int(result.get("lookahead_space")),
        "position": {
            "x": parse_float(position_payload.get("x")),
            "y": parse_float(position_payload.get("y")),
        },
        "axes": axes,
    }


def collect_poll_observation(
    client: TcpJsonClient,
    *,
    job_id: str,
    poll_index: int,
    coord_sys: int = DEFAULT_COORD_SYS,
) -> dict[str, dict[str, Any]]:
    sampled_at = utc_now()

    machine_response = client.send_request("status", None, timeout_seconds=5.0)
    if "error" in machine_response:
        raise RuntimeError("status failed during dry-run polling: " + truncate_json(machine_response))

    job_response = client.send_request(
        "dxf.job.status",
        {"job_id": job_id},
        timeout_seconds=5.0,
    )
    if "error" in job_response:
        raise RuntimeError("dxf.job.status failed: " + truncate_json(job_response))

    coord_response = client.send_request(
        "motion.coord.status",
        {"coord_sys": coord_sys},
        timeout_seconds=5.0,
    )
    if "error" in coord_response:
        raise RuntimeError("motion.coord.status failed: " + truncate_json(coord_response))

    return {
        "machine_status": normalize_machine_status_sample(
            machine_response,
            sampled_at=sampled_at,
            poll_index=poll_index,
        ),
        "job_status": normalize_job_status_sample(
            job_response,
            sampled_at=sampled_at,
            poll_index=poll_index,
        ),
        "coord_status": normalize_coord_status_sample(
            coord_response,
            sampled_at=sampled_at,
            poll_index=poll_index,
            coord_sys=coord_sys,
        ),
    }


def resolve_job_timeout_budget(
    *,
    configured_timeout_seconds: float,
    estimated_time_seconds: Any,
    timeout_scale: float,
    timeout_buffer_seconds: float,
) -> dict[str, Any]:
    estimated_seconds = parse_float(estimated_time_seconds)
    configured_seconds = max(0.0, float(configured_timeout_seconds))
    scale = max(1.0, float(timeout_scale))
    buffer_seconds = max(0.0, float(timeout_buffer_seconds))

    if estimated_seconds is None or estimated_seconds <= 0.0:
        effective_seconds = configured_seconds
    else:
        effective_seconds = max(configured_seconds, estimated_seconds * scale + buffer_seconds)

    return {
        "configured_timeout_seconds": configured_seconds,
        "estimated_time_seconds": estimated_seconds,
        "timeout_scale": scale,
        "timeout_buffer_seconds": buffer_seconds,
        "effective_timeout_seconds": effective_seconds,
    }


def coord_status_indicates_running(
    coord_status: dict[str, Any],
    *,
    velocity_epsilon_mm_s: float,
) -> bool:
    if bool(coord_status.get("is_moving", False)):
        return True
    if abs_or_zero(parse_float(coord_status.get("current_velocity"))) > velocity_epsilon_mm_s:
        return True
    remaining_segments = parse_int(coord_status.get("remaining_segments"))
    raw_segment = parse_int(coord_status.get("raw_segment"))
    raw_status_word = parse_int(coord_status.get("raw_status_word")) or 0
    return bool(
        (remaining_segments or 0) > 0
        or (raw_segment or 0) > 0
        or (raw_status_word & CRDSYS_STATUS_ACTIVE_MOTION_MASK) != 0
    )


def axis_window_shows_motion(
    coord_window: list[dict[str, Any]],
    *,
    position_epsilon_mm: float,
    velocity_epsilon_mm_s: float,
) -> bool:
    if not coord_window:
        return False

    for coord_status in coord_window:
        if abs_or_zero(parse_float(coord_status.get("current_velocity"))) > velocity_epsilon_mm_s:
            return True
        axes_payload = coord_status.get("axes", {})
        if not isinstance(axes_payload, dict):
            continue
        for axis_name in ("X", "Y"):
            axis_payload = axes_payload.get(axis_name)
            if not isinstance(axis_payload, dict):
                continue
            if abs_or_zero(parse_float(axis_payload.get("velocity"))) > velocity_epsilon_mm_s:
                return True

    first_sample = coord_window[0]
    last_sample = coord_window[-1]
    coord_delta_candidates = []
    for axis_key in ("x", "y"):
        first_position = parse_float(first_sample.get("position", {}).get(axis_key))
        last_position = parse_float(last_sample.get("position", {}).get(axis_key))
        if first_position is not None and last_position is not None:
            coord_delta_candidates.append(abs(last_position - first_position))
    if coord_delta_candidates and any(delta > position_epsilon_mm for delta in coord_delta_candidates):
        return True

    axis_delta_candidates = []
    for axis_name in ("X", "Y"):
        first_axis = first_sample.get("axes", {}).get(axis_name)
        last_axis = last_sample.get("axes", {}).get(axis_name)
        if not isinstance(first_axis, dict) or not isinstance(last_axis, dict):
            continue
        first_position = parse_float(first_axis.get("position"))
        last_position = parse_float(last_axis.get("position"))
        if first_position is not None and last_position is not None:
            axis_delta_candidates.append(abs(last_position - first_position))
    if axis_delta_candidates and any(delta > position_epsilon_mm for delta in axis_delta_candidates):
        return True

    return False


def update_runtime_phases(
    phase_timeline: list[dict[str, Any]],
    *,
    coord_status: dict[str, Any],
    coord_running_recorded: bool,
    axis_motion_recorded: bool,
    coord_window: list[dict[str, Any]],
    position_epsilon_mm: float,
    velocity_epsilon_mm_s: float,
) -> tuple[bool, bool]:
    if not coord_running_recorded and coord_status_indicates_running(
        coord_status,
        velocity_epsilon_mm_s=velocity_epsilon_mm_s,
    ):
        append_phase_timeline(
            phase_timeline,
            phase="coord_running",
            source="motion.coord.status",
            detail={
                "poll_index": coord_status.get("poll_index"),
                "sampled_at": coord_status.get("sampled_at"),
                "remaining_segments": coord_status.get("remaining_segments"),
                "raw_status_word": coord_status.get("raw_status_word"),
            },
        )
        coord_running_recorded = True

    if (
        not axis_motion_recorded
        and axis_window_shows_motion(
            coord_window,
            position_epsilon_mm=position_epsilon_mm,
            velocity_epsilon_mm_s=velocity_epsilon_mm_s,
        )
    ):
        append_phase_timeline(
            phase_timeline,
            phase="axis_motion",
            source="motion.coord.status.axes",
            detail={
                "poll_index": coord_status.get("poll_index"),
                "sampled_at": coord_status.get("sampled_at"),
            },
        )
        axis_motion_recorded = True

    return coord_running_recorded, axis_motion_recorded


def job_status_indicates_progress(
    job_status: dict[str, Any],
    *,
    progress_threshold_percent: float,
) -> bool:
    if str(job_status.get("state", "")).strip() != "running":
        return False

    cycle_progress = parse_float(job_status.get("cycle_progress_percent")) or 0.0
    overall_progress = parse_float(job_status.get("overall_progress_percent")) or 0.0
    current_segment = parse_int(job_status.get("current_segment")) or 0
    total_segments = parse_int(job_status.get("total_segments")) or 0

    if cycle_progress >= progress_threshold_percent or overall_progress >= progress_threshold_percent:
        return True
    if total_segments > 0 and current_segment >= max(1, total_segments - 1):
        return True
    return False


def coord_status_is_idle_empty(
    coord_status: dict[str, Any],
    *,
    velocity_epsilon_mm_s: float,
) -> bool:
    remaining_segments = parse_int(coord_status.get("remaining_segments")) or 0
    raw_segment = parse_int(coord_status.get("raw_segment")) or 0
    raw_status_word = parse_int(coord_status.get("raw_status_word")) or 0
    current_velocity = parse_float(coord_status.get("current_velocity")) or 0.0
    return (
        not bool(coord_status.get("is_moving", False))
        and remaining_segments == 0
        and raw_segment <= 0
        and (raw_status_word & ~CRDSYS_STATUS_IDLE_ALLOWED_MASK) == 0
        and abs(current_velocity) <= velocity_epsilon_mm_s
    )


def detect_state_contradiction(
    *,
    job_status_history: list[dict[str, Any]],
    coord_status_history: list[dict[str, Any]],
    progress_threshold_percent: float,
    consecutive_samples: int,
    grace_seconds: float,
    position_epsilon_mm: float,
    velocity_epsilon_mm_s: float,
) -> dict[str, Any] | None:
    sample_count = min(len(job_status_history), len(coord_status_history))
    if sample_count < consecutive_samples:
        return None

    job_window = job_status_history[-consecutive_samples:]
    coord_window = coord_status_history[-consecutive_samples:]

    if not all(
        job_status_indicates_progress(job_status, progress_threshold_percent=progress_threshold_percent)
        and coord_status_is_idle_empty(coord_status, velocity_epsilon_mm_s=velocity_epsilon_mm_s)
        for job_status, coord_status in zip(job_window, coord_window)
    ):
        return None

    first_sampled_at = parse_timestamp(job_window[0].get("sampled_at"))
    last_sampled_at = parse_timestamp(job_window[-1].get("sampled_at"))
    if first_sampled_at is None or last_sampled_at is None:
        return None
    if (last_sampled_at - first_sampled_at).total_seconds() < grace_seconds:
        return None

    if axis_window_shows_motion(
        coord_window,
        position_epsilon_mm=position_epsilon_mm,
        velocity_epsilon_mm_s=velocity_epsilon_mm_s,
    ):
        return None

    return {
        "kind": "state_contradiction",
        "reason": "job progress advanced while coord status and axis feedback stayed idle",
        "window_sample_count": consecutive_samples,
        "window_elapsed_seconds": (last_sampled_at - first_sampled_at).total_seconds(),
        "first_poll_index": job_window[0].get("poll_index"),
        "last_poll_index": job_window[-1].get("poll_index"),
        "first_job_status": job_window[0],
        "last_job_status": job_window[-1],
        "first_coord_status": coord_window[0],
        "last_coord_status": coord_window[-1],
    }


def build_evidence_contract(
    *,
    coord_sys: int = DEFAULT_COORD_SYS,
    progress_threshold_percent: float = DEFAULT_CONTRADICTION_PROGRESS_THRESHOLD_PERCENT,
    consecutive_samples: int = DEFAULT_CONTRADICTION_CONSECUTIVE_SAMPLES,
    grace_seconds: float = DEFAULT_CONTRADICTION_GRACE_SECONDS,
    position_epsilon_mm: float = DEFAULT_POSITION_EPSILON_MM,
    velocity_epsilon_mm_s: float = DEFAULT_VELOCITY_EPSILON_MM_S,
) -> dict[str, Any]:
    return {
        "version": 1,
        "authority": {
            "machine_status": "safety_gate_only",
            "job_status": "display_progress_only",
            "physical_execution": "motion.coord.status + axis feedback",
        },
        "coord_sys": coord_sys,
        "phase_enum": list(SUPPORTED_DRYRUN_PHASES),
        "phase_detection_enabled": list(SUPPORTED_DRYRUN_PHASES),
        "verdict_enum": list(SUPPORTED_DRYRUN_VERDICTS),
        "verdict_detection_enabled": list(SUPPORTED_DRYRUN_VERDICTS),
        "state_contradiction_thresholds": {
            "progress_threshold_percent": progress_threshold_percent,
            "consecutive_samples": consecutive_samples,
            "grace_seconds": grace_seconds,
            "position_epsilon_mm": position_epsilon_mm,
            "velocity_epsilon_mm_s": velocity_epsilon_mm_s,
        },
        "state_contradiction_signal_fields": list(STATE_CONTRADICTION_SIGNAL_FIELDS),
        "compatibility_fields_retained": ["job_status_history"],
    }


def append_phase_timeline(
    phase_timeline: list[dict[str, Any]],
    *,
    phase: str,
    source: str,
    detail: dict[str, Any] | None = None,
) -> None:
    if phase_timeline and phase_timeline[-1]["phase"] == phase:
        return
    phase_timeline.append(
        {
            "phase": phase,
            "source": source,
            "detail": detail or {},
            "timestamp": utc_now(),
        }
    )


def build_report_verdict(
    *,
    verdict_kind: str,
    final_job_status: dict[str, Any] | None,
    error_message: str,
) -> dict[str, Any]:
    final_state = ""
    final_job_error = ""
    if final_job_status:
        final_state = str(final_job_status.get("state", "")).strip()
        final_job_error = str(final_job_status.get("error_message", "")).strip()

    if verdict_kind not in SUPPORTED_DRYRUN_VERDICTS:
        verdict_kind = "canonical_step_failed"

    reason = error_message.strip() or final_job_error
    if verdict_kind == "completed" and not reason:
        reason = "job reached completed terminal state"
    elif verdict_kind == "motion_timeout_unclassified" and not reason:
        reason = "motion completion did not converge before timeout"
    elif verdict_kind == "state_contradiction" and not reason:
        reason = "job progress advanced while coord status and axis feedback stayed idle"
    elif verdict_kind == "canonical_step_failed" and not reason:
        reason = "canonical dry-run step failed before a successful completed terminal state"

    return {
        "kind": verdict_kind,
        "reason": reason,
        "terminal_job_state": final_state,
        "job_error_message": final_job_error,
    }


def build_report(
    *,
    args: argparse.Namespace,
    steps: list[Step],
    artifacts: dict[str, Any],
    overall_status: str,
    error_message: str,
    job_status_history: list[dict[str, Any]],
    machine_status_history: list[dict[str, Any]],
    coord_status_history: list[dict[str, Any]],
    phase_timeline: list[dict[str, Any]],
    verdict: dict[str, Any],
    first_contradiction_sample: dict[str, Any] | None,
) -> dict[str, Any]:
    gateway_host = artifacts.get("gateway_host", getattr(args, "host", ""))
    gateway_port = artifacts.get("gateway_port", getattr(args, "port", 0))
    return {
        "generated_at": utc_now(),
        "workspace_root": str(ROOT),
        "gateway_exe": str(args.gateway_exe),
        "gateway_host": str(gateway_host),
        "gateway_port": int(parse_int(gateway_port) or 0),
        "config_path": str(args.config_path),
        "dxf_file": str(args.dxf_file),
        "overall_status": overall_status,
        "evidence_contract": build_evidence_contract(
            progress_threshold_percent=args.contradiction_progress_threshold_percent,
            consecutive_samples=args.contradiction_consecutive_samples,
            grace_seconds=args.contradiction_grace_seconds,
            position_epsilon_mm=args.position_epsilon_mm,
            velocity_epsilon_mm_s=args.velocity_epsilon_mm_s,
        ),
        "verdict": verdict,
        "phase_timeline": phase_timeline,
        "first_contradiction_sample": first_contradiction_sample,
        "observation_summary": {
            "job_status_samples": len(job_status_history),
            "machine_status_samples": len(machine_status_history),
            "coord_status_samples": len(coord_status_history),
            "effective_job_timeout_seconds": artifacts.get("job_timeout_budget", {}).get("effective_timeout_seconds"),
        },
        "steps": [
            {
                "name": step.name,
                "status": step.status,
                "note": step.note,
                "payload": step.payload,
                "timestamp": step.timestamp,
            }
            for step in steps
        ],
        "artifacts": artifacts,
        "safety": {
            "before": summarize_safety(artifacts.get("status_before", {})),
            "non_homed_before": non_homed_axes(artifacts.get("status_before", {})),
            "after_home": summarize_safety(artifacts.get("status_after_home", {})),
            "non_homed_after_home": non_homed_axes(artifacts.get("status_after_home", {})),
            "after_home_boundary_escape": summarize_safety(artifacts.get("status_after_escape", {})),
            "after_preflight_normalization": summarize_safety(artifacts.get("status_after_preflight_normalization", {})),
            "after": summarize_safety(artifacts.get("status_after", {})),
        },
        "job_status_history": job_status_history,
        "machine_status_history": machine_status_history,
        "coord_status_history": coord_status_history,
        "error": error_message,
    }


def summarize_safety(status_payload: dict[str, Any]) -> dict[str, Any]:
    result = status_result(status_payload)
    io_payload = result.get("io", {})
    interlocks = result.get("effective_interlocks", {})
    if not isinstance(io_payload, dict):
        io_payload = {}
    if not isinstance(interlocks, dict):
        interlocks = {}
    return {
        "estop": bool(io_payload.get("estop", False)),
        "door_open": bool(io_payload.get("door", False)),
        "limit_x_pos": bool(io_payload.get("limit_x_pos", False)),
        "limit_x_neg": bool(io_payload.get("limit_x_neg", False)),
        "limit_y_pos": bool(io_payload.get("limit_y_pos", False)),
        "limit_y_neg": bool(io_payload.get("limit_y_neg", False)),
        "home_boundary_x_active": bool(interlocks.get("home_boundary_x_active", False)),
        "home_boundary_y_active": bool(interlocks.get("home_boundary_y_active", False)),
    }


def wait_for_mock_io_state(
    client: TcpJsonClient,
    *,
    requested_io: dict[str, bool],
    timeout_seconds: float,
) -> dict[str, Any]:
    deadline = time.time() + timeout_seconds
    last_status: dict[str, Any] = {}

    while time.time() < deadline:
        last_status = client.send_request("status", None, timeout_seconds=5.0)
        safety = summarize_safety(last_status)
        if all(
            bool(safety.get(MOCK_IO_TO_SAFETY_FIELD[key], False)) == expected
            for key, expected in requested_io.items()
        ):
            return last_status
        time.sleep(0.1)

    observed = summarize_safety(last_status)
    raise TimeoutError(
        "mock.io.set did not settle before dry-run preflight: "
        f"requested={requested_io} observed={observed}"
    )


def non_homed_axes(status_payload: dict[str, Any]) -> list[str]:
    result = status_result(status_payload)
    axes = result.get("axes", {})
    if not isinstance(axes, dict):
        return []
    pending: list[str] = []
    for axis_name, axis_payload in axes.items():
        if not isinstance(axis_payload, dict):
            continue
        if not bool(axis_payload.get("homed", False)):
            pending.append(str(axis_name))
    return pending


def active_home_boundary_axes(status_payload: dict[str, Any]) -> list[str]:
    safety = summarize_safety(status_payload)
    active_axes: list[str] = []
    if safety.get("home_boundary_x_active", False):
        active_axes.append("X")
    if safety.get("home_boundary_y_active", False):
        active_axes.append("Y")
    return active_axes


def extract_machine_position(status_payload: dict[str, Any]) -> dict[str, float]:
    result = status_result(status_payload)
    position_payload = result.get("position", {})
    axes_payload = result.get("axes", {})
    if not isinstance(position_payload, dict):
        position_payload = {}
    if not isinstance(axes_payload, dict):
        axes_payload = {}

    x = parse_float(position_payload.get("x"))
    y = parse_float(position_payload.get("y"))
    if x is None:
        axis_payload = axes_payload.get("X")
        if isinstance(axis_payload, dict):
            x = parse_float(axis_payload.get("position"))
    if y is None:
        axis_payload = axes_payload.get("Y")
        if isinstance(axis_payload, dict):
            y = parse_float(axis_payload.get("position"))

    return {
        "x": float(x if x is not None else 0.0),
        "y": float(y if y is not None else 0.0),
    }


def require_safe_for_motion(
    status_payload: dict[str, Any],
    *,
    ignore_home_boundaries: bool = False,
) -> None:
    safety = summarize_safety(status_payload)
    ignored = {"home_boundary_x_active", "home_boundary_y_active"} if ignore_home_boundaries else set()
    blocking = [
        key
        for key in (
            "estop",
            "door_open",
            "limit_x_pos",
            "limit_x_neg",
            "limit_y_pos",
            "limit_y_neg",
            "home_boundary_x_active",
            "home_boundary_y_active",
        )
        if safety.get(key) and key not in ignored
    ]
    if blocking:
        raise RuntimeError("unsafe machine state before dry-run: " + ",".join(blocking))


def read_gateway_logs() -> dict[str, str]:
    payload: dict[str, str] = {}
    for log_name in ("tcp_server.log", "control_runtime.log"):
        log_path = ROOT / "logs" / log_name
        if not log_path.exists():
            continue
        try:
            payload[log_name] = log_path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            payload[log_name] = f"<read failed: {exc}>"
    return payload


def build_report_markdown(report: dict[str, Any]) -> str:
    lines: list[str] = []
    lines.append("# Real DXF Machine Dry-Run Canonical Report")
    lines.append("")
    lines.append(f"- generated_at: `{report['generated_at']}`")
    lines.append(f"- overall_status: `{report['overall_status']}`")
    lines.append(f"- verdict: `{report.get('verdict', {}).get('kind', 'canonical_step_failed')}`")
    lines.append(f"- gateway_exe: `{report['gateway_exe']}`")
    lines.append(f"- gateway_host: `{report.get('gateway_host', '')}`")
    lines.append(f"- gateway_port: `{report.get('gateway_port', '')}`")
    lines.append(f"- config_path: `{report['config_path']}`")
    lines.append(f"- dxf_file: `{report['dxf_file']}`")
    job_timeout_budget = report.get("artifacts", {}).get("job_timeout_budget", {})
    if job_timeout_budget:
        lines.append(f"- effective_job_timeout_seconds: `{job_timeout_budget.get('effective_timeout_seconds')}`")
    lines.append("")
    lines.append("## Steps")
    lines.append("")
    lines.append("| step | status | note |")
    lines.append("|---|---|---|")
    for step in report["steps"]:
        note = str(step["note"]).replace("\r", " ").replace("\n", " ")
        lines.append(f"| `{step['name']}` | `{step['status']}` | {note} |")
    lines.append("")
    lines.append("## Safety")
    lines.append("")
    lines.append("```json")
    lines.append(json.dumps(report["safety"], ensure_ascii=False, indent=2))
    lines.append("```")
    if report.get("phase_timeline"):
        lines.append("")
        lines.append("## Phase Timeline")
        lines.append("")
        lines.append("| phase | source | timestamp |")
        lines.append("|---|---|---|")
        for item in report["phase_timeline"]:
            lines.append(f"| `{item['phase']}` | `{item['source']}` | `{item['timestamp']}` |")
    if report.get("observation_summary"):
        lines.append("")
        lines.append("## Observation Summary")
        lines.append("")
        lines.append("```json")
        lines.append(json.dumps(report["observation_summary"], ensure_ascii=False, indent=2))
        lines.append("```")
    if report.get("first_contradiction_sample"):
        lines.append("")
        lines.append("## First Contradiction Sample")
        lines.append("")
        lines.append("```json")
        lines.append(json.dumps(report["first_contradiction_sample"], ensure_ascii=False, indent=2))
        lines.append("```")
    if report.get("error"):
        lines.append("")
        lines.append("## Error")
        lines.append("")
        lines.append("```text")
        lines.append(str(report["error"]))
        lines.append("```")
    return "\n".join(lines) + "\n"


def main() -> int:
    args = parse_args()
    ensure_exists(args.gateway_exe, "gateway executable")
    ensure_exists(args.config_path, "config")
    ensure_exists(args.dxf_file, "dxf file")
    requested_mock_io = parse_mock_io_json(args.mock_io_json)
    effective_port = resolve_listen_port(args.host, args.port)

    report_dir = args.report_root / time.strftime("%Y%m%d-%H%M%S")
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "real-dxf-machine-dryrun-canonical.json"
    md_path = report_dir / "real-dxf-machine-dryrun-canonical.md"

    steps: list[Step] = []
    artifacts: dict[str, Any] = {}
    job_status_history: list[dict[str, Any]] = []
    machine_status_history: list[dict[str, Any]] = []
    coord_status_history: list[dict[str, Any]] = []
    phase_timeline: list[dict[str, Any]] = []
    error_message = ""
    overall_status = "failed"
    verdict_kind = "canonical_step_failed"
    process: subprocess.Popen[str] | None = None
    client = TcpJsonClient(args.host, effective_port)
    connected = False
    final_job_status: dict[str, Any] | None = None
    final_machine_status: dict[str, Any] | None = None
    final_coord_status: dict[str, Any] | None = None
    first_contradiction_sample: dict[str, Any] | None = None
    coord_running_recorded = False
    axis_motion_recorded = False
    artifacts["gateway_host"] = args.host
    artifacts["gateway_port"] = effective_port
    artifacts["home_boundary_auto_normalization_enabled"] = not bool(requested_mock_io)

    try:
        process = subprocess.Popen(
            [str(args.gateway_exe), "--config", str(args.config_path), "--port", str(effective_port)],
            cwd=str(ROOT),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            env=build_process_env(args.gateway_exe),
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        add_step(steps, "gateway-launch", "passed", f"pid={process.pid}")

        ready_status, ready_note = wait_gateway_ready(process, args.host, effective_port, args.gateway_ready_timeout)
        add_step(steps, "gateway-ready", ready_status, ready_note)
        if ready_status != "passed":
            overall_status = ready_status
            error_message = ready_note
            return KNOWN_FAILURE_EXIT_CODE if ready_status == "known_failure" else 1

        client.connect(timeout_seconds=args.connect_timeout)
        connected = True
        add_step(steps, "tcp-session-open", "passed", f"connected to {args.host}:{effective_port}")

        connect_params = load_connection_params(args.config_path)
        connect_response = client.send_request("connect", connect_params, timeout_seconds=args.connect_timeout)
        artifacts["connect_response"] = connect_response
        if "error" in connect_response:
            raise RuntimeError("connect failed: " + truncate_json(connect_response))
        add_step(
            steps,
            "tcp-connect",
            "passed",
            json.dumps({"params": connect_params, "result": status_result(connect_response)}, ensure_ascii=False),
        )

        if requested_mock_io:
            mock_io_response = client.send_request("mock.io.set", requested_mock_io, timeout_seconds=10.0)
            artifacts["mock_io_set"] = mock_io_response
            if "error" in mock_io_response:
                raise RuntimeError("mock.io.set failed: " + truncate_json(mock_io_response))
            status_before = wait_for_mock_io_state(
                client,
                requested_io=requested_mock_io,
                timeout_seconds=args.mock_io_settle_timeout_seconds,
            )
            artifacts["status_after_mock_io"] = status_before
            add_step(
                steps,
                "mock-io-set",
                "passed",
                json.dumps(
                    {
                        "requested": requested_mock_io,
                        "observed_safety": summarize_safety(status_before),
                    },
                    ensure_ascii=True,
                ),
            )
        else:
            status_before = client.send_request("status", None, timeout_seconds=5.0)
        artifacts["status_before"] = status_before
        safety_before = summarize_safety(status_before)
        add_step(
            steps,
            "status-before",
            "passed",
            json.dumps(
                {
                    "machine_state": status_result(status_before).get("machine_state", ""),
                    "machine_state_reason": status_result(status_before).get("machine_state_reason", ""),
                    "safety": safety_before,
                },
                ensure_ascii=True,
            ),
        )
        non_homed_before = non_homed_axes(status_before)
        home_boundary_before = active_home_boundary_axes(status_before)

        if requested_mock_io:
            require_safe_for_motion(status_before)
        else:
            require_safe_for_motion(status_before, ignore_home_boundaries=True)

        should_home = bool(non_homed_before or home_boundary_before)
        add_step(
            steps,
            "preflight-home-normalization-decision",
            "passed",
            json.dumps(
                {
                    "requested_mock_io": requested_mock_io,
                    "non_homed_axes": non_homed_before,
                    "home_boundary_axes": home_boundary_before,
                    "should_home": should_home,
                },
                ensure_ascii=True,
            ),
        )

        if should_home:
            home_response = client.send_request("home", None, timeout_seconds=args.home_timeout)
            artifacts["home_response"] = home_response
            if "error" in home_response:
                raise RuntimeError("home failed: " + truncate_json(home_response))
            add_step(steps, "home", "passed", truncate_json(home_response))

            status_after_home = client.send_request("status", None, timeout_seconds=5.0)
            artifacts["status_after_home"] = status_after_home
            non_homed_after_home = non_homed_axes(status_after_home)
            add_step(
                steps,
                "status-after-home",
                "passed",
                json.dumps(
                    {
                        "non_homed_axes": non_homed_after_home,
                        "safety": summarize_safety(status_after_home),
                    },
                    ensure_ascii=True,
                ),
            )
            if non_homed_after_home:
                raise RuntimeError("axes still not homed after home: " + ",".join(non_homed_after_home))
        else:
            status_after_home = status_before
            artifacts["status_after_home"] = status_after_home
            non_homed_after_home = []
            add_step(steps, "home", "passed", "already homed; no homing command sent")
            add_step(
                steps,
                "status-after-home",
                "passed",
                json.dumps(
                    {
                        "non_homed_axes": non_homed_after_home,
                        "safety": summarize_safety(status_after_home),
                    },
                    ensure_ascii=True,
                ),
            )

        status_for_motion = status_after_home
        home_boundary_after_home = active_home_boundary_axes(status_after_home)
        if home_boundary_after_home and not requested_mock_io:
            artifacts["post_home_boundary_latched"] = {
                "active_axes": home_boundary_after_home,
                "position_after_home": extract_machine_position(status_after_home),
                "accepted_for_real_preflight": True,
                "reason": "successful home on real hardware may still leave HOME signal active at machine origin",
            }
            add_step(
                steps,
                "home-boundary-post-home-observed",
                "passed",
                json.dumps(artifacts["post_home_boundary_latched"], ensure_ascii=True),
            )
        else:
            artifacts["post_home_boundary_latched"] = {
                "active_axes": home_boundary_after_home,
                "position_after_home": extract_machine_position(status_after_home),
                "accepted_for_real_preflight": False,
            }

        artifacts["status_after_escape"] = status_after_home
        add_step(
            steps,
            "status-after-home-boundary-observation",
            "passed",
            json.dumps(
                {
                    "safety": summarize_safety(status_after_home),
                    "position": extract_machine_position(status_after_home),
                    "post_home_boundary_axes": home_boundary_after_home,
                    "accepted_for_real_preflight": bool(home_boundary_after_home and not requested_mock_io),
                },
                ensure_ascii=True,
            ),
        )

        require_safe_for_motion(status_for_motion, ignore_home_boundaries=not requested_mock_io)
        artifacts["status_after_preflight_normalization"] = status_for_motion
        add_step(
            steps,
            "preflight-safe-for-motion",
            "passed",
            json.dumps(
                {
                    "safety": summarize_safety(status_for_motion),
                    "non_homed_axes": non_homed_axes(status_for_motion),
                },
                ensure_ascii=True,
            ),
        )

        dxf_bytes = args.dxf_file.read_bytes()
        artifact_response = client.send_request(
            "dxf.artifact.create",
            {
                "filename": args.dxf_file.name,
                "original_filename": args.dxf_file.name,
                "content_type": "application/dxf",
                "file_content_b64": base64.b64encode(dxf_bytes).decode("ascii"),
            },
            timeout_seconds=30.0,
        )
        artifacts["dxf_artifact_create"] = artifact_response
        if "error" in artifact_response:
            raise RuntimeError("dxf.artifact.create failed: " + truncate_json(artifact_response))
        artifact_id = str(status_result(artifact_response).get("artifact_id", "")).strip()
        if not artifact_id:
            raise RuntimeError("artifact.create missing artifact_id")
        add_step(steps, "dxf-artifact-create", "passed", f"artifact_id={artifact_id}")

        prepare_params = {
            "artifact_id": artifact_id,
            "dispensing_speed_mm_s": args.dispensing_speed_mm_s,
            "dry_run": True,
            "dry_run_speed_mm_s": args.dry_run_speed_mm_s,
            "rapid_speed_mm_s": args.rapid_speed_mm_s,
            "use_hardware_trigger": False,
            "velocity_trace_enabled": True,
            "velocity_trace_interval_ms": args.velocity_trace_interval_ms,
        }
        plan_response = client.send_request("dxf.plan.prepare", prepare_params, timeout_seconds=30.0)
        artifacts["dxf_plan_prepare"] = plan_response
        if "error" in plan_response:
            raise RuntimeError("dxf.plan.prepare failed: " + truncate_json(plan_response))
        plan_result = status_result(plan_response)
        plan_id = str(plan_result.get("plan_id", "")).strip()
        plan_fingerprint = str(plan_result.get("plan_fingerprint", "")).strip()
        if not plan_id or not plan_fingerprint:
            raise RuntimeError("plan.prepare missing plan_id/plan_fingerprint")
        append_phase_timeline(
            phase_timeline,
            phase="prepare",
            source="dxf.plan.prepare",
            detail={
                "plan_id": plan_id,
                "plan_fingerprint": plan_fingerprint,
                "segment_count": plan_result.get("segment_count", 0),
            },
        )
        add_step(
            steps,
            "dxf-plan-prepare",
            "passed",
            json.dumps(
                {
                    "plan_id": plan_id,
                    "plan_fingerprint": plan_fingerprint,
                    "segment_count": plan_result.get("segment_count", 0),
                    "estimated_time_s": plan_result.get("estimated_time_s", 0),
                },
                ensure_ascii=True,
            ),
        )
        job_timeout_budget = resolve_job_timeout_budget(
            configured_timeout_seconds=args.job_timeout,
            estimated_time_seconds=plan_result.get("estimated_time_s"),
            timeout_scale=args.job_timeout_scale,
            timeout_buffer_seconds=args.job_timeout_buffer_seconds,
        )
        artifacts["job_timeout_budget"] = job_timeout_budget
        add_step(
            steps,
            "job-timeout-budget",
            "passed",
            json.dumps(job_timeout_budget, ensure_ascii=True),
        )

        snapshot_response = client.send_request(
            "dxf.preview.snapshot",
            {"plan_id": plan_id, "max_polyline_points": 4000},
            timeout_seconds=15.0,
        )
        artifacts["dxf_preview_snapshot"] = snapshot_response
        if "error" in snapshot_response:
            raise RuntimeError("dxf.preview.snapshot failed: " + truncate_json(snapshot_response))
        snapshot_result = status_result(snapshot_response)
        snapshot_hash = str(snapshot_result.get("snapshot_hash", "")).strip()
        if not snapshot_hash:
            raise RuntimeError("preview.snapshot missing snapshot_hash")
        add_step(steps, "dxf-preview-snapshot", "passed", f"snapshot_hash={snapshot_hash}")

        confirm_response = client.send_request(
            "dxf.preview.confirm",
            {"plan_id": plan_id, "snapshot_hash": snapshot_hash},
            timeout_seconds=15.0,
        )
        artifacts["dxf_preview_confirm"] = confirm_response
        if "error" in confirm_response:
            raise RuntimeError("dxf.preview.confirm failed: " + truncate_json(confirm_response))
        append_phase_timeline(
            phase_timeline,
            phase="binding",
            source="dxf.preview.confirm",
            detail={"plan_id": plan_id, "snapshot_hash": snapshot_hash},
        )
        add_step(steps, "dxf-preview-confirm", "passed", truncate_json(confirm_response))

        job_start_response = client.send_request(
            "dxf.job.start",
            {"plan_id": plan_id, "plan_fingerprint": plan_fingerprint, "target_count": 1},
            timeout_seconds=15.0,
        )
        artifacts["dxf_job_start"] = job_start_response
        if "error" in job_start_response:
            raise RuntimeError("dxf.job.start failed: " + truncate_json(job_start_response))
        job_result = status_result(job_start_response)
        job_id = str(job_result.get("job_id", "")).strip()
        if not job_id:
            raise RuntimeError("job.start missing job_id")
        append_phase_timeline(
            phase_timeline,
            phase="start",
            source="dxf.job.start",
            detail={"job_id": job_id, "plan_id": plan_id},
        )
        add_step(steps, "dxf-job-start", "passed", f"job_id={job_id}")

        effective_job_timeout_seconds = float(job_timeout_budget["effective_timeout_seconds"])
        deadline = time.time() + effective_job_timeout_seconds
        poll_index = 0
        while time.time() < deadline:
            observation = collect_poll_observation(
                client,
                job_id=job_id,
                poll_index=poll_index,
            )
            poll_index += 1
            machine_status_history.append(observation["machine_status"])
            job_status_history.append(observation["job_status"])
            coord_status_history.append(observation["coord_status"])
            coord_window = coord_status_history[-args.contradiction_consecutive_samples :]
            coord_running_recorded, axis_motion_recorded = update_runtime_phases(
                phase_timeline,
                coord_status=observation["coord_status"],
                coord_running_recorded=coord_running_recorded,
                axis_motion_recorded=axis_motion_recorded,
                coord_window=coord_window,
                position_epsilon_mm=args.position_epsilon_mm,
                velocity_epsilon_mm_s=args.velocity_epsilon_mm_s,
            )

            first_contradiction_sample = detect_state_contradiction(
                job_status_history=job_status_history,
                coord_status_history=coord_status_history,
                progress_threshold_percent=args.contradiction_progress_threshold_percent,
                consecutive_samples=args.contradiction_consecutive_samples,
                grace_seconds=args.contradiction_grace_seconds,
                position_epsilon_mm=args.position_epsilon_mm,
                velocity_epsilon_mm_s=args.velocity_epsilon_mm_s,
            )
            if first_contradiction_sample is not None:
                verdict_kind = "state_contradiction"
                artifacts["first_contradiction_sample"] = first_contradiction_sample
                add_step(
                    steps,
                    "state-contradiction",
                    "failed",
                    first_contradiction_sample["reason"],
                    payload=first_contradiction_sample,
                )
                raise RuntimeError(first_contradiction_sample["reason"])

            state = str(observation["job_status"].get("state", "")).strip()
            if state in {"completed", "failed", "cancelled"}:
                final_job_status = observation["job_status"]
                final_machine_status = observation["machine_status"]
                final_coord_status = observation["coord_status"]
                break
            time.sleep(0.1)

        if final_job_status is None:
            raise TimeoutError(
                "job did not reach terminal state within "
                f"{effective_job_timeout_seconds:.3f}s "
                f"(configured_floor={job_timeout_budget['configured_timeout_seconds']:.3f}s "
                f"estimated_time_s={job_timeout_budget['estimated_time_seconds']} "
                f"scale={job_timeout_budget['timeout_scale']:.3f} "
                f"buffer_s={job_timeout_budget['timeout_buffer_seconds']:.3f})"
            )

        artifacts["final_job_status"] = final_job_status
        if final_machine_status is not None:
            artifacts["final_machine_status"] = final_machine_status
        if final_coord_status is not None:
            artifacts["final_coord_status"] = final_coord_status
        terminal_state = str(final_job_status.get("state", "")).strip()
        append_phase_timeline(
            phase_timeline,
            phase="terminal",
            source="dxf.job.status",
            detail={"job_id": job_id, "state": terminal_state},
        )
        terminal_status = "passed" if terminal_state == "completed" else "failed"
        add_step(steps, "dxf-job-terminal", terminal_status, truncate_json(final_job_status))
        status_after = client.send_request("status", None, timeout_seconds=5.0)
        artifacts["status_after"] = status_after
        add_step(steps, "status-after", "passed", truncate_json(status_after))
        if terminal_state != "completed":
            if "等待运动完成超时" in str(final_job_status.get("error_message", "")):
                verdict_kind = "motion_timeout_unclassified"
            raise RuntimeError("dry-run job did not complete successfully: " + truncate_json(final_job_status))
        overall_status = "passed"
        verdict_kind = "completed"
        return_code = 0
        return return_code
    except Exception as exc:  # noqa: BLE001
        error_message = str(exc)
        if isinstance(exc, TimeoutError):
            verdict_kind = "motion_timeout_unclassified"
        add_step(steps, "exception", "failed", error_message)
        overall_status = "failed"
        return 1
    finally:
        if connected:
            try:
                disconnect_response = client.send_request("disconnect", None, timeout_seconds=5.0)
                artifacts["disconnect"] = disconnect_response
                add_step(steps, "disconnect", "passed", truncate_json(disconnect_response))
            except Exception as exc:  # noqa: BLE001
                add_step(steps, "disconnect", "failed", str(exc))
            finally:
                client.close()

        if process is not None:
            if process.poll() is None:
                process.terminate()
            stdout, stderr = read_process_output(process)
            artifacts["gateway_stdout"] = stdout
            artifacts["gateway_stderr"] = stderr
            artifacts["gateway_exit_code"] = process.returncode

        log_payload = read_gateway_logs()
        if log_payload:
            artifacts["gateway_logs"] = log_payload

        report = build_report(
            args=args,
            steps=steps,
            artifacts=artifacts,
            overall_status=overall_status,
            error_message=error_message,
            job_status_history=job_status_history,
            machine_status_history=machine_status_history,
            coord_status_history=coord_status_history,
            phase_timeline=phase_timeline,
            verdict=build_report_verdict(
                verdict_kind=verdict_kind,
                final_job_status=final_job_status,
                error_message=error_message,
            ),
            first_contradiction_sample=first_contradiction_sample,
        )
        json_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        md_path.write_text(build_report_markdown(report), encoding="utf-8")
        print(f"report json: {json_path}")
        print(f"report md: {md_path}")


if __name__ == "__main__":
    raise SystemExit(main())
