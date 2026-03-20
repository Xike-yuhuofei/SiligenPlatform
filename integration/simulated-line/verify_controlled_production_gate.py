from __future__ import annotations

import argparse
import json
import sys
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


KNOWN_FAILURE_EXIT_CODE = 10

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT_DIR = ROOT / "integration" / "reports" / "controlled-production-test"

REQUIRED_WORKSPACE_CASES = (
    "simulation-engine-smoke",
    "simulation-engine-json-io",
    "simulation-engine-scheme-c",
    "simulation-runtime-core-contracts",
    "simulated-line",
)


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


def _resolve_paths(args: argparse.Namespace) -> tuple[Path, Path, Path]:
    report_dir = Path(args.report_dir).resolve()
    workspace_validation_json = (
        Path(args.workspace_validation_json).resolve()
        if args.workspace_validation_json
        else report_dir / "workspace-validation.json"
    )
    simulated_line_summary_json = (
        Path(args.simulated_line_summary_json).resolve()
        if args.simulated_line_summary_json
        else report_dir / "simulation" / "simulated-line" / "simulated-line-summary.json"
    )
    return report_dir, workspace_validation_json, simulated_line_summary_json


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
                expected="all scenarios passed and deterministic_replay_passed=true",
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
                expected="all scenarios passed and deterministic_replay_passed=true",
                actual="; ".join(parts),
                note="simulated-line scenario gate failed",
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
    report_dir, workspace_validation_json, simulated_line_summary_json = _resolve_paths(args)

    checks: list[GateCheck] = []
    error: str = ""

    if not workspace_validation_json.exists():
        error = f"missing workspace validation json: {workspace_validation_json}"
    elif not simulated_line_summary_json.exists():
        error = f"missing simulated-line summary json: {simulated_line_summary_json}"

    if not error:
        workspace_payload = _load_json(workspace_validation_json)
        simulated_line_payload = _load_json(simulated_line_summary_json)
        checks.append(_check_workspace_counts(workspace_payload))
        checks.append(_check_required_workspace_cases(workspace_payload))
        checks.extend(_check_simulated_line(simulated_line_payload))

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
