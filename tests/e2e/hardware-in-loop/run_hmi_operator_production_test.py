from __future__ import annotations

import argparse
import json
import os
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

from dxf_hil_observation import DEFAULT_COORD_SYS, collect_job_observation  # noqa: E402
from run_real_dxf_machine_dryrun import parse_int, status_result  # noqa: E402
from run_real_dxf_production_validation import (  # noqa: E402
    add_step,
    build_checklist,
    compute_axis_coverage,
    extract_gateway_log_summary,
    read_current_run_gateway_log,
    summarize_machine_bbox,
    summarize_points_bbox,
    write_json,
)
from runtime_gateway_harness import TcpJsonClient, VENDOR_DIR, resolve_default_exe, wait_gateway_ready  # noqa: E402


DEFAULT_GATEWAY_EXE = resolve_default_exe("siligen_runtime_gateway.exe")
DEFAULT_CONFIG_PATH = ROOT / "config" / "machine" / "machine_config.ini"
DEFAULT_DEMO1_DXF = ROOT / "samples" / "dxf" / "Demo-1.dxf"
DEFAULT_REPORT_ROOT = ROOT / "tests" / "reports" / "online-validation" / "hmi-production-operator-test"
DEFAULT_STATUS_POLL_INTERVAL_SECONDS = 0.20
DEFAULT_GATEWAY_READY_TIMEOUT = 8.0
DEFAULT_CONNECT_TIMEOUT = 15.0
DEFAULT_UI_TIMEOUT_MS = 900000
DEFAULT_MAX_POLYLINE_POINTS = 4000
DEFAULT_MAX_GLUE_POINTS = 5000
DEFAULT_MIN_AXIS_COVERAGE_RATIO = 0.80
REPORT_SCHEMA_VERSION = 1
UI_QTEST = ROOT / "apps" / "hmi-app" / "src" / "hmi_client" / "tools" / "ui_qtest.py"
HMI_SOURCE_ROOT = ROOT / "apps" / "hmi-app" / "src"
HMI_APPLICATION_ROOT = ROOT / "modules" / "hmi-application" / "application"
REQUIRED_OPERATOR_STAGES = (
    "preview-ready",
    "preview-refreshed",
    "production-started",
    "production-running",
    "production-completed",
    "next-job-ready",
)
REQUIRED_SCREENSHOT_STAGES = (
    "production-running",
    "production-completed",
    "next-job-ready",
)
TERMINAL_JOB_STATES = {"completed", "failed", "cancelled"}
TERMINAL_OPERATOR_STAGES = {"production-completed", "next-job-ready"}


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def read_text_if_exists(path: Path) -> str:
    if not path.exists():
        return ""
    return path.read_text(encoding="utf-8")


def finalize_process_log_capture(
    *,
    stdout_stream: Any | None,
    stderr_stream: Any | None,
    stdout_path: Path,
    stderr_path: Path,
) -> tuple[None, None, str, str]:
    if stdout_stream is not None:
        stdout_stream.close()
    if stderr_stream is not None:
        stderr_stream.close()
    return None, None, read_text_if_exists(stdout_path), read_text_if_exists(stderr_path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Drive Demo-1 production through the HMI and capture operator-facing evidence."
    )
    parser.add_argument("--python-exe", default=sys.executable or "python")
    parser.add_argument("--gateway-exe", type=Path, default=DEFAULT_GATEWAY_EXE)
    parser.add_argument("--config-path", type=Path, default=DEFAULT_CONFIG_PATH)
    parser.add_argument("--dxf-file", type=Path, default=DEFAULT_DEMO1_DXF)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--gateway-ready-timeout", type=float, default=DEFAULT_GATEWAY_READY_TIMEOUT)
    parser.add_argument("--connect-timeout", type=float, default=DEFAULT_CONNECT_TIMEOUT)
    parser.add_argument("--status-poll-interval-seconds", type=float, default=DEFAULT_STATUS_POLL_INTERVAL_SECONDS)
    parser.add_argument("--ui-timeout-ms", type=int, default=DEFAULT_UI_TIMEOUT_MS)
    parser.add_argument("--max-polyline-points", type=int, default=DEFAULT_MAX_POLYLINE_POINTS)
    parser.add_argument("--max-glue-points", type=int, default=DEFAULT_MAX_GLUE_POINTS)
    parser.add_argument("--min-axis-coverage-ratio", type=float, default=DEFAULT_MIN_AXIS_COVERAGE_RATIO)
    parser.add_argument("--report-root", type=Path, default=DEFAULT_REPORT_ROOT)
    args = parser.parse_args()
    if args.status_poll_interval_seconds <= 0:
        raise SystemExit("--status-poll-interval-seconds must be > 0")
    if args.ui_timeout_ms <= 0:
        raise SystemExit("--ui-timeout-ms must be > 0")
    if args.max_polyline_points <= 0:
        raise SystemExit("--max-polyline-points must be > 0")
    if args.max_glue_points <= 0:
        raise SystemExit("--max-glue-points must be > 0")
    if args.min_axis_coverage_ratio < 0.0:
        raise SystemExit("--min-axis-coverage-ratio must be >= 0")
    return args


def resolve_listen_port(host: str, requested_port: int) -> int:
    if requested_port > 0:
        return requested_port
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((host, 0))
        return int(sock.getsockname()[1])


def _normalize_existing_path_entries(paths: list[Path]) -> list[str]:
    entries: list[str] = []
    seen: set[str] = set()
    for candidate in paths:
        resolved = candidate.resolve()
        if not resolved.exists():
            continue
        normalized = os.path.normcase(str(resolved))
        if normalized in seen:
            continue
        seen.add(normalized)
        entries.append(str(resolved))
    return entries


def build_gateway_launch_spec_payload(*, args: argparse.Namespace, effective_port: int) -> dict[str, Any]:
    exe_path = args.gateway_exe.resolve()
    config_path = args.config_path.resolve()
    exe_dir = exe_path.parent
    path_candidates = [exe_dir]
    if exe_dir.parent.name.lower() == "bin":
        sibling_lib_dir = exe_dir.parent.parent / "lib" / exe_dir.name
        if sibling_lib_dir.exists():
            path_candidates.append(sibling_lib_dir)
    if VENDOR_DIR.exists():
        path_candidates.append(VENDOR_DIR.resolve())

    env_payload = {
        "SILIGEN_TCP_SERVER_HOST": args.host,
        "SILIGEN_TCP_SERVER_PORT": str(effective_port),
    }
    if VENDOR_DIR.exists():
        env_payload["SILIGEN_MULTICARD_VENDOR_DIR"] = str(VENDOR_DIR.resolve())

    return {
        "executable": str(exe_path),
        "cwd": str(ROOT.resolve()),
        "args": ["--config", str(config_path), "--port", str(effective_port)],
        "pathEntries": _normalize_existing_path_entries(path_candidates),
        "env": env_payload,
    }


def write_gateway_launch_spec(
    *,
    args: argparse.Namespace,
    effective_port: int,
    spec_path: Path,
) -> dict[str, Any]:
    payload = build_gateway_launch_spec_payload(args=args, effective_port=effective_port)
    spec_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return payload


def build_launch_spec_process_env(spec_payload: dict[str, Any]) -> dict[str, str]:
    env = os.environ.copy()
    env_payload = spec_payload.get("env", {})
    if isinstance(env_payload, dict):
        env.update({str(key): str(value) for key, value in env_payload.items()})

    path_entries_raw = spec_payload.get("pathEntries", [])
    path_entries = [str(item).strip() for item in path_entries_raw if str(item).strip()]
    existing = env.get("PATH", "")
    if path_entries:
        env["PATH"] = os.pathsep.join(path_entries + ([existing] if existing else []))
    return env


def build_hmi_process_env(*, gateway_launch_spec_path: Path, host: str, port: int) -> dict[str, str]:
    env = os.environ.copy()
    py_entries = [str(HMI_APPLICATION_ROOT), str(HMI_SOURCE_ROOT)]
    existing = env.get("PYTHONPATH", "")
    env["PYTHONPATH"] = os.pathsep.join(py_entries + ([existing] if existing else []))
    env["SILIGEN_GATEWAY_LAUNCH_SPEC"] = str(gateway_launch_spec_path.resolve())
    env["SILIGEN_GATEWAY_AUTOSTART"] = "1"
    env["SILIGEN_TCP_SERVER_HOST"] = host
    env["SILIGEN_TCP_SERVER_PORT"] = str(port)
    env.pop("SILIGEN_GATEWAY_EXE", None)
    return env


def build_hmi_command(*, args: argparse.Namespace, effective_port: int, screenshot_dir: Path) -> list[str]:
    return [
        args.python_exe,
        "-u",
        str(UI_QTEST),
        "--mode",
        "online",
        "--host",
        args.host,
        "--port",
        str(effective_port),
        "--no-mock",
        "--timeout-ms",
        str(args.ui_timeout_ms),
        "--exercise-runtime-actions",
        "--runtime-action-profile",
        "operator_production",
        "--dxf-browse-path",
        str(args.dxf_file),
        "--screenshot-dir",
        str(screenshot_dir),
    ]


