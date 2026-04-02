from __future__ import annotations

import argparse
import base64
import configparser
import json
import math
import statistics
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
ONLINE_SMOKE_SCRIPT = ROOT / "apps" / "hmi-app" / "scripts" / "online-smoke.ps1"
POINT_DUPLICATE_TOLERANCE_MM = 1e-4


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run canonical real DXF planned glue preview evidence flow.")
    parser.add_argument("--gateway-exe", type=Path, default=resolve_default_exe("siligen_runtime_gateway.exe"))
    parser.add_argument("--config-path", type=Path, default=DEFAULT_CONFIG_PATH)
    parser.add_argument("--config-mode", choices=("mock", "real"), default="mock")
    parser.add_argument("--dxf-file", type=Path, default=DEFAULT_DXF_FILE)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9527)
    parser.add_argument("--gateway-ready-timeout", type=float, default=30.0)
    parser.add_argument("--connect-timeout", type=float, default=15.0)
    parser.add_argument("--report-root", type=Path, default=DEFAULT_REPORT_ROOT)
    parser.add_argument("--dispensing-speed-mm-s", type=float, default=10.0)
    parser.add_argument("--max-polyline-points", type=int, default=4000)
    parser.add_argument("--hmi-smoke-timeout-ms", type=int, default=45000)
    return parser.parse_args()


def ensure_exists(path: Path, label: str) -> None:
    if not path.exists():
        raise FileNotFoundError(f"{label} missing: {path}")


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


def normalize_preview_source(value: Any) -> str:
    normalized = str(value or "").strip().lower()
    return normalized or "unknown"


def normalize_preview_kind(value: Any) -> str:
    normalized = str(value or "").strip().lower()
    return normalized or "unknown"


def write_json(path: Path, payload: Any) -> None:
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def extract_points(snapshot_payload: dict[str, Any], field_name: str) -> list[dict[str, float]]:
    result = status_result(snapshot_payload)
    raw_points = result.get(field_name, [])
    return extract_points_from_raw(raw_points)


def extract_points_from_raw(raw_points: Any) -> list[dict[str, float]]:
    output: list[dict[str, float]] = []
    if not isinstance(raw_points, list):
        return output
    for point in raw_points:
        if not isinstance(point, dict):
            continue
        try:
            output.append({"x": float(point.get("x")), "y": float(point.get("y"))})
        except (TypeError, ValueError):
            continue
    return output


def extract_motion_preview(snapshot_payload: dict[str, Any]) -> tuple[dict[str, Any], list[dict[str, float]]]:
    result = status_result(snapshot_payload)
    motion_preview = result.get("motion_preview", {})
    if not isinstance(motion_preview, dict):
        return {}, []
    return motion_preview, extract_points_from_raw(motion_preview.get("polyline", []))


def distance_mm(point_a: dict[str, float], point_b: dict[str, float]) -> float:
    return math.hypot(point_b["x"] - point_a["x"], point_b["y"] - point_a["y"])


def summarize_point_cloud(points: list[dict[str, float]]) -> dict[str, Any]:
    if not points:
        return {
            "point_count": 0,
            "x_range": [0.0, 0.0],
            "y_range": [0.0, 0.0],
            "first_point": None,
            "last_point": None,
        }

    xs = [point["x"] for point in points]
    ys = [point["y"] for point in points]
    return {
        "point_count": len(points),
        "x_range": [min(xs), max(xs)],
        "y_range": [min(ys), max(ys)],
        "first_point": points[0],
        "last_point": points[-1],
    }


def summarize_execution_polyline(points: list[dict[str, float]]) -> dict[str, Any]:
    summary = summarize_point_cloud(points)
    if not points:
        summary.update({"axis_aligned_segments": 0, "diagonal_segments": 0})
        return summary

    axis_aligned_segments = 0
    diagonal_segments = 0
    for index in range(1, len(points)):
        prev = points[index - 1]
        curr = points[index]
        dx = abs(curr["x"] - prev["x"])
        dy = abs(curr["y"] - prev["y"])
        if dx <= 1e-3 or dy <= 1e-3:
            axis_aligned_segments += 1
        elif dx > 1e-3 and dy > 1e-3:
            diagonal_segments += 1

    summary.update(
        {
            "axis_aligned_segments": axis_aligned_segments,
            "diagonal_segments": diagonal_segments,
        }
    )
    return summary


