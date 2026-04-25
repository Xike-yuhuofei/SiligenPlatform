from __future__ import annotations

import io
import json
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path

import ezdxf


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
PACKAGE_ROOT = WORKSPACE_ROOT / "modules" / "dxf-geometry" / "application"
CONTRACTS_ROOT = WORKSPACE_ROOT / "shared" / "contracts" / "engineering"
FIXTURE_ROOT = CONTRACTS_ROOT / "fixtures" / "cases" / "rect_diag"
TRUTH_MATRIX_PATH = CONTRACTS_ROOT / "fixtures" / "dxf-truth-matrix.json"

sys.path.insert(0, str(PACKAGE_ROOT))

from engineering_data.contracts.simulation_input import bundle_to_simulation_payload, load_path_bundle  # noqa: E402
from engineering_data.processing import dxf_to_pb  # noqa: E402


PREVIEW_SCRIPT = WORKSPACE_ROOT / "scripts" / "engineering-data" / "generate_preview.py"
DXF_VALIDATION_SCHEMA_PATH = (
    WORKSPACE_ROOT / "data" / "schemas" / "engineering" / "dxf" / "v1" / "dxf-validation-report.schema.json"
)
SHARED_DXF_VALIDATION_SCHEMA_PATH = (
    WORKSPACE_ROOT / "shared" / "contracts" / "engineering" / "schemas" / "v1" / "dxf-validation-report.schema.json"
)
DXF_COMMAND_SET_PATH = WORKSPACE_ROOT / "shared" / "contracts" / "application" / "commands" / "dxf.command-set.json"


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _full_chain_cases() -> list[dict]:
    payload = _load_json(TRUTH_MATRIX_PATH)
    return list(payload["full_chain_canonical_cases"])


def _run_preview_script(
    input_path: Path,
    output_dir: Path,
    *,
    title: str,
    speed_mm_s: float,
    deterministic: bool = False,
) -> dict:
    command = [
        sys.executable,
        str(PREVIEW_SCRIPT),
        "--input",
        str(input_path),
        "--output-dir",
        str(output_dir),
        "--title",
        title,
        "--speed",
        str(speed_mm_s),
        "--json",
    ]
    if deterministic:
        command.append("--deterministic")

    completed = subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if completed.returncode != 0:
        raise AssertionError(f"preview script failed\nstdout={completed.stdout}\nstderr={completed.stderr}")
    return json.loads(completed.stdout)


class EngineeringDataCompatibilityTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.pb_fixture_path = FIXTURE_ROOT / "rect_diag.pb"
        cls.dxf_fixture_path = FIXTURE_ROOT / "rect_diag.dxf"
        cls.preview_fixture_path = FIXTURE_ROOT / "preview-artifact.json"
        cls.sim_fixture_path = FIXTURE_ROOT / "simulation-input.json"

    def test_dxf_to_pb_matches_fixture_semantics(self) -> None:
        expected = load_path_bundle(self.pb_fixture_path)
        with tempfile.TemporaryDirectory() as tmp_dir:
            output_path = Path(tmp_dir) / "rect_diag.pb"
            exit_code = dxf_to_pb.main([
                "--input", str(self.dxf_fixture_path),
                "--output", str(output_path),
            ])
            self.assertEqual(exit_code, 0)
            actual = load_path_bundle(output_path)

        self.assertEqual(actual.header.schema_version, expected.header.schema_version)
        self.assertEqual(actual.header.units, expected.header.units)
        self.assertAlmostEqual(actual.header.unit_scale, expected.header.unit_scale, places=9)
        self.assertEqual(len(actual.primitives), len(expected.primitives))
        self.assertEqual(len(actual.metadata), len(expected.metadata))
        actual.header.source_path = ""
        expected.header.source_path = ""
        self.assertEqual(actual.SerializeToString(), expected.SerializeToString())

    def test_dxf_to_pb_rejects_insert_instead_of_expanding_block_content(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            dxf_path = Path(tmp_dir) / "mixed_noise.dxf"
            output_path = Path(tmp_dir) / "mixed_noise.pb"
            report_path = Path(tmp_dir) / "mixed_noise.validation.json"

            doc = getattr(ezdxf, "new")("R2000")
            doc.header["$INSUNITS"] = 4
            modelspace = doc.modelspace()
            modelspace.add_line((0.0, 0.0), (10.0, 0.0))
            modelspace.add_point((5.0, 5.0))
            modelspace.add_text("IGNORE", dxfattribs={"height": 1.0}).set_placement((2.0, 2.0))
            block = doc.blocks.new(name="NOISE_BLOCK")
            block.add_line((0.0, 0.0), (1.0, 0.0))
            modelspace.add_blockref("NOISE_BLOCK", (7.0, 7.0))
            doc.saveas(dxf_path)

            stderr = io.StringIO()
            with redirect_stderr(stderr):
                exit_code = dxf_to_pb.main([
                    "--input", str(dxf_path),
                    "--output", str(output_path),
                    "--validation-report", str(report_path),
                ])

            self.assertEqual(exit_code, 4)
            self.assertFalse(output_path.exists())
            report = _load_json(report_path)

        self.assertEqual(report["schema_version"], "DXFValidationReport.v1")
        self.assertEqual(report["summary"]["gate_result"], "FAIL")
        self.assertEqual(report["errors"][0]["error_code"], "DXF_BLOCK_INSERT_NOT_EXPLODED")
        self.assertEqual(report["errors"][0]["stage_id"], "S2")
        self.assertTrue(report["errors"][0]["is_blocking"])
        warning_text = stderr.getvalue()
        self.assertIn("Warning[DXF_W_TEXT_IGNORED]", warning_text)
        self.assertIn("Ignored non-production DXF entity type: TEXT.", warning_text)
        self.assertIn("Error[DXF_E_BLOCK_INSERT_NOT_EXPLODED]", warning_text)

    def test_dxf_validation_report_v1_schema_is_shared_contract_truth(self) -> None:
        data_schema = _load_json(DXF_VALIDATION_SCHEMA_PATH)
        shared_schema = _load_json(SHARED_DXF_VALIDATION_SCHEMA_PATH)

        self.assertEqual(shared_schema, data_schema)
        self.assertEqual(data_schema["title"], "DXFValidationReport.v1")
        self.assertFalse(data_schema["additionalProperties"])
        self.assertEqual(
            set(data_schema["required"]),
            {
                "schema_version",
                "file",
                "policy",
                "stage",
                "summary",
                "entity_summary",
                "layer_summary",
                "geometry_summary",
                "topology_summary",
                "errors",
                "warnings",
                "recommended_actions",
            },
        )
        diagnostic = data_schema["$defs"]["diagnostic"]
        self.assertFalse(diagnostic["additionalProperties"])
        self.assertIn("operator_message", diagnostic["required"])
        self.assertIn("is_blocking", diagnostic["required"])

    def test_retired_dxf_spline_approximation_public_surfaces_are_removed(self) -> None:
        command_set = _load_json(DXF_COMMAND_SET_PATH)
        prepare = next(item for item in command_set["operations"] if item["method"] == "dxf.plan.prepare")
        prepare_params = prepare["paramsSchema"]["properties"]
        self.assertNotIn("approximate_splines", prepare_params)

        forbidden_runtime_tokens = [
            "approx_splines",
            "--approx-splines",
            "spline_samples",
            "--spline-samples",
        ]
        allowed_fail_closed_files = {
            WORKSPACE_ROOT / "apps" / "runtime-service" / "runtime" / "configuration" / "ConfigFileAdapter.System.cpp",
            Path(__file__),
        }
        completed = subprocess.run(
            ["git", "ls-files"],
            cwd=WORKSPACE_ROOT,
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        searchable_suffixes = {".cpp", ".h", ".hpp", ".py", ".json", ".txt", ".ini", ".md"}
        violations: list[str] = []
        for relative in completed.stdout.splitlines():
            path = WORKSPACE_ROOT / relative
            if path.suffix not in searchable_suffixes or not path.is_file():
                continue
            text = path.read_text(encoding="utf-8", errors="ignore")
            for token in forbidden_runtime_tokens:
                if token in text and path not in allowed_fail_closed_files:
                    violations.append(f"{path.relative_to(WORKSPACE_ROOT)} contains {token}")
        self.assertEqual(violations, [])

    def test_generate_preview_matches_fixture(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            payload = _run_preview_script(
                self.dxf_fixture_path,
                Path(tmp_dir),
                title="rect_diag",
                speed_mm_s=10.0,
            )

        expected = _load_json(self.preview_fixture_path)
        self.assertEqual(payload["entity_count"], expected["entity_count"])
        self.assertEqual(payload["segment_count"], expected["segment_count"])
        self.assertEqual(payload["point_count"], expected["point_count"])
        self.assertAlmostEqual(payload["total_length_mm"], expected["total_length_mm"], places=9)
        self.assertAlmostEqual(payload["estimated_time_s"], expected["estimated_time_s"], places=9)
        self.assertEqual(payload["width_mm"], expected["width_mm"])
        self.assertEqual(payload["height_mm"], expected["height_mm"])

    def test_generate_preview_deterministic_output_is_stable(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            output_dir = Path(tmp_dir)
            payload_a = _run_preview_script(
                self.dxf_fixture_path,
                output_dir,
                title="rect_diag",
                speed_mm_s=10.0,
                deterministic=True,
            )
            html_a = Path(payload_a["preview_path"]).read_text(encoding="utf-8")

            payload_b = _run_preview_script(
                self.dxf_fixture_path,
                output_dir,
                title="rect_diag",
                speed_mm_s=10.0,
                deterministic=True,
            )
            html_b = Path(payload_b["preview_path"]).read_text(encoding="utf-8")

        self.assertEqual(Path(payload_a["preview_path"]).name, "rect_diag-preview.html")
        self.assertEqual(payload_a, payload_b)
        self.assertEqual(html_a, html_b)

    def test_simulation_payload_matches_fixture(self) -> None:
        payload = bundle_to_simulation_payload(load_path_bundle(self.pb_fixture_path))
        self.assertEqual(payload, _load_json(self.sim_fixture_path))

    def test_full_chain_canonical_cases_round_trip_to_their_fixtures(self) -> None:
        for case in _full_chain_cases():
            with self.subTest(case_id=case["case_id"]):
                dxf_fixture_path = WORKSPACE_ROOT / case["dxf_fixture"]
                pb_fixture_path = WORKSPACE_ROOT / case["pb_fixture"]
                preview_fixture_path = WORKSPACE_ROOT / case["preview_fixture"]
                sim_fixture_path = WORKSPACE_ROOT / case["simulation_fixture"]

                expected_bundle = load_path_bundle(pb_fixture_path)
                with tempfile.TemporaryDirectory() as tmp_dir:
                    output_path = Path(tmp_dir) / f"{case['case_id']}.pb"
                    exit_code = dxf_to_pb.main([
                        "--input", str(dxf_fixture_path),
                        "--output", str(output_path),
                    ])
                    self.assertEqual(exit_code, 0)
                    actual_bundle = load_path_bundle(output_path)

                actual_bundle.header.source_path = ""
                expected_bundle.header.source_path = ""
                self.assertEqual(actual_bundle.SerializeToString(), expected_bundle.SerializeToString())

                with tempfile.TemporaryDirectory() as tmp_dir:
                    preview_payload = _run_preview_script(
                        dxf_fixture_path,
                        Path(tmp_dir),
                        title=case["case_id"],
                        speed_mm_s=10.0,
                    )
                expected_preview = _load_json(preview_fixture_path)
                self.assertEqual(preview_payload["entity_count"], expected_preview["entity_count"])
                self.assertEqual(preview_payload["segment_count"], expected_preview["segment_count"])
                self.assertEqual(preview_payload["point_count"], expected_preview["point_count"])
                self.assertAlmostEqual(preview_payload["total_length_mm"], expected_preview["total_length_mm"], places=9)
                self.assertAlmostEqual(preview_payload["estimated_time_s"], expected_preview["estimated_time_s"], places=9)
                self.assertEqual(preview_payload["width_mm"], expected_preview["width_mm"])
                self.assertEqual(preview_payload["height_mm"], expected_preview["height_mm"])

                simulation_payload = bundle_to_simulation_payload(load_path_bundle(pb_fixture_path))
                self.assertEqual(simulation_payload, _load_json(sim_fixture_path))


if __name__ == "__main__":
    unittest.main()
