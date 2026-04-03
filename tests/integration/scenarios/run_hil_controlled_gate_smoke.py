from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
TEST_KIT_SRC = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"
VERIFY_SCRIPT = WORKSPACE_ROOT / "tests" / "e2e" / "hardware-in-loop" / "verify_hil_controlled_gate.py"
RENDER_SCRIPT = WORKSPACE_ROOT / "tests" / "e2e" / "hardware-in-loop" / "render_hil_controlled_release_summary.py"

if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.evidence_bundle import (  # noqa: E402
    EvidenceBundle,
    EvidenceCaseRecord,
    EvidenceLink,
    bundle_is_compatible,
    default_failure_classification,
    trace_fields,
    write_bundle_artifacts,
)


REQUIRED_OFFLINE_CASES = (
    "protocol-compatibility",
    "engineering-regression",
    "simulated-line",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run synthetic controlled HIL gate smoke with positive and negative cases.")
    parser.add_argument(
        "--report-dir",
        default="",
        help="Optional persistent report root. Defaults to a temporary directory.",
    )
    return parser.parse_args()


def _write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=True, indent=2), encoding="utf-8")


def _write_md(path: Path, title: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f"# {title}\n", encoding="utf-8")


def _offline_payload() -> dict[str, Any]:
    return {
        "counts": {
            "passed": 3,
            "failed": 0,
            "known_failure": 0,
            "skipped": 0,
        },
        "results": [
            {"name": name, "status": "passed"}
            for name in REQUIRED_OFFLINE_CASES
        ],
    }


def _safety_preflight_payload() -> dict[str, Any]:
    return {
        "passed": True,
        "snapshot": {
            "io": {
                "estop": False,
                "limit_x_pos": False,
                "limit_x_neg": False,
                "limit_y_pos": False,
                "limit_y_neg": False,
            }
        },
        "limit_blockers": [],
        "estop_active": False,
        "checks": [
            {
                "name": "safety-preflight-estop",
                "status": "passed",
                "expected": "estop=false",
                "actual": "estop=False",
            },
            {
                "name": "safety-preflight-limits",
                "status": "passed",
                "expected": "no active limit blockers",
                "actual": "none",
            },
        ],
        "failure_classification": default_failure_classification(status="passed"),
    }


def _admission_payload(
    offline_report_path: Path,
    *,
    operator_override_used: bool = False,
    operator_override_reason: str = "",
) -> dict[str, Any]:
    safety_preflight = _safety_preflight_payload()
    return {
        "schema_version": "hil-admission.v1",
        "required_layers": ["L0", "L1", "L2", "L3", "L4"],
        "required_cases": list(REQUIRED_OFFLINE_CASES),
        "offline_prerequisites_source": str(offline_report_path.resolve()),
        "offline_prerequisites_passed": True,
        "operator_override_used": operator_override_used,
        "operator_override_reason": operator_override_reason,
        "admission_decision": "override-admitted" if operator_override_used else "admitted",
        "checks": [
            {
                "name": "offline-prereq-counts",
                "status": "passed",
                "expected": "failed=0, known_failure=0, skipped=0",
                "actual": "failed=0, known_failure=0, skipped=0",
            },
            {
                "name": "offline-prereq-required-cases",
                "status": "passed",
                "expected": "protocol-compatibility, engineering-regression, simulated-line all passed",
                "actual": "all passed",
            },
        ],
        "safety_preflight_passed": True,
        "safety_preflight": safety_preflight,
        "failure_classification": (
            {
                "category": "validation",
                "code": "override-admitted",
                "blocking": False,
                "message": operator_override_reason,
            }
            if operator_override_used
            else default_failure_classification(status="passed")
        ),
    }


