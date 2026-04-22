from __future__ import annotations

import argparse
import base64
import json
import re
import subprocess
import sys
import time
from dataclasses import asdict
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[3]
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
if str(HIL_DIR) not in sys.path:
    sys.path.insert(0, str(HIL_DIR))

import run_real_dxf_machine_dryrun as dryrun  # noqa: E402
from runtime_gateway_harness import (  # noqa: E402
    KNOWN_FAILURE_EXIT_CODE,
    SKIPPED_EXIT_CODE,
    TcpJsonClient,
    build_process_env,
    ensure_matching_production_baseline,
    production_baseline_metadata,
    read_process_output,
    resolve_default_exe,
    truncate_json,
    wait_gateway_ready,
)


DEFAULT_REPORT_ROOT = ROOT / "tests" / "reports" / "adhoc" / "dxf-stop-home-auto-probe"
DEFAULT_COORD_SYS = dryrun.DEFAULT_COORD_SYS
DEFAULT_HOME_TIMEOUT_MS = 120_000
DEFAULT_STOP_TRIGGER_TIMEOUT_SECONDS = 30.0
DEFAULT_STOP_TERMINAL_TIMEOUT_SECONDS = 20.0
DEFAULT_STOP_RESPONSE_TIMEOUT_SECONDS = 15.0
DEFAULT_POLL_INTERVAL_SECONDS = 0.1
DEFAULT_MIN_RUNNING_SAMPLES = 2
DEFAULT_DISPENSING_SPEED_MM_S = 10.0
DEFAULT_DRY_RUN_SPEED_MM_S = 10.0
DEFAULT_RAPID_SPEED_MM_S = 20.0
DEFAULT_VELOCITY_TRACE_INTERVAL_MS = 50
PRFTRAP_PATTERN = re.compile(r"MC_PrfTrap|PrfTrap", re.IGNORECASE)


class ProbeAbort(RuntimeError):
    def __init__(self, *, exit_code: int, overall_status: str, verdict_kind: str, message: str) -> None:
        super().__init__(message)
        self.exit_code = exit_code
        self.overall_status = overall_status
        self.verdict_kind = verdict_kind
        self.message = message


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run a targeted real-machine probe: dxf.job.start -> dxf.job.stop -> immediate home.auto."
    )
    parser.add_argument(
        "--gateway-exe",
        type=Path,
        default=resolve_default_exe("siligen_runtime_gateway.exe", "siligen_tcp_server.exe"),
    )
    parser.add_argument("--config-path", type=Path, default=dryrun.DEFAULT_CONFIG_PATH)
    parser.add_argument("--dxf-file", type=Path, default=dryrun.DEFAULT_DXF_FILE)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--gateway-ready-timeout", type=float, default=8.0)
    parser.add_argument("--connect-timeout", type=float, default=15.0)
    parser.add_argument("--coord-sys", type=int, default=DEFAULT_COORD_SYS)
    parser.add_argument("--report-root", type=Path, default=DEFAULT_REPORT_ROOT)
    parser.add_argument("--target-count", type=int, default=1)
    parser.add_argument("--dispensing-speed-mm-s", type=float, default=DEFAULT_DISPENSING_SPEED_MM_S)
    parser.add_argument("--dry-run-speed-mm-s", type=float, default=DEFAULT_DRY_RUN_SPEED_MM_S)
    parser.add_argument("--rapid-speed-mm-s", type=float, default=DEFAULT_RAPID_SPEED_MM_S)
    parser.add_argument("--velocity-trace-interval-ms", type=int, default=DEFAULT_VELOCITY_TRACE_INTERVAL_MS)
    parser.add_argument("--home-timeout-ms", type=int, default=DEFAULT_HOME_TIMEOUT_MS)
    parser.add_argument("--home-axes", nargs="*", default=[])
    parser.add_argument("--force-home", action="store_true")
    parser.add_argument("--stop-trigger-timeout-seconds", type=float, default=DEFAULT_STOP_TRIGGER_TIMEOUT_SECONDS)
    parser.add_argument("--stop-terminal-timeout-seconds", type=float, default=DEFAULT_STOP_TERMINAL_TIMEOUT_SECONDS)
    parser.add_argument("--stop-response-timeout-seconds", type=float, default=DEFAULT_STOP_RESPONSE_TIMEOUT_SECONDS)
    parser.add_argument("--poll-interval-seconds", type=float, default=DEFAULT_POLL_INTERVAL_SECONDS)
    parser.add_argument("--min-running-samples", type=int, default=DEFAULT_MIN_RUNNING_SAMPLES)
    parser.add_argument("--position-epsilon-mm", type=float, default=dryrun.DEFAULT_POSITION_EPSILON_MM)
    parser.add_argument("--velocity-epsilon-mm-s", type=float, default=dryrun.DEFAULT_VELOCITY_EPSILON_MM_S)
    return parser.parse_args()


