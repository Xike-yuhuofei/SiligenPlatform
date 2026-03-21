from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
PACKAGE_ROOT = WORKSPACE_ROOT / "packages" / "engineering-contracts"
ENGINEERING_DATA_ROOT = WORKSPACE_ROOT / "packages" / "engineering-data"
HMI_ROOT = WORKSPACE_ROOT / "apps" / "hmi-app"
FIXTURE_ROOT = PACKAGE_ROOT / "fixtures" / "cases" / "rect_diag"
CANONICAL_SCHEMA_ROOT = WORKSPACE_ROOT / "data" / "schemas" / "engineering" / "dxf" / "v1"
LEGACY_BACKEND_CPP_ROOT = WORKSPACE_ROOT.parent / "Backend_CPP" / "proto"

import sys

sys.path.insert(0, str(ENGINEERING_DATA_ROOT / "src"))
sys.path.insert(0, str(HMI_ROOT / "src"))

from engineering_data.contracts.preview import PreviewRequest  # noqa: E402
from engineering_data.contracts.simulation_input import bundle_to_simulation_payload, load_path_bundle  # noqa: E402
from engineering_data.preview.html_preview import generate_preview  # noqa: E402
from engineering_data.proto import dxf_primitives_pb2 as pb  # noqa: E402
from hmi_client.integrations.dxf_pipeline.preview_client import DxfPreviewResult  # noqa: E402


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _assert_number(testcase: unittest.TestCase, value, *, minimum: float | None = None, exclusive_minimum: float | None = None) -> None:
    testcase.assertIsInstance(value, (int, float))
    if minimum is not None:
        testcase.assertGreaterEqual(float(value), minimum)
    if exclusive_minimum is not None:
        testcase.assertGreater(float(value), exclusive_minimum)


def _assert_point2d(testcase: unittest.TestCase, payload: dict) -> None:
    testcase.assertEqual(set(payload.keys()), {"x", "y"})
    _assert_number(testcase, payload["x"])
    _assert_number(testcase, payload["y"])


def _validate_preview_artifact(testcase: unittest.TestCase, payload: dict) -> None:
    testcase.assertEqual(
        set(payload.keys()),
        {
            "preview_path",
            "entity_count",
            "segment_count",
            "point_count",
            "total_length_mm",
            "estimated_time_s",
            "width_mm",
            "height_mm",
        },
    )
    testcase.assertIsInstance(payload["preview_path"], str)
    testcase.assertTrue(payload["preview_path"])
    testcase.assertTrue(payload["preview_path"].endswith(".html"))
    for key in ("entity_count", "segment_count", "point_count"):
        testcase.assertIsInstance(payload[key], int)
        testcase.assertGreaterEqual(payload[key], 0)
    for key in ("total_length_mm", "estimated_time_s", "width_mm", "height_mm"):
        _assert_number(testcase, payload[key], minimum=0)