def _write_bundle(report_dir: Path, admission: dict[str, Any], *, case_status: str = "passed") -> Path:
    summary_json_path = report_dir / "hil-closed-loop-summary.json"
    summary_md_path = report_dir / "hil-closed-loop-summary.md"
    summary_payload = {
        "generated_at": "2026-04-01T00:00:00+00:00",
        "workspace_root": str(WORKSPACE_ROOT),
        "duration_seconds": 60,
        "pause_resume_cycles": 3,
        "iterations": 12,
        "reconnect_count": 0,
        "timeout_count": 0,
        "elapsed_seconds": 60.0,
        "overall_status": case_status,
        "admission": admission,
        "failure_classification": default_failure_classification(status=case_status),
        "state_transition_checks": [
            {"name": "running-to-paused", "status": "passed"},
            {"name": "paused-to-running", "status": "passed"},
        ],
        "steps": [
            {"name": "hil-admission", "status": "passed"},
            {"name": "safety-preflight", "status": "passed"},
            {"name": "hil-closed-loop", "status": "passed"},
        ],
    }
    _write_json(summary_json_path, summary_payload)
    _write_md(summary_md_path, "HIL Closed Loop Summary")

    bundle = EvidenceBundle(
        bundle_id=f"hil-controlled-gate-smoke-{report_dir.name}",
        request_ref="hil-closed-loop",
        producer_lane_ref="limited-hil",
        report_root=str(report_dir.resolve()),
        summary_file=str(summary_md_path.resolve()),
        machine_file=str(summary_json_path.resolve()),
        verdict=case_status,
        linked_asset_refs=("sample.dxf.rect_diag",),
        offline_prerequisites=tuple(admission.get("required_layers", [])),
        abort_metadata={"admission": admission},
        metadata={"admission": admission},
        case_records=[
            EvidenceCaseRecord(
                case_id="hil-closed-loop",
                name="hil-closed-loop",
                suite_ref="e2e",
                owner_scope="runtime-execution",
                primary_layer="L5",
                producer_lane_ref="limited-hil",
                status=case_status,
                evidence_profile="hil-report",
                stability_state="stable",
                size_label="large",
                label_refs=("suite:e2e", "size:large", "layer:L5"),
                required_assets=("sample.dxf.rect_diag",),
                required_fixtures=("fixture.hil-closed-loop", "fixture.validation-evidence-bundle"),
                failure_classification=default_failure_classification(status=case_status),
                trace_fields=trace_fields(
                    stage_id="L5",
                    artifact_id="hil-closed-loop",
                    module_id="runtime-execution",
                    workflow_state="executed",
                    execution_state=case_status,
                    event_name="hil-closed-loop",
                    failure_code="" if case_status == "passed" else f"hil.{case_status}",
                    evidence_path=str(summary_json_path.resolve()),
                ),
            )
        ],
    )
    written = write_bundle_artifacts(
        bundle=bundle,
        report_root=report_dir,
        summary_json_path=summary_json_path,
        summary_md_path=summary_md_path,
        evidence_links=[
            EvidenceLink(label="hil-closed-loop-summary.json", path=str(summary_json_path.resolve()), role="machine-summary"),
            EvidenceLink(label="hil-closed-loop-summary.md", path=str(summary_md_path.resolve()), role="human-summary"),
        ],
    )
    bundle_payload = json.loads(Path(written["bundle_json"]).read_text(encoding="utf-8"))
    assert bundle_is_compatible(bundle_payload)
    return Path(written["bundle_json"])


def _prepare_case_matrix(report_dir: Path) -> None:
    case_matrix_json = report_dir / "hil-case-matrix" / "case-matrix-summary.json"
    case_matrix_md = report_dir / "hil-case-matrix" / "case-matrix-summary.md"
    _write_json(
        case_matrix_json,
        {
            "overall_status": "passed",
            "rounds_requested": 3,
            "rounds_executed": 3,
            "failure_classification": default_failure_classification(status="passed"),
        },
    )
    _write_md(case_matrix_md, "HIL Case Matrix Summary")


def _prepare_positive_case(report_dir: Path) -> dict[str, Any]:
    offline_json = report_dir / "offline-prereq" / "workspace-validation.json"
    offline_md = report_dir / "offline-prereq" / "workspace-validation.md"
    hardware_json = report_dir / "hardware-smoke" / "hardware-smoke-summary.json"
    hardware_md = report_dir / "hardware-smoke" / "hardware-smoke-summary.md"

    _write_json(offline_json, _offline_payload())
    _write_md(offline_md, "Workspace Validation")
    _write_json(hardware_json, {"overall_status": "passed", "result": "hardware smoke synthetic pass"})
    _write_md(hardware_md, "Hardware Smoke Summary")

    admission = _admission_payload(offline_json)
    bundle_json = _write_bundle(report_dir, admission)
    _prepare_case_matrix(report_dir)

    return {
        "offline_json": str(offline_json.resolve()),
        "hardware_json": str(hardware_json.resolve()),
        "bundle_json": str(bundle_json.resolve()),
    }


def _prepare_missing_offline_case(report_dir: Path) -> None:
    hardware_json = report_dir / "hardware-smoke" / "hardware-smoke-summary.json"
    hardware_md = report_dir / "hardware-smoke" / "hardware-smoke-summary.md"
    _write_json(hardware_json, {"overall_status": "passed"})
    _write_md(hardware_md, "Hardware Smoke Summary")

    missing_offline_path = report_dir / "offline-prereq" / "workspace-validation.json"
    admission = _admission_payload(missing_offline_path)
    _write_bundle(report_dir, admission)


