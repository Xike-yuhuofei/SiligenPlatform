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
REQUIRED_OFFLINE_CASES = (
    "protocol-compatibility",
    "engineering-regression",
    "simulated-line",
)
EXPECTED_BUNDLE_SCHEMA = "validation-evidence-bundle.v2"
EXPECTED_MANIFEST_SCHEMA = "validation-report-manifest.v1"
EXPECTED_INDEX_SCHEMA = "validation-report-index.v1"


@dataclass
class GateCheck:
    name: str
    status: str
    expected: str
    actual: str
    note: str = ""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Verify HIL controlled gate from offline/HIL evidence artifacts.")
    parser.add_argument("--report-dir", default=str(DEFAULT_REPORT_DIR))
    parser.add_argument("--offline-prereq-json", default="")
    parser.add_argument("--hardware-smoke-summary-json", default="")
    parser.add_argument("--hil-closed-loop-summary-json", default="")
    parser.add_argument("--hil-case-matrix-summary-json", default="")
    parser.add_argument("--hil-evidence-bundle-json", default="")
    parser.add_argument("--require-hil-case-matrix", action="store_true")
    return parser.parse_args()


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _to_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _compute_overall_status(checks: list[GateCheck]) -> str:
    statuses = {check.status for check in checks}
    if "failed" in statuses:
        return "failed"
    if statuses == {"skipped"}:
        return "skipped"
    return "passed"


def _resolve_path(explicit: str, fallback: Path) -> Path:
    return Path(explicit).resolve() if explicit else fallback.resolve()


def _resolve_paths(args: argparse.Namespace) -> dict[str, Path]:
    report_dir = Path(args.report_dir).resolve()
    return {
        "report_dir": report_dir,
        "offline_prereq_json": _resolve_path(
            args.offline_prereq_json,
            report_dir / "offline-prereq" / "workspace-validation.json",
        ),
        "hardware_smoke_summary_json": _resolve_path(
            args.hardware_smoke_summary_json,
            report_dir / "hardware-smoke" / "hardware-smoke-summary.json",
        ),
        "hil_closed_loop_summary_json": _resolve_path(
            args.hil_closed_loop_summary_json,
            report_dir / "hil-closed-loop-summary.json",
        ),
        "hil_case_matrix_summary_json": _resolve_path(
            args.hil_case_matrix_summary_json,
            report_dir / "hil-case-matrix" / "case-matrix-summary.json",
        ),
        "hil_evidence_bundle_json": _resolve_path(
            args.hil_evidence_bundle_json,
            report_dir / "validation-evidence-bundle.json",
        ),
    }


def _check_input_artifacts(paths: dict[str, Path], require_hil_case_matrix: bool) -> list[GateCheck]:
    checks: list[GateCheck] = []
    required = (
        "offline_prereq_json",
        "hardware_smoke_summary_json",
        "hil_closed_loop_summary_json",
        "hil_evidence_bundle_json",
    )
    for key in required:
        path = paths[key]
        checks.append(
            GateCheck(
                name=f"artifact:{key}",
                status="passed" if path.exists() else "failed",
                expected="file exists",
                actual=str(path),
                note="" if path.exists() else "required artifact missing",
            )
        )
    if require_hil_case_matrix:
        path = paths["hil_case_matrix_summary_json"]
        checks.append(
            GateCheck(
                name="artifact:hil_case_matrix_summary_json",
                status="passed" if path.exists() else "failed",
                expected="file exists",
                actual=str(path),
                note="" if path.exists() else "required case-matrix artifact missing",
            )
        )
    return checks


def _check_offline_counts(payload: dict[str, Any]) -> GateCheck:
    counts = payload.get("counts", {})
    failed = _to_int(counts.get("failed", 0))
    known_failure = _to_int(counts.get("known_failure", 0))
    skipped = _to_int(counts.get("skipped", 0))
    actual = f"failed={failed}, known_failure={known_failure}, skipped={skipped}"
    return GateCheck(
        name="offline-prereq-counts",
        status="passed" if failed == 0 and known_failure == 0 and skipped == 0 else "failed",
        expected="failed=0, known_failure=0, skipped=0",
        actual=actual,
        note="" if failed == 0 and known_failure == 0 and skipped == 0 else "offline prerequisites did not fully pass",
    )


def _check_offline_required_cases(payload: dict[str, Any]) -> GateCheck:
    status_by_name: dict[str, str] = {}
    for item in payload.get("results", []):
        name = str(item.get("name", ""))
        if name:
            status_by_name[name] = str(item.get("status", ""))
    missing = [name for name in REQUIRED_OFFLINE_CASES if name not in status_by_name]
    non_passed = [name for name in REQUIRED_OFFLINE_CASES if status_by_name.get(name) != "passed"]
    if not missing and not non_passed:
        return GateCheck(
            name="offline-prereq-required-cases",
            status="passed",
            expected="protocol-compatibility, engineering-regression, simulated-line all passed",
            actual="all required offline cases passed",
        )
    actual_parts: list[str] = []
    if missing:
        actual_parts.append("missing=" + ",".join(missing))
    if non_passed:
        actual_parts.append("non_passed=" + ",".join(f"{name}:{status_by_name.get(name, 'missing')}" for name in non_passed))
    return GateCheck(
        name="offline-prereq-required-cases",
        status="failed",
        expected="protocol-compatibility, engineering-regression, simulated-line all passed",
        actual="; ".join(actual_parts),
        note="offline release predicate for limited-hil is not satisfied",
    )


