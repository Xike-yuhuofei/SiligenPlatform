from __future__ import annotations

import argparse
import json
import subprocess
import sys
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
    default_deterministic_replay,
    infer_verdict,
    trace_fields,
    write_bundle_artifacts,
)


CHILD_RUNNER = ROOT / "tests" / "e2e" / "simulated-line" / "run_simulated_line.py"


@dataclass(frozen=True)
class MatrixScenario:
    case_id: str
    title: str
    expected_status: str
    args: tuple[str, ...]
    required_assets: tuple[str, ...] = ()
    fault_ids: tuple[str, ...] = ()


@dataclass
class MatrixScenarioResult:
    case_id: str
    title: str
    status: str
    expected_status: str
    actual_status: str
    child_exit_code: int | None
    command: list[str]
    report_dir: str
    required_assets: tuple[str, ...]
    fault_ids: tuple[str, ...]
    note: str = ""


SCENARIOS = (
    MatrixScenario(
        case_id="readiness-not-ready",
        title="controller readiness not ready blocks simulated line",
        expected_status="blocked",
        args=("--mode", "compat", "--compat-case-id", "rect_diag", "--hook-readiness", "not-ready"),
    ),
    MatrixScenario(
        case_id="abort-before-run",
        title="controller abort before run blocks simulated line",
        expected_status="blocked",
        args=("--mode", "compat", "--compat-case-id", "rect_diag", "--hook-abort", "before-run"),
    ),
    MatrixScenario(
        case_id="preview-ready-but-preflight-failed",
        title="preview-ready path blocks when fake preflight fails",
        expected_status="blocked",
        args=(
            "--mode",
            "compat",
            "--compat-case-id",
            "rect_diag",
            "--hook-preflight",
            "fail-after-preview-ready",
        ),
    ),
    MatrixScenario(
        case_id="fault-invalid-empty-segments",
        title="invalid empty segments fault stays replayable",
        expected_status="passed",
        args=("--mode", "scheme_c", "--fault-id", "fault.simulated.invalid-empty-segments"),
        required_assets=("sample.simulation.invalid_empty_segments", "baseline.simulation.invalid_empty_segments"),
        fault_ids=("fault.simulated.invalid-empty-segments",),
    ),
    MatrixScenario(
        case_id="fault-following-error-quantized",
        title="following error fault stays replayable",
        expected_status="passed",
        args=("--mode", "scheme_c", "--fault-id", "fault.simulated.following_error_quantized"),
        required_assets=(
            "sample.simulation.following_error_quantized",
            "baseline.simulation.following_error_quantized",
        ),
        fault_ids=("fault.simulated.following_error_quantized",),
    ),
    MatrixScenario(
        case_id="alarm-during-execution",
        title="alarm during execution fails simulated line replay",
        expected_status="failed",
        args=(
            "--mode",
            "scheme_c",
            "--fault-id",
            "fault.simulated.following_error_quantized",
            "--hook-alarm",
            "during-execution",
        ),
        required_assets=(
            "sample.simulation.following_error_quantized",
            "baseline.simulation.following_error_quantized",
        ),
        fault_ids=("fault.simulated.following_error_quantized",),
    ),
    MatrixScenario(
        case_id="abort-after-first-pass",
        title="abort after first pass fails simulated line replay",
        expected_status="failed",
        args=(
            "--mode",
            "scheme_c",
            "--fault-id",
            "fault.simulated.following_error_quantized",
            "--hook-abort",
            "after-first-pass",
        ),
        required_assets=(
            "sample.simulation.following_error_quantized",
            "baseline.simulation.following_error_quantized",
        ),
        fault_ids=("fault.simulated.following_error_quantized",),
    ),
    MatrixScenario(
        case_id="pause-resume-cycle",
        title="pause-resume control cycle remains replayable",
        expected_status="passed",
        args=(
            "--mode",
            "compat",
            "--compat-case-id",
            "rect_diag",
            "--hook-control-cycle",
            "pause-resume-once",
        ),
        required_assets=(
            "protocol.fixture.rect_diag_engineering",
            "baseline.simulation.compat_rect_diag",
        ),
    ),
    MatrixScenario(
        case_id="stop-reset-cycle",
        title="stop-reset-rerun control cycle remains replayable",
        expected_status="passed",
        args=(
            "--mode",
            "compat",
            "--compat-case-id",
            "rect_diag",
            "--hook-control-cycle",
            "stop-reset-rerun",
        ),
        required_assets=(
            "protocol.fixture.rect_diag_engineering",
            "baseline.simulation.compat_rect_diag",
        ),
    ),
    MatrixScenario(
        case_id="disconnect-after-first-pass",
        title="disconnect after first pass fails simulated line replay",
        expected_status="failed",
        args=(
            "--mode",
            "scheme_c",
            "--fault-id",
            "fault.simulated.following_error_quantized",
            "--hook-disconnect",
            "after-first-pass",
        ),
        required_assets=(
            "sample.simulation.following_error_quantized",
            "baseline.simulation.following_error_quantized",
        ),
        fault_ids=("fault.simulated.following_error_quantized",),
    ),
)


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _run_scenario(base_report_dir: Path, scenario: MatrixScenario) -> MatrixScenarioResult:
    child_report_dir = base_report_dir / scenario.case_id
    command = [sys.executable, str(CHILD_RUNNER), *scenario.args, "--report-dir", str(child_report_dir)]
    completed = subprocess.run(
        command,
        cwd=str(ROOT),
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )

    child_summary_path = child_report_dir / "simulated-line-summary.json"
    if not child_summary_path.exists():
        return MatrixScenarioResult(
            case_id=scenario.case_id,
            title=scenario.title,
            status="failed",
            expected_status=scenario.expected_status,
            actual_status="missing-summary",
            child_exit_code=completed.returncode,
            command=command,
            report_dir=str(child_report_dir),
            required_assets=scenario.required_assets,
            fault_ids=scenario.fault_ids,
            note=(completed.stderr or completed.stdout or "child summary missing").strip(),
        )

    child_summary = _load_json(child_summary_path)
    actual_status = str(child_summary.get("overall_status", "missing-status"))
    status = "passed" if actual_status == scenario.expected_status else "failed"
    note = ""
    if status != "passed":
        note = (
            f"expected child overall_status={scenario.expected_status} actual={actual_status}; "
            f"child_exit_code={completed.returncode}"
        )

    return MatrixScenarioResult(
        case_id=scenario.case_id,
        title=scenario.title,
        status=status,
        expected_status=scenario.expected_status,
        actual_status=actual_status,
        child_exit_code=completed.returncode,
        command=command,
        report_dir=str(child_report_dir),
        required_assets=scenario.required_assets,
        fault_ids=scenario.fault_ids,
        note=note,
    )


