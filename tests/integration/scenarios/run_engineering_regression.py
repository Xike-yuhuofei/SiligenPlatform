from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, cast


ROOT = Path(__file__).resolve().parents[3]
ENGINEERING_DATA_SRC = ROOT / "modules" / "dxf-geometry" / "application"
FIXTURE_ROOT = ROOT / "shared" / "contracts" / "engineering" / "fixtures" / "cases" / "rect_diag"
ENGINEERING_DATA_BRIDGE = ROOT / "scripts" / "engineering-data"
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"

if str(ENGINEERING_DATA_SRC) not in sys.path:
    sys.path.insert(0, str(ENGINEERING_DATA_SRC))
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from engineering_data.contracts.simulation_input import load_path_bundle  # noqa: E402
from test_kit.evidence_bundle import (  # noqa: E402
    EvidenceBundle,
    EvidenceCaseRecord,
    EvidenceLink,
    default_deterministic_replay,
    infer_verdict,
    trace_fields,
    write_bundle_artifacts,
)


@dataclass
class CaseResult:
    case_id: str
    title: str
    status: str
    sample_id: str
    detail: dict[str, Any]
    note: str = ""


def _load_json(path: Path) -> dict[str, Any]:
    return cast(dict[str, Any], json.loads(path.read_text(encoding="utf-8")))