def _check_summary_status(payload: dict[str, Any], *, name: str, expected_status: str = "passed") -> GateCheck:
    overall_status = str(payload.get("overall_status", ""))
    return GateCheck(
        name=name,
        status="passed" if overall_status == expected_status else "failed",
        expected=f"overall_status={expected_status}",
        actual=f"overall_status={overall_status}",
        note="" if overall_status == expected_status else f"{name} summary is not passed",
    )


def _check_hil_bundle(payload: dict[str, Any]) -> list[GateCheck]:
    checks: list[GateCheck] = []
    schema_version = str(payload.get("schema_version", ""))
    checks.append(
        GateCheck(
            name="hil-bundle-schema-version",
            status="passed" if schema_version == EXPECTED_BUNDLE_SCHEMA else "failed",
            expected=f"schema_version={EXPECTED_BUNDLE_SCHEMA}",
            actual=f"schema_version={schema_version}",
            note="" if schema_version == EXPECTED_BUNDLE_SCHEMA else "unexpected HIL evidence schema version",
        )
    )

    producer_lane_ref = str(payload.get("producer_lane_ref", ""))
    checks.append(
        GateCheck(
            name="hil-bundle-lane",
            status="passed" if producer_lane_ref == "limited-hil" else "failed",
            expected="producer_lane_ref=limited-hil",
            actual=f"producer_lane_ref={producer_lane_ref}",
            note="" if producer_lane_ref == "limited-hil" else "limited HIL bundle lane metadata mismatch",
        )
    )

    manifest_file = str(payload.get("report_manifest_file", "")).strip()
    if manifest_file:
        manifest_path = Path(manifest_file)
        manifest_exists = manifest_path.exists()
        manifest_schema = ""
        if manifest_exists:
            manifest_schema = str(_load_json(manifest_path).get("schema_version", ""))
        checks.append(
            GateCheck(
                name="hil-bundle-report-manifest",
                status="passed" if manifest_exists and manifest_schema == EXPECTED_MANIFEST_SCHEMA else "failed",
                expected=f"manifest exists and schema_version={EXPECTED_MANIFEST_SCHEMA}",
                actual=f"path={manifest_path}; schema_version={manifest_schema or 'missing'}",
                note="" if manifest_exists and manifest_schema == EXPECTED_MANIFEST_SCHEMA else "report manifest missing or incompatible",
            )
        )
    else:
        checks.append(
            GateCheck(
                name="hil-bundle-report-manifest",
                status="failed",
                expected=f"manifest exists and schema_version={EXPECTED_MANIFEST_SCHEMA}",
                actual="report_manifest_file missing",
                note="HIL bundle did not publish report-manifest.json",
            )
        )

    index_file = str(payload.get("report_index_file", "")).strip()
    if index_file:
        index_path = Path(index_file)
        index_exists = index_path.exists()
        index_schema = ""
        if index_exists:
            index_schema = str(_load_json(index_path).get("schema_version", ""))
        checks.append(
            GateCheck(
                name="hil-bundle-report-index",
                status="passed" if index_exists and index_schema == EXPECTED_INDEX_SCHEMA else "failed",
                expected=f"index exists and schema_version={EXPECTED_INDEX_SCHEMA}",
                actual=f"path={index_path}; schema_version={index_schema or 'missing'}",
                note="" if index_exists and index_schema == EXPECTED_INDEX_SCHEMA else "report index missing or incompatible",
            )
        )
    else:
        checks.append(
            GateCheck(
                name="hil-bundle-report-index",
                status="failed",
                expected=f"index exists and schema_version={EXPECTED_INDEX_SCHEMA}",
                actual="report_index_file missing",
                note="HIL bundle did not publish report-index.json",
            )
        )

    admission = payload.get("metadata", {}).get("admission", {})
    if not isinstance(admission, dict):
        admission = {}
    checks.append(
        GateCheck(
            name="hil-bundle-admission-metadata",
            status="passed" if admission else "failed",
            expected="metadata.admission exists",
            actual="present" if admission else "missing",
            note="" if admission else "HIL evidence bundle missing admission metadata",
        )
    )
    checks.append(
        GateCheck(
            name="hil-bundle-safety-preflight",
            status="passed" if bool(admission.get("safety_preflight_passed", False)) else "failed",
            expected="metadata.admission.safety_preflight_passed=true",
            actual=f"safety_preflight_passed={admission.get('safety_preflight_passed', False)}",
            note="" if bool(admission.get("safety_preflight_passed", False)) else "safety preflight must pass before limited-hil release",
        )
    )

    operator_override_used = bool(admission.get("operator_override_used", False))
    operator_override_reason = str(admission.get("operator_override_reason", "")).strip()
    override_ok = (not operator_override_used) or bool(operator_override_reason)
    checks.append(
        GateCheck(
            name="hil-bundle-operator-override",
            status="passed" if override_ok else "failed",
            expected="operator override must carry a non-empty reason",
            actual=f"operator_override_used={operator_override_used}; operator_override_reason={operator_override_reason!r}",
            note="" if override_ok else "operator override reason missing",
        )
    )

    case_records = payload.get("case_records", [])
    missing_trace_fields: list[str] = []
    failure_classification_missing: list[str] = []
    if isinstance(case_records, list):
        for record in case_records:
            trace = record.get("trace_fields", {})
            for field_name in REQUIRED_TRACE_FIELDS:
                if field_name not in trace:
                    missing_trace_fields.append(f"{record.get('case_id', 'unknown')}:{field_name}")
            classification = record.get("failure_classification", {})
            for field_name in ("category", "code", "blocking"):
                if field_name not in classification:
                    failure_classification_missing.append(f"{record.get('case_id', 'unknown')}:{field_name}")

    checks.append(
        GateCheck(
            name="hil-bundle-trace-fields",
            status="passed" if not missing_trace_fields else "failed",
            expected="all case records contain required trace fields",
            actual="all present" if not missing_trace_fields else "; ".join(missing_trace_fields),
            note="" if not missing_trace_fields else "required trace field missing from case records",
        )
    )
    checks.append(
        GateCheck(
            name="hil-bundle-failure-classification",
            status="passed" if not failure_classification_missing else "failed",
            expected="all case records contain failure classification category/code/blocking",
            actual="all present" if not failure_classification_missing else "; ".join(failure_classification_missing),
            note="" if not failure_classification_missing else "failure classification metadata incomplete",
        )
    )
    return checks


