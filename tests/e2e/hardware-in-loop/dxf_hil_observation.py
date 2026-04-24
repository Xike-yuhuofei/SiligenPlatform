from __future__ import annotations

from datetime import datetime, timezone
from typing import Any

from runtime_gateway_harness import TcpJsonClient, truncate_json


DEFAULT_COORD_SYS = 1
DEFAULT_POSITION_EPSILON_MM = 0.001
DEFAULT_VELOCITY_EPSILON_MM_S = 0.001

CRDSYS_STATUS_PROG_RUN = 0x00000001
CRDSYS_STATUS_PROG_STOP = 0x00000002
CRDSYS_STATUS_FIFO_FINISH_0 = 0x00000010
CRDSYS_STATUS_FIFO_FINISH_1 = 0x00000020
CRDSYS_STATUS_ACTIVE_MOTION_MASK = CRDSYS_STATUS_PROG_RUN
CRDSYS_STATUS_IDLE_ALLOWED_MASK = (
    CRDSYS_STATUS_PROG_STOP
    | CRDSYS_STATUS_FIFO_FINISH_0
    | CRDSYS_STATUS_FIFO_FINISH_1
)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def status_result(payload: dict[str, Any]) -> dict[str, Any]:
    result = payload.get("result", {})
    return result if isinstance(result, dict) else {}


def result_envelope(result: Any) -> dict[str, Any]:
    return {"result": result if isinstance(result, dict) else {}}


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


def normalize_supervision_payload(payload: Any) -> dict[str, Any]:
    supervision = payload if isinstance(payload, dict) else {}
    return {
        "current_state": str(supervision.get("current_state", "")).strip(),
        "requested_state": str(supervision.get("requested_state", "")).strip(),
        "state_reason": str(supervision.get("state_reason", "")).strip(),
        "state_change_in_process": bool(supervision.get("state_change_in_process", False)),
        "failure_stage": str(supervision.get("failure_stage", "")).strip(),
        "failure_code": str(supervision.get("failure_code", "")).strip(),
    }


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
        "door_known": bool(io_payload.get("door_known", False)),
        "limit_x_pos": bool(io_payload.get("limit_x_pos", False)),
        "limit_x_neg": bool(io_payload.get("limit_x_neg", False)),
        "limit_y_pos": bool(io_payload.get("limit_y_pos", False)),
        "limit_y_neg": bool(io_payload.get("limit_y_neg", False)),
        "home_boundary_x_active": bool(interlocks.get("home_boundary_x_active", False)),
        "home_boundary_y_active": bool(interlocks.get("home_boundary_y_active", False)),
    }


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

    job_execution = result.get("job_execution", {})
    if not isinstance(job_execution, dict):
        job_execution = {}

    return {
        "sampled_at": sampled_at,
        "poll_index": poll_index,
        "supervision": normalize_supervision_payload(result.get("supervision", {})),
        "job_execution": {
            "job_id": str(job_execution.get("job_id", "")).strip(),
            "state": str(job_execution.get("state", "")).strip(),
        },
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
        "position": extract_machine_position(status_payload),
        "safety": summarize_safety(status_payload),
        "axes": axes,
    }


def normalize_job_status_sample(
    payload: dict[str, Any],
    *,
    sampled_at: str,
    poll_index: int,
) -> dict[str, Any]:
    result = status_result(payload)
    return {
        "sampled_at": sampled_at,
        "poll_index": poll_index,
        "job_id": str(result.get("job_id", "")).strip(),
        "state": str(result.get("state", "")).strip(),
        "completed_count": int(parse_int(result.get("completed_count")) or 0),
        "target_count": int(parse_int(result.get("target_count")) or 0),
        "current_cycle": int(parse_int(result.get("current_cycle")) or 0),
        "current_segment": int(parse_int(result.get("current_segment")) or 0),
        "total_segments": int(parse_int(result.get("total_segments")) or 0),
        "cycle_progress_percent": int(parse_int(result.get("cycle_progress_percent")) or 0),
        "overall_progress_percent": int(parse_int(result.get("overall_progress_percent")) or 0),
        "elapsed_seconds": float(parse_float(result.get("elapsed_seconds")) or 0.0),
        "error_message": str(result.get("error_message", "")).strip(),
        "dry_run": bool(result.get("dry_run", False)),
    }


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


def collect_job_observation(
    client: TcpJsonClient,
    *,
    job_id: str,
    poll_index: int,
    coord_sys: int = DEFAULT_COORD_SYS,
    timeout_seconds: float = 5.0,
) -> dict[str, dict[str, Any]]:
    if not str(job_id or "").strip():
        raise ValueError("collect_job_observation requires non-empty job_id")

    observation_response = client.send_request(
        "dxf.job.observation",
        {"job_id": job_id, "coord_sys": coord_sys},
        timeout_seconds=timeout_seconds,
    )
    if "error" in observation_response:
        raise RuntimeError("dxf.job.observation failed: " + truncate_json(observation_response))

    observation_result = status_result(observation_response)
    machine_result = observation_result.get("machine_status")
    job_result = observation_result.get("job_status")
    coord_result = observation_result.get("coord_status")
    if not isinstance(machine_result, dict) or not isinstance(job_result, dict) or not isinstance(coord_result, dict):
        raise RuntimeError("dxf.job.observation returned partial observation payload")

    sampled_at = str(observation_result.get("sampled_at", "")).strip() or utc_now()
    observed_job_id = str(job_result.get("job_id", "")).strip()
    if observed_job_id != str(job_id).strip():
        raise RuntimeError(
            "dxf.job.observation returned mismatched job_id: "
            f"expected={job_id} actual={observed_job_id or 'null'}"
        )

    return {
        "machine_status": normalize_machine_status_sample(
            result_envelope(machine_result),
            sampled_at=sampled_at,
            poll_index=poll_index,
        ),
        "job_status": normalize_job_status_sample(
            result_envelope(job_result),
            sampled_at=sampled_at,
            poll_index=poll_index,
        ),
        "coord_status": normalize_coord_status_sample(
            result_envelope(coord_result),
            sampled_at=sampled_at,
            poll_index=poll_index,
            coord_sys=coord_sys,
        ),
    }


