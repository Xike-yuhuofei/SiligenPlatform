from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
PACKAGE_ROOT = WORKSPACE_ROOT / "packages" / "engineering-data"
CONTRACTS_ROOT = WORKSPACE_ROOT / "packages" / "engineering-contracts"
FIXTURE_ROOT = CONTRACTS_ROOT / "fixtures" / "cases" / "rect_diag"

sys.path.insert(0, str(PACKAGE_ROOT / "src"))

from engineering_data.contracts.preview import PreviewRequest  # noqa: E402
from engineering_data.contracts.simulation_input import bundle_to_simulation_payload, load_path_bundle  # noqa: E402
from engineering_data.preview.html_preview import generate_preview  # noqa: E402
from engineering_data.processing import dxf_to_pb  # noqa: E402


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


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

    def test_generate_preview_matches_fixture(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            artifact = generate_preview(
                PreviewRequest(
                    input_path=self.dxf_fixture_path,
                    output_dir=Path(tmp_dir),
                    title="rect_diag",
                    speed_mm_s=10.0,
                )
            )
            payload = artifact.to_dict()

        expected = _load_json(self.preview_fixture_path)
        self.assertEqual(payload["entity_count"], expected["entity_count"])
        self.assertEqual(payload["segment_count"], expected["segment_count"])
        self.assertEqual(payload["point_count"], expected["point_count"])
        self.assertAlmostEqual(payload["total_length_mm"], expected["total_length_mm"], places=9)
        self.assertAlmostEqual(payload["estimated_time_s"], expected["estimated_time_s"], places=9)
        self.assertEqual(payload["width_mm"], expected["width_mm"])
        self.assertEqual(payload["height_mm"], expected["height_mm"])

    def test_simulation_payload_matches_fixture(self) -> None:
        payload = bundle_to_simulation_payload(load_path_bundle(self.pb_fixture_path))
        self.assertEqual(payload, _load_json(self.sim_fixture_path))


if __name__ == "__main__":
    unittest.main()
