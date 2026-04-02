from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


KNOWN_FAILURE_EXIT_CODE = 10
ROOT = Path(__file__).resolve().parents[3]
REQUIRED_TRACE_FIELDS = (
    "stage_id",
    "artifact_id",
    "module_id",
    "workflow_state",
    "execution_state",
    "event_name",
    "failure_code",
    "evidence_path",
)


@dataclass
class EvidenceCheck:
    name: str
    status: str
    expected: str
    actual: str
    note: str = ""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Verify controlled-production simulated-line evidence bundle outputs.")
    parser.add_argument("--report-dir", default=str(ROOT / "tests" / "reports" / "controlled-production-test"))
    return parser.parse_args()


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _required_paths(report_dir: Path) -> dict[str, Path]:
    return {
        "workspace_validation_json": report_dir / "workspace-validation.json",
        "workspace_validation_md": report_dir / "workspace-validation.md",
        "simulated_line_summary_json": report_dir / "e2e" / "simulated-line" / "simulated-line-summary.json",
        "simulated_line_summary_md": report_dir / "e2e" / "simulated-line" / "simulated-line-summary.md",
        "simulated_line_bundle_json": report_dir / "e2e" / "simulated-line" / "validation-evidence-bundle.json",
        "simulated_line_case_index_json": report_dir / "e2e" / "simulated-line" / "case-index.json",
        "simulated_line_links_md": report_dir / "e2e" / "simulated-line" / "evidence-links.md",
        "controlled_gate_json": report_dir / "controlled-production-gate-summary.json",
        "controlled_gate_md": report_dir / "controlled-production-gate-summary.md",
        "release_summary_md": report_dir / "simulation-controlled-production-release-summary.md",
    }


def _write_report(report_dir: Path, payload: dict[str, Any]) -> tuple[Path, Path]:
    json_path = report_dir / "controlled-production-evidence-check.json"
    md_path = report_dir / "controlled-production-evidence-check.md"
    json_path.write_text(json.dumps(payload, ensure_ascii=True, indent=2), encoding="utf-8")

    lines = [
        "# Controlled Production Evidence Check",
        "",
        f"- generated_at: `{payload['generated_at']}`",
        f"- overall_status: `{payload['overall_status']}`",
        "",
        "## Checks",
        "",
    ]
    for check in payload["checks"]:
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
    report_dir = Path(args.report_dir).resolve()
    checks: list[EvidenceCheck] = []

    required = _required_paths(report_dir)
    missing = [name for name, path in required.items() if not path.exists()]
    if missing:
        checks.append(
            EvidenceCheck(
                name="required-artifacts",
                status="failed",
                expected="all required controlled-production artifacts exist",
                actual="missing=" + ",".join(missing),
                note="cannot validate evidence bundle without required artifacts",
            )
        )
    else:
        checks.append(
            EvidenceCheck(
                name="required-artifacts",
                status="passed",
                expected="all required controlled-production artifacts exist",
                actual="all present",
            )
        )

        bundle = _load_json(required["simulated_line_bundle_json"])
        case_index = _load_json(required["simulated_line_case_index_json"])
        gate_summary = _load_json(required["controlled_gate_json"])

        case_records = bundle.get("case_records", [])
        missing_trace = []
        for record in case_records:
            trace = record.get("trace_fields", {})
            absent = [field_name for field_name in REQUIRED_TRACE_FIELDS if field_name not in trace]
            if absent:
                missing_trace.append(f"{record.get('case_id', 'unknown')}:{','.join(absent)}")
        checks.append(
            EvidenceCheck(
                name="trace-fields",
                status="passed" if not missing_trace else "failed",
                expected="all simulated-line case records contain required trace fields",
                actual="all present" if not missing_trace else "; ".join(missing_trace),
            )
        )

        checks.append(
            EvidenceCheck(
                name="case-index",
                status="passed" if case_index.get("cases") else "failed",
                expected="case-index.json contains executed cases",
                actual=f"case_count={len(case_index.get('cases', []))}",
            )
        )

        replay_failures = []
        for record in case_records:
            if "fault_ids" not in record:
                replay_failures.append(f"{record.get('case_id', 'unknown')}:missing fault_ids")
            if "deterministic_replay" not in record:
                replay_failures.append(f"{record.get('case_id', 'unknown')}:missing deterministic_replay")
        checks.append(
            EvidenceCheck(
                name="fault-replay-fields",
                status="passed" if not replay_failures else "failed",
                expected="all simulated-line case records include fault_ids and deterministic_replay",
                actual="all present" if not replay_failures else "; ".join(replay_failures),
            )
        )

        bundle_metadata = bundle.get("metadata", {})
        double_surface = bundle_metadata.get("double_surface", {})
        matrix_ok = (
            bundle_metadata.get("fault_matrix_id") == "fault-matrix.simulated-line.v1"
            and bundle_metadata.get("deterministic_seed") is not None
            and bundle_metadata.get("clock_profile")
            and isinstance(bundle_metadata.get("selected_fault_ids"), list)
            and isinstance(double_surface, dict)
            and "fake_controller" in double_surface
            and "fake_io" in double_surface
        )
        checks.append(
            EvidenceCheck(
                name="fault-matrix-metadata",
                status="passed" if matrix_ok else "failed",
                expected="bundle metadata contains fault matrix id, deterministic replay config, and double surface",
                actual=json.dumps(bundle_metadata, ensure_ascii=True, sort_keys=True),
            )
        )

        gate_status = str(gate_summary.get("overall_status", ""))
        checks.append(
            EvidenceCheck(
                name="controlled-gate",
                status="passed" if gate_status == "passed" else "failed",
                expected="controlled-production gate overall_status=passed",
                actual=f"overall_status={gate_status}",
            )
        )

    overall_status = "failed" if any(check.status == "failed" for check in checks) else "passed"
    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "report_dir": str(report_dir),
        "overall_status": overall_status,
        "checks": [asdict(check) for check in checks],
    }
    json_path, md_path = _write_report(report_dir, payload)
    print(f"controlled-production evidence check: status={overall_status}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")

    if any(check.name == "required-artifacts" and check.status == "failed" for check in checks):
        return KNOWN_FAILURE_EXIT_CODE
    return 0 if overall_status == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