def _parse_key_value_line(*, prefix: str, line: str) -> dict[str, str]:
    if not line.startswith(prefix):
        return {}
    payload = line[len(prefix) :].strip()
    fields: dict[str, str] = {}
    for token in payload.split():
        key, separator, value = token.partition("=")
        if separator:
            fields[key] = value
    return fields


def refresh_operator_exclusive_windows(
    *,
    stdout_path: Path,
    read_offset: int,
    active_windows: set[str],
) -> tuple[int, set[str]]:
    if not stdout_path.exists():
        return read_offset, set(active_windows)

    with stdout_path.open("r", encoding="utf-8") as stream:
        stream.seek(read_offset)
        chunk = stream.read()
        next_offset = stream.tell()

    if not chunk:
        return next_offset, set(active_windows)

    updated = set(active_windows)
    for raw_line in chunk.splitlines():
        event = _parse_key_value_line(prefix="OPERATOR_EXCLUSIVE_WINDOW ", line=raw_line.strip())
        if not event:
            continue
        kind = str(event.get("kind", "") or "").strip()
        state = str(event.get("state", "") or "").strip().lower()
        if not kind:
            continue
        if state in {"started", "entered", "running"}:
            updated.add(kind)
        elif state in {"completed", "failed", "ended", "cleared"}:
            updated.discard(kind)

    return next_offset, updated


def refresh_production_started_job_id(
    *,
    stdout_path: Path,
    read_offset: int,
    current_job_id: str,
) -> tuple[int, str]:
    if not stdout_path.exists():
        return read_offset, current_job_id

    with stdout_path.open("r", encoding="utf-8") as stream:
        stream.seek(read_offset)
        chunk = stream.read()
        next_offset = stream.tell()

    if not chunk:
        return next_offset, current_job_id

    resolved_job_id = current_job_id
    for raw_line in chunk.splitlines():
        event = _parse_key_value_line(prefix="OPERATOR_CONTEXT ", line=raw_line.strip())
        if not event or str(event.get("stage", "")).strip() != "production-started":
            continue

        candidate = str(event.get("job_id", "")).strip()
        if not candidate or candidate.lower() == "null":
            raise RuntimeError("OPERATOR_CONTEXT stage=production-started missing job_id")
        if resolved_job_id and resolved_job_id != candidate:
            raise RuntimeError(
                "OPERATOR_CONTEXT stage=production-started changed job_id from "
                f"{resolved_job_id} to {candidate}"
            )
        resolved_job_id = candidate

    return next_offset, resolved_job_id


def refresh_operator_job_tracking(
    *,
    stdout_path: Path,
    read_offset: int,
    current_job_id: str,
    production_job_id: str,
) -> tuple[int, str, str]:
    if not stdout_path.exists():
        return read_offset, current_job_id, production_job_id

    with stdout_path.open("r", encoding="utf-8") as stream:
        stream.seek(read_offset)
        chunk = stream.read()
        next_offset = stream.tell()

    if not chunk:
        return next_offset, current_job_id, production_job_id

    resolved_current_job_id = current_job_id
    resolved_production_job_id = production_job_id
    for raw_line in chunk.splitlines():
        event = _parse_key_value_line(prefix="OPERATOR_CONTEXT ", line=raw_line.strip())
        if not event:
            continue

        stage = str(event.get("stage", "")).strip()
        candidate = str(event.get("job_id", "")).strip()
        normalized_candidate = "" if not candidate or candidate.lower() == "null" else candidate

        if stage == "production-started":
            if not normalized_candidate:
                raise RuntimeError("OPERATOR_CONTEXT stage=production-started missing job_id")
            if resolved_current_job_id and resolved_current_job_id != normalized_candidate:
                raise RuntimeError(
                    "OPERATOR_CONTEXT stage=production-started changed active job_id from "
                    f"{resolved_current_job_id} to {normalized_candidate}"
                )
            if resolved_production_job_id and resolved_production_job_id != normalized_candidate:
                raise RuntimeError(
                    "OPERATOR_CONTEXT stage=production-started changed production job_id from "
                    f"{resolved_production_job_id} to {normalized_candidate}"
                )
            resolved_current_job_id = normalized_candidate
            resolved_production_job_id = normalized_candidate
            continue

        if stage in TERMINAL_OPERATOR_STAGES:
            if normalized_candidate:
                raise RuntimeError(f"OPERATOR_CONTEXT stage={stage} expected null job_id after terminal completion")
            resolved_current_job_id = ""

    return next_offset, resolved_current_job_id, resolved_production_job_id


def is_terminal_job_state(state: Any) -> bool:
    return str(state or "").strip().lower() in TERMINAL_JOB_STATES


def _contains_stage_sequence(stages: list[str], required_stages: tuple[str, ...]) -> bool:
    cursor = 0
    for stage in stages:
        if cursor >= len(required_stages):
            break
        if stage == required_stages[cursor]:
            cursor += 1
    return cursor == len(required_stages)


def _latest_field(events: list[dict[str, str]], field: str, *, default: str = "null") -> str:
    for event in reversed(events):
        value = str(event.get(field, "")).strip()
        if value and value.lower() != "null":
            return value
    return default


def summarize_operator_output(output: str) -> dict[str, Any]:
    events = [
        _parse_key_value_line(prefix="OPERATOR_CONTEXT ", line=line.strip())
        for line in output.splitlines()
    ]
    events = [event for event in events if event]
    screenshots = [
        _parse_key_value_line(prefix="HMI_SCREENSHOT ", line=line.strip())
        for line in output.splitlines()
    ]
    screenshots = [event for event in screenshots if event]
    stages = [str(event.get("stage", "")).strip() for event in events if str(event.get("stage", "")).strip()]
    failure_stages = [stage for stage in stages if stage.endswith("-failed")]
    by_stage: dict[str, dict[str, str]] = {}
    for event in events:
        stage = str(event.get("stage", "")).strip()
        if stage:
            by_stage[stage] = event

    screenshots_by_stage: dict[str, str] = {}
    unscoped_screenshots: list[str] = []
    for event in screenshots:
        stage = str(event.get("stage", "")).strip()
        path = str(event.get("path", "")).strip()
        if not path:
            continue
        if stage:
            screenshots_by_stage[stage] = path
        else:
            unscoped_screenshots.append(path)

    preview_ready = by_stage.get("preview-ready", {})
    preview_refreshed = by_stage.get("preview-refreshed", {})
    production_started = by_stage.get("production-started", {})
    production_running = by_stage.get("production-running", {})
    production_completed = by_stage.get("production-completed", {})
    next_job_ready = by_stage.get("next-job-ready", {})

    missing_screenshot_stages = [
        stage for stage in REQUIRED_SCREENSHOT_STAGES if stage not in screenshots_by_stage
    ]
    stage_sequence_ok = _contains_stage_sequence(stages, REQUIRED_OPERATOR_STAGES)
    preview_source = str(
        preview_refreshed.get("preview_source")
        or preview_ready.get("preview_source")
        or production_started.get("preview_source")
        or _latest_field(events, "preview_source")
    )
    preview_kind = str(
        preview_refreshed.get("preview_kind")
        or preview_ready.get("preview_kind")
        or production_started.get("preview_kind")
        or _latest_field(events, "preview_kind")
    )
    glue_point_count = int(
        parse_int(
            preview_refreshed.get("glue_point_count")
            or preview_ready.get("glue_point_count")
            or production_started.get("glue_point_count")
            or _latest_field(events, "glue_point_count", default="0")
        )
        or 0
    )
    snapshot_hash = str(
        preview_refreshed.get("snapshot_hash")
        or preview_ready.get("snapshot_hash")
        or _latest_field(events, "snapshot_hash")
    )
    confirmed_snapshot_hash = str(
        production_started.get("confirmed_snapshot_hash")
        or production_running.get("confirmed_snapshot_hash")
        or production_completed.get("confirmed_snapshot_hash")
        or _latest_field(events, "confirmed_snapshot_hash")
    )
    preview_confirmed = any(
        str(event.get("preview_confirmed", "")).strip().lower() == "true"
        for event in (production_started, production_running, production_completed, next_job_ready)
        if event
    )
    production_started_job_id = str(production_started.get("job_id") or "null")
    plan_fingerprint = _latest_field(events, "plan_fingerprint")
    preview_gate_state = _latest_field(events, "preview_gate_state")
    preview_gate_error = _latest_field(events, "preview_gate_error")

    contract_ok = (
        stage_sequence_ok
        and not failure_stages
        and preview_source == "planned_glue_snapshot"
        and preview_kind == "glue_points"
        and glue_point_count > 0
        and snapshot_hash not in {"", "null"}
        and confirmed_snapshot_hash not in {"", "null"}
        and production_started_job_id not in {"", "null"}
        and preview_confirmed
        and not missing_screenshot_stages
    )
    contract_error = ""
    if not contract_ok:
        contract_error = (
            "operator production contract missing required stage sequence, preview authority evidence, "
            "or staged screenshots; "
            f"observed_stages={stages}; "
            f"failure_stages={failure_stages}; "
            f"missing_screenshots={missing_screenshot_stages}; "
            f"preview_gate_state={preview_gate_state}; "
            f"preview_gate_error={preview_gate_error}"
        )

    return {
        "operator_context_stages": stages,
        "failure_stages": failure_stages,
        "stage_sequence_ok": stage_sequence_ok,
        "artifact_id": str(
            preview_refreshed.get("artifact_id")
            or preview_ready.get("artifact_id")
            or production_started.get("artifact_id")
            or "null"
        ),
        "plan_id": str(
            preview_refreshed.get("plan_id")
            or preview_ready.get("plan_id")
            or production_started.get("plan_id")
            or "null"
        ),
        "plan_fingerprint": plan_fingerprint,
        "preview_source": preview_source,
        "preview_kind": preview_kind,
        "glue_point_count": glue_point_count,
        "preview_gate_state": preview_gate_state,
        "preview_gate_error": preview_gate_error,
        "snapshot_hash": snapshot_hash,
        "confirmed_snapshot_hash": confirmed_snapshot_hash,
        "snapshot_ready": str(preview_refreshed.get("snapshot_ready") or preview_ready.get("snapshot_ready") or "false")
        == "true",
        "preview_confirmed": preview_confirmed,
        "job_id": production_started_job_id,
        "target_count": int(
            parse_int(
                production_started.get("target_count")
                or production_running.get("target_count")
                or production_completed.get("target_count")
                or next_job_ready.get("target_count")
            )
            or 0
        ),
        "completed_count_label": str(
            next_job_ready.get("completed_count")
            or production_completed.get("completed_count")
            or production_running.get("completed_count")
            or "0/0"
        ),
        "current_operation": str(
            next_job_ready.get("current_operation")
            or production_completed.get("current_operation")
            or production_running.get("current_operation")
            or "null"
        ),
        "global_progress_percent": int(
            parse_int(
                next_job_ready.get("global_progress_percent")
                or production_completed.get("global_progress_percent")
                or production_running.get("global_progress_percent")
            )
            or 0
        ),
        "screenshots_by_stage": screenshots_by_stage,
        "unscoped_screenshots": unscoped_screenshots,
        "missing_screenshot_stages": missing_screenshot_stages,
        "contract_ok": contract_ok,
        "contract_error": contract_error,
    }


