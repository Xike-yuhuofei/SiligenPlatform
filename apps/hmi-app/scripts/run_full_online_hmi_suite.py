from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[3]
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.evidence_bundle import (  # noqa: E402
    EvidenceBundle,
    EvidenceCaseRecord,
    EvidenceLink,
    infer_verdict,
    trace_fields,
    write_bundle_artifacts,
)


ONLINE_SMOKE_SCRIPT = ROOT / "apps" / "hmi-app" / "scripts" / "online-smoke.ps1"
READY_TIMEOUT_SCRIPT = ROOT / "apps" / "hmi-app" / "scripts" / "verify-online-ready-timeout.ps1"
RECOVERY_LOOP_SCRIPT = ROOT / "apps" / "hmi-app" / "scripts" / "verify-online-recovery-loop.ps1"
RUNTIME_ACTION_MATRIX_SCRIPT = ROOT / "apps" / "hmi-app" / "scripts" / "run_online_runtime_action_matrix.py"
DEFAULT_REPORT_ROOT = ROOT / "tests" / "reports" / "online-validation" / "full-online-hmi-suite"


@dataclass(frozen=True)
class HmiSuiteScenario:
    scenario_id: str
    title: str
    kind: str


@dataclass(frozen=True)
class HmiSuiteScenarioResult:
    scenario_id: str
    title: str
    kind: str
    status: str
    exit_code: int | None
    command: list[str]
    report_dir: str
    launcher_log: str
    screenshot_path: str = ""
    machine_summary_path: str = ""
    note: str = ""


FULL_ONLINE_HMI_SCENARIOS = (
    HmiSuiteScenario(
        scenario_id="online-smoke",
        title="HMI online startup success",
        kind="powershell-online-smoke",
    ),
    HmiSuiteScenario(
        scenario_id="online-ready-timeout",
        title="HMI backend ready timeout diagnostics",
        kind="powershell-timeout-diagnostics",
    ),
    HmiSuiteScenario(
        scenario_id="online-recovery-loop",
        title="HMI runtime recovery loop",
        kind="powershell-recovery-loop",
    ),
    HmiSuiteScenario(
        scenario_id="runtime-action-matrix",
        title="HMI runtime action matrix aggregate",
        kind="python-runtime-action-matrix",
    ),
)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def scenario_ids() -> tuple[str, ...]:
    return tuple(scenario.scenario_id for scenario in FULL_ONLINE_HMI_SCENARIOS)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the full online HMI suite and write an aggregate summary.")
    parser.add_argument("--report-dir", type=Path, default=DEFAULT_REPORT_ROOT)
    parser.add_argument("--python-exe", default=sys.executable or "python")
    parser.add_argument("--timeout-ms", type=int, default=20000)
    parser.add_argument("--mock-startup-timeout-ms", type=int, default=2000)
    parser.add_argument("--gateway-exe", type=Path, default=None)
    parser.add_argument("--gateway-config", type=Path, default=None)
    parser.add_argument("--use-mock-gateway-config", action="store_true")
    return parser.parse_args()


def _run_subprocess(
    *,
    command: list[str],
    cwd: Path,
    launcher_log: Path,
) -> tuple[int | None, str]:
    completed = subprocess.run(
        command,
        cwd=str(cwd),
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )
    combined_output = (completed.stdout or "") + ("\n" if completed.stderr else "") + (completed.stderr or "")
    launcher_log.write_text(combined_output, encoding="utf-8")
    return completed.returncode, combined_output.strip()


def _online_smoke_command(args: argparse.Namespace, screenshot_path: Path) -> list[str]:
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
        "-ScreenshotPath",
        str(screenshot_path),
    ]
    if args.gateway_exe is not None:
        command.extend(["-GatewayExe", str(args.gateway_exe)])
    if args.gateway_config is not None:
        command.extend(["-GatewayConfig", str(args.gateway_config)])
    if args.use_mock_gateway_config:
        command.append("-UseMockGatewayConfig")
    return command


def _ready_timeout_command(args: argparse.Namespace) -> list[str]:
    return [
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        str(READY_TIMEOUT_SCRIPT),
        "-MockStartupTimeoutMs",
        str(args.mock_startup_timeout_ms),
        "-PythonExe",
        args.python_exe,
    ]


def _recovery_loop_command(args: argparse.Namespace) -> list[str]:
    return [
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        str(RECOVERY_LOOP_SCRIPT),
        "-TimeoutMs",
        str(args.timeout_ms),
        "-PythonExe",
        args.python_exe,
    ]


