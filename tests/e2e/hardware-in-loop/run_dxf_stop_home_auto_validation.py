from __future__ import annotations

import argparse
import json
import os
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

from runtime_gateway_harness import KNOWN_FAILURE_EXIT_CODE, SKIPPED_EXIT_CODE, resolve_default_exe  # noqa: E402


DEFAULT_REPORT_ROOT = ROOT / "tests" / "reports" / "online-validation" / "dxf-stop-home-auto-validation"
DEFAULT_CONFIG_PATH = ROOT / "config" / "machine" / "machine_config.ini"
DEFAULT_VENDOR_DIR = ROOT / "modules" / "runtime-execution" / "adapters" / "device" / "vendor" / "multicard"
DEFAULT_PROBE_SCRIPT = ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_dxf_stop_home_auto_probe.py"
DEFAULT_SMOKE_SCRIPT = ROOT / "scripts" / "validation" / "run-hardware-smoke-observation.ps1"
PROBE_REPORT_NAME = "dxf-stop-home-auto-probe.json"
VALIDATION_REPORT_NAME = "dxf-stop-home-auto-validation.json"
VALIDATION_REPORT_MD_NAME = "dxf-stop-home-auto-validation.md"
SMOKE_SUMMARY_NAME = "hardware-smoke-observation-summary.json"


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _to_float(value: Any) -> float | None:
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def _to_int(value: Any) -> int | None:
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run the stop/cancel -> immediate home.auto validation flow with hardware-smoke admission "
            "and repeated probe evidence."
        )
    )
    parser.add_argument("--python-exe", default=sys.executable or "python")
    parser.add_argument("--powershell-exe", default="powershell")
    parser.add_argument("--probe-script", type=Path, default=DEFAULT_PROBE_SCRIPT)
    parser.add_argument("--smoke-script", type=Path, default=DEFAULT_SMOKE_SCRIPT)
    parser.add_argument(
        "--gateway-exe",
        type=Path,
        default=resolve_default_exe("siligen_runtime_gateway.exe", "siligen_tcp_server.exe"),
    )
    parser.add_argument("--config-path", type=Path, default=DEFAULT_CONFIG_PATH)
    parser.add_argument("--vendor-dir", type=Path, default=DEFAULT_VENDOR_DIR)
    parser.add_argument("--dxf-file", type=Path, default=ROOT / "samples" / "dxf" / "rect_diag.dxf")
    parser.add_argument("--report-root", type=Path, default=DEFAULT_REPORT_ROOT)
    parser.add_argument("--build-config", default="Debug")
    parser.add_argument("--machine-id", default="unknown-machine")
    parser.add_argument("--operator", default="")
    parser.add_argument("--required-valid-runs", type=int, default=3)
    parser.add_argument("--max-skipped-runs", type=int, default=3)
    parser.add_argument("--max-attempts", type=int, default=0)
    parser.add_argument("--manual-checks-confirmed", action="store_true")
    parser.add_argument("--register-smoke-observation", action="store_true")
    args, probe_extra_args = parser.parse_known_args()
    args.probe_extra_args = probe_extra_args
    if args.required_valid_runs <= 0:
        raise SystemExit("--required-valid-runs must be > 0")
    if args.max_skipped_runs < 0:
        raise SystemExit("--max-skipped-runs must be >= 0")
    if args.max_attempts <= 0:
        args.max_attempts = args.required_valid_runs + args.max_skipped_runs
    if args.max_attempts < args.required_valid_runs:
        raise SystemExit("--max-attempts must be >= --required-valid-runs")
    return args


def load_timestamped_report(report_root: Path, file_name: str) -> tuple[Path, dict[str, Any]]:
    if not report_root.exists():
        raise FileNotFoundError(f"timestamped report root missing: {report_root}")
    timestamp_dirs = sorted(path for path in report_root.iterdir() if path.is_dir())
    if not timestamp_dirs:
        raise FileNotFoundError(f"timestamped report dir missing under: {report_root}")
    report_dir = timestamp_dirs[-1]
    json_path = report_dir / file_name
    if not json_path.exists():
        raise FileNotFoundError(f"report json missing: {json_path}")
    return report_dir, json.loads(json_path.read_text(encoding="utf-8-sig"))


def run_command(
    command: list[str],
    *,
    cwd: Path,
    env: dict[str, str] | None,
    log_path: Path,
) -> tuple[int, str, str]:
    completed = subprocess.run(
        command,
        cwd=str(cwd),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
        env=env,
    )
    combined_output = [
        f"$ {subprocess.list2cmdline(command)}",
        "",
        "## stdout",
        completed.stdout or "",
        "",
        "## stderr",
        completed.stderr or "",
    ]
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text("\n".join(combined_output), encoding="utf-8")
    return completed.returncode, completed.stdout or "", completed.stderr or ""


