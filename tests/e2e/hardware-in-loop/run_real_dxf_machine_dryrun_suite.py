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

from test_kit.dxf_truth_matrix import FullChainTruthCase, full_chain_cases, resolve_full_chain_case  # noqa: E402
from test_kit.evidence_bundle import (  # noqa: E402
    EvidenceBundle,
    EvidenceCaseRecord,
    EvidenceLink,
    infer_verdict,
    trace_fields,
    write_bundle_artifacts,
)


CHILD_RUNNER = ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_real_dxf_machine_dryrun.py"
DEFAULT_REPORT_ROOT = ROOT / "tests" / "reports" / "online-validation" / "real-dxf-machine-dryrun-suite"
DEFAULT_CASE_IDS = tuple(case.case_id for case in full_chain_cases(ROOT))


@dataclass(frozen=True)
class DryrunSuiteCaseResult:
    case_id: str
    dxf_file: str
    status: str
    child_overall_status: str
    verdict_kind: str
    exit_code: int | None
    command: list[str]
    report_dir: str
    launcher_log: str
    summary_json_path: str
    summary_md_path: str
    required_assets: tuple[str, ...]
    note: str = ""


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run the canonical multi-DXF dry-run suite and write an aggregate summary."
    )
    parser.add_argument("--report-dir", type=Path, default=DEFAULT_REPORT_ROOT)
    parser.add_argument("--case-id", action="append", default=[])
    parser.add_argument("--gateway-exe", type=Path, default=None)
    parser.add_argument("--config-path", type=Path, default=None)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--gateway-ready-timeout", type=float, default=8.0)
    parser.add_argument("--connect-timeout", type=float, default=15.0)
    parser.add_argument("--home-timeout", type=float, default=60.0)
    parser.add_argument("--dispensing-speed-mm-s", type=float, default=10.0)
    parser.add_argument("--dry-run-speed-mm-s", type=float, default=10.0)
    parser.add_argument("--rapid-speed-mm-s", type=float, default=20.0)
    parser.add_argument("--velocity-trace-interval-ms", type=int, default=50)
    return parser.parse_args()


def resolve_suite_cases(selected_case_ids: list[str] | tuple[str, ...]) -> tuple[FullChainTruthCase, ...]:
    if not selected_case_ids:
        return tuple(full_chain_cases(ROOT))
    return tuple(resolve_full_chain_case(ROOT, case_id) for case_id in selected_case_ids)


def _discover_child_report_dir(case_report_root: Path) -> Path:
    report_dirs = sorted(path for path in case_report_root.iterdir() if path.is_dir())
    if not report_dirs:
        raise FileNotFoundError(f"dry-run child report directory missing: {case_report_root}")
    return report_dirs[-1]


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _case_required_assets(truth_case: FullChainTruthCase) -> tuple[str, ...]:
    return (
        truth_case.sample_asset_id,
        truth_case.engineering_fixture_asset_id,
    )


def _build_command(
    *,
    args: argparse.Namespace,
    truth_case: FullChainTruthCase,
    case_report_root: Path,
) -> list[str]:
    command = [
        sys.executable,
        str(CHILD_RUNNER),
        "--dxf-file",
        str(truth_case.source_sample_absolute(ROOT)),
        "--report-root",
        str(case_report_root),
        "--host",
        args.host,
        "--port",
        str(args.port),
        "--gateway-ready-timeout",
        str(args.gateway_ready_timeout),
        "--connect-timeout",
        str(args.connect_timeout),
        "--home-timeout",
        str(args.home_timeout),
        "--dispensing-speed-mm-s",
        str(args.dispensing_speed_mm_s),
        "--dry-run-speed-mm-s",
        str(args.dry_run_speed_mm_s),
        "--rapid-speed-mm-s",
        str(args.rapid_speed_mm_s),
        "--velocity-trace-interval-ms",
        str(args.velocity_trace_interval_ms),
    ]
    if args.gateway_exe is not None:
        command.extend(["--gateway-exe", str(args.gateway_exe)])
    if args.config_path is not None:
        command.extend(["--config-path", str(args.config_path)])
    return command


