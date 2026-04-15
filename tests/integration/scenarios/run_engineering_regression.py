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
ENGINEERING_DATA_BRIDGE = ROOT / "scripts" / "engineering-data"
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"

if str(ENGINEERING_DATA_SRC) not in sys.path:
    sys.path.insert(0, str(ENGINEERING_DATA_SRC))
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from engineering_data.contracts.simulation_input import load_path_bundle  # noqa: E402
from test_kit.dxf_truth_matrix import (  # noqa: E402
    FullChainTruthCase,
    full_chain_cases,
    resolve_full_chain_case,
)
from test_kit.evidence_bundle import (  # noqa: E402
    EvidenceBundle,
    EvidenceCaseRecord,
    EvidenceLink,
    default_deterministic_replay,
    infer_verdict,
    trace_fields,
    write_bundle_artifacts,
)


STEP_DEFINITIONS = (
    ("dxf-to-pb", "DXF to PB canonical export"),
    ("simulation-input-export", "PB to simulation input canonical export"),
    ("preview-determinism", "deterministic preview generation"),
)


@dataclass
class CaseResult:
    case_id: str
    title: str
    status: str
    sample_id: str
    truth_case_id: str
    required_assets: tuple[str, ...]
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


def _result_id(case: FullChainTruthCase, step_id: str) -> str:
    return f"{case.case_id}-{step_id}"


def _step_assets(case: FullChainTruthCase, step_id: str) -> tuple[str, ...]:
    if step_id == "dxf-to-pb":
        return (case.dxf_fixture_asset_id, case.pb_fixture_asset_id)
    if step_id == "simulation-input-export":
        return (case.pb_fixture_asset_id, case.engineering_fixture_asset_id)
    if step_id == "preview-determinism":
        return (case.dxf_fixture_asset_id, case.preview_fixture_asset_id)
    raise KeyError(f"unsupported engineering regression step: {step_id}")


def _step_sample_id(case: FullChainTruthCase, step_id: str) -> str:
    if step_id == "simulation-input-export":
        return case.engineering_fixture_asset_id
    if step_id == "preview-determinism":
        return case.preview_fixture_asset_id
    return case.dxf_fixture_asset_id


def _assert_dxf_to_pb(
    case: FullChainTruthCase,
    dxf_path: Path,
    expected_pb_path: Path,
    output_pb_path: Path,
) -> CaseResult:
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
        case_id=_result_id(case, "dxf-to-pb"),
        title=f"{case.case_id}: DXF to PB canonical export",
        status="passed",
        sample_id=case.dxf_fixture_asset_id,
        truth_case_id=case.case_id,
        required_assets=_step_assets(case, "dxf-to-pb"),
        detail={
            "command": command_result["command"],
            "output_pb": str(output_pb_path),
            "expected_pb": str(expected_pb_path),
        },
    )


def _assert_simulation_export(
    case: FullChainTruthCase,
    pb_path: Path,
    expected_sim_path: Path,
    output_sim_path: Path,
) -> CaseResult:
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
        case_id=_result_id(case, "simulation-input-export"),
        title=f"{case.case_id}: PB to simulation input canonical export",
        status="passed",
        sample_id=case.engineering_fixture_asset_id,
        truth_case_id=case.case_id,
        required_assets=_step_assets(case, "simulation-input-export"),
        detail={
            "command": command_result["command"],
            "input_pb": str(pb_path),
            "output_sim": str(output_sim_path),
            "segment_count": len(actual_sim.get("segments", [])),
        },
    )


def _assert_preview_determinism(
    case: FullChainTruthCase,
    dxf_path: Path,
    output_root: Path,
    expected_preview_path: Path,
) -> CaseResult:
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
    assert (
        comparable_payload_a == comparable_expected_preview
    ), "preview artifact metadata differs from canonical fixture"

    return CaseResult(
        case_id=_result_id(case, "preview-determinism"),
        title=f"{case.case_id}: deterministic preview generation",
        status="passed",
        sample_id=case.preview_fixture_asset_id,
        truth_case_id=case.case_id,
        required_assets=_step_assets(case, "preview-determinism"),
        detail={
            "command": command,
            "preview_path": str(preview_path_a),
            "expected_preview": str(expected_preview_path),
            "entity_count": payload_a.get("entity_count", 0),
            "segment_count": payload_a.get("segment_count", 0),
            "total_length_mm": payload_a.get("total_length_mm", 0.0),
        },
    )


def _failed_case_result(
    case: FullChainTruthCase,
    step_id: str,
    step_title: str,
    error: Exception,
) -> CaseResult:
    return CaseResult(
        case_id=_result_id(case, step_id),
        title=f"{case.case_id}: {step_title}",
        status="failed",
        sample_id=_step_sample_id(case, step_id),
        truth_case_id=case.case_id,
        required_assets=_step_assets(case, step_id),
        detail={},
        note=str(error),
    )