def evaluate_hardware_smoke(
    summary: dict[str, Any],
    *,
    manual_checks_confirmed: bool,
) -> dict[str, Any]:
    summary_block = summary.get("summary", {})
    if not isinstance(summary_block, dict):
        summary_block = {}
    observation_result = str(summary_block.get("observation_result", "")).strip()
    next_action = str(summary_block.get("next_action", "")).strip()
    a4_signal = str(summary_block.get("a4_signal", "")).strip()
    dominant_gap = str(summary_block.get("dominant_gap", "")).strip()
    manual_checkpoints = summary.get("manual_checkpoints", [])
    if not isinstance(manual_checkpoints, list):
        manual_checkpoints = []

    if observation_result != "ready_for_gate_review":
        return {
            "gate_status": "blocked",
            "verdict_kind": "blocked_hardware_smoke",
            "message": next_action or "hardware smoke observation did not reach ready_for_gate_review",
            "observation_result": observation_result,
            "a4_signal": a4_signal,
            "dominant_gap": dominant_gap,
            "manual_checkpoints": manual_checkpoints,
        }

    if not manual_checks_confirmed:
        return {
            "gate_status": "blocked",
            "verdict_kind": "manual_confirmation_required",
            "message": "automatic hardware smoke passed, but manual safety checkpoints are not confirmed",
            "observation_result": observation_result,
            "a4_signal": a4_signal,
            "dominant_gap": dominant_gap,
            "manual_checkpoints": manual_checkpoints,
        }

    return {
        "gate_status": "passed",
        "verdict_kind": "passed",
        "message": "hardware smoke observation passed and manual safety checkpoints were confirmed",
        "observation_result": observation_result,
        "a4_signal": a4_signal,
        "dominant_gap": dominant_gap,
        "manual_checkpoints": manual_checkpoints,
    }


def classify_probe_failure(report: dict[str, Any]) -> dict[str, str]:
    summary = report.get("observation_summary", {})
    if not isinstance(summary, dict):
        summary = {}
    coord_after_stop = summary.get("coord_after_stop", {})
    if not isinstance(coord_after_stop, dict):
        coord_after_stop = {}

    is_moving = coord_after_stop.get("is_moving")
    remaining_segments = _to_int(coord_after_stop.get("remaining_segments"))
    current_velocity = _to_float(coord_after_stop.get("current_velocity"))
    axes_stopped_before_home = summary.get("axes_stopped_before_home")
    mc_prftrap_detected = bool(summary.get("mc_prftrap_detected", False))
    verdict_kind = str((report.get("verdict", {}) or {}).get("kind", "")).strip()

    motion_active_after_stop = (
        is_moving is True
        or (remaining_segments is not None and remaining_segments > 0)
        or (current_velocity is not None and abs(current_velocity) > 0.001)
    )

    if axes_stopped_before_home is False or motion_active_after_stop:
        return {
            "failure_branch": "stop_drain_incomplete",
            "message": "stop 后仍存在坐标系或轴运动，下一步应补 stop-drain / wait 证据或修 stop 完成语义链",
        }

    if axes_stopped_before_home is True and (mc_prftrap_detected or verdict_kind == "mc_prftrap_after_stop"):
        return {
            "failure_branch": "axis_stopped_but_prftrap",
            "message": "轴已停稳仍命中 MC_PrfTrap，下一步应转入 MoveAxisToPosition pre-stop/clear/wait 链",
        }

    return {
        "failure_branch": "probe_failed_unclassified",
        "message": "probe 失败但未落到既定分支，需要先补 stop 后状态证据再决定是否改码",
    }