def _write_report(report_dir: Path, results: list[MatrixScenarioResult], overall_status: str) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "simulated-line-matrix-summary.json"
    md_path = report_dir / "simulated-line-matrix-summary.md"
    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "overall_status": overall_status,
        "results": [asdict(item) for item in results],
    }
    json_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# Simulated Line Matrix Summary",
        "",
        f"- overall_status: `{overall_status}`",
        "",
        "## Cases",
        "",
    ]
    for item in results:
        lines.append(
            f"- `{item.status}` `{item.case_id}` expected=`{item.expected_status}` actual=`{item.actual_status}` exit=`{item.child_exit_code}`"
        )
        lines.append(f"  report_dir: `{item.report_dir}`")
        lines.append(f"  command: `{subprocess.list2cmdline(item.command)}`")
        if item.note:
            lines.append(f"  note: {item.note}")
    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def _write_bundle(report_dir: Path, json_path: Path, md_path: Path, results: list[MatrixScenarioResult], overall_status: str) -> None:
    bundle = EvidenceBundle(
        bundle_id="simulated-line-matrix",
        request_ref="simulated-line-matrix",
        producer_lane_ref="full-offline-gate",
        report_root=str(report_dir.resolve()),
        summary_file=str(md_path.resolve()),
        machine_file=str(json_path.resolve()),
        verdict=infer_verdict([item.status for item in results]),
        linked_asset_refs=tuple(sorted({asset for item in results for asset in item.required_assets})),
        metadata={"overall_status": overall_status},
        case_records=[
            EvidenceCaseRecord(
                case_id=item.case_id,
                name=item.title,
                suite_ref="e2e",
                owner_scope="runtime-execution",
                primary_layer="L4",
                producer_lane_ref="full-offline-gate",
                status=item.status,
                evidence_profile="simulated-e2e-report",
                stability_state="stable",
                size_label="medium",
                label_refs=("suite:e2e", "kind:e2e", "size:medium", "layer:L4"),
                risk_tags=("acceptance", "regression"),
                required_assets=item.required_assets,
                required_fixtures=(
                    "fixture.simulated-line-matrix",
                    "fixture.simulated-line-regression",
                    "fixture.validation-evidence-bundle",
                ),
                fault_ids=item.fault_ids,
                deterministic_replay=default_deterministic_replay(
                    passed=item.status == "passed",
                    seed=0,
                    clock_profile="deterministic-monotonic",
                    repeat_count=1,
                ),
                note=item.note,
                trace_fields=trace_fields(
                    stage_id="L4",
                    artifact_id=item.case_id,
                    module_id="runtime-execution",
                    workflow_state="executed",
                    execution_state=item.status,
                    event_name=item.title,
                    failure_code="" if item.status == "passed" else f"simulated-line-matrix.{item.case_id}",
                    evidence_path=str(json_path.resolve()),
                ),
            )
            for item in results
        ],
    )
    links = [
        EvidenceLink(label="run_simulated_line.py", path=str(CHILD_RUNNER.resolve()), role="runner"),
    ]
    for item in results:
        links.append(
            EvidenceLink(
                label=f"{item.case_id}/simulated-line-summary.json",
                path=str((Path(item.report_dir) / "simulated-line-summary.json").resolve()),
                role="child-summary",
            )
        )
    write_bundle_artifacts(
        bundle=bundle,
        report_root=report_dir,
        summary_json_path=json_path,
        summary_md_path=md_path,
        evidence_links=links,
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run simulated-line abnormal path matrix and emit evidence.")
    parser.add_argument(
        "--report-dir",
        default=str(ROOT / "tests" / "reports" / "e2e" / "simulated-line-matrix"),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir).resolve()
    results = [_run_scenario(report_dir, scenario) for scenario in SCENARIOS]
    overall_status = infer_verdict([item.status for item in results])
    if overall_status == "known-failure":
        overall_status = "known_failure"
    json_path, md_path = _write_report(report_dir, results, overall_status)
    _write_bundle(report_dir, json_path, md_path, results, overall_status)
    print(f"simulated-line matrix overall_status={overall_status}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")
    return 0 if overall_status == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