def _home_rpc_timeout_seconds(timeout_ms: int) -> float:
    return max(30.0, float(max(0, timeout_ms)) / 1000.0 + 10.0)


def _build_home_auto_params(args: argparse.Namespace) -> dict[str, Any]:
    params: dict[str, Any] = {
        "wait_for_completion": True,
        "timeout_ms": max(1, int(args.home_timeout_ms)),
    }
    if args.home_axes:
        params["axes"] = list(args.home_axes)
    if args.force_home:
        params["force"] = True
    return params


def evaluate_preflight_home_normalization(status_payload: dict[str, Any]) -> dict[str, Any]:
    non_homed_axes = dryrun.non_homed_axes(status_payload)
    home_boundary_axes = dryrun.active_home_boundary_axes(status_payload)
    dryrun.require_safe_for_motion(status_payload, ignore_home_boundaries=True)
    return {
        "non_homed_axes": non_homed_axes,
        "home_boundary_axes": home_boundary_axes,
        "should_home": bool(non_homed_axes or home_boundary_axes),
    }


def extract_prftrap_hits(text: str) -> list[str]:
    hits: list[str] = []
    for raw_line in str(text or "").splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if PRFTRAP_PATTERN.search(line):
            hits.append(line)
    return hits


def _contains_prftrap(text: str) -> bool:
    return bool(PRFTRAP_PATTERN.search(str(text or "")))


def summarize_home_auto_response(response: dict[str, Any]) -> dict[str, Any]:
    if "error" in response:
        error_payload = response.get("error", {})
        message = str(error_payload.get("message", "Unknown error"))
        return {
            "ok": False,
            "accepted": False,
            "summary_state": "error",
            "message": message,
            "failed_axis_messages": [],
            "error_code": error_payload.get("code"),
            "response": response,
            "contains_prftrap": _contains_prftrap(message),
            "summary_text": message,
        }

    result = dryrun.status_result(response)
    accepted = bool(result.get("accepted", False))
    summary_state = str(result.get("summary_state", "")).strip()
    message = str(result.get("message", "")).strip()
    axis_results = result.get("axis_results", [])
    failed_axis_messages: list[str] = []
    if isinstance(axis_results, list):
        for axis_result in axis_results:
            if not isinstance(axis_result, dict):
                continue
            if bool(axis_result.get("success", False)):
                continue
            axis = str(axis_result.get("axis", "")).strip()
            axis_message = str(axis_result.get("message", "")).strip() or "Ready-zero failed"
            failed_axis_messages.append(f"{axis}: {axis_message}" if axis else axis_message)

    ok = accepted and summary_state in {"completed", "noop", "in_progress"}
    detail = "; ".join(failed_axis_messages) if failed_axis_messages else message
    if not detail:
        detail = "home.auto accepted" if ok else "home.auto rejected"

    return {
        "ok": ok,
        "accepted": accepted,
        "summary_state": summary_state,
        "message": message,
        "failed_axis_messages": failed_axis_messages,
        "response": response,
        "contains_prftrap": _contains_prftrap(detail),
        "summary_text": detail,
    }


def _normalize_snapshot_payload(payload: dict[str, Any]) -> dict[str, Any]:
    return payload if isinstance(payload, dict) else {}


def collect_named_snapshot(
    client: TcpJsonClient,
    *,
    name: str,
    coord_sys: int,
    job_id: str = "",
) -> dict[str, Any]:
    status_raw = client.send_request("status", None, timeout_seconds=5.0)
    coord_raw = client.send_request("motion.coord.status", {"coord_sys": coord_sys}, timeout_seconds=5.0)

    job_raw: dict[str, Any] = {}
    if job_id:
        job_raw = client.send_request("dxf.job.status", {"job_id": job_id}, timeout_seconds=5.0)

    sampled_at = dryrun.utc_now()
    snapshot: dict[str, Any] = {
        "name": name,
        "sampled_at": sampled_at,
        "status_raw": status_raw,
        "coord_raw": coord_raw,
        "job_status_raw": job_raw,
    }

    if "error" not in status_raw:
        snapshot["machine_status"] = dryrun.normalize_machine_status_sample(
            status_raw,
            sampled_at=sampled_at,
            poll_index=-1,
        )
        snapshot["safety"] = dryrun.summarize_safety(status_raw)
        snapshot["non_homed_axes"] = dryrun.non_homed_axes(status_raw)
    else:
        snapshot["machine_status"] = {}
        snapshot["safety"] = {}
        snapshot["non_homed_axes"] = []

    if "error" not in coord_raw:
        snapshot["coord_status"] = dryrun.normalize_coord_status_sample(
            coord_raw,
            sampled_at=sampled_at,
            poll_index=-1,
            coord_sys=coord_sys,
        )
    else:
        snapshot["coord_status"] = {}

    if job_id and "error" not in job_raw:
        snapshot["job_status"] = dryrun.normalize_job_status_sample(
            job_raw,
            sampled_at=sampled_at,
            poll_index=-1,
        )
    else:
        snapshot["job_status"] = {}

    return snapshot


