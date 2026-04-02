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


@dataclass(frozen=True)
class RuntimeActionCase:
    case_id: str
    profile: str
    description: str


RUNTIME_ACTION_CASES = (
    RuntimeActionCase(
        case_id="runtime-preview",
        profile="preview",
        description="Preview canonical planned glue snapshot and capture the HMI surface.",
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


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run HMI runtime action matrix and write a summary report.")
    parser.add_argument("--report-root", type=Path, default=DEFAULT_REPORT_ROOT)
    parser.add_argument("--python-exe", default=sys.executable or "python")
    parser.add_argument("--timeout-ms", type=int, default=20000)
    parser.add_argument("--preview-payload-path", type=Path, default=None)
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
    if args.preview_payload_path is not None and case.profile == "preview":
        command.extend(["-PreviewPayloadPath", str(args.preview_payload_path)])
    if args.gateway_exe is not None:
        command.extend(["-GatewayExe", str(args.gateway_exe)])
    if args.gateway_config is not None:
        command.extend(["-GatewayConfig", str(args.gateway_config)])
    if args.use_mock_gateway_config:
        command.append("-UseMockGatewayConfig")
    return command


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
        case_status = "passed" if completed.returncode == 0 and screenshot_path.exists() else "failed"
        case_results.append(
            {
                "case_id": case.case_id,
                "profile": case.profile,
                "description": case.description,
                "status": case_status,
                "exit_code": completed.returncode,
                "command": command,
                "screenshot_path": str(screenshot_path),
                "online_smoke_log": str(log_path),
                "error": "" if case_status == "passed" else combined_output.strip(),
            }
        )

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