def _runtime_action_matrix_command(args: argparse.Namespace, report_root: Path) -> list[str]:
    command = [
        args.python_exe,
        str(RUNTIME_ACTION_MATRIX_SCRIPT),
        "--report-root",
        str(report_root),
        "--python-exe",
        args.python_exe,
        "--timeout-ms",
        str(args.timeout_ms),
    ]
    if args.gateway_exe is not None:
        command.extend(["--gateway-exe", str(args.gateway_exe)])
    if args.gateway_config is not None:
        command.extend(["--gateway-config", str(args.gateway_config)])
    if args.use_mock_gateway_config:
        command.append("--use-mock-gateway-config")
    return command


def _run_scenario(
    *,
    args: argparse.Namespace,
    suite_report_dir: Path,
    scenario: HmiSuiteScenario,
) -> HmiSuiteScenarioResult:
    scenario_dir = suite_report_dir / "scenarios" / scenario.scenario_id
    scenario_dir.mkdir(parents=True, exist_ok=True)
    launcher_log = scenario_dir / "launcher.log"
    screenshot_path = ""
    machine_summary_path = ""
    note = ""

    if scenario.scenario_id == "online-smoke":
        screenshot = scenario_dir / "online-smoke.png"
        command = _online_smoke_command(args, screenshot)
        exit_code, output = _run_subprocess(command=command, cwd=ROOT, launcher_log=launcher_log)
        screenshot_path = str(screenshot.resolve()) if screenshot.exists() else ""
        status = "passed" if exit_code == 0 and screenshot.exists() else "failed"
        if status != "passed":
            note = output or "online smoke failed"
    elif scenario.scenario_id == "online-ready-timeout":
        command = _ready_timeout_command(args)
        exit_code, output = _run_subprocess(command=command, cwd=ROOT, launcher_log=launcher_log)
        status = "passed" if exit_code == 0 else "failed"
        if status != "passed":
            note = output or "ready timeout verification failed"
    elif scenario.scenario_id == "online-recovery-loop":
        command = _recovery_loop_command(args)
        exit_code, output = _run_subprocess(command=command, cwd=ROOT, launcher_log=launcher_log)
        status = "passed" if exit_code == 0 else "failed"
        if status != "passed":
            note = output or "recovery loop verification failed"
    else:
        matrix_report_root = scenario_dir / "matrix"
        command = _runtime_action_matrix_command(args, matrix_report_root)
        exit_code, output = _run_subprocess(command=command, cwd=ROOT, launcher_log=launcher_log)
        child_dirs = sorted(path for path in matrix_report_root.iterdir() if path.is_dir()) if matrix_report_root.exists() else []
        if child_dirs:
            child_dir = child_dirs[-1]
            summary_path = child_dir / "online-runtime-action-matrix-summary.json"
            if summary_path.exists():
                summary_payload = json.loads(summary_path.read_text(encoding="utf-8"))
                child_status = str(summary_payload.get("overall_status", "failed"))
                machine_summary_path = str(summary_path.resolve())
                status = "passed" if exit_code == 0 and child_status == "passed" else "failed"
                if status != "passed":
                    note = output or f"runtime action matrix overall_status={child_status}"
            else:
                status = "failed"
                note = "runtime action matrix summary missing"
        else:
            status = "failed"
            note = "runtime action matrix report directory missing"

    return HmiSuiteScenarioResult(
        scenario_id=scenario.scenario_id,
        title=scenario.title,
        kind=scenario.kind,
        status=status,
        exit_code=exit_code,
        command=command,
        report_dir=str(scenario_dir.resolve()),
        launcher_log=str(launcher_log.resolve()),
        screenshot_path=screenshot_path,
        machine_summary_path=machine_summary_path,
        note=note,
    )