def _validate_simulation_input(testcase: unittest.TestCase, payload: dict) -> None:
    testcase.assertEqual(
        set(payload.keys()),
        {
            "timestep_seconds",
            "max_simulation_time_seconds",
            "max_velocity",
            "max_acceleration",
            "segments",
            "io_delays",
            "triggers",
            "valve",
        },
    )
    _assert_number(testcase, payload["timestep_seconds"], exclusive_minimum=0)
    _assert_number(testcase, payload["max_simulation_time_seconds"], exclusive_minimum=0)
    _assert_number(testcase, payload["max_velocity"], minimum=0)
    _assert_number(testcase, payload["max_acceleration"], minimum=0)

    testcase.assertIsInstance(payload["segments"], list)
    for segment in payload["segments"]:
        testcase.assertIsInstance(segment, dict)
        seg_type = segment.get("type")
        testcase.assertIn(seg_type, {"LINE", "ARC"})
        if seg_type == "LINE":
            testcase.assertEqual(set(segment.keys()), {"type", "start", "end"})
            _assert_point2d(testcase, segment["start"])
            _assert_point2d(testcase, segment["end"])
        else:
            testcase.assertEqual(set(segment.keys()), {"type", "center", "radius", "start_angle", "end_angle"})
            _assert_point2d(testcase, segment["center"])
            _assert_number(testcase, segment["radius"], exclusive_minimum=0)
            _assert_number(testcase, segment["start_angle"])
            _assert_number(testcase, segment["end_angle"])

    testcase.assertIsInstance(payload["io_delays"], list)
    for item in payload["io_delays"]:
        testcase.assertEqual(set(item.keys()), {"channel", "delay_seconds"})
        testcase.assertIsInstance(item["channel"], str)
        testcase.assertTrue(item["channel"])
        _assert_number(testcase, item["delay_seconds"], minimum=0)

    testcase.assertIsInstance(payload["triggers"], list)
    for item in payload["triggers"]:
        testcase.assertEqual(set(item.keys()), {"channel", "trigger_position", "state"})
        testcase.assertIsInstance(item["channel"], str)
        testcase.assertTrue(item["channel"])
        _assert_number(testcase, item["trigger_position"], minimum=0)
        testcase.assertIsInstance(item["state"], bool)

    testcase.assertEqual(set(payload["valve"].keys()), {"open_delay_seconds", "close_delay_seconds"})
    _assert_number(testcase, payload["valve"]["open_delay_seconds"], minimum=0)
    _assert_number(testcase, payload["valve"]["close_delay_seconds"], minimum=0)


class EngineeringContractsCompatibilityTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.proto_path = PACKAGE_ROOT / "proto" / "v1" / "dxf_primitives.proto"
        cls.workspace_canonical_proto_path = CANONICAL_SCHEMA_ROOT / "dxf_primitives.proto"
        cls.legacy_backend_proto_path = LEGACY_BACKEND_CPP_ROOT / "dxf_primitives.proto"
        cls.preview_schema_path = PACKAGE_ROOT / "schemas" / "v1" / "preview-artifact.schema.json"
        cls.sim_schema_path = PACKAGE_ROOT / "schemas" / "v1" / "simulation-input.schema.json"
        cls.preview_fixture_path = FIXTURE_ROOT / "preview-artifact.json"
        cls.sim_fixture_path = FIXTURE_ROOT / "simulation-input.json"
        cls.pb_fixture_path = FIXTURE_ROOT / "rect_diag.pb"
        cls.dxf_fixture_path = FIXTURE_ROOT / "rect_diag.dxf"

    def test_owner_proto_matches_workspace_canonical_source(self) -> None:
        self.assertTrue(
            self.workspace_canonical_proto_path.exists(),
            msg=f"workspace canonical proto missing: {self.workspace_canonical_proto_path}",
        )
        owner_proto = self.proto_path.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
        workspace_canonical = self.workspace_canonical_proto_path.read_text(encoding="utf-8").replace("\r\n", "\n").strip()

        self.assertEqual(owner_proto, workspace_canonical)

    def test_owner_proto_matches_backend_cpp_source_when_available(self) -> None:
        if not self.legacy_backend_proto_path.exists():
            self.skipTest(f"legacy Backend_CPP proto not available: {self.legacy_backend_proto_path}")

        owner_proto = self.proto_path.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
        backend_proto = self.legacy_backend_proto_path.read_text(encoding="utf-8").replace("\r\n", "\n").strip()

        self.assertEqual(owner_proto, backend_proto)

    def test_schema_documents_are_present_and_parseable(self) -> None:
        preview_schema = _load_json(self.preview_schema_path)
        sim_schema = _load_json(self.sim_schema_path)

        self.assertEqual(preview_schema["title"], "DXF Preview Artifact")
        self.assertEqual(sim_schema["title"], "Simulation Input")
        self.assertIn("required", preview_schema)
        self.assertIn("$defs", sim_schema)

    def test_pb_fixture_parses_with_current_generated_code(self) -> None:
        bundle = pb.PathBundle()
        bundle.ParseFromString(self.pb_fixture_path.read_bytes())

        self.assertEqual(bundle.header.schema_version, 1)
        self.assertEqual(bundle.header.units, "mm")
        self.assertGreater(len(bundle.primitives), 0)
        self.assertEqual(len(bundle.primitives), len(bundle.metadata))

    def test_preview_fixture_matches_canonical_shape(self) -> None:
        payload = _load_json(self.preview_fixture_path)
        _validate_preview_artifact(self, payload)

    def test_preview_fixture_is_compatible_with_hmi_preview_consumer(self) -> None:
        payload = _load_json(self.preview_fixture_path)
        result = DxfPreviewResult(
            ok=True,
            preview_path=str(payload.get("preview_path", "")),
            entity_count=int(payload.get("entity_count", 0)),
            segment_count=int(payload.get("segment_count", 0)),
            point_count=int(payload.get("point_count", 0)),
            total_length_mm=float(payload.get("total_length_mm", 0.0)),
            estimated_time_s=float(payload.get("estimated_time_s", 0.0)),
        )

        self.assertTrue(result.ok)
        self.assertTrue(result.preview_path)
        self.assertEqual(result.segment_count, 5)
        self.assertAlmostEqual(result.estimated_time_s, 54.14213562373095, places=9)

    def test_engineering_data_generate_preview_matches_canonical_preview_contract(self) -> None:
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

        _validate_preview_artifact(self, payload)
        expected = _load_json(self.preview_fixture_path)
        self.assertEqual(payload["entity_count"], expected["entity_count"])
        self.assertEqual(payload["segment_count"], expected["segment_count"])
        self.assertEqual(payload["point_count"], expected["point_count"])
        self.assertAlmostEqual(payload["total_length_mm"], expected["total_length_mm"], places=9)
        self.assertAlmostEqual(payload["estimated_time_s"], expected["estimated_time_s"], places=9)
        self.assertEqual(payload["width_mm"], expected["width_mm"])
        self.assertEqual(payload["height_mm"], expected["height_mm"])

    def test_simulation_fixture_matches_canonical_shape(self) -> None:
        payload = _load_json(self.sim_fixture_path)
        _validate_simulation_input(self, payload)

    def test_simulation_engine_example_matches_canonical_shape(self) -> None:
        payload = _load_json(WORKSPACE_ROOT / "examples" / "simulation" / "sample_trajectory.json")
        _validate_simulation_input(self, payload)

    def test_engineering_data_simulation_export_matches_canonical_fixture(self) -> None:
        payload = bundle_to_simulation_payload(load_path_bundle(self.pb_fixture_path))
        _validate_simulation_input(self, payload)
        self.assertEqual(payload, _load_json(self.sim_fixture_path))

    def test_simulation_engine_binary_accepts_canonical_fixture(self) -> None:
        exe_path = WORKSPACE_ROOT / "packages" / "simulation-engine" / "build" / "Debug" / "simulate_dxf_path.exe"
        if not exe_path.exists():
            self.skipTest(f"simulation-engine executable missing: {exe_path}")

        with tempfile.TemporaryDirectory() as tmp_dir:
            output_path = Path(tmp_dir) / "result.json"
            completed = subprocess.run(
                [str(exe_path), str(self.sim_fixture_path), str(output_path)],
                check=False,
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=30,
            )

            self.assertEqual(
                completed.returncode,
                0,
                msg=f"stdout={completed.stdout}\nstderr={completed.stderr}",
            )
            self.assertTrue(output_path.exists())
            result = _load_json(output_path)
            self.assertIn("total_time", result)
            self.assertIn("motion_distance", result)
            self.assertGreater(result["motion_distance"], 0)


if __name__ == "__main__":
    unittest.main()
