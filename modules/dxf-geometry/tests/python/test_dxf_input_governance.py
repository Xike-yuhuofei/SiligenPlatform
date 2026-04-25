from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys

import ezdxf

from support import pythonpath_env


def _run_dxf_to_pb(input_path: Path, output_path: Path, report_path: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            "-m",
            "engineering_data.cli.dxf_to_pb",
            "--input",
            str(input_path),
            "--output",
            str(output_path),
            "--validation-report",
            str(report_path),
        ],
        capture_output=True,
        text=True,
        env=pythonpath_env(),
        check=False,
    )


def _write_doc(path: Path, *, units: int | None = 4, entity: str = "line") -> None:
    doc = ezdxf.new("R2000")
    if units is not None:
        doc.header["$INSUNITS"] = units
    else:
        doc.header["$INSUNITS"] = 0
    msp = doc.modelspace()
    if entity == "line":
        msp.add_line((0.0, 0.0), (10.0, 0.0))
    elif entity == "insert":
        block = doc.blocks.new(name="BLOCKED")
        block.add_line((0.0, 0.0), (1.0, 0.0))
        msp.add_blockref("BLOCKED", (2.0, 2.0))
    elif entity == "spline":
        msp.add_spline([(0.0, 0.0), (2.0, 1.0), (4.0, 0.0)])
    elif entity == "hatch":
        hatch = msp.add_hatch()
        hatch.paths.add_polyline_path([(0, 0), (1, 0), (1, 1), (0, 1)], is_closed=True)
    else:
        raise AssertionError(entity)
    doc.saveas(path)


def _report(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def test_golden_mm_dxf_passes_with_validation_report(tmp_path: Path) -> None:
    dxf_path = tmp_path / "golden.dxf"
    output_path = tmp_path / "golden.pb"
    report_path = tmp_path / "golden.validation.json"
    _write_doc(dxf_path)

    result = _run_dxf_to_pb(dxf_path, output_path, report_path)

    assert result.returncode == 0, result.stderr
    payload = _report(report_path)
    assert payload["schema_version"] == "DXFValidationReport.v1"
    assert payload["policy"] == {
        "spec_version": "DXFInputSpec.v1",
        "policy_version": "DXFValidationPolicy.v1",
        "tolerance_policy_version": "GeometryTolerancePolicy.v1",
        "layer_semantic_policy_version": "LayerSemanticPolicy.v1",
    }
    assert payload["summary"]["gate_result"] == "PASS"
    assert payload["file"]["file_hash"]
    assert payload["file"]["source_drawing_ref"].startswith("sha256:")


def test_missing_units_fail_closed_without_mm_assumption(tmp_path: Path) -> None:
    dxf_path = tmp_path / "missing_units.dxf"
    output_path = tmp_path / "missing_units.pb"
    report_path = tmp_path / "missing_units.validation.json"
    _write_doc(dxf_path, units=None)

    result = _run_dxf_to_pb(dxf_path, output_path, report_path)

    assert result.returncode == 4
    assert not output_path.exists()
    payload = _report(report_path)
    assert payload["summary"]["gate_result"] == "FAIL"
    assert payload["errors"][0]["error_code"] == "DXF_UNIT_MISSING"
    assert payload["errors"][0]["operator_message"]
    assert "DXF_W_UNIT_ASSUMED_MM" not in result.stderr


def test_insert_spline_and_hatch_are_blocking_input_governance_errors(tmp_path: Path) -> None:
    cases = {
        "insert": "DXF_BLOCK_INSERT_NOT_EXPLODED",
        "spline": "DXF_SPLINE_NOT_SUPPORTED",
        "hatch": "DXF_HATCH_NOT_SUPPORTED",
    }
    for entity, expected_code in cases.items():
        dxf_path = tmp_path / f"{entity}.dxf"
        output_path = tmp_path / f"{entity}.pb"
        report_path = tmp_path / f"{entity}.validation.json"
        _write_doc(dxf_path, entity=entity)

        result = _run_dxf_to_pb(dxf_path, output_path, report_path)

        assert result.returncode == 4, result.stderr
        assert not output_path.exists()
        payload = _report(report_path)
        assert payload["summary"]["gate_result"] == "FAIL"
        assert payload["errors"][0]["error_code"] == expected_code
        assert payload["errors"][0]["is_blocking"] is True