def _write_report(report_dir: Path, payload: dict[str, Any]) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "hil-controlled-gate-summary.json"
    md_path = report_dir / "hil-controlled-gate-summary.md"
    json_path.write_text(json.dumps(payload, ensure_ascii=True, indent=2), encoding="utf-8")

    lines = [
        "# HIL Controlled Gate Summary",
        "",
        f"- generated_at: `{payload['generated_at']}`",
        f"- overall_status: `{payload['overall_status']}`",
        f"- offline_prereq_json: `{payload['offline_prereq_json']}`",
        f"- hardware_smoke_summary_json: `{payload['hardware_smoke_summary_json']}`",
        f"- hil_closed_loop_summary_json: `{payload['hil_closed_loop_summary_json']}`",
        f"- hil_evidence_bundle_json: `{payload['hil_evidence_bundle_json']}`",
        f"- hil_case_matrix_summary_json: `{payload['hil_case_matrix_summary_json']}`",
        f"- require_hil_case_matrix: `{payload['require_hil_case_matrix']}`",
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
    paths = _resolve_paths(args)
    checks = _check_input_artifacts(paths, bool(args.require_hil_case_matrix))

    if all(check.status == "passed" for check in checks):
        offline_payload = _load_json(paths["offline_prereq_json"])
        hardware_payload = _load_json(paths["hardware_smoke_summary_json"])
        hil_payload = _load_json(paths["hil_closed_loop_summary_json"])
        bundle_payload = _load_json(paths["hil_evidence_bundle_json"])

        checks.extend(
            [
                _check_offline_counts(offline_payload),
                _check_offline_required_cases(offline_payload),
                _check_summary_status(hardware_payload, name="hardware-smoke-overall-status"),
                _check_summary_status(hil_payload, name="hil-closed-loop-overall-status"),
            ]
        )
        checks.extend(_check_hil_bundle(bundle_payload))

        if bool(args.require_hil_case_matrix):
            case_matrix_payload = _load_json(paths["hil_case_matrix_summary_json"])
            checks.append(_check_summary_status(case_matrix_payload, name="hil-case-matrix-overall-status"))

    overall_status = _compute_overall_status(checks)
    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "offline_prereq_json": str(paths["offline_prereq_json"]),
        "hardware_smoke_summary_json": str(paths["hardware_smoke_summary_json"]),
        "hil_closed_loop_summary_json": str(paths["hil_closed_loop_summary_json"]),
        "hil_case_matrix_summary_json": str(paths["hil_case_matrix_summary_json"]) if args.require_hil_case_matrix else "",
        "hil_evidence_bundle_json": str(paths["hil_evidence_bundle_json"]),
        "require_hil_case_matrix": bool(args.require_hil_case_matrix),
        "required_offline_cases": list(REQUIRED_OFFLINE_CASES),
        "overall_status": overall_status,
        "checks": [asdict(check) for check in checks],
    }
    json_path, md_path = _write_report(paths["report_dir"], payload)
    print(f"hil controlled gate complete: status={overall_status}")
    print(f"gate json report: {json_path}")
    print(f"gate markdown report: {md_path}")
    if overall_status == "passed":
        return 0
    if any(check.name.startswith("artifact:") and check.status == "failed" for check in checks):
        return KNOWN_FAILURE_EXIT_CODE
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