def evaluate_dryrun_case_status(
    *,
    exit_code: int | None,
    child_overall_status: str,
    verdict_kind: str,
) -> str:
    if child_overall_status == "passed" and verdict_kind == "completed" and exit_code == 0:
        return "passed"
    if child_overall_status in {"known_failure", "skipped"}:
        return child_overall_status
    return "failed"


def _run_case(
    *,
    args: argparse.Namespace,
    suite_report_dir: Path,
    truth_case: FullChainTruthCase,
) -> DryrunSuiteCaseResult:
    case_report_root = suite_report_dir / "cases" / truth_case.case_id
    case_report_root.mkdir(parents=True, exist_ok=True)
    launcher_log_path = case_report_root / "launcher.log"
    command = _build_command(args=args, truth_case=truth_case, case_report_root=case_report_root)
    completed = subprocess.run(
        command,
        cwd=str(ROOT),
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )
    combined_output = (completed.stdout or "") + ("\n" if completed.stderr else "") + (completed.stderr or "")
    launcher_log_path.write_text(combined_output, encoding="utf-8")

    child_overall_status = "missing"
    verdict_kind = "missing"
    note = ""
    child_report_dir = case_report_root
    summary_json_path = ""
    summary_md_path = ""
    try:
        child_report_dir = _discover_child_report_dir(case_report_root)
        dryrun_summary = _load_json(child_report_dir / "real-dxf-machine-dryrun-canonical.json")
        child_overall_status = str(dryrun_summary.get("overall_status", "missing"))
        verdict_kind = str(dryrun_summary.get("verdict", {}).get("kind", "missing"))
        summary_json_path = str((child_report_dir / "real-dxf-machine-dryrun-canonical.json").resolve())
        summary_md_path = str((child_report_dir / "real-dxf-machine-dryrun-canonical.md").resolve())
        if child_overall_status != "passed" or verdict_kind != "completed":
            note = (
                f"child_overall_status={child_overall_status} "
                f"verdict_kind={verdict_kind} exit_code={completed.returncode}"
            )
    except Exception as exc:  # noqa: BLE001
        note = str(exc)

    status = evaluate_dryrun_case_status(
        exit_code=completed.returncode,
        child_overall_status=child_overall_status,
        verdict_kind=verdict_kind,
    )
    return DryrunSuiteCaseResult(
        case_id=truth_case.case_id,
        dxf_file=str(truth_case.source_sample_absolute(ROOT)),
        status=status,
        child_overall_status=child_overall_status,
        verdict_kind=verdict_kind,
        exit_code=completed.returncode,
        command=command,
        report_dir=str(child_report_dir.resolve()),
        launcher_log=str(launcher_log_path.resolve()),
        summary_json_path=summary_json_path,
        summary_md_path=summary_md_path,
        required_assets=_case_required_assets(truth_case),
        note=note,
    )


