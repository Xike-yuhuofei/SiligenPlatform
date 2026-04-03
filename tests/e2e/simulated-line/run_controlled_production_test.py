from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
POWERSHELL_SCRIPT = ROOT / "tests" / "e2e" / "simulated-line" / "run_controlled_production_test.ps1"
VERIFY_SCRIPT = ROOT / "tests" / "e2e" / "simulated-line" / "verify_controlled_production_test.py"
TIMESTAMPED_REPORT_DIR_PATTERN = re.compile(r"^\d{8}T\d{6}Z$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the controlled simulated-line production test via canonical root entry.")
    parser.add_argument("--profile", choices=("Local", "CI"), default="Local")
    parser.add_argument("--report-dir", default="tests\\reports\\controlled-production-test")
    parser.add_argument("--use-timestamped-report-dir", action="store_true")
    parser.add_argument("--publish-latest-on-pass", choices=("true", "false"), default="true")
    parser.add_argument("--publish-latest-report-dir", default="tests\\reports\\controlled-production-test")
    parser.add_argument("--python-exe", default=sys.executable)
    parser.add_argument("--executor", default="")
    parser.add_argument("--fault-id", action="append", default=[])
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--clock-profile", default="deterministic-monotonic")
    parser.add_argument("--hook-readiness", choices=("ready", "not-ready"), default="ready")
    parser.add_argument("--hook-abort", choices=("none", "before-run", "after-first-pass"), default="none")
    parser.add_argument("--hook-disconnect", choices=("none", "after-first-pass"), default="none")
    parser.add_argument("--hook-preflight", choices=("none", "fail-after-preview-ready"), default="none")
    parser.add_argument("--hook-alarm", choices=("none", "during-execution"), default="none")
    parser.add_argument(
        "--hook-control-cycle",
        choices=("none", "pause-resume-once", "stop-reset-rerun"),
        default="none",
    )
    return parser.parse_args()


def _resolve_report_root(path_text: str) -> Path:
    path = Path(path_text)
    return path if path.is_absolute() else (ROOT / path).resolve()


def _resolve_effective_report_dir(report_root: Path, use_timestamped: bool) -> Path:
    if not use_timestamped:
        return report_root
    candidates = [
        path
        for path in report_root.iterdir()
        if path.is_dir() and TIMESTAMPED_REPORT_DIR_PATTERN.fullmatch(path.name)
    ]
    if not candidates:
        return report_root
    return max(candidates, key=lambda path: path.name)


def _ps_quote(value: str) -> str:
    return "'" + value.replace("'", "''") + "'"


def main() -> int:
    args = parse_args()
    report_root = _resolve_report_root(args.report_dir)
    report_root.mkdir(parents=True, exist_ok=True)

    ps_segments = [
        f"& {_ps_quote(str(POWERSHELL_SCRIPT))}",
        f"-Profile {_ps_quote(args.profile)}",
        f"-ReportDir {_ps_quote(str(report_root))}",
        f"-PublishLatestOnPass:${args.publish_latest_on_pass}",
        f"-PublishLatestReportDir {_ps_quote(args.publish_latest_report_dir)}",
        f"-PythonExe {_ps_quote(args.python_exe)}",
    ]
    if args.use_timestamped_report_dir:
        ps_segments.append("-UseTimestampedReportDir")
    if args.executor.strip():
        ps_segments.append(f"-Executor {_ps_quote(args.executor.strip())}")
    command = [
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-Command",
        " ".join(ps_segments),
    ]

    env = os.environ.copy()
    if args.fault_id:
        env["SILIGEN_SIMULATED_LINE_FAULT_IDS"] = ",".join(args.fault_id)
    else:
        env.pop("SILIGEN_SIMULATED_LINE_FAULT_IDS", None)
    env["SILIGEN_SIMULATED_LINE_SEED"] = str(args.seed)
    env["SILIGEN_SIMULATED_LINE_CLOCK_PROFILE"] = args.clock_profile
    env["SILIGEN_SIMULATED_LINE_READINESS"] = args.hook_readiness
    env["SILIGEN_SIMULATED_LINE_ABORT"] = args.hook_abort
    env["SILIGEN_SIMULATED_LINE_DISCONNECT"] = args.hook_disconnect
    env["SILIGEN_SIMULATED_LINE_PREFLIGHT"] = args.hook_preflight
    env["SILIGEN_SIMULATED_LINE_ALARM"] = args.hook_alarm
    env["SILIGEN_SIMULATED_LINE_CONTROL_CYCLE"] = args.hook_control_cycle

    completed = subprocess.run(command, cwd=str(ROOT), check=False, env=env)
    if completed.returncode != 0:
        return completed.returncode

    effective_report_dir = _resolve_effective_report_dir(report_root, args.use_timestamped_report_dir)
    verify_completed = subprocess.run(
        [args.python_exe, str(VERIFY_SCRIPT), "--report-dir", str(effective_report_dir)],
        cwd=str(ROOT),
        check=False,
    )
    return verify_completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
