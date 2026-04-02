from __future__ import annotations

import argparse
import json
import sys
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


KNOWN_FAILURE_EXIT_CODE = 10

ROOT = Path(__file__).resolve().parents[3]
DEFAULT_REPORT_DIR = ROOT / "tests" / "reports" / "controlled-production-test"

REQUIRED_WORKSPACE_CASES = (
    "engineering-regression",
    "simulated-line",
)
EXPECTED_FAULT_MATRIX_ID = "fault-matrix.simulated-line.v1"


@dataclass
class GateCheck:
    name: str
    status: str
    expected: str
    actual: str
    note: str = ""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Verify simulation controlled-production gate from generated reports.")
    parser.add_argument("--report-dir", default=str(DEFAULT_REPORT_DIR))
    parser.add_argument("--workspace-validation-json", default="")
    parser.add_argument("--simulated-line-summary-json", default="")
    parser.add_argument("--simulated-line-bundle-json", default="")
    return parser.parse_args()


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _compute_overall_status(checks: list[GateCheck]) -> str:
    statuses = {item.status for item in checks}
    if "failed" in statuses:
        return "failed"
    if statuses == {"skipped"}:
        return "skipped"
    return "passed"


def _resolve_paths(args: argparse.Namespace) -> tuple[Path, Path, Path, Path]:
    report_dir = Path(args.report_dir).resolve()
    workspace_validation_json = (
        Path(args.workspace_validation_json).resolve()
        if args.workspace_validation_json
        else report_dir / "workspace-validation.json"
    )
    simulated_line_summary_json = (
        Path(args.simulated_line_summary_json).resolve()
        if args.simulated_line_summary_json
        else report_dir / "e2e" / "simulated-line" / "simulated-line-summary.json"
    )
    simulated_line_bundle_json = (
        Path(args.simulated_line_bundle_json).resolve()
        if args.simulated_line_bundle_json
        else report_dir / "e2e" / "simulated-line" / "validation-evidence-bundle.json"
    )
    return report_dir, workspace_validation_json, simulated_line_summary_json, simulated_line_bundle_json


def _check_workspace_counts(payload: dict[str, Any]) -> GateCheck:
    counts = payload.get("counts", {})
    failed = int(counts.get("failed", 0))
    known_failure = int(counts.get("known_failure", 0))
    skipped = int(counts.get("skipped", 0))
    actual = f"failed={failed}, known_failure={known_failure}, skipped={skipped}"
    if failed == 0 and known_failure == 0 and skipped == 0:
        return GateCheck(
            name="workspace-counts",
            status="passed",
            expected="failed=0, known_failure=0, skipped=0",
            actual=actual,
        )
    return GateCheck(
        name="workspace-counts",
        status="failed",
        expected="failed=0, known_failure=0, skipped=0",
        actual=actual,
        note="workspace validation counts did not satisfy controlled-production gate",
    )


def _check_required_workspace_cases(payload: dict[str, Any]) -> GateCheck:
    results = payload.get("results", [])
    status_by_name: dict[str, str] = {}
    for item in results:
        name = str(item.get("name", ""))
        status = str(item.get("status", ""))
        if name:
            status_by_name[name] = status

    missing = [name for name in REQUIRED_WORKSPACE_CASES if name not in status_by_name]
    failed = [name for name in REQUIRED_WORKSPACE_CASES if status_by_name.get(name) != "passed"]

    if not missing and not failed:
        return GateCheck(
            name="workspace-required-cases",
            status="passed",
            expected="all required cases passed",
            actual="all required cases are passed",
        )

    details = []
    if missing:
        details.append("missing=" + ",".join(missing))
    if failed:
        details.append(
            "non_passed="
            + ",".join(f"{name}:{status_by_name.get(name, 'unknown')}" for name in failed)
        )
    return GateCheck(
        name="workspace-required-cases",
        status="failed",
        expected="all required cases passed",
        actual="; ".join(details),
        note="required simulation cases are incomplete or non-passed",
    )


def _check_simulated_line(payload: dict[str, Any]) -> list[GateCheck]:
    checks: list[GateCheck] = []
    overall_status = str(payload.get("overall_status", ""))
    if overall_status == "passed":
        checks.append(
            GateCheck(
                name="simulated-line-overall",
                status="passed",
                expected="overall_status=passed",
                actual=f"overall_status={overall_status}",
            )
        )
    else:
        checks.append(
            GateCheck(
                name="simulated-line-overall",
                status="failed",
                expected="overall_status=passed",
                actual=f"overall_status={overall_status}",
                note="simulated-line summary indicates non-passed terminal status",
            )
        )

    scenarios = payload.get("scenarios", [])
    if not scenarios:
        checks.append(
            GateCheck(
                name="simulated-line-scenarios",
                status="failed",
                expected="at least one scenario executed and all deterministic_replay_passed=true",
                actual="scenario_count=0",
                note="simulated-line exited before any scenario evidence was produced",
            )
        )
        return checks

    non_passed: list[str] = []
    replay_failed: list[str] = []
    for item in scenarios:
        name = str(item.get("name", "unknown"))
        status = str(item.get("status", "unknown"))
        replay = bool(item.get("deterministic_replay_passed", False))
        if status != "passed":
            non_passed.append(f"{name}:{status}")
        if not replay:
            replay_failed.append(name)

    if not non_passed and not replay_failed:
        checks.append(
            GateCheck(
                name="simulated-line-scenarios",
                status="passed",
                expected="at least one scenario executed and all deterministic_replay_passed=true",
                actual=f"scenario_count={len(scenarios)} all passed",
            )
        )
    else:
        parts = []
        if non_passed:
            parts.append("non_passed=" + ",".join(non_passed))
        if replay_failed:
            parts.append("deterministic_replay_failed=" + ",".join(replay_failed))
        checks.append(
            GateCheck(
                name="simulated-line-scenarios",
                status="failed",
                expected="at least one scenario executed and all deterministic_replay_passed=true",
                actual="; ".join(parts),
                note="simulated-line scenario gate failed",
            )
        )
    return checks