def _write_report(
    *,
    report_dir: Path,
    results: list[DryrunSuiteCaseResult],
    selected_case_ids: tuple[str, ...],
) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "real-dxf-machine-dryrun-suite-summary.json"
    md_path = report_dir / "real-dxf-machine-dryrun-suite-summary.md"
    passed = sum(1 for item in results if item.status == "passed")
    failed = sum(1 for item in results if item.status == "failed")
    known_failures = sum(1 for item in results if item.status == "known_failure")
    skipped = sum(1 for item in results if item.status == "skipped")
    overall_status = "passed" if passed == len(results) and results else infer_verdict([item.status for item in results])
    payload = {
        "generated_at": utc_now(),
        "workspace_root": str(ROOT),
        "overall_status": overall_status,
        "signed_publish_authority": "tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1",
        "selected_case_ids": list(selected_case_ids),
        "counts": {
            "requested": len(results),
            "passed": passed,
            "failed": failed,
            "known_failure": known_failures,
            "skipped": skipped,
        },
        "cases": [asdict(item) for item in results],
    }
    json_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# Real DXF Machine Dry-Run Suite Summary",
        "",
        f"- generated_at: `{payload['generated_at']}`",
        f"- overall_status: `{overall_status}`",
        f"- signed_publish_authority: `{payload['signed_publish_authority']}`",
        f"- requested: `{len(results)}`",
        f"- passed: `{passed}`",
        f"- failed: `{failed}`",
        f"- known_failure: `{known_failures}`",
        f"- skipped: `{skipped}`",
        "",
        "## Cases",
        "",
    ]
    for item in results:
        lines.append(
            f"- `{item.status}` `{item.case_id}` verdict=`{item.verdict_kind}` "
            f"child_overall_status=`{item.child_overall_status}` exit_code=`{item.exit_code}`"
        )
        lines.append(f"  dxf_file: `{item.dxf_file}`")
        lines.append(f"  report_dir: `{item.report_dir}`")
        lines.append(f"  launcher_log: `{item.launcher_log}`")
        if item.summary_json_path:
            lines.append(f"  summary_json: `{item.summary_json_path}`")
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
    linked_assets: set[str] = set()
    for item in payload.get("cases", []):
        required_assets = tuple(str(asset) for asset in item.get("required_assets", ()))
        linked_assets.update(required_assets)
        status = str(item.get("status", "failed"))
        evidence_path = str(item.get("summary_json_path") or item.get("report_dir") or summary_json_path.resolve())
        case_records.append(
            EvidenceCaseRecord(
                case_id=str(item.get("case_id", "")),
                name=f"dryrun-{item.get('case_id', '')}",
                suite_ref="e2e",
                owner_scope="runtime-execution",
                primary_layer="L3",
                producer_lane_ref="limited-hil",
                status=status,
                evidence_profile="real-dxf-dryrun",
                stability_state="stable",
                required_assets=required_assets,
                required_fixtures=("fixture.validation-evidence-bundle",),
                risk_tags=("dryrun", "hardware"),
                note=str(item.get("note", "")),
                trace_fields=trace_fields(
                    stage_id="L3",
                    artifact_id=f"dryrun-{item.get('case_id', '')}",
                    module_id="runtime-execution",
                    workflow_state="executed",
                    execution_state=status,
                    event_name="real-dxf-machine-dryrun-suite",
                    failure_code="" if status == "passed" else f"dryrun.{status}",
                    evidence_path=evidence_path,
                ),
            )
        )

    bundle = EvidenceBundle(
        bundle_id=f"real-dxf-machine-dryrun-suite-{int(time.time())}",
        request_ref="real-dxf-machine-dryrun-suite",
        producer_lane_ref="limited-hil",
        report_root=str(report_dir.resolve()),
        summary_file=str(summary_md_path.resolve()),
        machine_file=str(summary_json_path.resolve()),
        verdict=str(payload.get("overall_status", "incomplete")),
        case_records=case_records,
        linked_asset_refs=tuple(sorted(linked_assets)),
        offline_prerequisites=("L0", "L1", "L2", "L3", "L4"),
        skip_justification=(
            "multi-DXF dry-run suite is supplementary G8 evidence and does not replace the controlled HIL signed publish path"
        ),
        metadata={
            "selected_case_ids": payload.get("selected_case_ids", []),
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
                label="real-dxf-machine-dryrun-suite-summary.json",
                path=str(summary_json_path.resolve()),
                role="machine-summary",
            ),
            EvidenceLink(
                label="real-dxf-machine-dryrun-suite-summary.md",
                path=str(summary_md_path.resolve()),
                role="human-summary",
            ),
        ],
    )


def main() -> int:
    args = parse_args()
    selected_cases = resolve_suite_cases(tuple(args.case_id))
    report_dir = args.report_dir / time.strftime("%Y%m%d-%H%M%S")
    results = [_run_case(args=args, suite_report_dir=report_dir, truth_case=truth_case) for truth_case in selected_cases]
    json_path, md_path = _write_report(
        report_dir=report_dir,
        results=results,
        selected_case_ids=tuple(case.case_id for case in selected_cases),
    )
    overall_status = json.loads(json_path.read_text(encoding="utf-8")).get("overall_status", "failed")
    print(f"dry-run suite complete: status={overall_status}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")
    return 0 if overall_status == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
