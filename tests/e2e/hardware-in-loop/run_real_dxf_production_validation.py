from __future__ import annotations

import argparse
import base64
import json
import re
import socket
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[3]
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
if str(HIL_DIR) not in sys.path:
    sys.path.insert(0, str(HIL_DIR))

from dxf_hil_observation import (  # noqa: E402
    DEFAULT_COORD_SYS,
    DEFAULT_POSITION_EPSILON_MM,
    DEFAULT_VELOCITY_EPSILON_MM_S,
    collect_machine_snapshot,
    normalize_job_status_sample,
    normalize_machine_status_sample,
    summarize_blocked_motion_observation,
)
from run_real_dxf_machine_dryrun import (  # noqa: E402
    active_home_boundary_axes,
    extract_machine_position,
    non_homed_axes,
    parse_float,
    parse_int,
    require_safe_for_motion,
    resolve_job_timeout_budget,
    status_result,
    summarize_safety,
)
from runtime_gateway_harness import (  # noqa: E402
    KNOWN_FAILURE_EXIT_CODE,
    TcpJsonClient,
    build_process_env,
    ensure_matching_production_baseline,
    input_quality_metadata,
    read_process_output,
    production_baseline_metadata,
    resolve_default_exe,
    tcp_connect_and_ensure_ready,
    truncate_json,
    wait_gateway_ready,
)


DEFAULT_REPO_DXF = ROOT / "samples" / "dxf" / "rect_diag.dxf"
DEFAULT_CONFIG_PATH = ROOT / "config" / "machine" / "machine_config.ini"
DEFAULT_REPORT_ROOT = ROOT / "tests" / "reports" / "online-validation" / "dxf-production-path-trigger"
DEFAULT_STATUS_POLL_INTERVAL_SECONDS = 0.10
DEFAULT_GATEWAY_READY_TIMEOUT = 8.0
DEFAULT_CONNECT_TIMEOUT = 15.0
DEFAULT_HOME_TIMEOUT = 60.0
DEFAULT_BLOCKED_OBSERVATION_WINDOW_SECONDS = 1.0
DEFAULT_DISPENSING_SPEED_MM_S = 10.0
DEFAULT_RAPID_SPEED_MM_S = 10.0
DEFAULT_MIN_AXIS_COVERAGE_RATIO = 0.80

START_TRIGGER_RE = re.compile(
    r"StartPositionTriggeredDispenser: events=(?P<events>\d+), coordinate_system=(?P<coordinate_system>-?\d+), "
    r"(?:start_level=(?P<start_level>-?\d+), )?"
    r"tolerance=(?P<tolerance>-?\d+), pulse_width=(?P<pulse_width_ms>\d+)ms"
)
PATH_TRIGGER_DONE_RE = re.compile(
    r"PathTriggeredDispenserLoop: .*completedCount=(?P<completed_count>\d+)"
)
STOP_DISPENSER_RE = re.compile(
    r"StopDispenser: completedCount=(?P<completed_count>\d+), totalCount=(?P<total_count>\d+)"
)
PROFILE_COMPARE_REQUEST_RE = re.compile(
    r"profile_compare_request_summary span_index=(?P<span_index>\d+) compare_source_axis=(?P<compare_source_axis>-?\d+) "
    r"business_trigger_count=(?P<business_trigger_count>\d+) "
    r"start_boundary_trigger_count=(?P<start_boundary_trigger_count>\d+) "
    r"future_compare_count=(?P<future_compare_count>\d+)"
)
CALL_CMP_PULSE_ONCE_RE = re.compile(r"CallCmpPulseOnce:")
SUPPLY_OPEN_RE = re.compile(r"OpenSupply: 供胶阀 .* 打开成功")
SUPPLY_CLOSE_RE = re.compile(r"CloseSupply: 供胶阀 .* 关闭成功")
EXECUTION_COMPLETE_RE = re.compile(
    r"点胶执行完成: segments=(?P<segments>\d+), distance=(?P<distance_mm>\d+(?:\.\d+)?)mm, time=(?P<time_s>\d+(?:\.\d+)?)s"
)
BOOT_MARK_TEMPLATE = "###BOOT_MARK:TCP_SERVER_DIAG_20260203###"


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run a real-machine production validation for the path-trigger DXF chain."
    )
    parser.add_argument("--gateway-exe", type=Path, default=resolve_default_exe("siligen_runtime_gateway.exe"))
    parser.add_argument("--config-path", type=Path, default=DEFAULT_CONFIG_PATH)
    parser.add_argument("--dxf-file", type=Path, default=DEFAULT_REPO_DXF)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--gateway-ready-timeout", type=float, default=DEFAULT_GATEWAY_READY_TIMEOUT)
    parser.add_argument("--connect-timeout", type=float, default=DEFAULT_CONNECT_TIMEOUT)
    parser.add_argument("--home-timeout", type=float, default=DEFAULT_HOME_TIMEOUT)
    parser.add_argument("--status-poll-interval-seconds", type=float, default=DEFAULT_STATUS_POLL_INTERVAL_SECONDS)
    parser.add_argument(
        "--blocked-observation-window-seconds",
        type=float,
        default=DEFAULT_BLOCKED_OBSERVATION_WINDOW_SECONDS,
    )
    parser.add_argument("--dispensing-speed-mm-s", type=float, default=DEFAULT_DISPENSING_SPEED_MM_S)
    parser.add_argument("--rapid-speed-mm-s", type=float, default=DEFAULT_RAPID_SPEED_MM_S)
    parser.add_argument("--target-count", type=int, default=1)
    parser.add_argument("--max-polyline-points", type=int, default=4000)
    parser.add_argument("--max-glue-points", type=int, default=4000)
    parser.add_argument("--min-axis-coverage-ratio", type=float, default=DEFAULT_MIN_AXIS_COVERAGE_RATIO)
    parser.add_argument("--report-root", type=Path, default=DEFAULT_REPORT_ROOT)
    args = parser.parse_args()
    if args.status_poll_interval_seconds <= 0:
        raise SystemExit("--status-poll-interval-seconds must be > 0")
    if args.blocked_observation_window_seconds <= 0:
        raise SystemExit("--blocked-observation-window-seconds must be > 0")
    if args.target_count <= 0:
        raise SystemExit("--target-count must be > 0")
    if args.min_axis_coverage_ratio < 0.0:
        raise SystemExit("--min-axis-coverage-ratio must be >= 0")
    return args