def collect_machine_snapshot(
    client: TcpJsonClient,
    *,
    poll_index: int,
    coord_sys: int = DEFAULT_COORD_SYS,
    timeout_seconds: float = 5.0,
) -> dict[str, dict[str, Any]]:
    sampled_at = utc_now()

    machine_response = client.send_request("status", None, timeout_seconds=timeout_seconds)
    if "error" in machine_response:
        raise RuntimeError("status failed during HIL polling: " + truncate_json(machine_response))

    coord_response = client.send_request(
        "motion.coord.status",
        {"coord_sys": coord_sys},
        timeout_seconds=timeout_seconds,
    )
    if "error" in coord_response:
        raise RuntimeError("motion.coord.status failed: " + truncate_json(coord_response))

    return {
        "machine_status": normalize_machine_status_sample(
            machine_response,
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


def summarize_points_bbox(points: list[dict[str, Any]]) -> dict[str, Any]:
    xs: list[float] = []
    ys: list[float] = []
    for point in points:
        x = parse_float(point.get("x"))
        y = parse_float(point.get("y"))
        if x is None or y is None:
            continue
        xs.append(float(x))
        ys.append(float(y))

    if not xs or not ys:
        return {
            "count": 0,
            "min_x": 0.0,
            "max_x": 0.0,
            "min_y": 0.0,
            "max_y": 0.0,
            "span_x": 0.0,
            "span_y": 0.0,
        }

    min_x = min(xs)
    max_x = max(xs)
    min_y = min(ys)
    max_y = max(ys)
    return {
        "count": len(xs),
        "min_x": round(min_x, 6),
        "max_x": round(max_x, 6),
        "min_y": round(min_y, 6),
        "max_y": round(max_y, 6),
        "span_x": round(max_x - min_x, 6),
        "span_y": round(max_y - min_y, 6),
    }


def summarize_blocked_motion_observation(
    machine_status_history: list[dict[str, Any]],
    coord_status_history: list[dict[str, Any]],
    *,
    position_epsilon_mm: float = DEFAULT_POSITION_EPSILON_MM,
    velocity_epsilon_mm_s: float = DEFAULT_VELOCITY_EPSILON_MM_S,
) -> dict[str, Any]:
    sample_count = min(len(machine_status_history), len(coord_status_history))
    first_machine_sample = machine_status_history[0] if machine_status_history else {}
    last_machine_sample = machine_status_history[-1] if machine_status_history else {}
    first_coord_sample = coord_status_history[0] if coord_status_history else {}
    last_coord_sample = coord_status_history[-1] if coord_status_history else {}

    first_sampled_at = parse_timestamp(first_machine_sample.get("sampled_at") or first_coord_sample.get("sampled_at"))
    last_sampled_at = parse_timestamp(last_machine_sample.get("sampled_at") or last_coord_sample.get("sampled_at"))
    window_seconds = 0.0
    if first_sampled_at is not None and last_sampled_at is not None:
        window_seconds = max(0.0, (last_sampled_at - first_sampled_at).total_seconds())

    machine_points = [
        sample.get("position", {})
        for sample in machine_status_history
        if isinstance(sample.get("position", {}), dict)
    ]
    coord_points = [
        sample.get("position", {})
        for sample in coord_status_history
        if isinstance(sample.get("position", {}), dict)
    ]
    machine_position_bbox = summarize_points_bbox(machine_points)
    coord_position_bbox = summarize_points_bbox(coord_points)

    coord_motion_detected = any(
        coord_status_indicates_running(sample, velocity_epsilon_mm_s=velocity_epsilon_mm_s)
        for sample in coord_status_history
    ) or axis_window_shows_motion(
        coord_status_history,
        position_epsilon_mm=position_epsilon_mm,
        velocity_epsilon_mm_s=velocity_epsilon_mm_s,
    )
    machine_position_motion_detected = (
        float(machine_position_bbox.get("span_x", 0.0) or 0.0) > position_epsilon_mm
        or float(machine_position_bbox.get("span_y", 0.0) or 0.0) > position_epsilon_mm
    )
    motion_detected = coord_motion_detected or machine_position_motion_detected

    return {
        "sample_count": sample_count,
        "machine_sample_count": len(machine_status_history),
        "coord_sample_count": len(coord_status_history),
        "window_seconds": round(window_seconds, 6),
        "position_epsilon_mm": float(position_epsilon_mm),
        "velocity_epsilon_mm_s": float(velocity_epsilon_mm_s),
        "coord_motion_detected": coord_motion_detected,
        "machine_position_motion_detected": machine_position_motion_detected,
        "motion_detected": motion_detected,
        "coord_idle_window": sample_count > 0 and not coord_motion_detected,
        "position_within_epsilon": sample_count > 0 and not machine_position_motion_detected,
        "machine_position_bbox": machine_position_bbox,
        "coord_position_bbox": coord_position_bbox,
        "first_sample": {
            "machine_status": first_machine_sample,
            "coord_status": first_coord_sample,
        }
        if sample_count > 0
        else {},
        "last_sample": {
            "machine_status": last_machine_sample,
            "coord_status": last_coord_sample,
        }
        if sample_count > 0
        else {},
    }