def collect_observer_sample(
    client: TcpJsonClient,
    *,
    poll_index: int,
    job_id: str,
    timeout_seconds: float = 2.0,
) -> dict[str, Any]:
    if not str(job_id or "").strip():
        raise ValueError("collect_observer_sample requires non-empty job_id")
    return collect_job_observation(
        client,
        job_id=job_id,
        poll_index=poll_index,
        coord_sys=DEFAULT_COORD_SYS,
        timeout_seconds=timeout_seconds,
    )


def read_preview_snapshot(
    client: TcpJsonClient,
    *,
    plan_id: str,
    max_polyline_points: int,
    max_glue_points: int,
) -> dict[str, Any]:
    response = client.send_request(
        "dxf.preview.snapshot",
        {
            "plan_id": plan_id,
            "max_polyline_points": max_polyline_points,
            "max_glue_points": max_glue_points,
        },
        timeout_seconds=15.0,
    )
    if "error" in response:
        raise RuntimeError("dxf.preview.snapshot readback failed: " + json.dumps(response, ensure_ascii=True))
    return status_result(response)


def build_operator_issue(
    *,
    issue_type: str,
    phenomenon: str,
    impact: str,
    reproduction_steps: str,
    current_evidence: str,
    owner_layer: str,
) -> dict[str, str]:
    return {
        "type": issue_type,
        "phenomenon": phenomenon,
        "impact": impact,
        "reproduction_steps": reproduction_steps,
        "current_evidence": current_evidence,
        "owner_layer": owner_layer,
    }


def build_observer_poll_failure(
    *,
    phase: str,
    poll_index: int,
    job_id: str,
    exc: Exception,
) -> dict[str, Any]:
    failure: dict[str, Any] = {
        "phase": phase,
        "poll_index": poll_index,
        "job_id": str(job_id or "").strip(),
        "message": str(exc).strip(),
    }
    method = str(getattr(exc, "method", "") or "").strip()
    request_id = str(getattr(exc, "request_id", "") or "").strip()
    elapsed_ms = getattr(exc, "elapsed_ms", None)
    if method:
        failure["method"] = method
    if request_id:
        failure["request_id"] = request_id
    if isinstance(elapsed_ms, (int, float)):
        failure["elapsed_ms"] = float(elapsed_ms)
    return failure


def append_observer_poll_error(
    observer_poll_errors: list[str],
    *,
    message: str,
) -> None:
    normalized_message = str(message).strip()
    if normalized_message and (not observer_poll_errors or observer_poll_errors[-1] != normalized_message):
        observer_poll_errors.append(normalized_message)


def record_observer_poll_failure(
    observer_poll_failures: list[dict[str, Any]],
    observer_poll_errors: list[str],
    *,
    phase: str,
    poll_index: int,
    job_id: str,
    exc: Exception,
) -> dict[str, Any]:
    failure = build_observer_poll_failure(
        phase=phase,
        poll_index=poll_index,
        job_id=job_id,
        exc=exc,
    )
    observer_poll_failures.append(failure)
    append_observer_poll_error(observer_poll_errors, message=str(failure["message"]))
    return failure


def collect_terminal_job_readback(
    client: TcpJsonClient,
    *,
    poll_index: int,
    job_id: str,
    final_job_status: dict[str, Any],
    timeout_seconds: float = 2.0,
) -> dict[str, Any] | None:
    normalized_job_id = str(job_id or "").strip()
    if not normalized_job_id:
        return None
    if is_terminal_job_state(final_job_status.get("state")):
        return None
    return collect_observer_sample(
        client,
        poll_index=poll_index,
        job_id=normalized_job_id,
        timeout_seconds=timeout_seconds,
    )


def finalize_terminal_job_readback(
    client: TcpJsonClient,
    *,
    steps: list[dict[str, str]],
    machine_status_history: list[dict[str, Any]],
    coord_status_history: list[dict[str, Any]],
    job_status_history: list[dict[str, Any]],
    observer_poll_failures: list[dict[str, Any]],
    observer_poll_errors: list[str],
    poll_index: int,
    job_id: str,
    final_job_status: dict[str, Any],
    timeout_seconds: float = 2.0,
) -> tuple[int, dict[str, Any], bool]:
    normalized_job_id = str(job_id or "").strip()
    if not normalized_job_id:
        return poll_index, final_job_status, False

    try:
        terminal_readback = collect_terminal_job_readback(
            client,
            poll_index=poll_index,
            job_id=normalized_job_id,
            final_job_status=final_job_status,
            timeout_seconds=timeout_seconds,
        )
    except Exception as exc:  # noqa: BLE001
        failure = record_observer_poll_failure(
            observer_poll_failures,
            observer_poll_errors,
            phase="terminal-readback",
            poll_index=poll_index,
            job_id=normalized_job_id,
            exc=exc,
        )
        add_step(
            steps,
            "job-terminal-readback",
            "failed",
            f"job_id={normalized_job_id} {failure['message']}",
        )
        return poll_index, final_job_status, False

    if terminal_readback is not None:
        machine_status_history.append(terminal_readback["machine_status"])
        if terminal_readback["coord_status"]:
            coord_status_history.append(terminal_readback["coord_status"])
        updated_final_job_status = final_job_status
        if terminal_readback["job_status"]:
            job_status_history.append(terminal_readback["job_status"])
            updated_final_job_status = terminal_readback["job_status"]
        add_step(
            steps,
            "job-terminal-readback",
            "passed",
            f"job_id={normalized_job_id} state={updated_final_job_status.get('state', '')}",
        )
        return poll_index + 1, updated_final_job_status, is_terminal_job_state(updated_final_job_status.get("state"))

    final_state = str(final_job_status.get("state", "") or "").strip().lower()
    if final_state:
        add_step(steps, "job-terminal-readback", "passed", f"skipped final_state={final_state}")
        return poll_index, final_job_status, is_terminal_job_state(final_state)

    add_step(
        steps,
        "job-terminal-readback",
        "failed",
        f"job_id={normalized_job_id} missing final job state after hmi exit",
    )
    return poll_index, final_job_status, False