def _prepare_missing_override_reason_case(report_dir: Path) -> None:
    offline_json = report_dir / "offline-prereq" / "workspace-validation.json"
    offline_md = report_dir / "offline-prereq" / "workspace-validation.md"
    hardware_json = report_dir / "hardware-smoke" / "hardware-smoke-summary.json"
    hardware_md = report_dir / "hardware-smoke" / "hardware-smoke-summary.md"

    _write_json(offline_json, _offline_payload())
    _write_md(offline_md, "Workspace Validation")
    _write_json(hardware_json, {"overall_status": "passed"})
    _write_md(hardware_md, "Hardware Smoke Summary")

    admission = _admission_payload(
        offline_json,
        operator_override_used=True,
        operator_override_reason="",
    )
    _write_bundle(report_dir, admission)


def _run_python(script: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(script), *args],
        cwd=WORKSPACE_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _run_positive_case(report_dir: Path) -> dict[str, Any]:
    _prepare_positive_case(report_dir)
    verify = _run_python(VERIFY_SCRIPT, "--report-dir", str(report_dir), "--require-hil-case-matrix")
    assert verify.returncode == 0, verify.stdout + verify.stderr

    render = _run_python(RENDER_SCRIPT, "--report-dir", str(report_dir), "--profile", "Local", "--executor", "synthetic-smoke")
    assert render.returncode == 0, render.stdout + render.stderr

    gate_summary = _load_json(report_dir / "hil-controlled-gate-summary.json")
    release_summary = (report_dir / "hil-controlled-release-summary.md").read_text(encoding="utf-8")
    assert gate_summary["overall_status"] == "passed"
    assert "通过" in release_summary

    return {
        "report_dir": str(report_dir.resolve()),
        "gate_returncode": verify.returncode,
        "render_returncode": render.returncode,
        "gate_summary_json": str((report_dir / "hil-controlled-gate-summary.json").resolve()),
        "release_summary_md": str((report_dir / "hil-controlled-release-summary.md").resolve()),
    }


def _run_missing_offline_case(report_dir: Path) -> dict[str, Any]:
    _prepare_missing_offline_case(report_dir)
    verify = _run_python(VERIFY_SCRIPT, "--report-dir", str(report_dir))
    assert verify.returncode == 10, verify.stdout + verify.stderr

    gate_summary = _load_json(report_dir / "hil-controlled-gate-summary.json")
    artifact_check = next(
        entry
        for entry in gate_summary["checks"]
        if entry["name"] == "artifact:offline_prereq_json"
    )
    assert artifact_check["status"] == "failed"

    return {
        "report_dir": str(report_dir.resolve()),
        "gate_returncode": verify.returncode,
        "gate_summary_json": str((report_dir / "hil-controlled-gate-summary.json").resolve()),
    }


def _run_missing_override_reason_case(report_dir: Path) -> dict[str, Any]:
    _prepare_missing_override_reason_case(report_dir)
    verify = _run_python(VERIFY_SCRIPT, "--report-dir", str(report_dir))
    assert verify.returncode == 1, verify.stdout + verify.stderr

    gate_summary = _load_json(report_dir / "hil-controlled-gate-summary.json")
    override_check = next(
        entry
        for entry in gate_summary["checks"]
        if entry["name"] == "hil-bundle-operator-override"
    )
    assert override_check["status"] == "failed"

    return {
        "report_dir": str(report_dir.resolve()),
        "gate_returncode": verify.returncode,
        "gate_summary_json": str((report_dir / "hil-controlled-gate-summary.json").resolve()),
    }


def _execute(root: Path) -> dict[str, Any]:
    positive_dir = root / "positive"
    missing_offline_dir = root / "negative-missing-offline"
    missing_override_dir = root / "negative-missing-override-reason"

    return {
        "report_root": str(root.resolve()),
        "positive": _run_positive_case(positive_dir),
        "negative_missing_offline": _run_missing_offline_case(missing_offline_dir),
        "negative_missing_override_reason": _run_missing_override_reason_case(missing_override_dir),
    }


def main() -> int:
    args = parse_args()
    if args.report_dir:
        report_root = Path(args.report_dir).expanduser().resolve()
        report_root.mkdir(parents=True, exist_ok=True)
        payload = _execute(report_root)
    else:
        with tempfile.TemporaryDirectory() as temp_dir:
            payload = _execute(Path(temp_dir))

    print(json.dumps(payload, ensure_ascii=True, indent=2))
    print("hil controlled gate smoke passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