def evaluate_probe_attempt(report: dict[str, Any]) -> dict[str, Any]:
    summary = report.get("observation_summary", {})
    if not isinstance(summary, dict):
        summary = {}
    overall_status = str(report.get("overall_status", "")).strip()
    verdict_kind = str((report.get("verdict", {}) or {}).get("kind", "")).strip()
    terminal_state = str(summary.get("job_terminal_state_after_stop", "")).strip()
    axes_stopped_before_home = summary.get("axes_stopped_before_home")
    home_auto_ok = summary.get("home_auto_ok")
    mc_prftrap_detected = bool(summary.get("mc_prftrap_detected", False))
    prftrap_hit_count = _to_int(summary.get("prftrap_hit_count"))

    validity_checks = {
        "overall_status_passed": overall_status == "passed",
        "terminal_not_completed": terminal_state not in {"", "completed"},
        "axes_stopped_before_home": axes_stopped_before_home is True,
        "home_auto_ok": home_auto_ok is True,
        "mc_prftrap_not_detected": mc_prftrap_detected is False,
        "prftrap_hit_count_zero": (prftrap_hit_count or 0) == 0,
    }

    if all(validity_checks.values()):
        return {
            "attempt_status": "valid_pass",
            "failure_branch": "",
            "message": "probe 样本满足 stop/cancel -> immediate home.auto 通过条件",
            "validity_checks": validity_checks,
            "probe_verdict_kind": verdict_kind,
        }

    if overall_status == "skipped" or terminal_state == "completed":
        return {
            "attempt_status": "skipped",
            "failure_branch": "",
            "message": "stop 触发过晚，job 已 completed；应更早触发 stop 重新取样",
            "validity_checks": validity_checks,
            "probe_verdict_kind": verdict_kind,
        }

    failure = classify_probe_failure(report)
    return {
        "attempt_status": "failed",
        "failure_branch": failure["failure_branch"],
        "message": failure["message"],
        "validity_checks": validity_checks,
        "probe_verdict_kind": verdict_kind,
    }


def evaluate_series_progress(
    *,
    valid_runs: int,
    skipped_runs: int,
    required_valid_runs: int,
    max_skipped_runs: int,
    last_attempt_status: str,
    last_failure_branch: str,
) -> dict[str, Any]:
    if valid_runs >= required_valid_runs:
        return {
            "done": True,
            "overall_status": "passed",
            "verdict_kind": "passed",
            "next_action": "已拿到足够有效样本；下一步补 incident 验证文档并进入 main 同步 + 正式收口",
        }

    if last_attempt_status == "failed":
        return {
            "done": True,
            "overall_status": "failed",
            "verdict_kind": last_failure_branch or "probe_failed_unclassified",
            "next_action": "首个失败样本已足够分类；停止联机加试，先整理证据再决定是否同步 main 后改码",
        }

    if skipped_runs > max_skipped_runs:
        return {
            "done": True,
            "overall_status": "blocked",
            "verdict_kind": "trigger_window_unstable",
            "next_action": "skipped 超出预算，说明 stop 触发窗口不稳定；先修正触发时机或脚本参数，再继续现场验证",
        }

    return {
        "done": False,
        "overall_status": "running",
        "verdict_kind": "running",
        "next_action": "继续执行下一次 probe",
    }


def write_report(report_dir: Path, report: dict[str, Any]) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / VALIDATION_REPORT_NAME
    md_path = report_dir / VALIDATION_REPORT_MD_NAME
    json_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    md_path.write_text(build_report_markdown(report), encoding="utf-8")
    return json_path, md_path


def build_report_markdown(report: dict[str, Any]) -> str:
    counts = report.get("counts", {})
    hardware_smoke = report.get("hardware_smoke", {})
    lines = [
        "# DXF Stop Then Home.auto Validation",
        "",
        f"- generated_at: `{report.get('generated_at', '')}`",
        f"- overall_status: `{report.get('overall_status', '')}`",
        f"- verdict: `{(report.get('verdict', {}) or {}).get('kind', '')}`",
        f"- next_action: `{report.get('next_action', '')}`",
        f"- required_valid_runs: `{(report.get('settings', {}) or {}).get('required_valid_runs', '')}`",
        f"- valid_passed: `{counts.get('valid_passed', 0)}`",
        f"- skipped: `{counts.get('skipped', 0)}`",
        f"- failed: `{counts.get('failed', 0)}`",
        "",
        "## Hardware Smoke Gate",
        "",
        f"- gate_status: `{hardware_smoke.get('gate_status', '')}`",
        f"- observation_result: `{hardware_smoke.get('observation_result', '')}`",
        f"- a4_signal: `{hardware_smoke.get('a4_signal', '')}`",
        f"- dominant_gap: `{hardware_smoke.get('dominant_gap', '')}`",
        f"- message: `{hardware_smoke.get('message', '')}`",
        f"- summary_json: `{hardware_smoke.get('summary_json', '')}`",
        f"- launcher_log: `{hardware_smoke.get('launcher_log', '')}`",
        "",
        "## Manual Checkpoints",
        "",
    ]

    manual_checkpoints = hardware_smoke.get("manual_checkpoints", [])
    if isinstance(manual_checkpoints, list) and manual_checkpoints:
        for checkpoint in manual_checkpoints:
            checkpoint_id = str((checkpoint or {}).get("id", "")).strip()
            detail = str((checkpoint or {}).get("detail", "")).strip()
            lines.append(f"- `{checkpoint_id}`: {detail}")
    else:
        lines.append("- `<none>`")

    lines.extend(
        [
            "",
            "## Probe Attempts",
            "",
        ]
    )

    attempts = report.get("attempts", [])
    if isinstance(attempts, list) and attempts:
        for attempt in attempts:
            attempt_index = attempt.get("attempt_index", "")
            attempt_status = attempt.get("attempt_status", "")
            failure_branch = attempt.get("failure_branch", "")
            lines.append(
                f"- attempt `{attempt_index}` status=`{attempt_status}` failure_branch=`{failure_branch}` "
                f"probe_status=`{attempt.get('probe_overall_status', '')}`"
            )
            lines.append(f"  report_json: `{attempt.get('report_json', '')}`")
            lines.append(f"  launcher_log: `{attempt.get('launcher_log', '')}`")
            lines.append(f"  message: `{attempt.get('message', '')}`")
    else:
        lines.append("- `<none>`")

    return "\n".join(lines) + "\n"