def _run_case(case: FullChainTruthCase) -> list[CaseResult]:
    case_results: list[CaseResult] = []
    try:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            pb_path = temp_root / f"{case.case_id}.pb"
            sim_path = temp_root / f"{case.case_id}.simulation-input.json"

            case_results.append(
                _assert_dxf_to_pb(
                    case,
                    case.dxf_fixture_absolute(ROOT),
                    case.pb_fixture_absolute(ROOT),
                    pb_path,
                )
            )
            case_results.append(
                _assert_simulation_export(
                    case,
                    pb_path,
                    case.simulation_fixture_absolute(ROOT),
                    sim_path,
                )
            )
            case_results.append(
                _assert_preview_determinism(
                    case,
                    case.dxf_fixture_absolute(ROOT),
                    temp_root,
                    case.preview_fixture_absolute(ROOT),
                )
            )
    except Exception as exc:
        completed_ids = {item.case_id for item in case_results}
        for step_id, step_title in STEP_DEFINITIONS:
            if _result_id(case, step_id) not in completed_ids:
                case_results.append(_failed_case_result(case, step_id, step_title, exc))
                break
    return case_results


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
        lines.append(
            f"- `{item.status}` `{item.case_id}` truth_case=`{item.truth_case_id}` sample=`{item.sample_id}`"
        )
        if item.note:
            lines.append(f"  note: {item.note}")
        lines.append("  ```json")
        lines.extend(
            f"  {line}"
            for line in json.dumps(item.detail, ensure_ascii=False, indent=2).splitlines()
        )
        lines.append("  ```")
    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def _write_bundle(
    report_dir: Path,
    json_path: Path,
    md_path: Path,
    results: list[CaseResult],
    overall_status: str,
    selected_cases: tuple[FullChainTruthCase, ...],
) -> None:
    bundle = EvidenceBundle(
        bundle_id="engineering-regression",
        request_ref="engineering-regression",
        producer_lane_ref="full-offline-gate",
        report_root=str(report_dir.resolve()),
        summary_file=str(md_path.resolve()),
        machine_file=str(json_path.resolve()),
        verdict=infer_verdict([item.status for item in results]),
        linked_asset_refs=tuple(
            sorted({asset_id for item in results for asset_id in item.required_assets})
        ),
        metadata={
            "overall_status": overall_status,
            "truth_case_ids": [case.case_id for case in selected_cases],
        },
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
                required_assets=item.required_assets,
                required_fixtures=("fixture.validation-evidence-bundle",),
                deterministic_replay=default_deterministic_replay(
                    passed=item.status == "passed",
                    seed=0,
                    clock_profile="deterministic",
                    repeat_count=2 if item.case_id.endswith("preview-determinism") else 1,
                ),
                note=item.note,
                trace_fields=trace_fields(
                    stage_id="L3",
                    artifact_id=item.case_id,
                    module_id="tests/integration",
                    workflow_state="executed",
                    execution_state=item.status,
                    event_name=item.title,
                    failure_code=""
                    if item.status == "passed"
                    else f"engineering-regression.{item.case_id}",
                    evidence_path=str(json_path.resolve()),
                ),
            )
            for item in results
        ],
    )
    evidence_links: list[EvidenceLink] = []
    for case in selected_cases:
        evidence_links.extend(
            [
                EvidenceLink(
                    label=f"{case.case_id}.dxf",
                    path=str(case.dxf_fixture_absolute(ROOT).resolve()),
                    role="sample",
                ),
                EvidenceLink(
                    label=f"{case.case_id}.pb",
                    path=str(case.pb_fixture_absolute(ROOT).resolve()),
                    role="fixture",
                ),
                EvidenceLink(
                    label=f"{case.case_id}.simulation-input.json",
                    path=str(case.simulation_fixture_absolute(ROOT).resolve()),
                    role="fixture",
                ),
                EvidenceLink(
                    label=f"{case.case_id}.preview-artifact.json",
                    path=str(case.preview_fixture_absolute(ROOT).resolve()),
                    role="fixture",
                ),
            ]
        )
    write_bundle_artifacts(
        bundle=bundle,
        report_root=report_dir,
        summary_json_path=json_path,
        summary_md_path=md_path,
        evidence_links=evidence_links,
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run engineering regression and emit evidence artifacts.")
    parser.add_argument(
        "--report-dir",
        default=str(ROOT / "tests" / "reports" / "integration" / "engineering-regression"),
    )
    parser.add_argument(
        "--case-id",
        action="append",
        default=[],
        help="Only run the named full-chain truth case. Can be repeated.",
    )
    return parser.parse_args()


def _selected_cases(requested_case_ids: list[str]) -> tuple[FullChainTruthCase, ...]:
    if not requested_case_ids:
        return full_chain_cases(ROOT)

    selected: list[FullChainTruthCase] = []
    seen: set[str] = set()
    for case_id in requested_case_ids:
        if case_id in seen:
            continue
        selected.append(resolve_full_chain_case(ROOT, case_id))
        seen.add(case_id)
    return tuple(selected)


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir).resolve()
    selected_cases = _selected_cases(list(args.case_id))
    results: list[CaseResult] = []

    for case in selected_cases:
        case_results = _run_case(case)
        results.extend(case_results)
        case_status = infer_verdict([item.status for item in case_results])
        print(f"engineering regression case={case.case_id} status={case_status}")

    overall_status = infer_verdict([item.status for item in results])
    json_path, md_path = _write_report(report_dir, results, overall_status)
    _write_bundle(report_dir, json_path, md_path, results, overall_status, selected_cases)
    print(f"engineering regression overall_status={overall_status}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")
    return 0 if overall_status == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
