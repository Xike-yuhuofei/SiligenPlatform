from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


KNOWN_FAILURE_EXIT_CODE = 10

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT_DIR = ROOT / "tests" / "reports" / "hil-controlled-test"

REQUIRED_WORKSPACE_CASES = (
    "hardware-smoke",
    "hil-closed-loop",
)


@dataclass
class GateCheck:
    name: str
    status: str
    expected: str
    actual: str
    note: str = ""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Verify HIL controlled-production gate from generated reports.")
    parser.add_argument("--report-dir", default=str(DEFAULT_REPORT_DIR))
    parser.add_argument("--workspace-validation-json", default="")
    parser.add_argument("--hil-closed-loop-summary-json", default="")
    return parser.parse_args()


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _to_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _to_float(value: Any, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


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
    if args.hil_closed_loop_summary_json:
        hil_closed_loop_summary_json = Path(args.hil_closed_loop_summary_json).resolve()
    else:
        candidates = (
            report_dir / "hil-closed-loop-summary.json",
            report_dir / "hil-controlled-test" / "hil-closed-loop-summary.json",
        )
        hil_closed_loop_summary_json = candidates[0]
        for candidate in candidates:
            if candidate.exists():
                hil_closed_loop_summary_json = candidate
                break
    return report_dir, workspace_validation_json, hil_closed_loop_summary_json


def _check_workspace_counts(payload: dict[str, Any]) -> GateCheck:
    counts = payload.get("counts", {})
    failed = _to_int(counts.get("failed", 0))
    known_failure = _to_int(counts.get("known_failure", 0))
    skipped = _to_int(counts.get("skipped", 0))
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
        note="workspace validation counts did not satisfy HIL controlled gate",
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
    non_passed = [name for name in REQUIRED_WORKSPACE_CASES if status_by_name.get(name) != "passed"]
    if not missing and not non_passed:
        return GateCheck(
            name="workspace-required-cases",
            status="passed",
            expected="all required cases passed",
            actual="all required cases are passed",
        )

    details = []
    if missing:
        details.append("missing=" + ",".join(missing))
    if non_passed:
        details.append(
            "non_passed="
            + ",".join(f"{name}:{status_by_name.get(name, 'unknown')}" for name in non_passed)
        )
    return GateCheck(
        name="workspace-required-cases",
        status="failed",
        expected="all required cases passed",
        actual="; ".join(details),
        note="required HIL cases are incomplete or non-passed",
    )


def _check_hil_overall(payload: dict[str, Any]) -> GateCheck:
    overall_status = str(payload.get("overall_status", ""))
    if overall_status == "passed":
        return GateCheck(
            name="hil-overall-status",
            status="passed",
            expected="overall_status=passed",
            actual=f"overall_status={overall_status}",
        )
    return GateCheck(
        name="hil-overall-status",
        status="failed",
        expected="overall_status=passed",
        actual=f"overall_status={overall_status}",
        note="hil summary indicates non-passed terminal status",
    )


def _check_state_transition(payload: dict[str, Any]) -> GateCheck:
    checks = payload.get("state_transition_checks", [])
    if not isinstance(checks, list) or not checks:
        return GateCheck(
            name="state-transition-checks",
            status="failed",
            expected="non-empty and all status=passed",
            actual="state_transition_checks missing or empty",
            note="state transition checks are required for HIL controlled gate",
        )

    non_passed = []
    for item in checks:
        name = str(item.get("name", "unknown"))
        status = str(item.get("status", "unknown"))
        if status != "passed":
            non_passed.append(f"{name}:{status}")
    if not non_passed:
        return GateCheck(
            name="state-transition-checks",
            status="passed",
            expected="non-empty and all status=passed",
            actual=f"count={len(checks)} all passed",
        )
    return GateCheck(
        name="state-transition-checks",
        status="failed",
        expected="non-empty and all status=passed",
        actual="non_passed=" + ",".join(non_passed),
        note="state transition checks contain non-passed entries",
    )


def _check_timeout_count(payload: dict[str, Any]) -> GateCheck:
    timeout_count = _to_int(payload.get("timeout_count", 0))
    if timeout_count == 0:
        return GateCheck(
            name="timeout-count",
            status="passed",
            expected="timeout_count=0",
            actual=f"timeout_count={timeout_count}",
        )
    return GateCheck(
        name="timeout-count",
        status="failed",
        expected="timeout_count=0",
        actual=f"timeout_count={timeout_count}",
        note="timeout count is non-zero",
    )


def _check_duration_threshold(payload: dict[str, Any]) -> GateCheck:
    duration_seconds = _to_float(payload.get("duration_seconds", 0))
    elapsed_seconds = _to_float(payload.get("elapsed_seconds", 0))
    actual = f"duration_seconds={duration_seconds}, elapsed_seconds={elapsed_seconds}"
    if elapsed_seconds >= duration_seconds and duration_seconds > 0:
        return GateCheck(
            name="duration-threshold",
            status="passed",
            expected="elapsed_seconds >= duration_seconds and duration_seconds > 0",
            actual=actual,
        )
    return GateCheck(
        name="duration-threshold",
        status="failed",
        expected="elapsed_seconds >= duration_seconds and duration_seconds > 0",
        actual=actual,
        note="HIL long-soak duration threshold was not satisfied",
    )


def _write_report(report_dir: Path, report: dict[str, Any]) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "hil-controlled-gate-summary.json"
    md_path = report_dir / "hil-controlled-gate-summary.md"

    json_path.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

    lines = [
        "# HIL Controlled Gate Summary",
        "",
        f"- generated_at: `{report['generated_at']}`",
        f"- overall_status: `{report['overall_status']}`",
        f"- workspace_validation_json: `{report['workspace_validation_json']}`",
        f"- hil_closed_loop_summary_json: `{report['hil_closed_loop_summary_json']}`",
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
    report_dir, workspace_validation_json, hil_closed_loop_summary_json = _resolve_paths(args)

    checks: list[GateCheck] = []
    error: str = ""

    if not workspace_validation_json.exists():
        error = f"missing workspace validation json: {workspace_validation_json}"
    elif not hil_closed_loop_summary_json.exists():
        error = f"missing HIL closed-loop summary json: {hil_closed_loop_summary_json}"

    if not error:
        workspace_payload = _load_json(workspace_validation_json)
        hil_payload = _load_json(hil_closed_loop_summary_json)
        checks.append(_check_workspace_counts(workspace_payload))
        checks.append(_check_required_workspace_cases(workspace_payload))
        checks.append(_check_hil_overall(hil_payload))
        checks.append(_check_state_transition(hil_payload))
        checks.append(_check_timeout_count(hil_payload))
        checks.append(_check_duration_threshold(hil_payload))

    if error:
        checks.append(
            GateCheck(
                name="input-artifacts",
                status="failed",
                expected="all required report artifacts exist",
                actual=error,
                note="cannot evaluate HIL gate without required report artifacts",
            )
        )

    overall_status = _compute_overall_status(checks)
    report = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "workspace_validation_json": str(workspace_validation_json),
        "hil_closed_loop_summary_json": str(hil_closed_loop_summary_json),
        "overall_status": overall_status,
        "checks": [asdict(item) for item in checks],
    }
    json_path, md_path = _write_report(report_dir, report)
    print(f"hil controlled gate complete: status={overall_status}")
    print(f"gate json report: {json_path}")
    print(f"gate markdown report: {md_path}")

    if error:
        return KNOWN_FAILURE_EXIT_CODE
    if overall_status == "passed":
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
