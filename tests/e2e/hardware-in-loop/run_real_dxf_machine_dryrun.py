from __future__ import annotations

import argparse
import base64
import json
import os
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[3]
FIRST_LAYER_DIR = ROOT / "tests" / "e2e" / "first-layer"
if str(FIRST_LAYER_DIR) not in sys.path:
    sys.path.insert(0, str(FIRST_LAYER_DIR))

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
    parser.add_argument("--port", type=int, default=9527)
    parser.add_argument("--gateway-ready-timeout", type=float, default=8.0)
    parser.add_argument("--connect-timeout", type=float, default=15.0)
    parser.add_argument("--home-timeout", type=float, default=60.0)
    parser.add_argument("--job-timeout", type=float, default=120.0)
    parser.add_argument("--report-root", type=Path, default=DEFAULT_REPORT_ROOT)
    parser.add_argument("--dispensing-speed-mm-s", type=float, default=10.0)
    parser.add_argument("--dry-run-speed-mm-s", type=float, default=10.0)
    parser.add_argument("--rapid-speed-mm-s", type=float, default=20.0)
    parser.add_argument("--velocity-trace-interval-ms", type=int, default=50)
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


def status_result(payload: dict[str, Any]) -> dict[str, Any]:
    result = payload.get("result", {})
    return result if isinstance(result, dict) else {}


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


def require_safe_for_motion(status_payload: dict[str, Any]) -> None:
    safety = summarize_safety(status_payload)
    blocking = [
        key
        for key in (
            "estop",
            "door_open",
            "limit_x_pos",
            "limit_x_neg",
            "limit_y_pos",
            "limit_y_neg",
        )
        if safety.get(key)
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
    lines.append(f"- gateway_exe: `{report['gateway_exe']}`")
    lines.append(f"- config_path: `{report['config_path']}`")
    lines.append(f"- dxf_file: `{report['dxf_file']}`")
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

    report_dir = args.report_root / time.strftime("%Y%m%d-%H%M%S")
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "real-dxf-machine-dryrun-canonical.json"
    md_path = report_dir / "real-dxf-machine-dryrun-canonical.md"

    steps: list[Step] = []
    artifacts: dict[str, Any] = {}
    job_status_history: list[dict[str, Any]] = []
    error_message = ""
    overall_status = "failed"
    process: subprocess.Popen[str] | None = None
    client = TcpJsonClient(args.host, args.port)
    connected = False

    try:
        process = subprocess.Popen(
            [str(args.gateway_exe), "--config", str(args.config_path), "--port", str(args.port)],
            cwd=str(ROOT),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            env=build_process_env(args.gateway_exe),
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        add_step(steps, "gateway-launch", "passed", f"pid={process.pid}")

        ready_status, ready_note = wait_gateway_ready(process, args.host, args.port, args.gateway_ready_timeout)
        add_step(steps, "gateway-ready", ready_status, ready_note)
        if ready_status != "passed":
            overall_status = ready_status
            error_message = ready_note
            return KNOWN_FAILURE_EXIT_CODE if ready_status == "known_failure" else 1

        client.connect(timeout_seconds=args.connect_timeout)
        connected = True
        add_step(steps, "tcp-session-open", "passed", f"connected to {args.host}:{args.port}")

        connect_response = client.send_request("connect", None, timeout_seconds=args.connect_timeout)
        artifacts["connect_response"] = connect_response
        if "error" in connect_response:
            raise RuntimeError("connect failed: " + truncate_json(connect_response))
        add_step(steps, "tcp-connect", "passed", truncate_json(connect_response))

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
        require_safe_for_motion(status_before)
        non_homed_before = non_homed_axes(status_before)

        if non_homed_before:
            home_response = client.send_request("home", None, timeout_seconds=args.home_timeout)
            artifacts["home_response"] = home_response
            if "error" in home_response:
                raise RuntimeError("home failed: " + truncate_json(home_response))
            add_step(steps, "home", "passed", truncate_json(home_response))

            status_after_home = client.send_request("status", None, timeout_seconds=5.0)
            artifacts["status_after_home"] = status_after_home
            non_homed_after_home = non_homed_axes(status_after_home)
            if non_homed_after_home:
                raise RuntimeError("axes still not homed after home: " + ",".join(non_homed_after_home))
        else:
            status_after_home = status_before
            artifacts["status_after_home"] = status_after_home
            non_homed_after_home = []
            add_step(steps, "home", "passed", "already homed; no homing command sent")

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
        add_step(steps, "dxf-job-start", "passed", f"job_id={job_id}")

        deadline = time.time() + args.job_timeout
        final_job_status: dict[str, Any] | None = None
        while time.time() < deadline:
            status_response = client.send_request(
                "dxf.job.status",
                {"job_id": job_id},
                timeout_seconds=5.0,
            )
            if "error" in status_response:
                raise RuntimeError("dxf.job.status failed: " + truncate_json(status_response))
            status_result_payload = status_result(status_response)
            job_status_history.append(status_result_payload)
            state = str(status_result_payload.get("state", "")).strip()
            if state in {"completed", "failed", "cancelled"}:
                final_job_status = status_result_payload
                break
            time.sleep(0.1)

        if final_job_status is None:
            raise TimeoutError(f"job did not reach terminal state within {args.job_timeout}s")

        artifacts["final_job_status"] = final_job_status
        terminal_state = str(final_job_status.get("state", "")).strip()
        terminal_status = "passed" if terminal_state == "completed" else "failed"
        add_step(steps, "dxf-job-terminal", terminal_status, truncate_json(final_job_status))
        status_after = client.send_request("status", None, timeout_seconds=5.0)
        artifacts["status_after"] = status_after
        add_step(steps, "status-after", "passed", truncate_json(status_after))
        if terminal_state != "completed":
            raise RuntimeError("dry-run job did not complete successfully: " + truncate_json(final_job_status))
        overall_status = "passed"
        return_code = 0
        return return_code
    except Exception as exc:  # noqa: BLE001
        error_message = str(exc)
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

        report = {
            "generated_at": utc_now(),
            "workspace_root": str(ROOT),
            "gateway_exe": str(args.gateway_exe),
            "config_path": str(args.config_path),
            "dxf_file": str(args.dxf_file),
            "overall_status": overall_status,
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
                "non_homed_after_home": non_homed_axes(artifacts.get("status_after_home", {})),
                "after": summarize_safety(artifacts.get("status_after", {})),
            },
            "job_status_history": job_status_history,
            "error": error_message,
        }
        json_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        md_path.write_text(build_report_markdown(report), encoding="utf-8")
        print(f"report json: {json_path}")
        print(f"report md: {md_path}")


if __name__ == "__main__":
    raise SystemExit(main())