def summarize_glue_points(points: list[dict[str, float]], duplicate_tolerance_mm: float) -> dict[str, Any]:
    summary = summarize_point_cloud(points)
    if len(points) < 2:
        summary.update(
            {
                "adjacent_distance_median_mm": 0.0,
                "adjacent_distance_min_mm": 0.0,
                "adjacent_distance_max_mm": 0.0,
                "consecutive_duplicate_pairs": 0,
                "corner_duplicate_point_count": 0,
            }
        )
        return summary

    distances = [distance_mm(points[index - 1], points[index]) for index in range(1, len(points))]
    duplicate_pairs = sum(1 for value in distances if value <= duplicate_tolerance_mm)
    summary.update(
        {
            "adjacent_distance_median_mm": float(statistics.median(distances)),
            "adjacent_distance_min_mm": float(min(distances)),
            "adjacent_distance_max_mm": float(max(distances)),
            "consecutive_duplicate_pairs": duplicate_pairs,
            "corner_duplicate_point_count": duplicate_pairs,
        }
    )
    return summary


def build_preview_verdict(
    *,
    launch_mode: str,
    online_ready: bool,
    dxf_file: Path,
    artifact_id: str,
    preview_source: str,
    preview_kind: str,
    snapshot_hash: str,
    plan_id: str,
    snapshot_plan_id: str,
    plan_fingerprint: str,
    glue_summary: dict[str, Any],
    motion_preview_payload: dict[str, Any],
    motion_preview_summary: dict[str, Any],
    execution_summary: dict[str, Any],
    snapshot_payload: dict[str, Any],
    confirmed: bool,
    error_message: str,
) -> dict[str, Any]:
    normalized_source = normalize_preview_source(preview_source)
    normalized_kind = normalize_preview_kind(preview_kind)
    effective_plan_id = str(snapshot_plan_id or plan_id).strip()
    snapshot_result = snapshot_payload if isinstance(snapshot_payload, dict) else {}
    glue_point_count = int(
        snapshot_result.get("glue_point_count", snapshot_result.get("point_count", glue_summary["point_count"])) or 0
    )
    normalized_motion_preview_source = normalize_preview_source(motion_preview_payload.get("source", ""))
    normalized_motion_preview_kind = normalize_preview_kind(motion_preview_payload.get("kind", ""))
    motion_preview_point_count = int(
        motion_preview_payload.get("point_count", motion_preview_summary["point_count"]) or 0
    )
    motion_preview_source_point_count = int(motion_preview_payload.get("source_point_count", 0) or 0)
    motion_preview_is_sampled = bool(
        motion_preview_payload.get(
            "is_sampled",
            motion_preview_point_count < motion_preview_source_point_count if motion_preview_source_point_count > 0 else False,
        )
    )
    motion_preview_sampling_strategy = str(motion_preview_payload.get("sampling_strategy", "")).strip()
    execution_polyline_point_count = int(
        snapshot_result.get("execution_polyline_point_count", execution_summary["point_count"]) or 0
    )
    execution_polyline_source_point_count = int(
        snapshot_result.get(
            "execution_polyline_source_point_count",
            snapshot_result.get("polyline_source_point_count", 0),
        )
        or 0
    )
    geometry_match = (
        normalized_source == "planned_glue_snapshot"
        and normalized_kind == "glue_points"
        and glue_point_count == int(glue_summary["point_count"])
        and glue_point_count > 0
        and normalized_motion_preview_kind == "polyline"
        and motion_preview_point_count == int(motion_preview_summary["point_count"])
        and motion_preview_point_count > 0
        and motion_preview_source_point_count >= motion_preview_point_count
        and int(glue_summary["corner_duplicate_point_count"]) == 0
        and (
            motion_preview_source_point_count == 0
            or glue_point_count < motion_preview_source_point_count
        )
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
    elif normalized_source != "planned_glue_snapshot" or normalized_kind != "glue_points":
        verdict = "invalid-source"
        if not failure_reason:
            failure_reason = f"preview_source={normalized_source} preview_kind={normalized_kind}"
    elif (
        not artifact_id
        or not plan_id
        or not effective_plan_id
        or not plan_fingerprint
        or not snapshot_hash
        or glue_point_count <= 0
        or motion_preview_point_count <= 0
    ):
        verdict = "incomplete"
        if not failure_reason:
            failure_reason = "required evidence fields are missing"
    elif not (geometry_match and order_match and dispatch_match):
        verdict = "mismatch"
        if not failure_reason:
            failure_reason = "preview context did not correlate with planned glue snapshot semantics"
    else:
        verdict = "passed"

    verdict_payload = {
        "verdict": verdict,
        "launch_mode": launch_mode,
        "online_ready": bool(online_ready),
        "dxf_file": str(dxf_file),
        "artifact_id": artifact_id,
        "preview_source": normalized_source,
        "preview_kind": normalized_kind,
        "snapshot_hash": snapshot_hash,
        "plan_id": effective_plan_id,
        "plan_fingerprint": plan_fingerprint,
        "geometry_semantics_match": bool(geometry_match),
        "order_semantics_match": bool(order_match),
        "dispense_motion_semantics_match": bool(dispatch_match),
        "glue_point_count": glue_point_count,
        "motion_preview_source": normalized_motion_preview_source,
        "motion_preview_kind": normalized_motion_preview_kind,
        "motion_preview_point_count": motion_preview_point_count,
        "motion_preview_source_point_count": motion_preview_source_point_count,
        "motion_preview_is_sampled": bool(motion_preview_is_sampled),
        "motion_preview_sampling_strategy": motion_preview_sampling_strategy,
        "execution_polyline_point_count": execution_polyline_point_count,
        "execution_polyline_source_point_count": execution_polyline_source_point_count,
        "glue_point_spacing_median_mm": float(glue_summary["adjacent_distance_median_mm"]),
        "corner_duplicate_point_count": int(glue_summary["corner_duplicate_point_count"]),
        "preview_confirmed": bool(confirmed),
    }
    if failure_reason and verdict != "passed":
        verdict_payload["failure_reason"] = failure_reason
    return verdict_payload


def build_preview_evidence_markdown(
    *,
    generated_at: str,
    overall_status: str,
    plan_payload: dict[str, Any],
    snapshot_payload: dict[str, Any],
    verdict_payload: dict[str, Any],
    glue_summary: dict[str, Any],
    motion_preview_summary: dict[str, Any],
    execution_summary: dict[str, Any],
    hmi_screenshot_path: Path,
    online_smoke_log_path: Path,
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
    lines.append(f"- preview_kind: `{verdict_payload.get('preview_kind', '')}`")
    lines.append("")
    lines.append("## Evidence Files")
    lines.append("")
    lines.append("- `plan-prepare.json`")
    lines.append("- `snapshot.json`")
    lines.append("- `glue_points.json`")
    lines.append("- `motion_preview.json`")
    lines.append("- `execution_polyline.json`")
    lines.append("- `preview-verdict.json`")
    lines.append("- `preview-evidence.md`")
    if hmi_screenshot_path.exists():
        lines.append(f"- `{hmi_screenshot_path.name}`")
    if online_smoke_log_path.exists():
        lines.append(f"- `{online_smoke_log_path.name}`")
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
    lines.append("## Glue Summary")
    lines.append("")
    lines.append("```json")
    lines.append(json.dumps(glue_summary, ensure_ascii=False, indent=2))
    lines.append("```")
    lines.append("")
    lines.append("## Motion Preview Summary")
    lines.append("")
    lines.append("```json")
    lines.append(json.dumps(motion_preview_summary, ensure_ascii=False, indent=2))
    lines.append("```")
    lines.append("")
    lines.append("## Execution Polyline Summary")
    lines.append("")
    lines.append("```json")
    lines.append(json.dumps(execution_summary, ensure_ascii=False, indent=2))
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
    lines.append(f"- preview_kind: `{report['preview_kind']}`")
    lines.append(f"- snapshot_hash: `{report['snapshot_hash']}`")
    if report.get("plan_id"):
        lines.append(f"- plan_id: `{report['plan_id']}`")
    if report.get("plan_fingerprint"):
        lines.append(f"- plan_fingerprint: `{report['plan_fingerprint']}`")
    if report.get("preview_verdict"):
        lines.append(
            f"- motion_preview_source: `{report['preview_verdict'].get('motion_preview_source', '')}`"
        )
        lines.append(
            f"- motion_preview_kind: `{report['preview_verdict'].get('motion_preview_kind', '')}`"
        )
    if report.get("preview_verdict"):
        lines.append(f"- verdict: `{report['preview_verdict'].get('verdict', 'incomplete')}`")
    if report.get("hmi_screenshot"):
        lines.append(f"- hmi_screenshot: `{report['hmi_screenshot']}`")
    lines.append("")
    lines.append("## Glue Summary")
    lines.append("")
    lines.append("```json")
    lines.append(json.dumps(report["glue_summary"], ensure_ascii=False, indent=2))
    lines.append("```")
    lines.append("")
    lines.append("## Motion Preview Summary")
    lines.append("")
    lines.append("```json")
    lines.append(json.dumps(report["motion_preview_summary"], ensure_ascii=False, indent=2))
    lines.append("```")
    lines.append("")
    lines.append("## Execution Polyline Summary")
    lines.append("")
    lines.append("```json")
    lines.append(json.dumps(report["execution_geometry_summary"], ensure_ascii=False, indent=2))
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


def run_hmi_online_smoke(
    *,
    args: argparse.Namespace,
    screenshot_path: Path,
    log_path: Path,
    preview_payload_path: Path,
) -> None:
    ensure_exists(ONLINE_SMOKE_SCRIPT, "online smoke script")

    command = [
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        str(ONLINE_SMOKE_SCRIPT),
        "-TimeoutMs",
        str(args.hmi_smoke_timeout_ms),
        "-GatewayExe",
        str(args.gateway_exe),
        "-GatewayConfig",
        str(args.config_path),
        "-ExerciseRuntimeActions",
        "-ScreenshotPath",
        str(screenshot_path),
        "-PreviewPayloadPath",
        str(preview_payload_path),
    ]
    if args.config_mode == "mock":
        command.append("-UseMockGatewayConfig")

    completed = subprocess.run(
        command,
        cwd=str(ROOT),
        capture_output=True,
        text=True,
        timeout=max(180, int(args.hmi_smoke_timeout_ms / 1000) + 60),
        check=False,
    )
    combined_output = (completed.stdout or "") + ("\n" if completed.stderr else "") + (completed.stderr or "")
    log_path.write_text(combined_output, encoding="utf-8")

    if completed.returncode != 0:
        raise RuntimeError(
            "online-smoke failed: "
            f"exit_code={completed.returncode}\n{combined_output.strip()}"
        )
    if not screenshot_path.exists():
        raise RuntimeError(f"online-smoke succeeded but screenshot is missing: {screenshot_path}")


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
    glue_points_json_path = report_dir / "glue_points.json"
    motion_preview_json_path = report_dir / "motion_preview.json"
    execution_polyline_json_path = report_dir / "execution_polyline.json"
    preview_verdict_json_path = report_dir / "preview-verdict.json"
    preview_evidence_md_path = report_dir / "preview-evidence.md"
    hmi_screenshot_path = report_dir / "hmi-preview.png"
    online_smoke_log_path = report_dir / "online-smoke.log"

    steps: list[dict[str, str]] = []
    artifacts: dict[str, Any] = {}
    artifact_id = ""
    plan_id = ""
    snapshot_plan_id = ""
    plan_fingerprint = ""
    preview_source = ""
    preview_kind = ""
    snapshot_hash = ""
    plan_result: dict[str, Any] = {}
    snapshot_result: dict[str, Any] = {}
    glue_points: list[dict[str, float]] = []
    motion_preview_payload: dict[str, Any] = {}
    motion_preview: list[dict[str, float]] = []
    execution_polyline: list[dict[str, float]] = []
    glue_summary = summarize_glue_points([], POINT_DUPLICATE_TOLERANCE_MM)
    motion_preview_summary = summarize_execution_polyline([])
    execution_summary = summarize_execution_polyline([])
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

        connect_params = load_connection_params(Path(effective_config_path))
        connect_response = client.send_request("connect", connect_params, timeout_seconds=args.connect_timeout)
        artifacts["connect_response"] = connect_response
        if "error" in connect_response:
            raise RuntimeError("connect failed: " + truncate_json(connect_response))
        online_ready = True
        add_step(
            steps,
            "tcp-connect",
            "passed",
            json.dumps({"params": connect_params, "result": status_result(connect_response)}, ensure_ascii=False),
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
        preview_kind = normalize_preview_kind(snapshot_result.get("preview_kind", ""))
        snapshot_hash = str(snapshot_result.get("snapshot_hash", "")).strip()
        if not snapshot_hash:
            raise RuntimeError("dxf.preview.snapshot missing snapshot_hash")
        if preview_source != "planned_glue_snapshot":
            raise RuntimeError(f"unexpected preview_source: {preview_source}")
        if preview_kind != "glue_points":
            raise RuntimeError(f"unexpected preview_kind: {preview_kind}")

        glue_points = extract_points(snapshot_response, "glue_points")
        motion_preview_payload, motion_preview = extract_motion_preview(snapshot_response)
        execution_polyline = extract_points(snapshot_response, "execution_polyline")
        motion_preview_source = normalize_preview_source(motion_preview_payload.get("source", ""))
        motion_preview_kind = normalize_preview_kind(motion_preview_payload.get("kind", ""))
        if not glue_points:
            raise RuntimeError("dxf.preview.snapshot missing glue_points")
        if motion_preview_source == "unknown":
            raise RuntimeError("dxf.preview.snapshot missing motion_preview.source")
        if motion_preview_kind != "polyline":
            raise RuntimeError(f"unexpected motion_preview.kind: {motion_preview_kind}")
        if not motion_preview:
            raise RuntimeError("dxf.preview.snapshot missing motion_preview.polyline")

        glue_summary = summarize_glue_points(glue_points, POINT_DUPLICATE_TOLERANCE_MM)
        motion_preview_summary = summarize_execution_polyline(motion_preview)
        execution_summary = summarize_execution_polyline(execution_polyline)
        write_json(snapshot_json_path, snapshot_result)
        write_json(glue_points_json_path, glue_points)
        write_json(motion_preview_json_path, motion_preview)
        write_json(execution_polyline_json_path, execution_polyline)
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
                    "preview_kind": preview_kind,
                    "glue_point_count": snapshot_result.get("glue_point_count", len(glue_points)),
                    "motion_preview_source": motion_preview_source,
                    "motion_preview_kind": motion_preview_kind,
                    "motion_preview_point_count": motion_preview_payload.get("point_count", len(motion_preview)),
                    "motion_preview_source_point_count": motion_preview_payload.get("source_point_count", 0),
                    "motion_preview_sampling_strategy": motion_preview_payload.get("sampling_strategy", ""),
                    "execution_polyline_source_point_count": snapshot_result.get(
                        "execution_polyline_source_point_count",
                        0,
                    ),
                    "glue_point_spacing_median_mm": glue_summary["adjacent_distance_median_mm"],
                    "corner_duplicate_point_count": glue_summary["corner_duplicate_point_count"],
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
        return_code = 1
        return return_code
    finally:
        if connected:
            try:
                disconnect_response = client.send_request("disconnect", {}, timeout_seconds=5.0)
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

        if temp_dir is not None:
            temp_dir.cleanup()

        if return_code == 0:
            try:
                run_hmi_online_smoke(
                    args=args,
                    screenshot_path=hmi_screenshot_path,
                    log_path=online_smoke_log_path,
                    preview_payload_path=snapshot_json_path,
                )
                add_step(
                    steps,
                    "hmi-online-smoke",
                    "passed",
                    json.dumps(
                        {
                            "screenshot_path": str(hmi_screenshot_path),
                            "log_path": str(online_smoke_log_path),
                        },
                        ensure_ascii=False,
                    ),
                )
            except Exception as exc:  # noqa: BLE001
                error_message = str(exc)
                overall_status = "failed"
                return_code = 1
                add_step(steps, "hmi-online-smoke", "failed", error_message)

        preview_verdict = build_preview_verdict(
            launch_mode="online",
            online_ready=online_ready,
            dxf_file=args.dxf_file,
            artifact_id=artifact_id,
            preview_source=preview_source,
            preview_kind=preview_kind,
            snapshot_hash=snapshot_hash,
            plan_id=plan_id,
            snapshot_plan_id=snapshot_plan_id,
            plan_fingerprint=plan_fingerprint,
            glue_summary=glue_summary,
            motion_preview_payload=motion_preview_payload,
            motion_preview_summary=motion_preview_summary,
            execution_summary=execution_summary,
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
            glue_summary=glue_summary,
            motion_preview_summary=motion_preview_summary,
            execution_summary=execution_summary,
            hmi_screenshot_path=hmi_screenshot_path,
            online_smoke_log_path=online_smoke_log_path,
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
            "preview_kind": preview_kind,
            "snapshot_hash": snapshot_hash,
            "glue_summary": glue_summary,
            "motion_preview_summary": motion_preview_summary,
            "execution_geometry_summary": execution_summary,
            "preview_verdict": preview_verdict,
            "steps": steps,
            "artifacts": artifacts,
            "hmi_screenshot": str(hmi_screenshot_path) if hmi_screenshot_path.exists() else "",
            "online_smoke_log": str(online_smoke_log_path) if online_smoke_log_path.exists() else "",
            "error": error_message,
        }
        write_json(report_json_path, report)
        report_md_path.write_text(build_report_markdown(report), encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
