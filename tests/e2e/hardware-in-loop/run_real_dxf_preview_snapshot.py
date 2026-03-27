from __future__ import annotations

import argparse
import base64
import json
import subprocess
import sys
import time
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
    prepare_mock_config,
    read_process_output,
    resolve_default_exe,
    truncate_json,
    wait_gateway_ready,
)


DEFAULT_DXF_FILE = ROOT / "samples" / "dxf" / "rect_diag.dxf"
DEFAULT_CONFIG_PATH = ROOT / "config" / "machine" / "machine_config.ini"
DEFAULT_REPORT_ROOT = ROOT / "tests" / "reports" / "adhoc" / "real-dxf-preview-snapshot-canonical"


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run canonical real DXF preview snapshot smoke without job.start.")
    parser.add_argument("--gateway-exe", type=Path, default=resolve_default_exe("siligen_runtime_gateway.exe"))
    parser.add_argument("--config-path", type=Path, default=DEFAULT_CONFIG_PATH)
    parser.add_argument("--config-mode", choices=("mock", "real"), default="mock")
    parser.add_argument("--dxf-file", type=Path, default=DEFAULT_DXF_FILE)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9527)
    parser.add_argument("--gateway-ready-timeout", type=float, default=8.0)
    parser.add_argument("--connect-timeout", type=float, default=15.0)
    parser.add_argument("--report-root", type=Path, default=DEFAULT_REPORT_ROOT)
    parser.add_argument("--dispensing-speed-mm-s", type=float, default=10.0)
    parser.add_argument("--max-polyline-points", type=int, default=4000)
    return parser.parse_args()


def ensure_exists(path: Path, label: str) -> None:
    if not path.exists():
        raise FileNotFoundError(f"{label} missing: {path}")


def status_result(payload: dict[str, Any]) -> dict[str, Any]:
    result = payload.get("result", {})
    return result if isinstance(result, dict) else {}


def extract_polyline(snapshot_payload: dict[str, Any]) -> list[dict[str, float]]:
    result = status_result(snapshot_payload)
    polyline = result.get("trajectory_polyline", [])
    output: list[dict[str, float]] = []
    if not isinstance(polyline, list):
        return output
    for point in polyline:
        if not isinstance(point, dict):
            continue
        try:
            output.append({"x": float(point.get("x")), "y": float(point.get("y"))})
        except (TypeError, ValueError):
            continue
    return output


def summarize_polyline(polyline: list[dict[str, float]]) -> dict[str, Any]:
    if not polyline:
        return {
            "point_count": 0,
            "x_range": [0.0, 0.0],
            "y_range": [0.0, 0.0],
            "first_point": None,
            "last_point": None,
            "axis_aligned_segments": 0,
            "diagonal_segments": 0,
        }

    xs = [point["x"] for point in polyline]
    ys = [point["y"] for point in polyline]
    axis_aligned_segments = 0
    diagonal_segments = 0
    for index in range(1, len(polyline)):
        prev = polyline[index - 1]
        curr = polyline[index]
        dx = abs(curr["x"] - prev["x"])
        dy = abs(curr["y"] - prev["y"])
        if dx <= 1e-3 or dy <= 1e-3:
            axis_aligned_segments += 1
        elif dx > 1e-3 and dy > 1e-3:
            diagonal_segments += 1

    return {
        "point_count": len(polyline),
        "x_range": [min(xs), max(xs)],
        "y_range": [min(ys), max(ys)],
        "first_point": polyline[0],
        "last_point": polyline[-1],
        "axis_aligned_segments": axis_aligned_segments,
        "diagonal_segments": diagonal_segments,
    }


def write_json(path: Path, payload: Any) -> None:
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def normalize_preview_source(value: Any) -> str:
    normalized = str(value or "").strip().lower()
    return normalized or "unknown"