def evaluate_operator_execution(
    *,
    operator_summary: dict[str, Any],
    final_job_status: dict[str, Any],
    hmi_exit_code: int,
    combined_output: str,
) -> dict[str, Any]:
    issues: list[dict[str, str]] = []
    if hmi_exit_code != 0:
        issues.append(
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon=f"HMI 自动化退出码为 {hmi_exit_code}",
                impact="操作主线未被正式入口完整覆盖或执行中断。",
                reproduction_steps="运行 run_hmi_operator_production_test.py 并观察 hmi-stdout.log / hmi-stderr.log。",
                current_evidence=f"hmi_exit_code={hmi_exit_code}",
                owner_layer="apps/hmi-app/src/hmi_client/tools/ui_qtest.py",
            )
        )
    if not bool(operator_summary.get("stage_sequence_ok", False)):
        issues.append(
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon="operator_production 缺少完整阶段序列。",
                impact="无法证明 HMI 正式入口已覆盖 browse -> preview -> start -> completed -> next-job-ready。",
                reproduction_steps="检查 OPERATOR_CONTEXT 阶段序列。",
                current_evidence=json.dumps(operator_summary.get("operator_context_stages", []), ensure_ascii=False),
                owner_layer="apps/hmi-app/src/hmi_client/tools/ui_qtest.py",
            )
        )
    if operator_summary.get("failure_stages"):
        issues.append(
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon="operator_production 进入 fail-closed 失败阶段。",
                impact="正式链路在 HMI 阶段已中止，不能把本轮结果视为成功生产。",
                reproduction_steps="检查 *-failed 阶段、preview_gate_state/preview_gate_error 与失败截图。",
                current_evidence=json.dumps(
                    {
                        "failure_stages": operator_summary.get("failure_stages", []),
                        "preview_gate_state": operator_summary.get("preview_gate_state", "null"),
                        "preview_gate_error": operator_summary.get("preview_gate_error", "null"),
                        "plan_fingerprint": operator_summary.get("plan_fingerprint", "null"),
                    },
                    ensure_ascii=False,
                ),
                owner_layer="apps/hmi-app/src/hmi_client/tools/ui_qtest.py",
            )
        )
    if str(operator_summary.get("preview_source", "")).strip() != "planned_glue_snapshot":
        issues.append(
            build_operator_issue(
                issue_type="误导类",
                phenomenon="预览 authority 不是 planned_glue_snapshot。",
                impact="正式生产前的 HMI 预览真值不可信。",
                reproduction_steps="检查 preview-ready / preview-refreshed 的 OPERATOR_CONTEXT。",
                current_evidence=f"preview_source={operator_summary.get('preview_source', 'null')}",
                owner_layer="apps/hmi-app/src/hmi_client/ui/main_window.py",
            )
        )
    if str(operator_summary.get("preview_kind", "")).strip() != "glue_points":
        issues.append(
            build_operator_issue(
                issue_type="误导类",
                phenomenon="预览 kind 不是 glue_points。",
                impact="生产前界面与 runtime-owned authority 不一致。",
                reproduction_steps="检查 preview-refreshed 的 OPERATOR_CONTEXT。",
                current_evidence=f"preview_kind={operator_summary.get('preview_kind', 'null')}",
                owner_layer="apps/hmi-app/src/hmi_client/ui/main_window.py",
            )
        )
    if not bool(operator_summary.get("preview_confirmed", False)):
        issues.append(
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon="生产开始前未观察到 preview.confirm 的已确认状态。",
                impact="不能证明本轮生产仍遵守 HMI 正式预检与确认语义。",
                reproduction_steps="检查 production-started / production-running 的 OPERATOR_CONTEXT。",
                current_evidence=f"confirmed_snapshot_hash={operator_summary.get('confirmed_snapshot_hash', 'null')}",
                owner_layer="apps/hmi-app/src/hmi_client/ui/main_window.py",
            )
        )
    final_state = str(final_job_status.get("state", "")).strip().lower()
    if final_state != "completed":
        issues.append(
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon="observer 未观察到 completed 终态。",
                impact="不能证明 HMI 发起的正式生产实际完成。",
                reproduction_steps="检查 job-status-history.json 与 hmi-stdout.log。",
                current_evidence=f"final_job_status.state={final_state or 'null'}",
                owner_layer="tests/e2e/hardware-in-loop/run_hmi_operator_production_test.py",
            )
        )
    target_count = int(parse_int(final_job_status.get("target_count")) or 0)
    completed_count = int(parse_int(final_job_status.get("completed_count")) or 0)
    if final_state == "completed" and (target_count <= 0 or completed_count != target_count):
        issues.append(
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon="completed_count 与 target_count 不一致。",
                impact="生产完成结论与 batch 统计不一致。",
                reproduction_steps="检查 final_job_status 与 HMI completed label。",
                current_evidence=json.dumps(
                    {
                        "completed_count": completed_count,
                        "target_count": target_count,
                        "completed_count_label": operator_summary.get("completed_count_label", "0/0"),
                    },
                    ensure_ascii=False,
                ),
                owner_layer="apps/hmi-app/src/hmi_client/ui/main_window.py",
            )
        )
    if "next-job-ready" not in tuple(operator_summary.get("operator_context_stages", ())):
        issues.append(
            build_operator_issue(
                issue_type="恢复类",
                phenomenon="完成后未观察到 next-job-ready 阶段。",
                impact="不能证明界面已恢复到下一单可发起状态。",
                reproduction_steps="检查 OPERATOR_CONTEXT 与 staged screenshots。",
                current_evidence=json.dumps(operator_summary.get("operator_context_stages", []), ensure_ascii=False),
                owner_layer="apps/hmi-app/src/hmi_client/tools/ui_qtest.py",
            )
        )
    if str(operator_summary.get("contract_error", "")).strip():
        issues.append(
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon="operator_production 合同不完整。",
                impact="本轮 HMI 正式入口证据不收敛。",
                reproduction_steps="检查 OPERATOR_CONTEXT / HMI_SCREENSHOT 输出。",
                current_evidence=str(operator_summary.get("contract_error", "")).strip(),
                owner_layer="apps/hmi-app/src/hmi_client/tools/ui_qtest.py",
            )
        )

    return {
        "status": "passed" if not issues else "failed",
        "issues": issues,
        "hmi_exit_code": hmi_exit_code,
        "final_job_status": final_job_status,
        "operator_context": operator_summary,
        "hmi_output_excerpt": "\n".join(
            line
            for line in combined_output.splitlines()
            if line.strip().startswith(("FAIL:", "OPERATOR_CONTEXT ", "HMI_SCREENSHOT ", "OPERATOR_EXCLUSIVE_WINDOW "))
        )[:4000],
    }


def evaluate_traceability_correspondence(
    *,
    job_id: str,
    job_traceability: dict[str, Any],
) -> dict[str, Any]:
    verdict = str(job_traceability.get("verdict", "failed") or "failed").strip().lower()
    if verdict not in {"passed", "failed", "insufficient_evidence"}:
        return build_traceability_query_failure(
            job_id=job_id,
            reason=f"runtime dxf.job.traceability returned unsupported verdict={verdict or 'null'}",
        )
    strict_one_to_one_proven = bool(job_traceability.get("strict_one_to_one_proven", False))
    verdict_reason = str(job_traceability.get("verdict_reason", "") or "").strip()
    expected_trace = job_traceability.get("expected_trace", [])
    actual_trace = job_traceability.get("actual_trace", [])
    mismatches = job_traceability.get("mismatches", [])
    terminal_state = str(job_traceability.get("terminal_state", "") or "").strip().lower()

    if verdict == "passed" and not strict_one_to_one_proven:
        return build_traceability_query_failure(
            job_id=job_id,
            reason="runtime dxf.job.traceability violated contract: passed requires strict_one_to_one_proven=true",
        )
    if verdict in {"failed", "insufficient_evidence"} and strict_one_to_one_proven:
        return build_traceability_query_failure(
            job_id=job_id,
            reason=(
                "runtime dxf.job.traceability violated contract: "
                "strict_one_to_one_proven=true is only allowed when verdict=passed"
            ),
        )

    reason = verdict_reason
    if not reason:
        if verdict == "passed":
            reason = "runtime dxf.job.traceability returned passed and strict_one_to_one_proven=true"
        elif verdict == "insufficient_evidence":
            if terminal_state:
                reason = f"runtime dxf.job.traceability returned insufficient_evidence in terminal_state={terminal_state}"
            else:
                reason = "runtime dxf.job.traceability returned insufficient_evidence"
        elif mismatches:
            reason = "runtime dxf.job.traceability returned failed with mismatches"
        elif terminal_state:
            reason = f"runtime dxf.job.traceability returned failed in terminal_state={terminal_state}"
        else:
            reason = "runtime dxf.job.traceability returned failed"

    return {
        "status": verdict,
        "reason": reason,
        "strict_one_to_one_proven": strict_one_to_one_proven,
        "job_id": job_id,
        "terminal_state": terminal_state,
        "expected_trace_count": len(expected_trace) if isinstance(expected_trace, list) else 0,
        "actual_trace_count": len(actual_trace) if isinstance(actual_trace, list) else 0,
        "mismatch_count": len(mismatches) if isinstance(mismatches, list) else 0,
        "traceability_payload": job_traceability,
    }


