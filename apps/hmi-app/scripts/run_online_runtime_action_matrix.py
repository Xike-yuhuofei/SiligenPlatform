from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[3]
ONLINE_SMOKE_SCRIPT = ROOT / "apps" / "hmi-app" / "scripts" / "online-smoke.ps1"
DEFAULT_REPORT_ROOT = ROOT / "tests" / "reports" / "adhoc" / "hmi-runtime-action-matrix"
DEFAULT_OPERATOR_PREVIEW_DXF = ROOT / "samples" / "dxf" / "rect_diag.dxf"


@dataclass(frozen=True)
class RuntimeActionCase:
    case_id: str
    profile: str
    description: str


RUNTIME_ACTION_CASES = (
    RuntimeActionCase(
        case_id="operator-preview",
        profile="operator_preview",
        description="Drive the runtime-owned production baseline preview journey and capture the HMI surface.",
    ),
    RuntimeActionCase(
        case_id="runtime-home-move",
        profile="home_move",
        description="Home the runtime axes and execute a bounded move_to action.",
    ),
    RuntimeActionCase(
        case_id="runtime-jog",
        profile="jog",
        description="Execute a bounded jog action and verify position changes.",
    ),
    RuntimeActionCase(
        case_id="runtime-supply-dispenser",
        profile="supply_dispenser",
        description="Open supply, start/stop dispenser, then close supply.",
    ),
    RuntimeActionCase(
        case_id="runtime-estop-reset",
        profile="estop_reset",
        description="Trigger estop and verify the reset path recovers the runtime surface.",
    ),
    RuntimeActionCase(
        case_id="runtime-door-interlock",
        profile="door_interlock",
        description="Assert door interlock blocks motion and clears after mock I/O reset.",
    ),
)
OPERATOR_PREVIEW_REQUIRED_STAGES = (
    "preview-ready",
    "preview-refreshed",
)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run HMI runtime action matrix and write a summary report.")
    parser.add_argument("--report-root", type=Path, default=DEFAULT_REPORT_ROOT)
    parser.add_argument("--python-exe", default=sys.executable or "python")
    parser.add_argument("--timeout-ms", type=int, default=20000)
    parser.add_argument("--gateway-exe", type=Path, default=None)
    parser.add_argument("--gateway-config", type=Path, default=None)
    parser.add_argument("--use-mock-gateway-config", action="store_true")
    return parser.parse_args()


def build_command(
    *,
    args: argparse.Namespace,
    case: RuntimeActionCase,
    screenshot_path: Path,
) -> list[str]:
    command = [
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        str(ONLINE_SMOKE_SCRIPT),
        "-PythonExe",
        args.python_exe,
        "-TimeoutMs",
        str(args.timeout_ms),
        "-ExerciseRuntimeActions",
        "-RuntimeActionProfile",
        case.profile,
        "-ScreenshotPath",
        str(screenshot_path),
    ]
    if args.gateway_exe is not None:
        command.extend(["-GatewayExe", str(args.gateway_exe)])
    if args.gateway_config is not None:
        command.extend(["-GatewayConfig", str(args.gateway_config)])
    if args.use_mock_gateway_config:
        command.append("-UseMockGatewayConfig")
    if case.profile == "operator_preview":
        command.extend(["-DxfBrowsePath", str(DEFAULT_OPERATOR_PREVIEW_DXF)])
    return command


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


def _contains_stage_sequence(stages: list[str], required_stages: tuple[str, ...]) -> bool:
    cursor = 0
    for stage in stages:
        if cursor >= len(required_stages):
            break
        if stage == required_stages[cursor]:
            cursor += 1
    return cursor == len(required_stages)


def summarize_operator_context(output: str) -> dict[str, Any]:
    events = [
        _parse_key_value_line(prefix="OPERATOR_CONTEXT ", line=line.strip())
        for line in output.splitlines()
    ]
    events = [event for event in events if event]
    stages = [str(event.get("stage", "")).strip() for event in events if str(event.get("stage", "")).strip()]
    by_stage = {stage: event for stage, event in ((event.get("stage", ""), event) for event in events) if stage}
    preview_ready = by_stage.get("preview-ready", {})
    preview_refreshed = by_stage.get("preview-refreshed", {})

    artifact_id = str(preview_refreshed.get("artifact_id") or preview_ready.get("artifact_id") or "null")
    plan_id = str(preview_refreshed.get("plan_id") or preview_ready.get("plan_id") or "null")
    snapshot_hash = str(preview_refreshed.get("snapshot_hash") or preview_ready.get("snapshot_hash") or "null")
    preview_source = str(preview_refreshed.get("preview_source") or preview_ready.get("preview_source") or "null")
    snapshot_ready = str(preview_refreshed.get("snapshot_ready") or preview_ready.get("snapshot_ready") or "false")
    preview_ready_observed = "preview-ready" in stages
    preview_refresh_observed = "preview-refreshed" in stages
    stage_sequence_ok = _contains_stage_sequence(stages, OPERATOR_PREVIEW_REQUIRED_STAGES)
    contract_ok = (
        stage_sequence_ok
        and preview_ready_observed
        and preview_refresh_observed
        and artifact_id not in {"", "null"}
        and plan_id not in {"", "null"}
        and snapshot_hash not in {"", "null"}
        and preview_source == "planned_glue_snapshot"
        and snapshot_ready == "true"
    )
    contract_error = ""
    if not contract_ok:
        contract_error = (
            "operator preview contract missing required OPERATOR_CONTEXT sequence or baseline-backed preview evidence; "
            f"observed_stages={stages}"
        )

    return {
        "artifact_id": artifact_id,
        "plan_id": plan_id,
        "snapshot_hash": snapshot_hash,
        "preview_source": preview_source,
        "snapshot_ready": snapshot_ready == "true",
        "preview_ready_observed": preview_ready_observed,
        "preview_refresh_observed": preview_refresh_observed,
        "operator_context_stages": stages,
        "stage_sequence_ok": stage_sequence_ok,
        "contract_ok": contract_ok,
        "contract_error": contract_error,
    }