def build_preview_verdict(
    *,
    launch_mode: str,
    online_ready: bool,
    dxf_file: Path,
    artifact_id: str,
    preview_source: str,
    snapshot_hash: str,
    plan_id: str,
    snapshot_plan_id: str,
    plan_fingerprint: str,
    polyline: list[dict[str, float]],
    snapshot_payload: dict[str, Any],
    confirmed: bool,
    error_message: str,
) -> dict[str, Any]:
    normalized_source = normalize_preview_source(preview_source)
    effective_plan_id = str(snapshot_plan_id or plan_id).strip()
    snapshot_result = snapshot_payload if isinstance(snapshot_payload, dict) else {}
    polyline_point_count = int(snapshot_result.get("polyline_point_count", len(polyline)) or 0)
    polyline_source_point_count = int(snapshot_result.get("polyline_source_point_count", 0) or 0)
    geometry_match = (
        normalized_source == "runtime_snapshot"
        and bool(polyline)
        and polyline_point_count == len(polyline)
        and polyline_source_point_count >= polyline_point_count
    )
    order_match = bool(plan_id) and bool(effective_plan_id) and plan_id == effective_plan_id
    dispatch_match = (
        bool(plan_fingerprint)
        and bool(snapshot_hash)
        and plan_fingerprint == snapshot_hash
        and confirmed
    )
    failure_reason = str(error_message or "").strip()

    if not online_ready:
        verdict = "not-ready"
        if not failure_reason:
            failure_reason = "online_ready=false"
    elif normalized_source != "runtime_snapshot":
        verdict = "invalid-source"
        if not failure_reason:
            failure_reason = f"preview_source={normalized_source}"
    elif not artifact_id or not plan_id or not effective_plan_id or not plan_fingerprint or not snapshot_hash or not polyline:
        verdict = "incomplete"
        if not failure_reason:
            failure_reason = "required evidence fields are missing"
    elif not (geometry_match and order_match and dispatch_match):
        verdict = "mismatch"
        if not failure_reason:
            failure_reason = "preview context did not correlate with prepare/confirm semantics"
    else:
        verdict = "passed"

    verdict_payload = {
        "verdict": verdict,
        "launch_mode": launch_mode,
        "online_ready": bool(online_ready),
        "dxf_file": str(dxf_file),
        "artifact_id": artifact_id,
        "preview_source": normalized_source,
        "snapshot_hash": snapshot_hash,
        "plan_id": effective_plan_id,
        "plan_fingerprint": plan_fingerprint,
        "geometry_semantics_match": bool(geometry_match),
        "order_semantics_match": bool(order_match),
        "dispense_motion_semantics_match": bool(dispatch_match),
        "polyline_point_count": polyline_point_count,
        "polyline_source_point_count": polyline_source_point_count,
        "preview_confirmed": bool(confirmed),
    }
    if failure_reason:
        verdict_payload["failure_reason"] = failure_reason
    return verdict_payload


def build_preview_evidence_markdown(
    *,
    generated_at: str,
    overall_status: str,
    plan_payload: dict[str, Any],
    snapshot_payload: dict[str, Any],
    verdict_payload: dict[str, Any],
    geometry_summary: dict[str, Any],
) -> str:
    lines: list[str] = []
    lines.append("# Online Preview Evidence Bundle")
    lines.append("")
    lines.append(f"- generated_at: `{generated_at}`")
    lines.append(f"- overall_status: `{overall_status}`")
    lines.append(f"- verdict: `{verdict_payload.get('verdict', 'incomplete')}`")
    lines.append(f"- plan_id: `{verdict_payload.get('plan_id', '')}`")
    lines.append(f"- plan_fingerprint: `{verdict_payload.get('plan_fingerprint', '')}`")
    lines.append(f"- snapshot_hash: `{verdict_payload.get('snapshot_hash', '')}`")
    lines.append(f"- preview_source: `{verdict_payload.get('preview_source', '')}`")
    lines.append("")
    lines.append("## Evidence Files")
    lines.append("")
    lines.append("- `plan-prepare.json`")
    lines.append("- `snapshot.json`")
    lines.append("- `trajectory_polyline.json`")
    lines.append("- `preview-verdict.json`")
    lines.append("- `preview-evidence.md`")
    lines.append("")
    lines.append("## Prepare Summary")
    lines.append("")
    lines.append("```json")
    lines.append(json.dumps(plan_payload, ensure_ascii=False, indent=2))
    lines.append("```")
    lines.append("")
    lines.append("## Snapshot Summary")
    lines.append("")
    lines.append("```json")
    lines.append(json.dumps(snapshot_payload, ensure_ascii=False, indent=2))
    lines.append("```")
    lines.append("")
    lines.append("## Verdict")
    lines.append("")
    lines.append("```json")
    lines.append(json.dumps(verdict_payload, ensure_ascii=False, indent=2))
    lines.append("```")
    lines.append("")
    lines.append("## Geometry Summary")
    lines.append("")
    lines.append("```json")
    lines.append(json.dumps(geometry_summary, ensure_ascii=False, indent=2))
    lines.append("```")
    return "\n".join(lines) + "\n"