def should_issue_stop(
    observation: dict[str, Any],
    *,
    coord_window: list[dict[str, Any]],
    running_sample_count: int,
    min_running_samples: int,
    position_epsilon_mm: float,
    velocity_epsilon_mm_s: float,
) -> bool:
    job_status = _normalize_snapshot_payload(observation.get("job_status", {}))
    if str(job_status.get("state", "")).strip() != "running":
        return False
    if running_sample_count < max(1, min_running_samples):
        return False

    coord_status = _normalize_snapshot_payload(observation.get("coord_status", {}))
    if dryrun.coord_status_indicates_running(coord_status, velocity_epsilon_mm_s=velocity_epsilon_mm_s):
        return True

    return dryrun.axis_window_shows_motion(
        coord_window,
        position_epsilon_mm=position_epsilon_mm,
        velocity_epsilon_mm_s=velocity_epsilon_mm_s,
    )


def snapshot_indicates_axes_stopped(snapshot: dict[str, Any], *, velocity_epsilon_mm_s: float) -> bool:
    coord_status = _normalize_snapshot_payload(snapshot.get("coord_status", {}))
    if dryrun.coord_status_indicates_running(coord_status, velocity_epsilon_mm_s=velocity_epsilon_mm_s):
        return False

    axes_payload = coord_status.get("axes", {})
    if isinstance(axes_payload, dict):
        for axis_payload in axes_payload.values():
            if not isinstance(axis_payload, dict):
                continue
            velocity = dryrun.parse_float(axis_payload.get("velocity")) or 0.0
            if abs(velocity) > velocity_epsilon_mm_s:
                return False

    return True


def _start_dxf_job(
    client: TcpJsonClient,
    *,
    args: argparse.Namespace,
    steps: list[dryrun.Step],
    artifacts: dict[str, Any],
) -> tuple[str, str, str, dict[str, str]]:
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
    artifact_id = str(dryrun.status_result(artifact_response).get("artifact_id", "")).strip()
    if not artifact_id:
        raise RuntimeError("artifact.create missing artifact_id")
    dryrun.add_step(steps, "dxf-artifact-create", "passed", f"artifact_id={artifact_id}")

    plan_response = client.send_request(
        "dxf.plan.prepare",
        {
            "artifact_id": artifact_id,
            "dispensing_speed_mm_s": args.dispensing_speed_mm_s,
            "dry_run": True,
            "dry_run_speed_mm_s": args.dry_run_speed_mm_s,
            "rapid_speed_mm_s": args.rapid_speed_mm_s,
            "velocity_trace_enabled": True,
            "velocity_trace_interval_ms": args.velocity_trace_interval_ms,
        },
        timeout_seconds=30.0,
    )
    artifacts["dxf_plan_prepare"] = plan_response
    if "error" in plan_response:
        raise RuntimeError("dxf.plan.prepare failed: " + truncate_json(plan_response))
    plan_result = dryrun.status_result(plan_response)
    plan_id = str(plan_result.get("plan_id", "")).strip()
    plan_fingerprint = str(plan_result.get("plan_fingerprint", "")).strip()
    if not plan_id or not plan_fingerprint:
        raise RuntimeError("plan.prepare missing plan_id/plan_fingerprint")
    production_baseline = production_baseline_metadata(plan_result, source="dxf.plan.prepare")
    dryrun.add_step(
        steps,
        "dxf-plan-prepare",
        "passed",
        json.dumps(
            {
                "plan_id": plan_id,
                "plan_fingerprint": plan_fingerprint,
                "segment_count": plan_result.get("segment_count", 0),
                "execution_nominal_time_s": plan_result.get("execution_nominal_time_s", 0),
                "production_baseline": production_baseline,
            },
            ensure_ascii=True,
        ),
    )

    snapshot_response = client.send_request(
        "dxf.preview.snapshot",
        {"plan_id": plan_id, "max_polyline_points": 4000},
        timeout_seconds=15.0,
    )
    artifacts["dxf_preview_snapshot"] = snapshot_response
    if "error" in snapshot_response:
        raise RuntimeError("dxf.preview.snapshot failed: " + truncate_json(snapshot_response))
    snapshot_hash = str(dryrun.status_result(snapshot_response).get("snapshot_hash", "")).strip()
    if not snapshot_hash:
        raise RuntimeError("dxf.preview.snapshot missing snapshot_hash")
    dryrun.add_step(steps, "dxf-preview-snapshot", "passed", f"snapshot_hash={snapshot_hash}")

    confirm_response = client.send_request(
        "dxf.preview.confirm",
        {"plan_id": plan_id, "snapshot_hash": snapshot_hash},
        timeout_seconds=15.0,
    )
    artifacts["dxf_preview_confirm"] = confirm_response
    if "error" in confirm_response:
        raise RuntimeError("dxf.preview.confirm failed: " + truncate_json(confirm_response))
    dryrun.add_step(steps, "dxf-preview-confirm", "passed", truncate_json(confirm_response))

    job_start_response = client.send_request(
        "dxf.job.start",
        {
            "plan_id": plan_id,
            "plan_fingerprint": plan_fingerprint,
            "target_count": max(1, int(args.target_count)),
        },
        timeout_seconds=15.0,
    )
    artifacts["dxf_job_start"] = job_start_response
    if "error" in job_start_response:
        raise RuntimeError("dxf.job.start failed: " + truncate_json(job_start_response))
    job_result = dryrun.status_result(job_start_response)
    production_baseline = ensure_matching_production_baseline(
        production_baseline,
        production_baseline_metadata(job_result, source="dxf.job.start"),
        label="dxf.job.start",
    )
    job_id = str(job_result.get("job_id", "")).strip()
    if not job_id:
        raise RuntimeError("dxf.job.start missing job_id")
    dryrun.add_step(steps, "dxf-job-start", "passed", f"job_id={job_id}")
    return artifact_id, plan_id, job_id, production_baseline