def build_traceability_query_failure(*, job_id: str, reason: str) -> dict[str, Any]:
    return {
        "status": "failed",
        "reason": reason,
        "strict_one_to_one_proven": False,
        "job_id": job_id,
        "terminal_state": "unknown",
        "expected_trace_count": 0,
        "actual_trace_count": 0,
        "mismatch_count": 0,
        "traceability_payload": {},
    }


def evaluate_observation_alignment(
    *,
    snapshot_result: dict[str, Any],
    final_job_status: dict[str, Any],
    log_summary: dict[str, Any],
    machine_status_history: list[dict[str, Any]],
    coord_status_history: list[dict[str, Any]],
    min_axis_coverage_ratio: float,
) -> dict[str, Any]:
    glue_points = snapshot_result.get("glue_points", [])
    if not isinstance(glue_points, list) or not glue_points:
        return {
            "status": "insufficient_evidence",
            "reason": "preview snapshot glue_points unavailable; cannot compute summary-level alignment",
            "summary_checks": {},
            "coord_status_sample_count": len(coord_status_history),
            "expected_bbox": {},
            "observed_bbox": {},
            "axis_coverage": {},
        }
    if not machine_status_history:
        return {
            "status": "insufficient_evidence",
            "reason": "machine status history unavailable; cannot compute summary-level alignment",
            "summary_checks": {},
            "coord_status_sample_count": len(coord_status_history),
            "expected_bbox": summarize_points_bbox(glue_points),
            "observed_bbox": {},
            "axis_coverage": {},
        }

    expected_bbox = summarize_points_bbox(glue_points)
    observed_bbox = summarize_machine_bbox(machine_status_history)
    axis_coverage = compute_axis_coverage(expected_bbox, observed_bbox)
    summary_checks = build_checklist(
        validation_mode="production_execution",
        plan_result={},
        snapshot_result=snapshot_result,
        job_start_response={},
        blocked_motion_observation={},
        final_job_status=final_job_status,
        log_summary=log_summary,
        axis_coverage=axis_coverage,
        observed_bbox=observed_bbox,
        min_axis_coverage_ratio=min_axis_coverage_ratio,
    )
    failed_checks = [
        name for name, payload in summary_checks.items() if not bool(payload.get("passed", False))
    ]
    if failed_checks:
        status = "failed"
        reason = "summary-level alignment checks failed: " + ", ".join(failed_checks)
    else:
        status = "passed"
        reason = "summary-level alignment checks passed"

    return {
        "status": status,
        "reason": reason,
        "summary_checks": summary_checks,
        "coord_status_sample_count": len(coord_status_history),
        "expected_bbox": expected_bbox,
        "observed_bbox": observed_bbox,
        "axis_coverage": axis_coverage,
    }


def evaluate_observation_integrity(
    *,
    combined_output: str,
    observer_poll_errors: list[str],
    observer_poll_failures: list[dict[str, Any]],
    job_status_history: list[dict[str, Any]],
    machine_status_history: list[dict[str, Any]],
    coord_status_history: list[dict[str, Any]],
    snapshot_result: dict[str, Any],
    job_traceability: dict[str, Any],
) -> dict[str, Any]:
    issues: list[str] = []
    warnings: list[str] = []
    supervisor_lines = [
        line.strip()
        for line in combined_output.splitlines()
        if line.strip().startswith("SUPERVISOR_EVENT ")
    ]
    recoverable_stage_failures = [
        line for line in supervisor_lines if "type=stage_failed" in line and "recoverable=true" in line
    ]
    unrecovered_stage_failures = [
        line for line in supervisor_lines if "type=stage_failed" in line and "recoverable=true" not in line
    ]
    recovered_online_ready = any(
        "type=stage_succeeded" in line and "stage=online_ready" in line for line in supervisor_lines
    )

    if not job_traceability:
        issues.append("job traceability payload missing")
    if not job_status_history:
        issues.append("job status history missing")
    if not machine_status_history:
        issues.append("machine status history missing")
    if unrecovered_stage_failures:
        issues.append("supervisor reported unrecoverable stage_failed event")
    if recoverable_stage_failures and not recovered_online_ready:
        issues.append("supervisor reported recoverable stage_failed without online_ready recovery")

    if recoverable_stage_failures and recovered_online_ready:
        warnings.append("supervisor reported recoverable stage_failed before online_ready recovery")
    if observer_poll_failures:
        warnings.append("observer polling reported transient errors")
    if not coord_status_history:
        warnings.append("coord status history is empty")
    if not snapshot_result:
        warnings.append("preview snapshot readback unavailable")

    if issues:
        status = "failed"
        reason = "; ".join(issues)
    elif warnings:
        status = "warning"
        reason = "; ".join(warnings)
    else:
        status = "passed"
        reason = "observation artifacts and supervisor timeline are complete"

    return {
        "status": status,
        "reason": reason,
        "issues": issues,
        "warnings": warnings,
        "job_status_sample_count": len(job_status_history),
        "machine_status_sample_count": len(machine_status_history),
        "coord_status_sample_count": len(coord_status_history),
        "recoverable_stage_failure_count": len(recoverable_stage_failures),
        "recovered_online_ready": recovered_online_ready,
        "observer_poll_error_count": len(observer_poll_failures),
    }


def determine_overall_status(
    *,
    operator_execution: dict[str, Any],
    traceability: dict[str, Any],
    observation_integrity: dict[str, Any],
) -> str:
    if operator_execution.get("status") != "passed":
        return "failed"
    if traceability.get("status") != "passed":
        return "failed"
    if observation_integrity.get("status") != "passed":
        return "failed"
    return "passed"


def build_report(
    *,
    args: argparse.Namespace,
    effective_port: int,
    started_at: str,
    finished_at: str,
    duration_seconds: float,
    overall_status: str,
    steps: list[dict[str, str]],
    operator_execution: dict[str, Any],
    traceability: dict[str, Any],
    observation_alignment: dict[str, Any],
    observation_integrity: dict[str, Any],
    artifacts: dict[str, Any],
    snapshot_result: dict[str, Any],
    log_summary: dict[str, Any],
    observer_poll_errors: list[str],
    observer_poll_failures: list[dict[str, Any]],
    error_message: str,
    hmi_command: list[str],
) -> dict[str, Any]:
    stages = tuple(operator_execution.get("operator_context", {}).get("operator_context_stages", ()))
    control_script_allowed = "production-started" in stages
    control_script_gaps: list[str] = []
    if not control_script_allowed:
        control_script_gaps.append("未观察到 production-started 阶段")
    if "next-job-ready" not in stages:
        control_script_gaps.append("未观察到 next-job-ready 阶段")

    return {
        "schema_version": REPORT_SCHEMA_VERSION,
        "started_at": started_at,
        "finished_at": finished_at,
        "duration_seconds": duration_seconds,
        "overall_status": overall_status,
        "test_goal": [
            f"通过 HMI 主链路执行 {args.dxf_file.name} 的真实生产。",
            "输出操作问题清单，并把生产可执行结论与追溯严格一一对应结论分开。",
        ],
        "scope": {
            "dxf_file": str(args.dxf_file),
            "hmi_entry": str(UI_QTEST),
            "gateway_exe": str(args.gateway_exe),
            "config_path": str(args.config_path),
            "gateway_host": args.host,
            "gateway_port": effective_port,
            "bypass_hmi_allowed": False,
        },
        "operator_execution": operator_execution,
        "traceability_correspondence": traceability,
        "observation_alignment": observation_alignment,
        "observation_integrity": observation_integrity,
        "control_script_capability": {
            "entry_script": str(UI_QTEST),
            "covers_production_start": control_script_allowed,
            "status": "allow" if control_script_allowed else "block",
            "gaps": control_script_gaps,
            "command": hmi_command,
        },
        "steps": steps,
        "artifacts": artifacts,
        "snapshot_result": snapshot_result,
        "log_summary": log_summary,
        "observer_poll_errors": observer_poll_errors,
        "observer_poll_failures": observer_poll_failures,
        "error": error_message,
    }


def format_runtime_evidence(artifacts: dict[str, Any]) -> str:
    runtime_stdout_log = str(artifacts.get("runtime_stdout_log", "") or "").strip()
    runtime_stderr_log = str(artifacts.get("runtime_stderr_log", "") or "").strip()
    runtime_log_note = str(artifacts.get("runtime_log_note", "") or "").strip()

    runtime_paths = [path for path in (runtime_stdout_log, runtime_stderr_log) if path]
    if runtime_paths:
        return " / ".join(f"`{path}`" for path in runtime_paths)
    if runtime_log_note:
        return runtime_log_note
    return "当前 runner 未采集独立 runtime 进程日志。"