def resolve_listen_port(host: str, requested_port: int) -> int:
    if requested_port > 0:
        return requested_port

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((host, 0))
        return int(sock.getsockname()[1])


def add_step(
    steps: list[dict[str, str]],
    step: str,
    status: str,
    detail: str,
) -> None:
    steps.append(
        {
            "step": step,
            "status": status,
            "detail": detail,
            "timestamp": utc_now(),
        }
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


def summarize_machine_bbox(machine_status_history: list[dict[str, Any]]) -> dict[str, Any]:
    points: list[dict[str, Any]] = []
    for sample in machine_status_history:
        position = sample.get("position", {})
        if isinstance(position, dict):
            points.append(position)
    return summarize_points_bbox(points)


def compute_axis_coverage(
    expected_bbox: dict[str, Any],
    observed_bbox: dict[str, Any],
) -> dict[str, Any]:
    expected_span_x = float(expected_bbox.get("span_x", 0.0) or 0.0)
    expected_span_y = float(expected_bbox.get("span_y", 0.0) or 0.0)
    observed_span_x = float(observed_bbox.get("span_x", 0.0) or 0.0)
    observed_span_y = float(observed_bbox.get("span_y", 0.0) or 0.0)

    ratio_x = 1.0 if expected_span_x <= 1e-6 else observed_span_x / expected_span_x
    ratio_y = 1.0 if expected_span_y <= 1e-6 else observed_span_y / expected_span_y
    return {
        "ratio_x": round(ratio_x, 6),
        "ratio_y": round(ratio_y, 6),
        "expected_span_x": round(expected_span_x, 6),
        "expected_span_y": round(expected_span_y, 6),
        "observed_span_x": round(observed_span_x, 6),
        "observed_span_y": round(observed_span_y, 6),
    }


def observe_blocked_motion_window(
    client: TcpJsonClient,
    *,
    machine_status_history: list[dict[str, Any]],
    coord_status_history: list[dict[str, Any]],
    observation_window_seconds: float,
    poll_interval_seconds: float,
    coord_sys: int = DEFAULT_COORD_SYS,
    position_epsilon_mm: float = DEFAULT_POSITION_EPSILON_MM,
    velocity_epsilon_mm_s: float = DEFAULT_VELOCITY_EPSILON_MM_S,
) -> dict[str, Any]:
    deadline = time.time() + observation_window_seconds
    poll_index = 0
    while True:
        observation = collect_machine_snapshot(
            client,
            poll_index=poll_index,
            coord_sys=coord_sys,
        )
        machine_status_history.append(observation["machine_status"])
        coord_status_history.append(observation["coord_status"])
        poll_index += 1
        if time.time() >= deadline:
            break
        time.sleep(min(poll_interval_seconds, max(0.0, deadline - time.time())))

    return summarize_blocked_motion_observation(
        machine_status_history,
        coord_status_history,
        position_epsilon_mm=position_epsilon_mm,
        velocity_epsilon_mm_s=velocity_epsilon_mm_s,
    )


def extract_last_log_match(log_text: str, pattern: re.Pattern[str]) -> dict[str, Any]:
    last_match: re.Match[str] | None = None
    last_line = ""
    for line in log_text.splitlines():
        match = pattern.search(line)
        if match is None:
            continue
        last_match = match
        last_line = line

    if last_match is None:
        return {}

    payload: dict[str, Any] = {"line": last_line}
    for key, value in last_match.groupdict().items():
        as_int = parse_int(value)
        payload[key] = int(as_int) if as_int is not None else value
    return payload


def count_log_matches(log_text: str, pattern: re.Pattern[str]) -> int:
    count = 0
    for line in log_text.splitlines():
        if pattern.search(line):
            count += 1
    return count


def summarize_profile_compare_requests(log_text: str) -> dict[str, Any]:
    request_count = 0
    sum_business_trigger_count = 0
    sum_start_boundary_trigger_count = 0
    sum_future_compare_count = 0
    first_span_index: int | None = None
    last_span_index: int | None = None
    first_line = ""
    last_line = ""

    for line in log_text.splitlines():
        match = PROFILE_COMPARE_REQUEST_RE.search(line)
        if match is None:
            continue
        span_index = int(match.group("span_index"))
        business_trigger_count = int(match.group("business_trigger_count"))
        start_boundary_trigger_count = int(match.group("start_boundary_trigger_count"))
        future_compare_count = int(match.group("future_compare_count"))

        if request_count == 0:
            first_span_index = span_index
            first_line = line

        request_count += 1
        sum_business_trigger_count += business_trigger_count
        sum_start_boundary_trigger_count += start_boundary_trigger_count
        sum_future_compare_count += future_compare_count
        last_span_index = span_index
        last_line = line

    if request_count <= 0:
        return {}

    return {
        "request_count": request_count,
        "sum_business_trigger_count": sum_business_trigger_count,
        "sum_start_boundary_trigger_count": sum_start_boundary_trigger_count,
        "sum_future_compare_count": sum_future_compare_count,
        "first_span_index": first_span_index,
        "last_span_index": last_span_index,
        "first_line": first_line,
        "last_line": last_line,
    }


def extract_gateway_log_summary(log_text: str) -> dict[str, Any]:
    profile_compare_summary = summarize_profile_compare_requests(log_text)
    execution_complete = extract_last_log_match(log_text, EXECUTION_COMPLETE_RE)
    execution_complete["count"] = count_log_matches(log_text, EXECUTION_COMPLETE_RE)

    summary = {
        "start_position_triggered": extract_last_log_match(log_text, START_TRIGGER_RE),
        "path_trigger_completed": extract_last_log_match(log_text, PATH_TRIGGER_DONE_RE),
        "stop_dispenser": extract_last_log_match(log_text, STOP_DISPENSER_RE),
        "profile_compare_request_summary": profile_compare_summary,
        "cmp_pulse_once": {
            "count": count_log_matches(log_text, CALL_CMP_PULSE_ONCE_RE),
        },
        "supply_open": {
            "count": count_log_matches(log_text, SUPPLY_OPEN_RE),
        },
        "supply_close": {
            "count": count_log_matches(log_text, SUPPLY_CLOSE_RE),
        },
        "execution_complete": execution_complete,
    }
    if profile_compare_summary:
        summary["execution_mode"] = "profile_compare"
    elif summary["path_trigger_completed"] or summary["stop_dispenser"] or summary["start_position_triggered"]:
        summary["execution_mode"] = "path_trigger"
    else:
        summary["execution_mode"] = "unknown"
    return summary


def read_log_segment(log_path: Path, start_offset: int) -> str:
    if not log_path.exists():
        return ""
    try:
        raw = log_path.read_bytes()
    except OSError:
        return ""
    if start_offset <= 0 or len(raw) < start_offset:
        segment = raw
    else:
        segment = raw[start_offset:]
    return segment.decode("utf-8", errors="replace")


def read_current_run_gateway_log(log_path: Path, start_offset: int, effective_port: int) -> str:
    appended_text = read_log_segment(log_path, start_offset)
    if effective_port <= 0 or not log_path.exists():
        return appended_text

    try:
        full_text = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return appended_text

    boot_pattern = re.compile(
        rf"(?m)^\[[^\r\n]+\].*{re.escape(BOOT_MARK_TEMPLATE)} .*port={re.escape(str(effective_port))}\s*$"
    )
    matches = list(boot_pattern.finditer(full_text))
    if matches:
        return full_text[matches[-1].start():]
    return appended_text or full_text


def extract_error_message(payload: dict[str, Any]) -> str:
    error = payload.get("error", {}) if isinstance(payload, dict) else {}
    if not isinstance(error, dict):
        return ""
    return str(error.get("message", "")).strip()


def is_production_blocked_gate(gate: Any) -> bool:
    return isinstance(gate, dict) and str(gate.get("status", "")).strip() == "production_blocked"


def resolve_validation_mode(plan_result: dict[str, Any]) -> str:
    input_quality = input_quality_metadata(plan_result)
    if (
        bool(input_quality.get("preview_ready", False))
        and not bool(input_quality.get("production_ready", True))
        and is_production_blocked_gate(plan_result.get("formal_compare_gate"))
    ):
        return "production_blocked"
    return "production_execution"


def build_checklist(
    *,
    validation_mode: str,
    plan_result: dict[str, Any],
    snapshot_result: dict[str, Any],
    job_start_response: dict[str, Any],
    blocked_motion_observation: dict[str, Any],
    final_job_status: dict[str, Any],
    log_summary: dict[str, Any],
    axis_coverage: dict[str, Any],
    observed_bbox: dict[str, Any],
    min_axis_coverage_ratio: float,
) -> dict[str, dict[str, Any]]:
    expected_trigger_count = int(parse_int(snapshot_result.get("glue_point_count")) or 0)
    job_start_error_message = extract_error_message(job_start_response)
    input_quality = input_quality_metadata(plan_result)
    input_summary = str(input_quality.get("summary", "")).strip()
    formal_compare_gate = plan_result.get("formal_compare_gate")

    if validation_mode == "production_blocked":
        return {
            "preview_source_planned_glue_snapshot": {
                "passed": str(snapshot_result.get("preview_source", "")).strip() == "planned_glue_snapshot",
            },
            "preview_kind_glue_points": {
                "passed": str(snapshot_result.get("preview_kind", "")).strip() == "glue_points",
            },
            "expected_trigger_count_positive": {
                "passed": expected_trigger_count > 0,
            },
            "plan_import_preview_ready": {
                "passed": bool(input_quality.get("preview_ready", False)),
            },
            "plan_import_production_blocked": {
                "passed": not bool(input_quality.get("production_ready", True)),
            },
            "formal_compare_gate_present": {
                "passed": isinstance(formal_compare_gate, dict) and bool(formal_compare_gate),
            },
            "formal_compare_gate_status_production_blocked": {
                "passed": is_production_blocked_gate(formal_compare_gate),
            },
            "job_start_rejected": {
                "passed": bool(job_start_error_message),
            },
            "job_start_matches_import_summary": {
                "passed": bool(job_start_error_message) and job_start_error_message == input_summary,
            },
            "post_block_observation_samples_collected": {
                "passed": int(parse_int(blocked_motion_observation.get("sample_count")) or 0) > 0,
            },
            "no_axis_motion_observed_after_block": {
                "passed": (
                    int(parse_int(blocked_motion_observation.get("sample_count")) or 0) > 0
                    and not bool(blocked_motion_observation.get("motion_detected", True))
                ),
            },
        }

    path_trigger_completed = int(
        parse_int((log_summary.get("path_trigger_completed") or {}).get("completed_count")) or 0
    )
    stop_completed = int(parse_int((log_summary.get("stop_dispenser") or {}).get("completed_count")) or 0)
    stop_total = int(parse_int((log_summary.get("stop_dispenser") or {}).get("total_count")) or 0)
    profile_compare_summary = log_summary.get("profile_compare_request_summary") or {}
    profile_compare_request_count = int(parse_int(profile_compare_summary.get("request_count")) or 0)
    profile_compare_business_count = int(
        parse_int(profile_compare_summary.get("sum_business_trigger_count")) or 0
    )
    profile_compare_start_boundary_count = int(
        parse_int(profile_compare_summary.get("sum_start_boundary_trigger_count")) or 0
    )
    cmp_pulse_once_count = int(parse_int((log_summary.get("cmp_pulse_once") or {}).get("count")) or 0)
    supply_close_count = int(parse_int((log_summary.get("supply_close") or {}).get("count")) or 0)
    execution_complete_count = int(parse_int((log_summary.get("execution_complete") or {}).get("count")) or 0)
    execution_mode = str(log_summary.get("execution_mode", "")).strip().lower()
    if not execution_mode:
        if profile_compare_request_count > 0:
            execution_mode = "profile_compare"
        elif path_trigger_completed > 0 or stop_completed > 0:
            execution_mode = "path_trigger"
        else:
            execution_mode = "unknown"

    final_state = str(final_job_status.get("state", "")).strip().lower()
    final_progress = int(parse_int(final_job_status.get("overall_progress_percent")) or 0)
    final_completed_count = int(parse_int(final_job_status.get("completed_count")) or 0)
    final_target_count = int(parse_int(final_job_status.get("target_count")) or 0)
    completed_cycles = final_completed_count if final_completed_count > 0 else 0
    common_checks = {
        "preview_source_planned_glue_snapshot": {
            "passed": str(snapshot_result.get("preview_source", "")).strip() == "planned_glue_snapshot",
        },
        "preview_kind_glue_points": {
            "passed": str(snapshot_result.get("preview_kind", "")).strip() == "glue_points",
        },
        "expected_trigger_count_positive": {
            "passed": expected_trigger_count > 0,
        },
        "job_terminal_completed": {
            "passed": final_state == "completed",
        },
        "job_overall_progress_100": {
            "passed": final_progress == 100,
        },
        "job_completed_count_matches_target_count": {
            "passed": final_completed_count == final_target_count and final_target_count > 0,
        },
    }

    if execution_mode == "profile_compare":
        return {
            **common_checks,
            "profile_compare_business_trigger_matches_glue_count": {
                "passed": (
                    profile_compare_business_count == expected_trigger_count
                    and expected_trigger_count > 0
                    and profile_compare_request_count > 0
                ),
                "expected_trigger_count": expected_trigger_count,
                "observed_business_trigger_count": profile_compare_business_count,
                "request_count": profile_compare_request_count,
            },
            "profile_compare_immediate_pulse_matches_start_boundary_count": {
                "passed": (
                    cmp_pulse_once_count == profile_compare_start_boundary_count
                    and profile_compare_request_count > 0
                ),
                "observed_cmp_pulse_once_count": cmp_pulse_once_count,
                "observed_start_boundary_trigger_count": profile_compare_start_boundary_count,
            },
            "execution_complete_logged": {
                "passed": execution_complete_count >= completed_cycles and completed_cycles > 0,
                "observed_execution_complete_count": execution_complete_count,
                "expected_completed_cycles": completed_cycles,
            },
            "supply_close_logged": {
                "passed": supply_close_count >= completed_cycles and completed_cycles > 0,
                "observed_supply_close_count": supply_close_count,
                "expected_completed_cycles": completed_cycles,
            },
            "observed_axis_coverage": {
                "passed": (
                    float(axis_coverage.get("ratio_x", 0.0)) >= min_axis_coverage_ratio
                    and float(axis_coverage.get("ratio_y", 0.0)) >= min_axis_coverage_ratio
                ),
            },
        }

    return {
        **common_checks,
        "path_trigger_completed_matches_glue_count": {
            "passed": path_trigger_completed == expected_trigger_count and expected_trigger_count > 0,
        },
        "stop_counts_match_glue_count": {
            "passed": stop_completed == stop_total == expected_trigger_count and expected_trigger_count > 0,
        },
        "observed_axis_coverage": {
            "passed": (
                float(axis_coverage.get("ratio_x", 0.0)) >= min_axis_coverage_ratio
                and float(axis_coverage.get("ratio_y", 0.0)) >= min_axis_coverage_ratio
            ),
        },
    }


def determine_overall_status(checks: dict[str, dict[str, Any]], validation_mode: str) -> tuple[str, str]:
    failed = [name for name, payload in checks.items() if not bool(payload.get("passed", False))]
    if not failed:
        return ("known_failure", "") if validation_mode == "production_blocked" else ("passed", "")
    return "failed", "failed checks: " + ", ".join(failed)


def build_timing_summary(
    *,
    validation_mode: str,
    plan_result: dict[str, Any],
    artifacts: dict[str, Any],
    log_summary: dict[str, Any],
) -> dict[str, Any]:
    if validation_mode == "production_blocked":
        return {
            "validation_mode": validation_mode,
            "execution_nominal_time_s": None,
            "execution_budget_s": None,
            "execution_budget_breakdown": None,
            "observed_execution_time_s": None,
        }

    job_timeout_budget = artifacts.get("job_timeout_budget", {})
    execution_budget_breakdown = None
    if isinstance(job_timeout_budget, dict):
        raw_breakdown = job_timeout_budget.get("execution_budget_breakdown")
        if isinstance(raw_breakdown, dict):
            execution_budget_breakdown = raw_breakdown

    return {
        "validation_mode": validation_mode,
        "execution_nominal_time_s": parse_float(plan_result.get("execution_nominal_time_s")),
        "execution_budget_s": parse_float(job_timeout_budget.get("execution_budget_seconds")),
        "execution_budget_breakdown": execution_budget_breakdown,
        "observed_execution_time_s": parse_float((log_summary.get("execution_complete") or {}).get("time_s")),
    }


def build_report(
    *,
    args: argparse.Namespace,
    effective_port: int,
    overall_status: str,
    validation_mode: str,
    job_id: str,
    plan_id: str,
    plan_fingerprint: str,
    snapshot_hash: str,
    steps: list[dict[str, str]],
    checks: dict[str, dict[str, Any]],
    axis_coverage: dict[str, Any],
    expected_bbox: dict[str, Any],
    observed_bbox: dict[str, Any],
    blocked_motion_observation: dict[str, Any],
    snapshot_result: dict[str, Any],
    final_job_status: dict[str, Any],
    log_summary: dict[str, Any],
    artifacts: dict[str, Any],
    error_message: str,
    plan_result: dict[str, Any],
    production_baseline: dict[str, Any],
) -> dict[str, Any]:
    timing_summary = build_timing_summary(
        validation_mode=validation_mode,
        plan_result=plan_result,
        artifacts=artifacts,
        log_summary=log_summary,
    )
    return {
        "generated_at": utc_now(),
        "overall_status": overall_status,
        "validation_mode": validation_mode,
        "gateway_exe": str(args.gateway_exe),
        "gateway_host": args.host,
        "gateway_port": effective_port,
        "config_path": str(args.config_path),
        "dxf_file": str(args.dxf_file),
        **production_baseline,
        "job_id": job_id,
        "plan_id": plan_id,
        "plan_fingerprint": plan_fingerprint,
        "snapshot_hash": snapshot_hash,
        "timing_summary": timing_summary,
        "steps": steps,
        "checks": checks,
        "axis_coverage": axis_coverage,
        "expected_bbox": expected_bbox,
        "observed_bbox": observed_bbox,
        "blocked_motion_observation": blocked_motion_observation,
        "snapshot_result": snapshot_result,
        "final_job_status": final_job_status,
        "log_summary": log_summary,
        "artifacts": {
            **artifacts,
        },
        "error": error_message,
    }


def build_report_markdown(report: dict[str, Any]) -> str:
    lines = [
        "# Real DXF Production Validation Report",
        "",
        f"- generated_at: `{report['generated_at']}`",
        f"- overall_status: `{report['overall_status']}`",
        f"- validation_mode: `{report.get('validation_mode', 'production_execution')}`",
        f"- gateway_exe: `{report['gateway_exe']}`",
        f"- gateway_host: `{report['gateway_host']}`",
        f"- gateway_port: `{report['gateway_port']}`",
        f"- config_path: `{report['config_path']}`",
        f"- dxf_file: `{report['dxf_file']}`",
        f"- baseline_id: `{report.get('baseline_id', '')}`",
        f"- baseline_fingerprint: `{report.get('baseline_fingerprint', '')}`",
        f"- production_baseline_source: `{report.get('production_baseline_source', '')}`",
        f"- job_id: `{report.get('job_id', '')}`",
        "",
        "## Timing Summary",
        "",
    ]
    timing_summary = report.get("timing_summary", {})
    if timing_summary:
        lines.extend(
            [
                f"- `execution_nominal_time_s`: `{timing_summary.get('execution_nominal_time_s')}`",
                f"- `execution_budget_s`: `{timing_summary.get('execution_budget_s')}`",
                f"- `observed_execution_time_s`: `{timing_summary.get('observed_execution_time_s')}`",
                f"- `execution_budget_breakdown`: `{json.dumps(timing_summary.get('execution_budget_breakdown'), ensure_ascii=False)}`",
                "",
            ]
        )
    lines.extend(
        [
        "## Checks",
        "",
        ]
    )
    for name, payload in report.get("checks", {}).items():
        status = "passed" if payload.get("passed", False) else "failed"
        lines.append(f"- `{name}`: `{status}`")
    blocked_motion_observation = report.get("blocked_motion_observation", {})
    if blocked_motion_observation:
        lines.extend(
            [
                "",
                "## Blocked Motion Observation",
                "",
                f"- `sample_count`: `{blocked_motion_observation.get('sample_count', 0)}`",
                f"- `window_seconds`: `{blocked_motion_observation.get('window_seconds', 0.0)}`",
                f"- `motion_detected`: `{bool(blocked_motion_observation.get('motion_detected', False))}`",
            ]
        )
    lines.extend(["", "## Steps", ""])
    for step in report.get("steps", []):
        lines.append(f"- `{step['step']}`: `{step['status']}` {step['detail']}")
    if report.get("error"):
        lines.extend(["", "## Error", "", report["error"]])
    return "\n".join(lines) + "\n"


def write_json(path: Path, payload: Any) -> None:
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")


def attempt_safe_stop(client: TcpJsonClient, job_id: str) -> None:
    if not job_id:
        return
    try:
        client.send_request("dxf.job.stop", {"job_id": job_id}, timeout_seconds=5.0)
    except Exception:
        pass
    try:
        client.send_request("stop", None, timeout_seconds=5.0)
    except Exception:
        pass


def main() -> int:
    args = parse_args()
    effective_port = resolve_listen_port(args.host, args.port)
    report_dir = args.report_root / time.strftime("%Y%m%d-%H%M%S")
    report_dir.mkdir(parents=True, exist_ok=True)

    report_json_path = report_dir / "real-dxf-production-validation.json"
    report_md_path = report_dir / "real-dxf-production-validation.md"
    job_status_json_path = report_dir / "job-status-history.json"
    machine_status_json_path = report_dir / "machine-status-history.json"
    coord_status_json_path = report_dir / "coord-status-history.json"
    gateway_log_copy_path = report_dir / "tcp_server.log"
    gateway_stdout_path = report_dir / "gateway-stdout.log"
    gateway_stderr_path = report_dir / "gateway-stderr.log"

    steps: list[dict[str, str]] = []
    artifacts: dict[str, Any] = {}
    job_status_history: list[dict[str, Any]] = []
    machine_status_history: list[dict[str, Any]] = []
    coord_status_history: list[dict[str, Any]] = []
    blocked_motion_observation: dict[str, Any] = {}
    overall_status = "failed"
    error_message = ""
    return_code = 1
    job_id = ""
    plan_id = ""
    plan_fingerprint = ""
    snapshot_hash = ""
    validation_mode = "production_execution"
    plan_result: dict[str, Any] = {}
    snapshot_result: dict[str, Any] = {}
    job_start_response: dict[str, Any] = {}
    final_job_status: dict[str, Any] = {}
    production_baseline: dict[str, Any] = {}
    process: subprocess.Popen[str] | None = None
    client = TcpJsonClient(args.host, effective_port)
    connected = False
    gateway_log_path = ROOT / "logs" / "tcp_server.log"
    gateway_log_offset = gateway_log_path.stat().st_size if gateway_log_path.exists() else 0

    try:
        if not args.gateway_exe.exists():
            raise FileNotFoundError(f"gateway executable missing: {args.gateway_exe}")
        if not args.config_path.exists():
            raise FileNotFoundError(f"config path missing: {args.config_path}")
        if not args.dxf_file.exists():
            raise FileNotFoundError(f"dxf file missing: {args.dxf_file}")

        add_step(
            steps,
            "input-check",
            "passed",
            json.dumps(
                {
                    "dxf": str(args.dxf_file),
                    "expected_baseline_owner": "runtime_owned",
                },
                ensure_ascii=True,
            ),
        )

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
            return_code = KNOWN_FAILURE_EXIT_CODE if ready_status == "known_failure" else 1
            return return_code

        client.connect(timeout_seconds=args.connect_timeout)
        connected = True
        add_step(steps, "tcp-session-open", "passed", f"connected to {args.host}:{effective_port}")

        admission = tcp_connect_and_ensure_ready(
            client,
            config_path=args.config_path,
            connect_timeout_seconds=args.connect_timeout,
            status_timeout_seconds=min(8.0, args.connect_timeout),
            ready_timeout_seconds=3.0,
        )
        artifacts["connect_response"] = admission.connect_response or {}
        artifacts["connect_status_response"] = admission.status_response or {}
        if admission.status != "passed":
            raise RuntimeError(
                "connect failed: "
                + truncate_json(
                    {
                        "note": admission.note,
                        "connect_params": admission.connect_params,
                        "connect_response": admission.connect_response,
                        "status_response": admission.status_response,
                    }
                )
            )
        add_step(steps, "tcp-connect", "passed", json.dumps(admission.connect_params, ensure_ascii=True))

        status_before = client.send_request("status", None, timeout_seconds=5.0)
        artifacts["status_before"] = status_before
        add_step(
            steps,
            "status-before",
            "passed",
            json.dumps(
                {
                    "safety": summarize_safety(status_before),
                    "non_homed_axes": non_homed_axes(status_before),
                    "position": extract_machine_position(status_before),
                },
                ensure_ascii=True,
            ),
        )

        should_home = bool(non_homed_axes(status_before) or active_home_boundary_axes(status_before))
        if should_home:
            home_response = client.send_request("home", None, timeout_seconds=args.home_timeout)
            artifacts["home_response"] = home_response
            if "error" in home_response:
                raise RuntimeError("home failed: " + truncate_json(home_response))
            add_step(steps, "home", "passed", truncate_json(home_response))
            status_after_home = client.send_request("status", None, timeout_seconds=5.0)
        else:
            status_after_home = status_before
            add_step(steps, "home", "passed", "already homed; no homing command sent")

        artifacts["status_after_home"] = status_after_home
        add_step(
            steps,
            "status-after-home",
            "passed",
            json.dumps(
                {
                    "safety": summarize_safety(status_after_home),
                    "non_homed_axes": non_homed_axes(status_after_home),
                    "position": extract_machine_position(status_after_home),
                    "home_boundary_axes": active_home_boundary_axes(status_after_home),
                },
                ensure_ascii=True,
            ),
        )

        require_safe_for_motion(status_after_home, ignore_home_boundaries=True)
        add_step(
            steps,
            "preflight-safe-for-motion",
            "passed",
            json.dumps(
                {
                    "safety": summarize_safety(status_after_home),
                    "position": extract_machine_position(status_after_home),
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

        plan_response = client.send_request(
            "dxf.plan.prepare",
            {
                "artifact_id": artifact_id,
                "dispensing_speed_mm_s": args.dispensing_speed_mm_s,
                "dry_run": False,
                "rapid_speed_mm_s": args.rapid_speed_mm_s,
                "velocity_trace_enabled": False,
            },
            timeout_seconds=30.0,
        )
        artifacts["dxf_plan_prepare"] = plan_response
        if "error" in plan_response:
            raise RuntimeError("dxf.plan.prepare failed: " + truncate_json(plan_response))
        plan_result = status_result(plan_response)
        plan_id = str(plan_result.get("plan_id", "")).strip()
        plan_fingerprint = str(plan_result.get("plan_fingerprint", "")).strip()
        if not plan_id or not plan_fingerprint:
            raise RuntimeError("plan.prepare missing plan_id/plan_fingerprint")
        production_baseline = production_baseline_metadata(plan_result, source="dxf.plan.prepare")
        validation_mode = resolve_validation_mode(plan_result)
        add_step(
            steps,
            "dxf-plan-prepare",
            "passed",
            json.dumps(
                {
                    "plan_id": plan_id,
                    "plan_fingerprint": plan_fingerprint,
                    "execution_nominal_time_s": plan_result.get("execution_nominal_time_s", 0),
                    "segment_count": plan_result.get("segment_count", 0),
                    "production_baseline": production_baseline,
                },
                ensure_ascii=True,
            ),
        )
        if validation_mode == "production_blocked":
            gate = plan_result.get("formal_compare_gate", {})
            add_step(
                steps,
                "formal-compare-gate",
                "known_failure",
                json.dumps(
                    {
                        "status": gate.get("status", ""),
                        "reason_code": gate.get("reason_code", ""),
                        "trigger_begin_index": gate.get("trigger_begin_index", 0),
                        "authority_span_ref": gate.get("authority_span_ref", ""),
                    },
                    ensure_ascii=True,
                ),
            )

        snapshot_response = client.send_request(
            "dxf.preview.snapshot",
            {
                "plan_id": plan_id,
                "max_polyline_points": args.max_polyline_points,
                "max_glue_points": args.max_glue_points,
            },
            timeout_seconds=15.0,
        )
        artifacts["dxf_preview_snapshot"] = snapshot_response
        if "error" in snapshot_response:
            raise RuntimeError("dxf.preview.snapshot failed: " + truncate_json(snapshot_response))
        snapshot_result = status_result(snapshot_response)
        snapshot_hash = str(snapshot_result.get("snapshot_hash", "")).strip()
        if not snapshot_hash:
            raise RuntimeError("preview.snapshot missing snapshot_hash")
        add_step(
            steps,
            "dxf-preview-snapshot",
            "passed",
            json.dumps(
                {
                    "snapshot_hash": snapshot_hash,
                    "preview_source": snapshot_result.get("preview_source", ""),
                    "preview_kind": snapshot_result.get("preview_kind", ""),
                    "glue_point_count": snapshot_result.get("glue_point_count", 0),
                },
                ensure_ascii=True,
            ),
        )

        confirm_response = client.send_request(
            "dxf.preview.confirm",
            {"plan_id": plan_id, "snapshot_hash": snapshot_hash},
            timeout_seconds=15.0,
        )
        artifacts["dxf_preview_confirm"] = confirm_response
        if "error" in confirm_response:
            raise RuntimeError("dxf.preview.confirm failed: " + truncate_json(confirm_response))
        add_step(steps, "dxf-preview-confirm", "passed", f"snapshot_hash={snapshot_hash}")

        job_start_response = client.send_request(
            "dxf.job.start",
            {
                "plan_id": plan_id,
                "target_count": args.target_count,
                "plan_fingerprint": plan_fingerprint,
            },
            timeout_seconds=15.0,
        )
        artifacts["dxf_job_start"] = job_start_response
        if "error" in job_start_response:
            if validation_mode == "production_blocked":
                add_step(steps, "dxf-job-start", "known_failure", truncate_json(job_start_response))
                blocked_motion_observation = observe_blocked_motion_window(
                    client,
                    machine_status_history=machine_status_history,
                    coord_status_history=coord_status_history,
                    observation_window_seconds=args.blocked_observation_window_seconds,
                    poll_interval_seconds=args.status_poll_interval_seconds,
                )
                add_step(
                    steps,
                    "blocked-motion-observation",
                    "passed" if not blocked_motion_observation.get("motion_detected", True) else "failed",
                    json.dumps(blocked_motion_observation, ensure_ascii=True),
                )
                overall_status = "known_failure"
                error_message = extract_error_message(job_start_response)
                return_code = KNOWN_FAILURE_EXIT_CODE
            else:
                raise RuntimeError("dxf.job.start failed: " + truncate_json(job_start_response))
        job_result = status_result(job_start_response)
        if validation_mode == "production_blocked" and "error" not in job_start_response:
            job_id = str(job_result.get("job_id", "")).strip()
            raise RuntimeError(
                "dxf.job.start unexpectedly succeeded while formal_compare_gate reported production_blocked"
            )
        if validation_mode != "production_blocked":
            production_baseline = ensure_matching_production_baseline(
                production_baseline,
                production_baseline_metadata(job_result, source="dxf.job.start"),
                label="dxf.job.start",
            )
            job_id = str(job_result.get("job_id", "")).strip()
            if not job_id:
                raise RuntimeError("dxf.job.start missing job_id")
            job_timeout_budget = resolve_job_timeout_budget(
                execution_budget_seconds=job_result.get("execution_budget_s"),
                execution_budget_breakdown=job_result.get("execution_budget_breakdown"),
            )
            artifacts["job_timeout_budget"] = job_timeout_budget
            add_step(
                steps,
                "job-timeout-budget",
                "passed",
                json.dumps(job_timeout_budget, ensure_ascii=True),
            )
            add_step(steps, "dxf-job-start", "passed", f"job_id={job_id}")

        if validation_mode != "production_blocked":
            effective_job_timeout_seconds = float(job_timeout_budget["effective_timeout_seconds"])
            deadline = time.time() + effective_job_timeout_seconds
            poll_index = 0
            while time.time() < deadline:
                sampled_at = utc_now()

                job_status_payload = client.send_request(
                    "dxf.job.status",
                    {"job_id": job_id},
                    timeout_seconds=5.0,
                )
                if "error" in job_status_payload:
                    raise RuntimeError("dxf.job.status failed: " + truncate_json(job_status_payload))
                normalized_job_status = normalize_job_status_sample(
                    job_status_payload,
                    sampled_at=sampled_at,
                    poll_index=poll_index,
                )
                job_status_history.append(normalized_job_status)
                final_job_status = normalized_job_status

                machine_status_payload = client.send_request("status", None, timeout_seconds=5.0)
                if "error" in machine_status_payload:
                    raise RuntimeError("status failed during production polling: " + truncate_json(machine_status_payload))
                machine_status_history.append(
                    normalize_machine_status_sample(
                        machine_status_payload,
                        sampled_at=sampled_at,
                        poll_index=poll_index,
                    )
                )

                if normalized_job_status["state"].lower() in {"completed", "failed", "cancelled"}:
                    break

                poll_index += 1
                time.sleep(args.status_poll_interval_seconds)
            else:
                raise TimeoutError(
                    "production validation timed out after "
                    f"{effective_job_timeout_seconds:.1f}s "
                    f"(execution_budget_s={job_timeout_budget['execution_budget_seconds']} "
                    f"cycle_budget_s={job_timeout_budget['cycle_budget_seconds']})"
                )

            add_step(
                steps,
                "job-terminal",
                "passed" if final_job_status.get("state", "").lower() == "completed" else "failed",
                json.dumps(final_job_status, ensure_ascii=True),
            )

            overall_status = "passed"
            return_code = 0
    except Exception as exc:
        error_message = str(exc)
        add_step(steps, "exception", "failed", error_message)
        if connected:
            attempt_safe_stop(client, job_id)
        if overall_status != "known_failure":
            overall_status = "failed"
        return_code = KNOWN_FAILURE_EXIT_CODE if overall_status == "known_failure" else 1
    finally:
        if connected:
            client.close()
        if process is not None:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)

            stdout_text, stderr_text = read_process_output(process)
            gateway_stdout_path.write_text(stdout_text, encoding="utf-8")
            gateway_stderr_path.write_text(stderr_text, encoding="utf-8")

        time.sleep(0.3)
        gateway_log_text = read_current_run_gateway_log(gateway_log_path, gateway_log_offset, effective_port)
        gateway_log_copy_path.write_text(gateway_log_text, encoding="utf-8")

        write_json(job_status_json_path, job_status_history)
        write_json(machine_status_json_path, machine_status_history)
        write_json(coord_status_json_path, coord_status_history)

        expected_bbox = summarize_points_bbox(snapshot_result.get("glue_points", []))
        observed_bbox = summarize_machine_bbox(machine_status_history)
        axis_coverage = compute_axis_coverage(expected_bbox, observed_bbox)
        log_summary = extract_gateway_log_summary(gateway_log_text)
        checks = build_checklist(
            validation_mode=validation_mode,
            plan_result=plan_result,
            snapshot_result=snapshot_result,
            job_start_response=job_start_response,
            blocked_motion_observation=blocked_motion_observation,
            final_job_status=final_job_status,
            log_summary=log_summary,
            axis_coverage=axis_coverage,
            observed_bbox=observed_bbox,
            min_axis_coverage_ratio=args.min_axis_coverage_ratio,
        )

        if overall_status in {"passed", "known_failure"}:
            overall_status, error_from_checks = determine_overall_status(checks, validation_mode)
            if error_from_checks:
                error_message = error_from_checks
                return_code = 1
            elif overall_status == "known_failure":
                return_code = KNOWN_FAILURE_EXIT_CODE
            else:
                return_code = 0

        report = build_report(
            args=args,
            effective_port=effective_port,
            overall_status=overall_status,
            validation_mode=validation_mode,
            job_id=job_id,
            plan_id=plan_id,
            plan_fingerprint=plan_fingerprint,
            snapshot_hash=snapshot_hash,
            steps=steps,
            checks=checks,
            axis_coverage=axis_coverage,
            expected_bbox=expected_bbox,
            observed_bbox=observed_bbox,
            blocked_motion_observation=blocked_motion_observation,
            snapshot_result=snapshot_result,
            final_job_status=final_job_status,
            log_summary=log_summary,
            artifacts={
                **artifacts,
                "gateway_log_path": str(gateway_log_path),
                "gateway_log_copy": str(gateway_log_copy_path),
                "gateway_stdout_log": str(gateway_stdout_path),
                "gateway_stderr_log": str(gateway_stderr_path),
                "job_status_history": str(job_status_json_path),
                "machine_status_history": str(machine_status_json_path),
                "coord_status_history": str(coord_status_json_path),
            },
            error_message=error_message,
            plan_result=plan_result,
            production_baseline=production_baseline,
        )
        write_json(report_json_path, report)
        report_md_path.write_text(build_report_markdown(report), encoding="utf-8")
        print(f"report json: {report_json_path}")
        print(f"report md: {report_md_path}")

    return return_code


if __name__ == "__main__":
    raise SystemExit(main())