def _write_text(path: Path, text: str) -> None:
    path.write_text(str(text or ""), encoding="utf-8")


def _build_observation_summary(
    *,
    artifacts: dict[str, Any],
    pre_stop_observations: list[dict[str, Any]],
    post_stop_observations: list[dict[str, Any]],
    prftrap_hits: list[str],
    velocity_epsilon_mm_s: float,
) -> dict[str, Any]:
    pre_home_snapshot = artifacts.get("pre_home_snapshot", {})
    post_home_snapshot = artifacts.get("post_home_snapshot", {})
    terminal_observation = artifacts.get("stop_terminal_observation", {})
    home_auto_summary = artifacts.get("home_auto_summary", {})

    terminal_job_status = terminal_observation.get("job_status", {})
    terminal_coord_status = terminal_observation.get("coord_status", {})

    return {
        "pre_stop_samples": len(pre_stop_observations),
        "post_stop_samples": len(post_stop_observations),
        "stop_triggered": "dxf_job_stop" in artifacts,
        "job_terminal_state_after_stop": str(terminal_job_status.get("state", "")).strip(),
        "job_terminal_error_after_stop": str(terminal_job_status.get("error_message", "")).strip(),
        "coord_after_stop": {
            "is_moving": terminal_coord_status.get("is_moving"),
            "remaining_segments": terminal_coord_status.get("remaining_segments"),
            "current_velocity": terminal_coord_status.get("current_velocity"),
            "raw_status_word": terminal_coord_status.get("raw_status_word"),
            "raw_segment": terminal_coord_status.get("raw_segment"),
        },
        "axes_stopped_before_home": snapshot_indicates_axes_stopped(
            pre_home_snapshot,
            velocity_epsilon_mm_s=velocity_epsilon_mm_s,
        )
        if pre_home_snapshot
        else None,
        "non_homed_after_home": post_home_snapshot.get("non_homed_axes", []),
        "home_auto_ok": home_auto_summary.get("ok"),
        "home_auto_summary_state": home_auto_summary.get("summary_state"),
        "home_auto_message": home_auto_summary.get("summary_text", ""),
        "mc_prftrap_detected": bool(prftrap_hits or home_auto_summary.get("contains_prftrap", False)),
        "prftrap_hit_count": len(prftrap_hits),
    }