def build_report_markdown(report: dict[str, Any]) -> str:
    lines: list[str] = []
    lines.append("# Real DXF Preview Snapshot Report")
    lines.append("")
    lines.append(f"- generated_at: `{report['generated_at']}`")
    lines.append(f"- overall_status: `{report['overall_status']}`")
    lines.append(f"- gateway_exe: `{report['gateway_exe']}`")
    lines.append(f"- config_path: `{report['config_path']}`")
    lines.append(f"- config_mode: `{report['config_mode']}`")
    lines.append(f"- dxf_file: `{report['dxf_file']}`")
    lines.append(f"- preview_source: `{report['preview_source']}`")
    lines.append(f"- snapshot_hash: `{report['snapshot_hash']}`")
    if report.get("plan_id"):
        lines.append(f"- plan_id: `{report['plan_id']}`")
    if report.get("plan_fingerprint"):
        lines.append(f"- plan_fingerprint: `{report['plan_fingerprint']}`")
    if report.get("preview_verdict"):
        lines.append(f"- verdict: `{report['preview_verdict'].get('verdict', 'incomplete')}`")
    lines.append("")
    lines.append("## Geometry Summary")
    lines.append("")
    lines.append("```json")
    lines.append(json.dumps(report["geometry_summary"], ensure_ascii=False, indent=2))
    lines.append("```")
    lines.append("")
    lines.append("## Steps")
    lines.append("")
    lines.append("| step | status | note |")
    lines.append("|---|---|---|")
    for step in report["steps"]:
        note = str(step["note"]).replace("\r", " ").replace("\n", " ")
        lines.append(f"| `{step['name']}` | `{step['status']}` | {note} |")
    if report.get("error"):
        lines.append("")
        lines.append("## Error")
        lines.append("")
        lines.append("```text")
        lines.append(str(report["error"]))
        lines.append("```")
    return "\n".join(lines) + "\n"


def add_step(steps: list[dict[str, str]], name: str, status: str, note: str) -> None:
    steps.append({"name": name, "status": status, "note": note, "timestamp": utc_now()})


