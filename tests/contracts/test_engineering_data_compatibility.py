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

sys.path.insert(0, str(PACKAGE_ROOT))

from engineering_data.contracts.simulation_input import bundle_to_simulation_payload, load_path_bundle  # noqa: E402
from engineering_data.processing import dxf_to_pb  # noqa: E402
from engineering_data.proto import dxf_primitives_pb2 as pb  # noqa: E402


PREVIEW_SCRIPT = WORKSPACE_ROOT / "scripts" / "engineering-data" / "generate_preview.py"


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


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

    def test_dxf_to_pb_skips_text_and_insert_without_affecting_supported_entities(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            dxf_path = Path(tmp_dir) / "mixed_noise.dxf"
            output_path = Path(tmp_dir) / "mixed_noise.pb"

            doc = getattr(ezdxf, "new")("R12")
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
                ])

            self.assertEqual(exit_code, 0)
            actual = load_path_bundle(output_path)

        entity_types = [meta.entity_type for meta in actual.metadata]
        self.assertEqual(len(actual.primitives), 2)
        self.assertEqual(entity_types.count(pb.DXF_ENTITY_LINE), 1)
        self.assertEqual(entity_types.count(pb.DXF_ENTITY_POINT), 1)
        warning_text = stderr.getvalue()
        self.assertIn("Unsupported DXF entities will be skipped", warning_text)
        self.assertIn("INSERT=1", warning_text)
        self.assertIn("TEXT=1", warning_text)

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


if __name__ == "__main__":
    unittest.main()