def _check_simulated_line_bundle(payload: dict[str, Any]) -> list[GateCheck]:
    checks: list[GateCheck] = []
    metadata = payload.get("metadata", {})
    matrix_id = str(metadata.get("fault_matrix_id", ""))
    checks.append(
        GateCheck(
            name="simulated-line-fault-matrix",
            status="passed" if matrix_id == EXPECTED_FAULT_MATRIX_ID else "failed",
            expected=f"fault_matrix_id={EXPECTED_FAULT_MATRIX_ID}",
            actual=f"fault_matrix_id={matrix_id}",
            note="" if matrix_id == EXPECTED_FAULT_MATRIX_ID else "simulated-line bundle metadata is missing canonical fault matrix id",
        )
    )

    required_metadata = {
        "selected_fault_ids": isinstance(metadata.get("selected_fault_ids"), list),
        "deterministic_seed": metadata.get("deterministic_seed") is not None,
        "clock_profile": bool(metadata.get("clock_profile")),
        "double_surface": isinstance(metadata.get("double_surface"), dict),
    }
    missing = [name for name, present in required_metadata.items() if not present]
    checks.append(
        GateCheck(
            name="simulated-line-bundle-metadata",
            status="passed" if not missing else "failed",
            expected="selected_fault_ids, deterministic_seed, clock_profile, double_surface are present",
            actual="all present" if not missing else "missing=" + ",".join(missing),
            note="" if not missing else "phase9 replay/fault metadata is incomplete",
        )
    )

    records = payload.get("case_records", [])
    incomplete_records: list[str] = []
    for record in records:
        case_id = str(record.get("case_id", "unknown"))
        if "fault_ids" not in record:
            incomplete_records.append(f"{case_id}:fault_ids")
        deterministic_replay = record.get("deterministic_replay")
        if not isinstance(deterministic_replay, dict) or "passed" not in deterministic_replay:
            incomplete_records.append(f"{case_id}:deterministic_replay")
    checks.append(
        GateCheck(
            name="simulated-line-case-replay-metadata",
            status="passed" if not incomplete_records else "failed",
            expected="all case records include fault_ids and deterministic_replay",
            actual="all present" if not incomplete_records else "; ".join(incomplete_records),
            note="" if not incomplete_records else "case-level replay metadata is incomplete",
        )
    )
    return checks


def _write_report(report_dir: Path, report: dict[str, Any]) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "controlled-production-gate-summary.json"
    md_path = report_dir / "controlled-production-gate-summary.md"

    json_path.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

    lines = [
        "# Controlled Production Gate Summary",
        "",
        f"- generated_at: `{report['generated_at']}`",
        f"- overall_status: `{report['overall_status']}`",
        f"- workspace_validation_json: `{report['workspace_validation_json']}`",
        f"- simulated_line_summary_json: `{report['simulated_line_summary_json']}`",
        "",
        "## Checks",
        "",
    ]
    for check in report["checks"]:
        lines.append(
            f"- `{check['status']}` `{check['name']}` expected=`{check['expected']}` actual=`{check['actual']}`"
        )
        if check.get("note"):
            lines.append(f"  note: {check['note']}")
    lines.append("")
    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def main() -> int:
    args = parse_args()
    report_dir, workspace_validation_json, simulated_line_summary_json, simulated_line_bundle_json = _resolve_paths(args)

    checks: list[GateCheck] = []
    error: str = ""

    if not workspace_validation_json.exists():
        error = f"missing workspace validation json: {workspace_validation_json}"
    elif not simulated_line_summary_json.exists():
        error = f"missing simulated-line summary json: {simulated_line_summary_json}"
    elif not simulated_line_bundle_json.exists():
        error = f"missing simulated-line bundle json: {simulated_line_bundle_json}"

    if not error:
        workspace_payload = _load_json(workspace_validation_json)
        simulated_line_payload = _load_json(simulated_line_summary_json)
        simulated_line_bundle_payload = _load_json(simulated_line_bundle_json)
        checks.append(_check_workspace_counts(workspace_payload))
        checks.append(_check_required_workspace_cases(workspace_payload))
        checks.extend(_check_simulated_line(simulated_line_payload))
        checks.extend(_check_simulated_line_bundle(simulated_line_bundle_payload))

    if error:
        checks.append(
            GateCheck(
                name="input-artifacts",
                status="failed",
                expected="all required report artifacts exist",
                actual=error,
                note="cannot evaluate gate without required report artifacts",
            )
        )

    overall_status = _compute_overall_status(checks)
    report = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "workspace_validation_json": str(workspace_validation_json),
        "simulated_line_summary_json": str(simulated_line_summary_json),
        "simulated_line_bundle_json": str(simulated_line_bundle_json),
        "overall_status": overall_status,
        "checks": [asdict(item) for item in checks],
    }
    json_path, md_path = _write_report(report_dir, report)
    print(f"controlled-production gate complete: status={overall_status}")
    print(f"gate json report: {json_path}")
    print(f"gate markdown report: {md_path}")

    if error:
        return KNOWN_FAILURE_EXIT_CODE
    if overall_status == "passed":
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
