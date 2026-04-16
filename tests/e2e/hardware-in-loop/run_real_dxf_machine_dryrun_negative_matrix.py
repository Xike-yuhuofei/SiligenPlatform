from __future__ import annotations

import argparse
import json
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
    SKIPPED_EXIT_CODE,
    prepare_mock_config,
    resolve_default_exe,
)


DRYRUN_SCRIPT = ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_real_dxf_machine_dryrun.py"
DEFAULT_REPORT_ROOT = ROOT / "tests" / "reports" / "adhoc" / "real-dxf-machine-dryrun-negative-matrix"
CASE_REPORT_NAME = "real-dxf-machine-dryrun-canonical.json"
SAFETY_KEYS = (
    "estop",
    "door_open",
    "limit_x_pos",
    "limit_x_neg",
    "limit_y_pos",
    "limit_y_neg",
    "home_boundary_x_active",
    "home_boundary_y_active",
)


@dataclass(frozen=True)
class NegativeCase:
    case_id: str
    description: str
    mock_io: dict[str, bool]
    expected_blockers: tuple[str, ...]


NEGATIVE_CASES = (
    NegativeCase(
        case_id="dryrun-estop-active",
        description="Preflight blocks canonical dry-run when estop is active.",
        mock_io={"estop": True},
        expected_blockers=("estop",),
    ),
    NegativeCase(
        case_id="dryrun-door-open",
        description="Preflight blocks canonical dry-run when the safety door is open.",
        mock_io={"door": True},
        expected_blockers=("door_open",),
    ),
    NegativeCase(
        case_id="dryrun-limit-x-neg",
        description="Preflight blocks canonical dry-run when X HOME boundary is active.",
        mock_io={"limit_x_neg": True},
        expected_blockers=("home_boundary_x_active",),
    ),
    NegativeCase(
        case_id="dryrun-limit-y-neg",
        description="Preflight blocks canonical dry-run when Y HOME boundary is active.",
        mock_io={"limit_y_neg": True},
        expected_blockers=("home_boundary_y_active",),
    ),
)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run canonical dry-run negative preflight matrix and write a summary.")
    parser.add_argument(
        "--gateway-exe",
        type=Path,
        default=resolve_default_exe("siligen_runtime_gateway.exe", "siligen_tcp_server.exe"),
    )
    parser.add_argument("--python-exe", default=sys.executable or "python")
    parser.add_argument("--report-root", type=Path, default=DEFAULT_REPORT_ROOT)
    parser.add_argument("--dxf-file", type=Path, default=ROOT / "samples" / "dxf" / "rect_diag.dxf")
    parser.add_argument("--recipe-id", required=True)
    parser.add_argument("--version-id", required=True)
    parser.add_argument("--allow-skip-on-missing-gateway", action="store_true")
    return parser.parse_args()


def allocate_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def load_child_report(case_report_root: Path) -> tuple[Path, dict[str, Any]]:
    timestamp_dirs = sorted(path for path in case_report_root.iterdir() if path.is_dir())
    if not timestamp_dirs:
        raise FileNotFoundError(f"matrix case did not create a timestamped report dir: {case_report_root}")
    report_dir = timestamp_dirs[-1]
    json_path = report_dir / CASE_REPORT_NAME
    if not json_path.exists():
        raise FileNotFoundError(f"matrix case report missing: {json_path}")
    return report_dir, json.loads(json_path.read_text(encoding="utf-8"))


def evaluate_case(case: NegativeCase, *, exit_code: int, report: dict[str, Any]) -> tuple[str, dict[str, Any]]:
    safety_before = report.get("safety", {}).get("before", {})
    if not isinstance(safety_before, dict):
        safety_before = {}
    steps = report.get("steps", [])
    if not isinstance(steps, list):
        steps = []
    step_names = tuple(str(step.get("name", "")) for step in steps if isinstance(step, dict))
    observed_blockers = tuple(key for key in SAFETY_KEYS if bool(safety_before.get(key, False)))
    error_text = str(report.get("error", "")).strip()
    preflight_blocked = error_text.startswith("unsafe machine state before dry-run:")
    canonical_steps_started = any(name.startswith("dxf-") for name in step_names)
    blockers_match = all(blocker in observed_blockers for blocker in case.expected_blockers)
    status = "passed" if exit_code != 0 and preflight_blocked and blockers_match and not canonical_steps_started else "failed"
    detail = {
        "expected_blockers": case.expected_blockers,
        "observed_blockers": observed_blockers,
        "preflight_blocked": preflight_blocked,
        "canonical_steps_started": canonical_steps_started,
        "error": error_text,
        "step_names": step_names,
    }
    return status, detail