def build_report_markdown(report: dict[str, Any]) -> str:
    scope = report.get("scope", {})
    operator_execution = report.get("operator_execution", {})
    traceability = report.get("traceability_correspondence", {})
    observation_alignment = report.get("observation_alignment", {})
    observation_integrity = report.get("observation_integrity", {})
    control_script_capability = report.get("control_script_capability", {})
    artifacts = report.get("artifacts", {})
    issues = operator_execution.get("issues", [])
    runtime_evidence = format_runtime_evidence(artifacts)

    lines = [
        "# HMI 生产操作测试单",
        "",
        "### 1. 测试目标",
    ]
    for item in report.get("test_goal", []):
        lines.append(f"- {item}")

    lines.extend(
        [
            "",
            "### 2. 操作范围",
            f"- DXF / 生产对象：`{scope.get('dxf_file', '')}`",
            f"- HMI 入口：`{scope.get('hmi_entry', '')}`",
            "- 是否允许绕过 HMI：不允许",
            f"- 测试开始：`{report.get('started_at', '')}`",
            f"- 测试结束：`{report.get('finished_at', '')}`",
            f"- 总耗时（秒）：`{report.get('duration_seconds', 0.0)}`",
            "",
            "### 3. 操作主线",
            "1. 启动 runtime gateway 与 HMI qtest 正式入口。",
            "2. 通过 HMI browse DXF，等待 preview-ready 与 preview-refreshed。",
            "3. 触发生产开始，观察运行中状态与 staged screenshots。",
            "4. 观察 completed 终态与 next-job-ready 恢复状态。",
            "5. 同步冻结 gateway 日志、job/machine/coord 历史与最终报告。",
            "",
            "### 4. 操作问题清单",
        ]
    )
    if issues:
        for issue in issues:
            lines.extend(
                [
                    f"- 类型：`{issue['type']}`",
                    f"  现象：{issue['phenomenon']}",
                    f"  影响：{issue['impact']}",
                    f"  复现步骤：{issue['reproduction_steps']}",
                    f"  当前证据：{issue['current_evidence']}",
                    f"  建议归属层：{issue['owner_layer']}",
                ]
            )
    else:
        lines.append("- 无新增阻塞/误导/恢复问题；operator_production 正式入口已完成覆盖。")

    lines.extend(
        [
            "",
            "### 5. 追溯一致性结论",
            f"- 生产执行结论：`{operator_execution.get('status', 'failed')}`",
            f"- 严格一一对应结论：`{traceability.get('status', 'failed')}`",
            f"- strict truth 来源：`dxf.job.traceability`",
            f"- 证据边界：{traceability.get('reason', '')}",
            f"- summary-level alignment：`{observation_alignment.get('status', 'insufficient_evidence')}`",
            f"- observation integrity：`{observation_integrity.get('status', 'failed')}`",
            "",
            "### 6. 控制脚本能力结论",
            f"- 入口脚本：`{control_script_capability.get('entry_script', '')}`",
            f"- 是否覆盖 production start：`{str(bool(control_script_capability.get('covers_production_start', False))).lower()}`",
            f"- 结论：`{'允许' if control_script_capability.get('status') == 'allow' else '阻断'}`",
        ]
    )
    if traceability:
        lines.append(
            "- formal traceability："
            f" expected=`{traceability.get('expected_trace_count', 0)}`"
            f" actual=`{traceability.get('actual_trace_count', 0)}`"
            f" mismatches=`{traceability.get('mismatch_count', 0)}`"
            f" terminal_state=`{traceability.get('terminal_state', '')}`"
        )
    gaps = control_script_capability.get("gaps", [])
    if gaps:
        lines.append(f"- 若阻断，缺口：`{json.dumps(gaps, ensure_ascii=False)}`")

    lines.extend(
        [
            "",
            "### 7. 关键证据",
            f"- HMI：`{artifacts.get('hmi_stdout_log', '')}`",
            f"- runtime：{runtime_evidence}",
            f"- gateway stdout：`{artifacts.get('gateway_stdout_log', '')}`",
            f"- gateway stderr：`{artifacts.get('gateway_stderr_log', '')}`",
            f"- gateway tcp log：`{artifacts.get('gateway_log_copy', '')}`",
            f"- gateway launch spec：`{artifacts.get('gateway_launch_spec', '')}`",
            f"- dxf.job.traceability：`{artifacts.get('job_traceability', '')}`",
            f"- report：`{artifacts.get('report_json', '')}` / `{artifacts.get('report_markdown', '')}`",
            f"- coord-status-history：`{artifacts.get('coord_status_history', '')}`",
            f"- staged screenshots：`{artifacts.get('hmi_screenshot_dir', '')}`",
            "",
            "### 8. 中止条件与风险",
        ]
    )
    if operator_execution.get("status") != "passed":
        lines.append("- HMI operator 正式入口仍有阻塞，不能把本轮结论升级为“人工操作可稳定完成生产”。")
    if traceability.get("status") == "failed":
        lines.append("- runtime strict traceability 已明确失败，不能把生产成功升级为严格一一对应。")
    if traceability.get("status") == "insufficient_evidence":
        lines.append("- runtime strict traceability 仍未证明，当前 run 不能按严格一一对应收尾。")
    if observation_integrity.get("status") == "failed":
        lines.append(f"- observation integrity 已失败：`{observation_integrity.get('reason', '')}`")
    elif observation_integrity.get("status") == "warning":
        lines.append(f"- observation integrity 有告警：`{observation_integrity.get('reason', '')}`")
    if report.get("observer_poll_errors"):
        lines.append(f"- observer polling 存在波动：`{json.dumps(report['observer_poll_errors'][:3], ensure_ascii=False)}`")

    lines.extend(["", "### 9. 建议下一步"])
    if operator_execution.get("status") != "passed":
        lines.append("- 先修操作阻塞问题，再重跑本 runner 复核完整阶段序列。")
    elif traceability.get("status") != "passed":
        lines.append("- 先修 runtime/gateway strict traceability 失败项，再重跑 operator runner。")
    elif observation_integrity.get("status") != "passed":
        lines.append("- 先修 observation integrity 告警或失败项，再重跑 operator runner。")
    else:
        lines.append("- 同步把该入口接入文档矩阵，明确它与 P1-04 runtime action matrix 的边界。")

    return "\n".join(lines) + "\n"