def main() -> int:
    args = parse_args()
    ensure_exists(args.gateway_exe, "gateway executable")
    ensure_exists(args.config_path, "config")
    ensure_exists(args.dxf_file, "dxf file")

    report_dir = args.report_root / time.strftime("%Y%m%d-%H%M%S")
    report_dir.mkdir(parents=True, exist_ok=True)
    report_json_path = report_dir / "real-dxf-preview-snapshot.json"
    report_md_path = report_dir / "real-dxf-preview-snapshot.md"
    plan_prepare_json_path = report_dir / "plan-prepare.json"
    snapshot_json_path = report_dir / "snapshot.json"
    polyline_json_path = report_dir / "trajectory_polyline.json"
    preview_verdict_json_path = report_dir / "preview-verdict.json"
    preview_evidence_md_path = report_dir / "preview-evidence.md"

    steps: list[dict[str, str]] = []
    artifacts: dict[str, Any] = {}
    artifact_id = ""
    plan_id = ""
    snapshot_plan_id = ""
    plan_fingerprint = ""
    preview_source = ""
    snapshot_hash = ""
    plan_result: dict[str, Any] = {}
    snapshot_result: dict[str, Any] = {}
    confirm_result: dict[str, Any] = {}
    polyline: list[dict[str, float]] = []
    geometry_summary: dict[str, Any] = {}
    error_message = ""
    overall_status = "failed"
    return_code = 1

    process: subprocess.Popen[str] | None = None
    client = TcpJsonClient(args.host, args.port)
    connected = False
    online_ready = False
    preview_confirmed = False
    temp_dir = None
    effective_config_path = args.config_path

    try:
        if args.config_mode == "mock":
            temp_dir, effective_config_path = prepare_mock_config(prefix="real-dxf-preview-")
            add_step(steps, "config-prepare", "passed", f"mock config prepared at {effective_config_path}")
        else:
            add_step(steps, "config-prepare", "passed", f"using real config {effective_config_path}")

        process = subprocess.Popen(
            [str(args.gateway_exe), "--config", str(effective_config_path), "--port", str(args.port)],
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
            return_code = KNOWN_FAILURE_EXIT_CODE if ready_status == "known_failure" else 1
            return return_code

        client.connect(timeout_seconds=args.connect_timeout)
        connected = True
        add_step(steps, "tcp-session-open", "passed", f"connected to {args.host}:{args.port}")

        connect_response = client.send_request("connect", None, timeout_seconds=args.connect_timeout)
        artifacts["connect_response"] = connect_response
        if "error" in connect_response:
            raise RuntimeError("connect failed: " + truncate_json(connect_response))
        online_ready = True
        add_step(steps, "tcp-connect", "passed", truncate_json(connect_response))

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
                "use_hardware_trigger": False,
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
        if not plan_id:
            raise RuntimeError("dxf.plan.prepare missing plan_id")
        if not plan_fingerprint:
            raise RuntimeError("dxf.plan.prepare missing plan_fingerprint")
        write_json(plan_prepare_json_path, plan_result)
        add_step(
            steps,
            "dxf-plan-prepare",
            "passed",
            json.dumps(
                {
                    "plan_id": plan_id,
                    "plan_fingerprint": plan_fingerprint,
                    "segment_count": plan_result.get("segment_count", 0),
                },
                ensure_ascii=False,
            ),
        )

        snapshot_response = client.send_request(
            "dxf.preview.snapshot",
            {"plan_id": plan_id, "max_polyline_points": max(2, int(args.max_polyline_points))},
            timeout_seconds=15.0,
        )
        artifacts["dxf_preview_snapshot"] = snapshot_response
        if "error" in snapshot_response:
            raise RuntimeError("dxf.preview.snapshot failed: " + truncate_json(snapshot_response))
        snapshot_result = status_result(snapshot_response)
        snapshot_plan_id = str(snapshot_result.get("plan_id", "")).strip() or plan_id
        preview_source = normalize_preview_source(snapshot_result.get("preview_source", ""))
        snapshot_hash = str(snapshot_result.get("snapshot_hash", "")).strip()
        if not snapshot_hash:
            raise RuntimeError("dxf.preview.snapshot missing snapshot_hash")
        if preview_source == "unknown":
            raise RuntimeError("dxf.preview.snapshot missing preview_source")
        if preview_source != "runtime_snapshot":
            raise RuntimeError(f"unexpected preview_source: {preview_source}")
        polyline = extract_polyline(snapshot_response)
        if not polyline:
            raise RuntimeError("dxf.preview.snapshot missing trajectory_polyline")

        geometry_summary = summarize_polyline(polyline)
        write_json(snapshot_json_path, snapshot_result)
        write_json(polyline_json_path, polyline)
        add_step(
            steps,
            "dxf-preview-snapshot",
            "passed",
            json.dumps(
                {
                    "snapshot_hash": snapshot_hash,
                    "plan_id": snapshot_plan_id,
                    "plan_fingerprint": plan_fingerprint,
                    "preview_source": preview_source,
                    "polyline_point_count": snapshot_result.get("polyline_point_count", 0),
                    "geometry_summary": geometry_summary,
                },
                ensure_ascii=False,
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
        confirm_result = status_result(confirm_response)
        preview_confirmed = bool(confirm_result.get("confirmed", False))
        if not preview_confirmed:
            raise RuntimeError("dxf.preview.confirm did not confirm snapshot")
        add_step(
            steps,
            "dxf-preview-confirm",
            "passed",
            json.dumps(
                {
                    "plan_id": str(confirm_result.get("plan_id", "")).strip(),
                    "snapshot_hash": str(confirm_result.get("snapshot_hash", "")).strip(),
                    "preview_state": str(confirm_result.get("preview_state", "")).strip(),
                },
                ensure_ascii=False,
            ),
        )

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

        preview_verdict = build_preview_verdict(
            launch_mode="online",
            online_ready=online_ready,
            dxf_file=args.dxf_file,
            artifact_id=artifact_id,
            preview_source=preview_source,
            snapshot_hash=snapshot_hash,
            plan_id=plan_id,
            snapshot_plan_id=snapshot_plan_id,
            plan_fingerprint=plan_fingerprint,
            polyline=polyline,
            snapshot_payload=snapshot_result,
            confirmed=preview_confirmed,
            error_message=error_message,
        )
        write_json(preview_verdict_json_path, preview_verdict)
        preview_evidence_markdown = build_preview_evidence_markdown(
            generated_at=utc_now(),
            overall_status=overall_status,
            plan_payload=plan_result,
            snapshot_payload=snapshot_result,
            verdict_payload=preview_verdict,
            geometry_summary=geometry_summary,
        )
        preview_evidence_md_path.write_text(preview_evidence_markdown, encoding="utf-8")

        report = {
            "generated_at": utc_now(),
            "overall_status": overall_status,
            "gateway_exe": str(args.gateway_exe),
            "config_path": str(effective_config_path),
            "config_mode": args.config_mode,
            "dxf_file": str(args.dxf_file),
            "artifact_id": artifact_id,
            "plan_id": plan_id,
            "plan_fingerprint": plan_fingerprint,
            "preview_source": preview_source,
            "snapshot_hash": snapshot_hash,
            "geometry_summary": geometry_summary,
            "preview_verdict": preview_verdict,
            "steps": steps,
            "artifacts": artifacts,
            "error": error_message,
        }
        write_json(report_json_path, report)
        report_md_path.write_text(build_report_markdown(report), encoding="utf-8")

        if temp_dir is not None:
            temp_dir.cleanup()


if __name__ == "__main__":
    raise SystemExit(main())