def write_report(report_dir: Path, report: dict[str, Any]) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "real-dxf-machine-dryrun-negative-matrix.json"
    md_path = report_dir / "real-dxf-machine-dryrun-negative-matrix.md"
    json_path.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

    counts = report["counts"]
    lines = [
        "# Real DXF Machine Dry-Run Negative Matrix",
        "",
        f"- generated_at: `{report['generated_at']}`",
        f"- overall_status: `{report['overall_status']}`",
        f"- gateway_exe: `{report['gateway_exe']}`",
        f"- dxf_file: `{report['dxf_file']}`",
        f"- cases_requested: `{counts['requested']}`",
        f"- cases_passed: `{counts['passed']}`",
        f"- cases_failed: `{counts['failed']}`",
        "",
        "## Cases",
        "",
    ]
    for case in report["cases"]:
        lines.append(
            f"- `{case['status']}` `{case['case_id']}` blockers=`{','.join(case['expected_blockers'])}` "
            f"observed=`{','.join(case['observed_blockers'])}` exit_code=`{case['exit_code']}`"
        )
        lines.append(f"  report_dir: `{case['report_dir']}`")
        if case.get("error"):
            lines.append(f"  error: `{case['error']}`")
    lines.append("")
    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def main() -> int:
    args = parse_args()
    timestamp = time.strftime("%Y%m%d-%H%M%S")
    report_dir = args.report_root / timestamp

    if not args.gateway_exe.exists():
        overall_status = "skipped" if args.allow_skip_on_missing_gateway else "known_failure"
        report = {
            "generated_at": utc_now(),
            "workspace_root": str(ROOT),
            "gateway_exe": str(args.gateway_exe),
            "dxf_file": str(args.dxf_file),
            "overall_status": overall_status,
            "counts": {"requested": len(NEGATIVE_CASES), "passed": 0, "failed": 0},
            "cases": [],
            "error": f"gateway executable missing: {args.gateway_exe}",
        }
        json_path, md_path = write_report(report_dir, report)
        print(f"negative dry-run matrix complete: status={overall_status}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        return SKIPPED_EXIT_CODE if overall_status == "skipped" else KNOWN_FAILURE_EXIT_CODE

    temp_config_dir, temp_config_path = prepare_mock_config(prefix="dryrun-negative-matrix-", door_input=6)
    case_results: list[dict[str, Any]] = []

    try:
        for case in NEGATIVE_CASES:
            case_report_root = report_dir / case.case_id
            case_report_root.mkdir(parents=True, exist_ok=True)
            case_log_path = case_report_root / "launcher.log"
            port = allocate_port()
            command = [
                args.python_exe,
                str(DRYRUN_SCRIPT),
                "--gateway-exe",
                str(args.gateway_exe),
                "--config-path",
                str(temp_config_path),
                "--dxf-file",
                str(args.dxf_file),
                "--report-root",
                str(case_report_root),
                "--recipe-id",
                args.recipe_id,
                "--version-id",
                args.version_id,
                "--port",
                str(port),
                "--mock-io-json",
                json.dumps(case.mock_io, ensure_ascii=True),
            ]
            completed = subprocess.run(
                command,
                cwd=str(ROOT),
                capture_output=True,
                text=True,
                check=False,
            )
            combined_output = (completed.stdout or "") + ("\n" if completed.stderr else "") + (completed.stderr or "")
            case_log_path.write_text(combined_output, encoding="utf-8")
            child_report_dir, child_report = load_child_report(case_report_root)
            case_status, detail = evaluate_case(case, exit_code=completed.returncode, report=child_report)
            case_results.append(
                {
                    "case_id": case.case_id,
                    "description": case.description,
                    "status": case_status,
                    "exit_code": completed.returncode,
                    "expected_blockers": list(detail["expected_blockers"]),
                    "observed_blockers": list(detail["observed_blockers"]),
                    "error": detail["error"],
                    "preflight_blocked": detail["preflight_blocked"],
                    "canonical_steps_started": detail["canonical_steps_started"],
                    "report_dir": str(child_report_dir),
                    "report_json": str(child_report_dir / CASE_REPORT_NAME),
                    "launcher_log": str(case_log_path),
                    "step_names": list(detail["step_names"]),
                }
            )
    finally:
        temp_config_dir.cleanup()

    passed = sum(1 for item in case_results if item["status"] == "passed")
    failed = len(case_results) - passed
    overall_status = "passed" if failed == 0 else "failed"
    report = {
        "generated_at": utc_now(),
        "workspace_root": str(ROOT),
        "gateway_exe": str(args.gateway_exe),
        "dxf_file": str(args.dxf_file),
        "overall_status": overall_status,
        "counts": {
            "requested": len(case_results),
            "passed": passed,
            "failed": failed,
        },
        "cases": case_results,
    }
    json_path, md_path = write_report(report_dir, report)
    print(f"negative dry-run matrix complete: status={overall_status}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")
    return 0 if overall_status == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