def main() -> int:
    args = parse_args()
    started_at = utc_now()
    started_monotonic = time.perf_counter()
    effective_port = resolve_listen_port(args.host, args.port)
    report_dir = args.report_root / time.strftime("%Y%m%d-%H%M%S")
    report_dir.mkdir(parents=True, exist_ok=True)

    report_json_path = report_dir / "hmi-production-operator-test.json"
    report_md_path = report_dir / "hmi-production-operator-test.md"
    job_status_json_path = report_dir / "job-status-history.json"
    job_traceability_json_path = report_dir / "job-traceability.json"
    machine_status_json_path = report_dir / "machine-status-history.json"
    coord_status_json_path = report_dir / "coord-status-history.json"
    hmi_stdout_log_path = report_dir / "hmi-stdout.log"
    hmi_stderr_log_path = report_dir / "hmi-stderr.log"
    hmi_screenshot_dir = report_dir / "hmi-screenshots"
    gateway_log_copy_path = report_dir / "tcp_server.log"
    gateway_stdout_path = report_dir / "gateway-stdout.log"
    gateway_stderr_path = report_dir / "gateway-stderr.log"
    gateway_launch_spec_path = report_dir / "gateway-launch.json"

    steps: list[dict[str, str]] = []
    artifacts: dict[str, Any] = {}
    machine_status_history: list[dict[str, Any]] = []
    coord_status_history: list[dict[str, Any]] = []
    job_status_history: list[dict[str, Any]] = []
    observer_poll_errors: list[str] = []
    observer_poll_failures: list[dict[str, Any]] = []
    overall_status = "failed"
    error_message = ""
    gateway_process: subprocess.Popen[str] | None = None
    hmi_process: subprocess.Popen[str] | None = None
    observer_client = TcpJsonClient(args.host, effective_port)
    observer_connected = False
    current_job_id = ""
    production_job_id = ""
    final_job_status: dict[str, Any] = {}
    job_traceability: dict[str, Any] = {}
    snapshot_result: dict[str, Any] = {}
    snapshot_readback_error = ""
    combined_output = ""
    hmi_stdout = ""
    hmi_stderr = ""
    hmi_exit_code = 1
    hmi_command: list[str] = []
    gateway_launch_spec_payload: dict[str, Any] = {}
    gateway_log_path = ROOT / "logs" / "tcp_server.log"
    gateway_log_offset = gateway_log_path.stat().st_size if gateway_log_path.exists() else 0
    gateway_stdout_stream = gateway_stdout_path.open("w", encoding="utf-8")
    gateway_stderr_stream = gateway_stderr_path.open("w", encoding="utf-8")
    hmi_stdout_stream = hmi_stdout_log_path.open("w", encoding="utf-8")
    hmi_stderr_stream = hmi_stderr_log_path.open("w", encoding="utf-8")
    hmi_stdout_read_offset = 0
    operator_context_read_offset = 0
    observer_exclusive_windows: set[str] = set()

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
                    "dxf_file": str(args.dxf_file),
                    "operator_trigger": "hmi_only",
                    "target_count": 1,
                },
                ensure_ascii=True,
            ),
        )

        gateway_launch_spec_payload = write_gateway_launch_spec(
            args=args,
            effective_port=effective_port,
            spec_path=gateway_launch_spec_path,
        )
        add_step(steps, "gateway-launch-spec", "passed", str(gateway_launch_spec_path))

        gateway_process = subprocess.Popen(
            [
                str(gateway_launch_spec_payload["executable"]),
                *[str(item) for item in gateway_launch_spec_payload.get("args", [])],
            ],
            cwd=str(gateway_launch_spec_payload.get("cwd") or ROOT),
            stdout=gateway_stdout_stream,
            stderr=gateway_stderr_stream,
            text=True,
            encoding="utf-8",
            env=build_launch_spec_process_env(gateway_launch_spec_payload),
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        add_step(steps, "gateway-launch", "passed", f"pid={gateway_process.pid}")

        ready_status, ready_note = wait_gateway_ready(gateway_process, args.host, effective_port, args.gateway_ready_timeout)
        add_step(steps, "gateway-ready", ready_status, ready_note)
        if ready_status != "passed":
            raise RuntimeError(ready_note)

        observer_client.connect(timeout_seconds=args.connect_timeout)
        observer_connected = True
        add_step(steps, "observer-session-open", "passed", f"connected to {args.host}:{effective_port}")

        hmi_command = build_hmi_command(args=args, effective_port=effective_port, screenshot_dir=hmi_screenshot_dir)
        add_step(steps, "hmi-command", "passed", json.dumps(hmi_command, ensure_ascii=False))
        hmi_process = subprocess.Popen(
            hmi_command,
            cwd=str(ROOT),
            stdout=hmi_stdout_stream,
            stderr=hmi_stderr_stream,
            text=True,
            encoding="utf-8",
            env=build_hmi_process_env(
                gateway_launch_spec_path=gateway_launch_spec_path,
                host=args.host,
                port=effective_port,
            ),
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        add_step(steps, "hmi-launch", "passed", f"pid={hmi_process.pid}")

        deadline = time.time() + (args.ui_timeout_ms / 1000.0) + 5.0
        poll_index = 0
        while True:
            if time.time() >= deadline:
                raise TimeoutError(f"HMI operator production timed out after {args.ui_timeout_ms}ms")
            operator_context_read_offset, current_job_id, production_job_id = refresh_operator_job_tracking(
                stdout_path=hmi_stdout_log_path,
                read_offset=operator_context_read_offset,
                current_job_id=current_job_id,
                production_job_id=production_job_id,
            )
            hmi_stdout_read_offset, observer_exclusive_windows = refresh_operator_exclusive_windows(
                stdout_path=hmi_stdout_log_path,
                read_offset=hmi_stdout_read_offset,
                active_windows=observer_exclusive_windows,
            )
            if observer_connected:
                if current_job_id and not observer_exclusive_windows:
                    try:
                        sample = collect_observer_sample(
                            observer_client,
                            poll_index=poll_index,
                            job_id=current_job_id,
                        )
                        machine_status_history.append(sample["machine_status"])
                        if sample["coord_status"]:
                            coord_status_history.append(sample["coord_status"])
                        if sample["job_status"]:
                            job_status_history.append(sample["job_status"])
                            final_job_status = sample["job_status"]
                            if is_terminal_job_state(final_job_status.get("state")):
                                current_job_id = ""
                        poll_index += 1
                    except Exception as exc:  # noqa: BLE001
                        hmi_stdout_read_offset, observer_exclusive_windows = refresh_operator_exclusive_windows(
                            stdout_path=hmi_stdout_log_path,
                            read_offset=hmi_stdout_read_offset,
                            active_windows=observer_exclusive_windows,
                        )
                        if not observer_exclusive_windows:
                            record_observer_poll_failure(
                                observer_poll_failures,
                                observer_poll_errors,
                                phase="polling",
                                poll_index=poll_index,
                                job_id=current_job_id,
                                exc=exc,
                            )
            if hmi_process.poll() is not None:
                break
            time.sleep(args.status_poll_interval_seconds)

        hmi_exit_code = int(hmi_process.wait(timeout=5) or 0)
        hmi_stdout_stream, hmi_stderr_stream, hmi_stdout, hmi_stderr = finalize_process_log_capture(
            stdout_stream=hmi_stdout_stream,
            stderr_stream=hmi_stderr_stream,
            stdout_path=hmi_stdout_log_path,
            stderr_path=hmi_stderr_log_path,
        )
        add_step(
            steps,
            "hmi-exit",
            "passed" if hmi_exit_code == 0 else "failed",
            f"exit_code={hmi_exit_code}",
        )

        if observer_connected:
            operator_context_read_offset, current_job_id, production_job_id = refresh_operator_job_tracking(
                stdout_path=hmi_stdout_log_path,
                read_offset=operator_context_read_offset,
                current_job_id=current_job_id,
                production_job_id=production_job_id,
            )
            hmi_stdout_read_offset, observer_exclusive_windows = refresh_operator_exclusive_windows(
                stdout_path=hmi_stdout_log_path,
                read_offset=hmi_stdout_read_offset,
                active_windows=observer_exclusive_windows,
            )
            if current_job_id:
                try:
                    sample = collect_observer_sample(
                        observer_client,
                        poll_index=poll_index,
                        job_id=current_job_id,
                    )
                    machine_status_history.append(sample["machine_status"])
                    if sample["coord_status"]:
                        coord_status_history.append(sample["coord_status"])
                    if sample["job_status"]:
                        job_status_history.append(sample["job_status"])
                        final_job_status = sample["job_status"]
                        if is_terminal_job_state(final_job_status.get("state")):
                            current_job_id = ""
                    poll_index += 1
                except Exception as exc:  # noqa: BLE001
                    hmi_stdout_read_offset, observer_exclusive_windows = refresh_operator_exclusive_windows(
                        stdout_path=hmi_stdout_log_path,
                        read_offset=hmi_stdout_read_offset,
                        active_windows=observer_exclusive_windows,
                    )
                    if not observer_exclusive_windows:
                        record_observer_poll_failure(
                            observer_poll_failures,
                            observer_poll_errors,
                            phase="final-polling",
                            poll_index=poll_index,
                            job_id=current_job_id,
                            exc=exc,
                        )

            terminal_readback_job_id = str(production_job_id or "").strip()
            poll_index, final_job_status, clear_current_job = finalize_terminal_job_readback(
                observer_client,
                steps=steps,
                machine_status_history=machine_status_history,
                coord_status_history=coord_status_history,
                job_status_history=job_status_history,
                observer_poll_failures=observer_poll_failures,
                observer_poll_errors=observer_poll_errors,
                poll_index=poll_index,
                job_id=terminal_readback_job_id,
                final_job_status=final_job_status,
            )
            if clear_current_job:
                current_job_id = ""

        combined_output = (hmi_stdout or "") + ("\n" if hmi_stderr else "") + (hmi_stderr or "")
        operator_summary = summarize_operator_output(combined_output)

        plan_id = str(operator_summary.get("plan_id", "") or "").strip()
        if observer_connected and plan_id and plan_id != "null":
            try:
                snapshot_result = read_preview_snapshot(
                    observer_client,
                    plan_id=plan_id,
                    max_polyline_points=args.max_polyline_points,
                    max_glue_points=args.max_glue_points,
                )
                add_step(steps, "snapshot-readback", "passed", f"plan_id={plan_id}")
            except Exception as exc:  # noqa: BLE001
                snapshot_readback_error = str(exc).strip()
                add_step(steps, "snapshot-readback", "failed", snapshot_readback_error)
        else:
            snapshot_readback_error = "plan_id missing from operator output; cannot read back preview snapshot"
            add_step(steps, "snapshot-readback", "failed", snapshot_readback_error)

        operator_execution = evaluate_operator_execution(
            operator_summary=operator_summary,
            final_job_status=final_job_status,
            hmi_exit_code=hmi_exit_code,
            combined_output=combined_output,
        )
        overall_status = "passed" if operator_execution["status"] == "passed" else "failed"
    except Exception as exc:  # noqa: BLE001
        error_message = str(exc).strip()
        add_step(steps, "exception", "failed", error_message)
        if hmi_process is not None:
            if hmi_process.poll() is None:
                hmi_process.terminate()
                try:
                    hmi_process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    hmi_process.kill()
                    hmi_process.wait(timeout=5)
            hmi_exit_code = int(hmi_process.returncode or 0)
        hmi_stdout_stream, hmi_stderr_stream, hmi_stdout, hmi_stderr = finalize_process_log_capture(
            stdout_stream=hmi_stdout_stream,
            stderr_stream=hmi_stderr_stream,
            stdout_path=hmi_stdout_log_path,
            stderr_path=hmi_stderr_log_path,
        )

        combined_output = (hmi_stdout or "") + ("\n" if hmi_stderr else "") + (hmi_stderr or "")
        operator_summary = summarize_operator_output(combined_output)
        plan_id = str(operator_summary.get("plan_id", "") or "").strip()
        if observer_connected and plan_id and plan_id != "null":
            try:
                snapshot_result = read_preview_snapshot(
                    observer_client,
                    plan_id=plan_id,
                    max_polyline_points=args.max_polyline_points,
                    max_glue_points=args.max_glue_points,
                )
                add_step(steps, "snapshot-readback", "passed", f"plan_id={plan_id}")
            except Exception as snapshot_exc:  # noqa: BLE001
                snapshot_readback_error = str(snapshot_exc).strip()
                add_step(steps, "snapshot-readback", "failed", snapshot_readback_error)
        elif not snapshot_readback_error:
            snapshot_readback_error = "plan_id missing from operator output; cannot read back preview snapshot"
            add_step(steps, "snapshot-readback", "failed", snapshot_readback_error)

        operator_execution = evaluate_operator_execution(
            operator_summary=operator_summary,
            final_job_status=final_job_status,
            hmi_exit_code=hmi_exit_code,
            combined_output=combined_output,
        )
        operator_execution["issues"].insert(
            0,
            build_operator_issue(
                issue_type="阻塞类",
                phenomenon="runner 执行异常",
                impact="本轮 HMI operator 证据未收敛。",
                reproduction_steps="查看 runner exception 与 artifacts。",
                current_evidence=error_message,
                owner_layer="tests/e2e/hardware-in-loop/run_hmi_operator_production_test.py",
            ),
        )
        operator_execution["status"] = "failed"
        overall_status = "failed"
    finally:
        if hmi_process is not None and hmi_process.poll() is None:
            hmi_process.terminate()
            try:
                hmi_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                hmi_process.kill()
                hmi_process.wait(timeout=5)
            hmi_exit_code = int(hmi_process.returncode or 0)
        if hmi_stdout_stream is not None:
            hmi_stdout_stream.close()
            hmi_stdout_stream = None
        if hmi_stderr_stream is not None:
            hmi_stderr_stream.close()
            hmi_stderr_stream = None
        if not hmi_stdout:
            hmi_stdout = read_text_if_exists(hmi_stdout_log_path)
        if not hmi_stderr:
            hmi_stderr = read_text_if_exists(hmi_stderr_log_path)

        traceability_job_id = str(production_job_id or current_job_id or "").strip()
        traceability = build_traceability_query_failure(
            job_id=traceability_job_id,
            reason="dxf.job.traceability was not queried",
        )
        if traceability_job_id and observer_connected:
            try:
                traceability_response = observer_client.send_request(
                    "dxf.job.traceability",
                    {"job_id": traceability_job_id},
                    timeout_seconds=15.0,
                )
            except Exception as exc:  # noqa: BLE001
                reason = "dxf.job.traceability raised: " + str(exc).strip()
                job_traceability = {
                    "job_id": traceability_job_id,
                    "query_error": {"message": reason},
                }
                add_step(steps, "job-traceability", "failed", reason)
                traceability = build_traceability_query_failure(job_id=traceability_job_id, reason=reason)
            else:
                if "error" in traceability_response:
                    reason = "dxf.job.traceability failed: " + json.dumps(traceability_response, ensure_ascii=True)
                    job_traceability = {
                        "job_id": traceability_job_id,
                        "query_error": traceability_response.get("error", {}),
                    }
                    add_step(steps, "job-traceability", "failed", reason)
                    traceability = build_traceability_query_failure(job_id=traceability_job_id, reason=reason)
                else:
                    job_traceability = status_result(traceability_response)
                    add_step(steps, "job-traceability", "passed", f"job_id={traceability_job_id}")
                    traceability = evaluate_traceability_correspondence(
                        job_id=traceability_job_id,
                        job_traceability=job_traceability,
                    )
        else:
            reason = (
                "job_id missing after production completion; cannot read dxf.job.traceability"
                if not traceability_job_id
                else "observer session closed before dxf.job.traceability could be queried"
            )
            add_step(steps, "job-traceability", "failed", reason)
            job_traceability = {
                "job_id": traceability_job_id,
                "query_error": {"message": reason},
            }
            traceability = build_traceability_query_failure(job_id=traceability_job_id, reason=reason)
        write_json(job_traceability_json_path, job_traceability)

        if observer_connected:
            observer_client.close()
            observer_connected = False

        if gateway_process is not None:
            if gateway_process.poll() is None:
                gateway_process.terminate()
                try:
                    gateway_process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    gateway_process.kill()
                    gateway_process.wait(timeout=5)
        if gateway_stdout_stream is not None:
            gateway_stdout_stream.close()
            gateway_stdout_stream = None
        if gateway_stderr_stream is not None:
            gateway_stderr_stream.close()
            gateway_stderr_stream = None
        gateway_stdout = read_text_if_exists(gateway_stdout_path)
        gateway_stderr = read_text_if_exists(gateway_stderr_path)
        time.sleep(0.3)
        gateway_log_text = read_current_run_gateway_log(gateway_log_path, gateway_log_offset, effective_port)
        gateway_log_copy_path.write_text(gateway_log_text, encoding="utf-8")

        write_json(job_status_json_path, job_status_history)
        write_json(machine_status_json_path, machine_status_history)
        write_json(coord_status_json_path, coord_status_history)

        log_summary = extract_gateway_log_summary(gateway_log_text)
        observation_alignment = evaluate_observation_alignment(
            snapshot_result=snapshot_result,
            final_job_status=final_job_status,
            log_summary=log_summary,
            machine_status_history=machine_status_history,
            coord_status_history=coord_status_history,
            min_axis_coverage_ratio=args.min_axis_coverage_ratio,
        )
        observation_integrity = evaluate_observation_integrity(
            combined_output=combined_output,
            observer_poll_errors=observer_poll_errors,
            observer_poll_failures=observer_poll_failures,
            job_status_history=job_status_history,
            machine_status_history=machine_status_history,
            coord_status_history=coord_status_history,
            snapshot_result=snapshot_result,
            job_traceability=job_traceability,
        )
        overall_status = determine_overall_status(
            operator_execution=operator_execution,
            traceability=traceability,
            observation_integrity=observation_integrity,
        )
        finished_at = utc_now()
        duration_seconds = round(max(0.0, time.perf_counter() - started_monotonic), 3)

        artifacts.update(
            {
                "hmi_stdout_log": str(hmi_stdout_log_path),
                "hmi_stderr_log": str(hmi_stderr_log_path),
                "hmi_screenshot_dir": str(hmi_screenshot_dir),
                "runtime_stdout_log": "",
                "runtime_stderr_log": "",
                "runtime_log_note": "当前 runner 直接启动 runtime gateway，无独立 runtime 进程日志；请以 gateway stdout/stderr 与 tcp_server.log 为准。",
                "gateway_log_path": str(gateway_log_path),
                "gateway_stdout_log": str(gateway_stdout_path),
                "gateway_stderr_log": str(gateway_stderr_path),
                "gateway_log_copy": str(gateway_log_copy_path),
                "gateway_launch_spec": str(gateway_launch_spec_path),
                "job_status_history": str(job_status_json_path),
                "job_traceability": str(job_traceability_json_path),
                "machine_status_history": str(machine_status_json_path),
                "coord_status_history": str(coord_status_json_path),
                "report_json": str(report_json_path),
                "report_markdown": str(report_md_path),
            }
        )

        report = build_report(
            args=args,
            effective_port=effective_port,
            started_at=started_at,
            finished_at=finished_at,
            duration_seconds=duration_seconds,
            overall_status=overall_status,
            steps=steps,
            operator_execution=operator_execution,
            traceability=traceability,
            observation_alignment=observation_alignment,
            observation_integrity=observation_integrity,
            artifacts=artifacts,
            snapshot_result=snapshot_result,
            log_summary=log_summary,
            observer_poll_errors=observer_poll_errors,
            observer_poll_failures=observer_poll_failures,
            error_message=error_message,
            hmi_command=hmi_command,
        )
        write_json(report_json_path, report)
        report_md_path.write_text(build_report_markdown(report), encoding="utf-8")
        print(f"report json: {report_json_path}")
        print(f"report md: {report_md_path}")

    return 0 if overall_status == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