def evaluate_case_result(
    *,
    case: RuntimeActionCase,
    exit_code: int,
    screenshot_path: Path,
    combined_output: str,
) -> tuple[str, dict[str, Any], str]:
    screenshot_exists = screenshot_path.exists()
    diagnostics: dict[str, Any] = {}
    status = "passed" if exit_code == 0 and screenshot_exists else "failed"

    if case.profile == "operator_preview":
        diagnostics = summarize_operator_context(combined_output)
        if not diagnostics.get("contract_ok", False):
            status = "failed"

    error = ""
    if status != "passed":
        reasons: list[str] = []
        if exit_code != 0:
            reasons.append(f"online-smoke exit_code={exit_code}")
        if not screenshot_exists:
            reasons.append("screenshot missing")
        contract_error = str(diagnostics.get("contract_error", "")).strip()
        if contract_error:
            reasons.append(contract_error)
        detail = combined_output.strip()
        error = "; ".join(reasons)
        if detail:
            error = f"{error}\n{detail}" if error else detail

    return status, diagnostics, error


def write_report(report_dir: Path, report: dict[str, Any]) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "online-runtime-action-matrix-summary.json"
    md_path = report_dir / "online-runtime-action-matrix-summary.md"
    json_path.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

    counts = report["counts"]
    lines = [
        "# Online Runtime Action Matrix Summary",
        "",
        f"- generated_at: `{report['generated_at']}`",
        f"- overall_status: `{report['overall_status']}`",
        f"- cases_requested: `{counts['requested']}`",
        f"- cases_passed: `{counts['passed']}`",
        f"- cases_failed: `{counts['failed']}`",
        "",
        "## Cases",
        "",
    ]
    for case in report["cases"]:
        lines.append(
            f"- `{case['status']}` `{case['case_id']}` profile=`{case['profile']}` exit_code=`{case['exit_code']}`"
        )
        lines.append(f"  screenshot: `{case['screenshot_path']}`")
        lines.append(f"  log: `{case['online_smoke_log']}`")
        if case.get("operator_context_stages"):
            lines.append(f"  operator_context_stages: `{case['operator_context_stages']}`")
            lines.append(f"  artifact_id: `{case.get('artifact_id', 'null')}`")
            lines.append(f"  plan_id: `{case.get('plan_id', 'null')}`")
            lines.append(f"  snapshot_hash: `{case.get('snapshot_hash', 'null')}`")
            lines.append(f"  preview_source: `{case.get('preview_source', 'null')}`")
        if case.get("error"):
            lines.append(f"  error: `{case['error']}`")
    lines.append("")
    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def main() -> int:
    args = parse_args()
    timestamp = time.strftime("%Y%m%d-%H%M%S")
    report_dir = args.report_root / timestamp
    case_results: list[dict[str, Any]] = []

    for case in RUNTIME_ACTION_CASES:
        case_dir = report_dir / case.case_id
        case_dir.mkdir(parents=True, exist_ok=True)
        screenshot_path = case_dir / f"{case.case_id}.png"
        log_path = case_dir / "online-smoke.log"
        command = build_command(args=args, case=case, screenshot_path=screenshot_path)
        completed = subprocess.run(
            command,
            cwd=str(ROOT),
            capture_output=True,
            text=True,
            timeout=max(180, int(args.timeout_ms / 1000) + 60),
            check=False,
        )
        combined_output = (completed.stdout or "") + ("\n" if completed.stderr else "") + (completed.stderr or "")
        log_path.write_text(combined_output, encoding="utf-8")
        case_status, diagnostics, error = evaluate_case_result(
            case=case,
            exit_code=completed.returncode,
            screenshot_path=screenshot_path,
            combined_output=combined_output,
        )
        case_payload = {
            "case_id": case.case_id,
            "profile": case.profile,
            "description": case.description,
            "status": case_status,
            "exit_code": completed.returncode,
            "command": command,
            "screenshot_path": str(screenshot_path),
            "online_smoke_log": str(log_path),
            "error": error,
        }
        case_payload.update(diagnostics)
        case_results.append(case_payload)

    passed = sum(1 for item in case_results if item["status"] == "passed")
    failed = len(case_results) - passed
    overall_status = "passed" if failed == 0 else "failed"
    report = {
        "generated_at": utc_now(),
        "workspace_root": str(ROOT),
        "overall_status": overall_status,
        "counts": {
            "requested": len(case_results),
            "passed": passed,
            "failed": failed,
        },
        "cases": case_results,
    }
    json_path, md_path = write_report(report_dir, report)
    print(f"online runtime action matrix complete: status={overall_status}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")
    return 0 if overall_status == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