def _run_command(command: list[str]) -> dict[str, Any]:
    completed = subprocess.run(
        command,
        cwd=str(ROOT),
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    payload = {
        "command": command,
        "returncode": completed.returncode,
        "stdout": completed.stdout.strip(),
        "stderr": completed.stderr.strip(),
    }
    if completed.returncode != 0:
        raise AssertionError(json.dumps(payload, ensure_ascii=False, indent=2))
    return payload


def _assert_dxf_to_pb(dxf_path: Path, expected_pb_path: Path, output_pb_path: Path) -> CaseResult:
    command_result = _run_command(
        [
            sys.executable,
            str(ENGINEERING_DATA_BRIDGE / "dxf_to_pb.py"),
            "--input",
            str(dxf_path),
            "--output",
            str(output_pb_path),
        ]
    )
    assert output_pb_path.exists(), f"generated pb missing: {output_pb_path}"

    actual_pb = load_path_bundle(output_pb_path)
    expected_pb = load_path_bundle(expected_pb_path)
    actual_pb.header.source_path = ""
    expected_pb.header.source_path = ""
    assert (
        actual_pb.SerializeToString() == expected_pb.SerializeToString()
    ), "exported PathBundle differs from canonical fixture"

    return CaseResult(
        case_id="dxf-to-pb",
        title="DXF to PB canonical export",
        status="passed",
        sample_id="protocol.fixture.rect_diag_dxf",
        detail={
            "command": command_result["command"],
            "output_pb": str(output_pb_path),
            "expected_pb": str(expected_pb_path),
        },
    )


def _assert_simulation_export(pb_path: Path, expected_sim_path: Path, output_sim_path: Path) -> CaseResult:
    command_result = _run_command(
        [
            sys.executable,
            str(ENGINEERING_DATA_BRIDGE / "export_simulation_input.py"),
            "--input",
            str(pb_path),
            "--output",
            str(output_sim_path),
        ]
    )
    assert output_sim_path.exists(), f"generated simulation input missing: {output_sim_path}"

    actual_sim = _load_json(output_sim_path)
    expected_sim = _load_json(expected_sim_path)
    assert actual_sim == expected_sim, "simulation input differs from canonical fixture"

    return CaseResult(
        case_id="simulation-input-export",
        title="PB to simulation input canonical export",
        status="passed",
        sample_id="protocol.fixture.rect_diag_engineering",
        detail={
            "command": command_result["command"],
            "input_pb": str(pb_path),
            "output_sim": str(output_sim_path),
            "segment_count": len(actual_sim.get("segments", [])),
        },
    )


def _assert_preview_determinism(dxf_path: Path, output_root: Path, expected_preview_path: Path) -> CaseResult:
    command = [
        sys.executable,
        str(ENGINEERING_DATA_BRIDGE / "generate_preview.py"),
        "--input",
        str(dxf_path),
        "--output-dir",
        str(output_root),
        "--json",
        "--deterministic",
    ]
    run_a = _run_command(command)
    payload_a = json.loads((run_a["stdout"] or "").splitlines()[-1])
    preview_path_a = Path(payload_a["preview_path"])
    assert preview_path_a.exists(), f"preview artifact missing: {preview_path_a}"
    preview_html_a = preview_path_a.read_bytes()

    run_b = _run_command(command)
    payload_b = json.loads((run_b["stdout"] or "").splitlines()[-1])
    preview_path_b = Path(payload_b["preview_path"])
    assert preview_path_b.exists(), f"preview artifact missing: {preview_path_b}"
    preview_html_b = preview_path_b.read_bytes()

    expected_preview = _load_json(expected_preview_path)
    comparable_payload_a = dict(payload_a)
    comparable_payload_b = dict(payload_b)
    comparable_expected_preview = dict(expected_preview)
    comparable_payload_a["preview_path"] = Path(str(payload_a["preview_path"])).name
    comparable_payload_b["preview_path"] = Path(str(payload_b["preview_path"])).name
    comparable_expected_preview["preview_path"] = Path(str(expected_preview["preview_path"])).name

    assert comparable_payload_a == comparable_payload_b, "deterministic preview metadata mismatch"
    assert preview_html_a == preview_html_b, "deterministic preview html mismatch"
    assert comparable_payload_a == comparable_expected_preview, "preview artifact metadata differs from canonical fixture"

    return CaseResult(
        case_id="preview-determinism",
        title="deterministic preview generation",
        status="passed",
        sample_id="protocol.fixture.rect_diag_preview_artifact",
        detail={
            "command": command,
            "preview_path": str(preview_path_a),
            "expected_preview": str(expected_preview_path),
            "entity_count": payload_a.get("entity_count", 0),
            "segment_count": payload_a.get("segment_count", 0),
            "total_length_mm": payload_a.get("total_length_mm", 0.0),
        },
    )


def _write_report(report_dir: Path, results: list[CaseResult], overall_status: str) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "engineering-regression-summary.json"
    md_path = report_dir / "engineering-regression-summary.md"

    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "overall_status": overall_status,
        "results": [asdict(item) for item in results],
    }
    json_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# Engineering Regression Summary",
        "",
        f"- overall_status: `{overall_status}`",
        "",
        "## Cases",
        "",
    ]
    for item in results:
        lines.append(f"- `{item.status}` `{item.case_id}` sample=`{item.sample_id}`")
        if item.note:
            lines.append(f"  note: {item.note}")
        lines.append("  ```json")
        lines.extend(f"  {line}" for line in json.dumps(item.detail, ensure_ascii=False, indent=2).splitlines())
        lines.append("  ```")
    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def _write_bundle(report_dir: Path, json_path: Path, md_path: Path, results: list[CaseResult], overall_status: str) -> None:
    asset_map = {
        "dxf-to-pb": ("protocol.fixture.rect_diag_dxf", "protocol.fixture.rect_diag_pb"),
        "simulation-input-export": ("protocol.fixture.rect_diag_pb", "protocol.fixture.rect_diag_engineering"),
        "preview-determinism": ("protocol.fixture.rect_diag_dxf", "protocol.fixture.rect_diag_preview_artifact"),
    }
    bundle = EvidenceBundle(
        bundle_id="engineering-regression",
        request_ref="engineering-regression",
        producer_lane_ref="full-offline-gate",
        report_root=str(report_dir.resolve()),
        summary_file=str(md_path.resolve()),
        machine_file=str(json_path.resolve()),
        verdict=infer_verdict([item.status for item in results]),
        linked_asset_refs=tuple(sorted({asset for item in results for asset in asset_map.get(item.case_id, ())})),
        metadata={"overall_status": overall_status},
        case_records=[
            EvidenceCaseRecord(
                case_id=item.case_id,
                name=item.title,
                suite_ref="integration",
                owner_scope="tests/integration",
                primary_layer="L3",
                producer_lane_ref="full-offline-gate",
                status=item.status,
                evidence_profile="integration-report",
                stability_state="stable",
                size_label="medium",
                label_refs=("suite:integration", "kind:integration", "size:medium", "layer:L3"),
                required_assets=asset_map.get(item.case_id, ()),
                required_fixtures=("fixture.validation-evidence-bundle",),
                deterministic_replay=default_deterministic_replay(
                    passed=item.status == "passed",
                    seed=0,
                    clock_profile="deterministic",
                    repeat_count=2 if item.case_id == "preview-determinism" else 1,
                ),
                note=item.note,
                trace_fields=trace_fields(
                    stage_id="L3",
                    artifact_id=item.case_id,
                    module_id="tests/integration",
                    workflow_state="executed",
                    execution_state=item.status,
                    event_name=item.title,
                    failure_code="" if item.status == "passed" else f"engineering-regression.{item.case_id}",
                    evidence_path=str(json_path.resolve()),
                ),
            )
            for item in results
        ],
    )
    write_bundle_artifacts(
        bundle=bundle,
        report_root=report_dir,
        summary_json_path=json_path,
        summary_md_path=md_path,
        evidence_links=[
            EvidenceLink(label="rect_diag.dxf", path=str((FIXTURE_ROOT / "rect_diag.dxf").resolve()), role="sample"),
            EvidenceLink(label="rect_diag.pb", path=str((FIXTURE_ROOT / "rect_diag.pb").resolve()), role="fixture"),
            EvidenceLink(
                label="simulation-input.json",
                path=str((FIXTURE_ROOT / "simulation-input.json").resolve()),
                role="fixture",
            ),
            EvidenceLink(
                label="preview-artifact.json",
                path=str((FIXTURE_ROOT / "preview-artifact.json").resolve()),
                role="fixture",
            ),
        ],
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run engineering regression and emit evidence artifacts.")
    parser.add_argument(
        "--report-dir",
        default=str(ROOT / "tests" / "reports" / "integration" / "engineering-regression"),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir).resolve()
    dxf_path = FIXTURE_ROOT / "rect_diag.dxf"
    expected_pb_path = FIXTURE_ROOT / "rect_diag.pb"
    expected_sim_path = FIXTURE_ROOT / "simulation-input.json"
    expected_preview_path = FIXTURE_ROOT / "preview-artifact.json"
    results: list[CaseResult] = []

    case_steps = [
        ("dxf-to-pb", "DXF to PB canonical export"),
        ("simulation-input-export", "PB to simulation input canonical export"),
        ("preview-determinism", "deterministic preview generation"),
    ]

    try:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            pb_path = temp_root / "rect_diag.pb"
            sim_path = temp_root / "rect_diag.simulation-input.json"

            results.append(_assert_dxf_to_pb(dxf_path, expected_pb_path, pb_path))
            results.append(_assert_simulation_export(pb_path, expected_sim_path, sim_path))
            results.append(_assert_preview_determinism(dxf_path, temp_root, expected_preview_path))
        overall_status = "passed"
    except Exception as exc:
        overall_status = "failed"
        completed_ids = {item.case_id for item in results}
        failed_case_id, failed_title = next(
            (case_id, title) for case_id, title in case_steps if case_id not in completed_ids
        )
        results.append(
            CaseResult(
                case_id=failed_case_id,
                title=failed_title,
                status="failed",
                sample_id="protocol.fixture.rect_diag_dxf",
                detail={},
                note=str(exc),
            )
        )
        print(f"engineering regression failed: {exc}", file=sys.stderr)

    json_path, md_path = _write_report(report_dir, results, overall_status)
    _write_bundle(report_dir, json_path, md_path, results, overall_status)
    print(f"engineering regression overall_status={overall_status}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")
    return 0 if overall_status == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