def _build_smoke_command(args: argparse.Namespace, report_root: Path) -> list[str]:
    command = [
        args.powershell_exe,
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        str(args.smoke_script),
        "-BuildConfig",
        str(args.build_config),
        "-ConfigPath",
        str(args.config_path),
        "-VendorDir",
        str(args.vendor_dir),
        "-ReportRoot",
        str(report_root),
        "-MachineId",
        str(args.machine_id),
    ]
    if args.operator:
        command.extend(["-Operator", str(args.operator)])
    if args.register_smoke_observation:
        command.append("-RegisterObservation")
    return command


def _build_probe_command(args: argparse.Namespace, attempt_root: Path) -> list[str]:
    command = [
        args.python_exe,
        str(args.probe_script),
        "--gateway-exe",
        str(args.gateway_exe),
        "--config-path",
        str(args.config_path),
        "--dxf-file",
        str(args.dxf_file),
        "--report-root",
        str(attempt_root),
    ]
    command.extend(args.probe_extra_args)
    return command


def _workspace_build_env() -> dict[str, str]:
    env = os.environ.copy()
    env.setdefault("SILIGEN_CONTROL_APPS_BUILD_ROOT", str(ROOT / "build"))
    return env


def main() -> int:
    args = parse_args()
    timestamp = time.strftime("%Y%m%d-%H%M%S")
    report_dir = args.report_root / timestamp
    logs_dir = report_dir / "logs"
    logs_dir.mkdir(parents=True, exist_ok=True)

    hardware_smoke_root = report_dir / "hardware-smoke"
    smoke_log_path = logs_dir / "hardware-smoke-launcher.log"
    smoke_command = _build_smoke_command(args, hardware_smoke_root)
    smoke_exit_code, _, _ = run_command(
        smoke_command,
        cwd=ROOT,
        env=_workspace_build_env(),
        log_path=smoke_log_path,
    )

    smoke_report_dir: Path | None = None
    smoke_summary: dict[str, Any] = {}
    smoke_gate: dict[str, Any]
    try:
        smoke_report_dir, smoke_summary = load_timestamped_report(hardware_smoke_root, SMOKE_SUMMARY_NAME)
        smoke_gate = evaluate_hardware_smoke(
            smoke_summary,
            manual_checks_confirmed=bool(args.manual_checks_confirmed),
        )
    except FileNotFoundError as exc:
        smoke_gate = {
            "gate_status": "blocked",
            "verdict_kind": "blocked_hardware_smoke",
            "message": str(exc),
            "observation_result": "",
            "a4_signal": "",
            "dominant_gap": "",
            "manual_checkpoints": [],
        }

    hardware_smoke_report = {
        "command": smoke_command,
        "exit_code": smoke_exit_code,
        "gate_status": smoke_gate["gate_status"],
        "message": smoke_gate["message"],
        "observation_result": smoke_gate.get("observation_result", ""),
        "a4_signal": smoke_gate.get("a4_signal", ""),
        "dominant_gap": smoke_gate.get("dominant_gap", ""),
        "manual_checkpoints": smoke_gate.get("manual_checkpoints", []),
        "report_dir": str(smoke_report_dir) if smoke_report_dir else "",
        "summary_json": str(smoke_report_dir / SMOKE_SUMMARY_NAME) if smoke_report_dir else "",
        "launcher_log": str(smoke_log_path),
    }

    attempts: list[dict[str, Any]] = []
    valid_runs = 0
    skipped_runs = 0
    final_outcome = {
        "overall_status": "blocked",
        "verdict_kind": smoke_gate["verdict_kind"],
        "next_action": smoke_gate["message"],
    }

    if smoke_gate["gate_status"] == "passed":
        for attempt_index in range(1, args.max_attempts + 1):
            attempt_root = report_dir / "probe-attempts" / f"attempt-{attempt_index:02d}"
            attempt_log_path = attempt_root / "launcher.log"
            probe_command = _build_probe_command(args, attempt_root)
            probe_exit_code, _, _ = run_command(
                probe_command,
                cwd=ROOT,
                env=_workspace_build_env(),
                log_path=attempt_log_path,
            )
            try:
                probe_report_dir, probe_report = load_timestamped_report(attempt_root, PROBE_REPORT_NAME)
            except FileNotFoundError:
                probe_report_dir = attempt_root
                probe_report = {
                    "overall_status": "failed",
                    "verdict": {"kind": "probe_report_missing", "message": "probe report json missing"},
                    "observation_summary": {},
                    "error": "probe report json missing",
                }

            attempt_eval = evaluate_probe_attempt(probe_report)
            attempt_record = {
                "attempt_index": attempt_index,
                "command": probe_command,
                "exit_code": probe_exit_code,
                "attempt_status": attempt_eval["attempt_status"],
                "failure_branch": attempt_eval["failure_branch"],
                "message": attempt_eval["message"],
                "probe_overall_status": str(probe_report.get("overall_status", "")).strip(),
                "probe_verdict_kind": attempt_eval["probe_verdict_kind"],
                "report_dir": str(probe_report_dir),
                "report_json": str(probe_report_dir / PROBE_REPORT_NAME),
                "launcher_log": str(attempt_log_path),
                "validity_checks": attempt_eval["validity_checks"],
            }
            attempts.append(attempt_record)

            if attempt_eval["attempt_status"] == "valid_pass":
                valid_runs += 1
            elif attempt_eval["attempt_status"] == "skipped":
                skipped_runs += 1

            progress = evaluate_series_progress(
                valid_runs=valid_runs,
                skipped_runs=skipped_runs,
                required_valid_runs=args.required_valid_runs,
                max_skipped_runs=args.max_skipped_runs,
                last_attempt_status=attempt_eval["attempt_status"],
                last_failure_branch=attempt_eval["failure_branch"],
            )
            if progress["done"]:
                final_outcome = progress
                break
        else:
            final_outcome = {
                "overall_status": "blocked",
                "verdict_kind": "attempt_budget_exhausted",
                "next_action": "达到 attempt 预算仍未拿到足够有效样本；先整理现有证据再决定是否继续现场验证",
            }

    counts = {
        "attempted": len(attempts),
        "valid_passed": valid_runs,
        "skipped": skipped_runs,
        "failed": sum(1 for attempt in attempts if attempt.get("attempt_status") == "failed"),
    }

    report = {
        "generated_at": utc_now(),
        "workspace_root": str(ROOT),
        "overall_status": final_outcome["overall_status"],
        "verdict": {
            "kind": final_outcome["verdict_kind"],
            "message": final_outcome["next_action"],
        },
        "next_action": final_outcome["next_action"],
        "settings": {
            "probe_script": str(args.probe_script),
            "smoke_script": str(args.smoke_script),
            "python_exe": args.python_exe,
            "powershell_exe": args.powershell_exe,
            "build_config": args.build_config,
            "config_path": str(args.config_path),
            "vendor_dir": str(args.vendor_dir),
            "gateway_exe": str(args.gateway_exe),
            "dxf_file": str(args.dxf_file),
            "machine_id": args.machine_id,
            "operator": args.operator,
            "required_valid_runs": args.required_valid_runs,
            "max_skipped_runs": args.max_skipped_runs,
            "max_attempts": args.max_attempts,
            "manual_checks_confirmed": bool(args.manual_checks_confirmed),
            "probe_extra_args": list(args.probe_extra_args),
        },
        "hardware_smoke": hardware_smoke_report,
        "attempts": attempts,
        "counts": counts,
    }
    json_path, md_path = write_report(report_dir, report)

    print(f"dxf stop/home validation complete: status={report['overall_status']} verdict={report['verdict']['kind']}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")

    if report["overall_status"] == "passed":
        return 0
    if report["overall_status"] == "blocked":
        return KNOWN_FAILURE_EXIT_CODE
    if report["verdict"]["kind"] == "trigger_window_unstable":
        return SKIPPED_EXIT_CODE
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