def build_report(
    *,
    args: argparse.Namespace,
    production_baseline: dict[str, str],
    effective_port: int,
    steps: list[dryrun.Step],
    artifacts: dict[str, Any],
    pre_stop_observations: list[dict[str, Any]],
    post_stop_observations: list[dict[str, Any]],
    prftrap_hits: list[str],
    overall_status: str,
    verdict_kind: str,
    error_message: str,
    stdout_path: Path,
    stderr_path: Path,
) -> dict[str, Any]:
    return {
        "generated_at": dryrun.utc_now(),
        "overall_status": overall_status,
        "verdict": {
            "kind": verdict_kind,
            "message": error_message,
        },
        "gateway_exe": str(args.gateway_exe),
        "config_path": str(args.config_path),
        "dxf_file": str(args.dxf_file),
        **production_baseline,
        "gateway_host": args.host,
        "gateway_port": effective_port,
        "parameters": {
            "coord_sys": args.coord_sys,
            "target_count": args.target_count,
            "dispensing_speed_mm_s": args.dispensing_speed_mm_s,
            "dry_run_speed_mm_s": args.dry_run_speed_mm_s,
            "rapid_speed_mm_s": args.rapid_speed_mm_s,
            "velocity_trace_interval_ms": args.velocity_trace_interval_ms,
            "home_timeout_ms": args.home_timeout_ms,
            "home_axes": list(args.home_axes),
            "force_home": bool(args.force_home),
            "stop_trigger_timeout_seconds": args.stop_trigger_timeout_seconds,
            "stop_terminal_timeout_seconds": args.stop_terminal_timeout_seconds,
            "stop_response_timeout_seconds": args.stop_response_timeout_seconds,
            "poll_interval_seconds": args.poll_interval_seconds,
            "min_running_samples": args.min_running_samples,
            "position_epsilon_mm": args.position_epsilon_mm,
            "velocity_epsilon_mm_s": args.velocity_epsilon_mm_s,
        },
        "steps": [asdict(step) for step in steps],
        "artifacts": artifacts,
        "pre_stop_observations": pre_stop_observations,
        "post_stop_observations": post_stop_observations,
        "observation_summary": _build_observation_summary(
            artifacts=artifacts,
            pre_stop_observations=pre_stop_observations,
            post_stop_observations=post_stop_observations,
            prftrap_hits=prftrap_hits,
            velocity_epsilon_mm_s=args.velocity_epsilon_mm_s,
        ),
        "prftrap_hits": prftrap_hits,
        "logs": {
            "gateway_stdout_path": str(stdout_path),
            "gateway_stderr_path": str(stderr_path),
        },
        "error": error_message,
    }


def build_report_markdown(report: dict[str, Any]) -> str:
    lines = [
        "# DXF Stop Then Home.auto Probe",
        "",
        f"- overall_status: `{report['overall_status']}`",
        f"- verdict: `{report['verdict']['kind']}`",
        f"- gateway_exe: `{report['gateway_exe']}`",
        f"- config_path: `{report['config_path']}`",
        f"- dxf_file: `{report['dxf_file']}`",
        f"- baseline_id: `{report.get('baseline_id', '')}`",
        f"- baseline_fingerprint: `{report.get('baseline_fingerprint', '')}`",
        f"- production_baseline_source: `{report.get('production_baseline_source', '')}`",
        f"- gateway: `{report['gateway_host']}:{report['gateway_port']}`",
        "",
        "## Observation Summary",
        "",
        "```json",
        json.dumps(report["observation_summary"], ensure_ascii=False, indent=2),
        "```",
        "",
        "## Steps",
        "",
        "| step | status | note |",
        "| --- | --- | --- |",
    ]

    for step in report["steps"]:
        note = str(step.get("note", "")).replace("\n", "<br>")
        lines.append(f"| `{step['name']}` | `{step['status']}` | {note} |")

    interesting_artifacts = (
        "trigger_observation",
        "dxf_job_stop",
        "stop_ack_snapshot",
        "stop_terminal_observation",
        "pre_home_snapshot",
        "home_auto_summary",
        "post_home_snapshot",
    )
    for key in interesting_artifacts:
        payload = report["artifacts"].get(key)
        if payload is None:
            continue
        lines.extend(
            [
                "",
                f"## {key}",
                "",
                "```json",
                json.dumps(payload, ensure_ascii=False, indent=2),
                "```",
            ]
        )

    if report.get("prftrap_hits"):
        lines.extend(
            [
                "",
                "## MC_PrfTrap Hits",
                "",
                "```text",
                "\n".join(report["prftrap_hits"]),
                "```",
            ]
        )

    if report.get("error"):
        lines.extend(
            [
                "",
                "## Error",
                "",
                "```text",
                str(report["error"]),
                "```",
            ]
        )

    return "\n".join(lines) + "\n"