def _write_report(report_dir: Path, results: list[HmiSuiteScenarioResult]) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "full-online-hmi-suite-summary.json"
    md_path = report_dir / "full-online-hmi-suite-summary.md"
    overall_status = "passed" if results and all(item.status == "passed" for item in results) else infer_verdict([item.status for item in results])
    payload = {
        "generated_at": utc_now(),
        "workspace_root": str(ROOT),
        "overall_status": overall_status,
        "signed_publish_authority": "tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1",
        "counts": {
            "requested": len(results),
            "passed": sum(1 for item in results if item.status == "passed"),
            "failed": sum(1 for item in results if item.status == "failed"),
        },
        "scenarios": [asdict(item) for item in results],
    }
    json_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# Full Online HMI Suite Summary",
        "",
        f"- generated_at: `{payload['generated_at']}`",
        f"- overall_status: `{payload['overall_status']}`",
        f"- signed_publish_authority: `{payload['signed_publish_authority']}`",
        f"- requested: `{payload['counts']['requested']}`",
        f"- passed: `{payload['counts']['passed']}`",
        f"- failed: `{payload['counts']['failed']}`",
        "",
        "## Scenarios",
        "",
    ]
    for item in results:
        lines.append(
            f"- `{item.status}` `{item.scenario_id}` kind=`{item.kind}` exit_code=`{item.exit_code}`"
        )
        lines.append(f"  report_dir: `{item.report_dir}`")
        lines.append(f"  launcher_log: `{item.launcher_log}`")
        if item.screenshot_path:
            lines.append(f"  screenshot: `{item.screenshot_path}`")
        if item.machine_summary_path:
            lines.append(f"  machine_summary: `{item.machine_summary_path}`")
        if item.note:
            lines.append(f"  note: `{item.note}`")
    lines.append("")
    md_path.write_text("\n".join(lines), encoding="utf-8")
    _write_evidence_bundle(report_dir=report_dir, summary_json_path=json_path, summary_md_path=md_path, payload=payload)
    return json_path, md_path


def _write_evidence_bundle(
    *,
    report_dir: Path,
    summary_json_path: Path,
    summary_md_path: Path,
    payload: dict[str, Any],
) -> None:
    case_records: list[EvidenceCaseRecord] = []
    for item in payload.get("scenarios", []):
        status = str(item.get("status", "failed"))
        evidence_path = str(item.get("machine_summary_path") or item.get("launcher_log") or summary_json_path.resolve())
        case_records.append(
            EvidenceCaseRecord(
                case_id=str(item.get("scenario_id", "")),
                name=str(item.get("title", "")),
                suite_ref="hmi-online",
                owner_scope="hmi-app",
                primary_layer="L5",
                producer_lane_ref="limited-hil",
                status=status,
                evidence_profile="hmi-online",
                stability_state="stable",
                required_assets=(),
                required_fixtures=("fixture.validation-evidence-bundle",),
                risk_tags=("hmi", "online"),
                note=str(item.get("note", "")),
                trace_fields=trace_fields(
                    stage_id="L5",
                    artifact_id=str(item.get("scenario_id", "")),
                    module_id="hmi-app",
                    workflow_state="executed",
                    execution_state=status,
                    event_name="full-online-hmi-suite",
                    failure_code="" if status == "passed" else f"hmi.{status}",
                    evidence_path=evidence_path,
                ),
            )
        )

    bundle = EvidenceBundle(
        bundle_id=f"full-online-hmi-suite-{int(time.time())}",
        request_ref="full-online-hmi-suite",
        producer_lane_ref="limited-hil",
        report_root=str(report_dir.resolve()),
        summary_file=str(summary_md_path.resolve()),
        machine_file=str(summary_json_path.resolve()),
        verdict=str(payload.get("overall_status", "incomplete")),
        case_records=case_records,
        offline_prerequisites=("L0", "L1", "L2", "L3", "L4"),
        skip_justification=(
            "full-online HMI suite is supplementary G8 evidence and does not replace the controlled HIL signed publish path"
        ),
        metadata={
            "scenario_ids": list(scenario_ids()),
            "signed_publish_authority": payload.get("signed_publish_authority", ""),
        },
    )
    write_bundle_artifacts(
        bundle=bundle,
        report_root=report_dir,
        summary_json_path=summary_json_path,
        summary_md_path=summary_md_path,
        evidence_links=[
            EvidenceLink(
                label="full-online-hmi-suite-summary.json",
                path=str(summary_json_path.resolve()),
                role="machine-summary",
            ),
            EvidenceLink(
                label="full-online-hmi-suite-summary.md",
                path=str(summary_md_path.resolve()),
                role="human-summary",
            ),
        ],
    )


def main() -> int:
    args = parse_args()
    report_dir = args.report_dir / time.strftime("%Y%m%d-%H%M%S")
    results = [_run_scenario(args=args, suite_report_dir=report_dir, scenario=scenario) for scenario in FULL_ONLINE_HMI_SCENARIOS]
    json_path, md_path = _write_report(report_dir=report_dir, results=results)
    overall_status = json.loads(json_path.read_text(encoding="utf-8")).get("overall_status", "failed")
    print(f"full online HMI suite complete: status={overall_status}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")
    return 0 if overall_status == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