def main() -> int:
    args = parse_args()
    dryrun.ensure_exists(args.gateway_exe, "gateway executable")
    dryrun.ensure_exists(args.config_path, "config")
    dryrun.ensure_exists(args.dxf_file, "dxf file")

    effective_port = dryrun.resolve_listen_port(args.host, args.port)
    report_dir = args.report_root / time.strftime("%Y%m%d-%H%M%S")
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "dxf-stop-home-auto-probe.json"
    md_path = report_dir / "dxf-stop-home-auto-probe.md"
    stdout_path = report_dir / "gateway-stdout.log"
    stderr_path = report_dir / "gateway-stderr.log"

    steps: list[dryrun.Step] = []
    artifacts: dict[str, Any] = {
        "gateway_host": args.host,
        "gateway_port": effective_port,
    }
    pre_stop_observations: list[dict[str, Any]] = []
    post_stop_observations: list[dict[str, Any]] = []
    error_message = ""
    verdict_kind = "canonical_step_failed"
    overall_status = "failed"
    exit_code = 1
    process: subprocess.Popen[str] | None = None
    client = TcpJsonClient(args.host, effective_port)
    connected = False
    gateway_stdout = ""
    gateway_stderr = ""
    prftrap_hits: list[str] = []
    production_baseline: dict[str, str] = {}

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
        dryrun.add_step(steps, "gateway-launch", "passed", f"pid={process.pid}")

        ready_status, ready_note = wait_gateway_ready(process, args.host, effective_port, args.gateway_ready_timeout)
        dryrun.add_step(steps, "gateway-ready", ready_status, ready_note)
        if ready_status != "passed":
            raise ProbeAbort(
                exit_code=KNOWN_FAILURE_EXIT_CODE if ready_status == "known_failure" else 1,
                overall_status=ready_status,
                verdict_kind="gateway_ready_failed",
                message=ready_note,
            )

        client.connect(timeout_seconds=args.connect_timeout)
        connected = True
        dryrun.add_step(steps, "tcp-session-open", "passed", f"connected to {args.host}:{effective_port}")

        connect_params = dryrun.load_connection_params(args.config_path)
        connect_response = client.send_request("connect", connect_params, timeout_seconds=args.connect_timeout)
        artifacts["connect_response"] = connect_response
        if "error" in connect_response:
            raise RuntimeError("connect failed: " + truncate_json(connect_response))
        dryrun.add_step(steps, "connect", "passed", truncate_json(connect_response))

        status_before = client.send_request("status", None, timeout_seconds=5.0)
        artifacts["status_before"] = status_before
        safety_before = dryrun.summarize_safety(status_before)
        non_homed_before = dryrun.non_homed_axes(status_before)
        home_boundary_before = dryrun.active_home_boundary_axes(status_before)
        dryrun.add_step(
            steps,
            "status-before",
            "passed",
            json.dumps(
                {
                    "safety": safety_before,
                    "non_homed_axes": non_homed_before,
                    "home_boundary_axes": home_boundary_before,
                },
                ensure_ascii=True,
            ),
        )

        preflight_normalization = evaluate_preflight_home_normalization(status_before)
        artifacts["preflight_home_normalization_decision"] = preflight_normalization
        dryrun.add_step(
            steps,
            "preflight-home-normalization-decision",
            "passed",
            json.dumps(preflight_normalization, ensure_ascii=True),
        )

        if preflight_normalization["should_home"]:
            preflight_home_response = client.send_request(
                "home.auto",
                _build_home_auto_params(args),
                timeout_seconds=_home_rpc_timeout_seconds(args.home_timeout_ms),
            )
            artifacts["preflight_home_auto_response"] = preflight_home_response
            preflight_home_summary = summarize_home_auto_response(preflight_home_response)
            artifacts["preflight_home_auto_summary"] = preflight_home_summary
            dryrun.add_step(
                steps,
                "preflight-home-auto",
                "passed" if preflight_home_summary["ok"] else "failed",
                preflight_home_summary["summary_text"],
                payload=preflight_home_summary,
            )
            if not preflight_home_summary["ok"]:
                raise RuntimeError("preflight home.auto failed: " + preflight_home_summary["summary_text"])

            status_after_preflight_home = client.send_request("status", None, timeout_seconds=5.0)
            coord_after_preflight_home = client.send_request(
                "motion.coord.status",
                {"coord_sys": args.coord_sys},
                timeout_seconds=5.0,
            )
            artifacts["status_after_preflight_home"] = status_after_preflight_home
            artifacts["coord_after_preflight_home"] = coord_after_preflight_home
            remaining_non_homed = dryrun.non_homed_axes(status_after_preflight_home)
            if remaining_non_homed:
                raise RuntimeError("axes still not homed after preflight home.auto: " + ",".join(remaining_non_homed))
            dryrun.add_step(
                steps,
                "status-after-preflight-home",
                "passed",
                json.dumps(
                    {
                        "safety": dryrun.summarize_safety(status_after_preflight_home),
                        "non_homed_axes": remaining_non_homed,
                    },
                    ensure_ascii=True,
                ),
            )
        else:
            dryrun.add_step(steps, "preflight-home-auto", "passed", "already homed; no preflight home.auto sent")

        dryrun.require_safe_for_motion(
            artifacts.get("status_after_preflight_home", status_before),
            ignore_home_boundaries=True,
        )
        dryrun.add_step(steps, "preflight-safe-for-motion", "passed", "status is safe for motion")

        _, _, job_id, production_baseline = _start_dxf_job(
            client,
            args=args,
            steps=steps,
            artifacts=artifacts,
        )

        coord_window: list[dict[str, Any]] = []
        running_sample_count = 0
        trigger_observation: dict[str, Any] | None = None
        terminal_before_stop: dict[str, Any] | None = None
        poll_index = 0
        stop_trigger_deadline = time.time() + args.stop_trigger_timeout_seconds
        while time.time() < stop_trigger_deadline:
            observation = dryrun.collect_poll_observation(
                client,
                job_id=job_id,
                poll_index=poll_index,
                coord_sys=args.coord_sys,
            )
            pre_stop_observations.append(observation)
            coord_window.append(observation["coord_status"])
            coord_window = coord_window[-max(2, args.min_running_samples) :]

            job_state = str(observation["job_status"].get("state", "")).strip()
            if job_state == "running":
                running_sample_count += 1
            else:
                running_sample_count = 0

            if should_issue_stop(
                observation,
                coord_window=coord_window,
                running_sample_count=running_sample_count,
                min_running_samples=args.min_running_samples,
                position_epsilon_mm=args.position_epsilon_mm,
                velocity_epsilon_mm_s=args.velocity_epsilon_mm_s,
            ):
                trigger_observation = observation
                artifacts["trigger_observation"] = observation
                break

            if job_state in {"completed", "failed", "cancelled"}:
                terminal_before_stop = observation
                artifacts["terminal_before_stop"] = observation
                break

            poll_index += 1
            time.sleep(args.poll_interval_seconds)

        if trigger_observation is None:
            if terminal_before_stop is not None:
                terminal_state = str(terminal_before_stop["job_status"].get("state", "")).strip()
                skip_message = f"job reached terminal state before stop trigger: {terminal_state}"
                raise ProbeAbort(
                    exit_code=SKIPPED_EXIT_CODE if terminal_state == "completed" else 1,
                    overall_status="skipped" if terminal_state == "completed" else "failed",
                    verdict_kind="job_terminal_before_stop_trigger",
                    message=skip_message,
                )
            raise ProbeAbort(
                exit_code=1,
                overall_status="failed",
                verdict_kind="stop_trigger_timeout",
                message=(
                    "job did not reach a stop trigger state within "
                    f"{args.stop_trigger_timeout_seconds:.1f}s"
                ),
            )

        stop_response = client.send_request(
            "dxf.job.stop",
            {"job_id": job_id},
            timeout_seconds=args.stop_response_timeout_seconds,
        )
        artifacts["dxf_job_stop"] = stop_response
        if "error" in stop_response:
            raise RuntimeError("dxf.job.stop failed: " + truncate_json(stop_response))
        dryrun.add_step(steps, "dxf-job-stop", "passed", truncate_json(stop_response))

        stop_ack_snapshot = collect_named_snapshot(
            client,
            name="after-stop-ack",
            coord_sys=args.coord_sys,
            job_id=job_id,
        )
        artifacts["stop_ack_snapshot"] = stop_ack_snapshot

        terminal_observation: dict[str, Any] | None = None
        stop_terminal_deadline = time.time() + args.stop_terminal_timeout_seconds
        while time.time() < stop_terminal_deadline:
            observation = dryrun.collect_poll_observation(
                client,
                job_id=job_id,
                poll_index=poll_index,
                coord_sys=args.coord_sys,
            )
            post_stop_observations.append(observation)
            job_state = str(observation["job_status"].get("state", "")).strip()
            if job_state in {"completed", "failed", "cancelled"}:
                terminal_observation = observation
                break
            poll_index += 1
            time.sleep(args.poll_interval_seconds)

        if terminal_observation is None:
            artifacts["stop_timeout_snapshot"] = collect_named_snapshot(
                client,
                name="stop-terminal-timeout",
                coord_sys=args.coord_sys,
                job_id=job_id,
            )
            raise ProbeAbort(
                exit_code=1,
                overall_status="failed",
                verdict_kind="stop_terminal_timeout",
                message=(
                    "dxf.job.stop did not reach terminal state within "
                    f"{args.stop_terminal_timeout_seconds:.1f}s"
                ),
            )

        artifacts["stop_terminal_observation"] = terminal_observation
        terminal_state = str(terminal_observation["job_status"].get("state", "")).strip()
        terminal_error = str(terminal_observation["job_status"].get("error_message", "")).strip()
        dryrun.add_step(
            steps,
            "dxf-job-terminal-after-stop",
            "passed" if terminal_state == "cancelled" else ("skipped" if terminal_state == "completed" else "failed"),
            truncate_json(terminal_observation["job_status"]),
        )

        if terminal_state != "cancelled":
            raise ProbeAbort(
                exit_code=SKIPPED_EXIT_CODE if terminal_state == "completed" else 1,
                overall_status="skipped" if terminal_state == "completed" else "failed",
                verdict_kind="job_not_cancelled_after_stop",
                message=f"job terminal state after stop was {terminal_state}: {terminal_error}",
            )

        pre_home_snapshot = collect_named_snapshot(
            client,
            name="pre-home-auto-after-stop",
            coord_sys=args.coord_sys,
            job_id=job_id,
        )
        artifacts["pre_home_snapshot"] = pre_home_snapshot

        home_auto_response = client.send_request(
            "home.auto",
            _build_home_auto_params(args),
            timeout_seconds=_home_rpc_timeout_seconds(args.home_timeout_ms),
        )
        artifacts["home_auto_response"] = home_auto_response
        home_auto_summary = summarize_home_auto_response(home_auto_response)
        artifacts["home_auto_summary"] = home_auto_summary
        dryrun.add_step(
            steps,
            "home-auto-after-stop",
            "passed" if home_auto_summary["ok"] else "failed",
            home_auto_summary["summary_text"],
            payload=home_auto_summary,
        )

        post_home_snapshot = collect_named_snapshot(
            client,
            name="post-home-auto-after-stop",
            coord_sys=args.coord_sys,
            job_id="",
        )
        artifacts["post_home_snapshot"] = post_home_snapshot

        if not home_auto_summary["ok"]:
            raise ProbeAbort(
                exit_code=1,
                overall_status="failed",
                verdict_kind="mc_prftrap_after_stop" if home_auto_summary["contains_prftrap"] else "home_auto_failed_after_stop",
                message="home.auto after stop failed: " + home_auto_summary["summary_text"],
            )

        non_homed_after_home = dryrun.non_homed_axes(post_home_snapshot.get("status_raw", {}))
        if non_homed_after_home:
            raise ProbeAbort(
                exit_code=1,
                overall_status="failed",
                verdict_kind="home_auto_incomplete_after_stop",
                message="axes still not homed after home.auto: " + ",".join(non_homed_after_home),
            )

        if not snapshot_indicates_axes_stopped(
            post_home_snapshot,
            velocity_epsilon_mm_s=args.velocity_epsilon_mm_s,
        ):
            raise ProbeAbort(
                exit_code=1,
                overall_status="failed",
                verdict_kind="home_auto_left_motion_active",
                message="home.auto returned but coord or axis motion was still active afterwards",
            )

        overall_status = "passed"
        verdict_kind = "passed"
        exit_code = 0

    except ProbeAbort as exc:
        overall_status = exc.overall_status
        verdict_kind = exc.verdict_kind
        error_message = exc.message
        exit_code = exc.exit_code
    except Exception as exc:  # noqa: BLE001
        overall_status = "failed"
        verdict_kind = "canonical_step_failed"
        error_message = str(exc)
        exit_code = 1
    finally:
        if connected:
            try:
                disconnect_response = client.send_request("disconnect", None, timeout_seconds=5.0)
                artifacts["disconnect_response"] = disconnect_response
                dryrun.add_step(steps, "disconnect", "passed", truncate_json(disconnect_response))
            except Exception as exc:  # noqa: BLE001
                artifacts["disconnect_error"] = str(exc)
                dryrun.add_step(steps, "disconnect", "failed", str(exc))
        client.close()

        if process is not None:
            if process.poll() is None:
                process.terminate()
            gateway_stdout, gateway_stderr = read_process_output(process)

        _write_text(stdout_path, gateway_stdout)
        _write_text(stderr_path, gateway_stderr)
        prftrap_hits = extract_prftrap_hits("\n".join(part for part in (gateway_stdout, gateway_stderr) if part))

        home_auto_summary = artifacts.get("home_auto_summary", {})
        if overall_status == "failed" and verdict_kind == "home_auto_failed_after_stop" and (
            prftrap_hits or home_auto_summary.get("contains_prftrap", False)
        ):
            verdict_kind = "mc_prftrap_after_stop"

        report = build_report(
            args=args,
            production_baseline=production_baseline,
            effective_port=effective_port,
            steps=steps,
            artifacts=artifacts,
            pre_stop_observations=pre_stop_observations,
            post_stop_observations=post_stop_observations,
            prftrap_hits=prftrap_hits,
            overall_status=overall_status,
            verdict_kind=verdict_kind,
            error_message=error_message,
            stdout_path=stdout_path,
            stderr_path=stderr_path,
        )
        json_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        md_path.write_text(build_report_markdown(report), encoding="utf-8")

    print(f"dxf stop/home probe complete: status={overall_status} verdict={verdict_kind}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
