import json
import tempfile
import unittest
from pathlib import Path

from dxf_pipeline.contracts.simulation_input import (
    bundle_contains_fallback_primitives,
    bundle_to_simulation_payload,
    collect_export_notes,
    load_path_bundle,
    load_trigger_points_csv,
    primitive_to_segments_fallback,
    primitive_to_segments_r12,
)
from proto import dxf_primitives_pb2 as pb


class ExportSimulationInputTest(unittest.TestCase):
    def setUp(self) -> None:
        self.project_root = Path(__file__).resolve().parents[2]

    def test_rect_diag_bundle_exports_loader_shape(self) -> None:
        bundle_path = (
            self.project_root
            / "tests"
            / "fixtures"
            / "imported"
            / "uploads-dxf"
            / "live"
            / "d1a4fe0c-664e-4177-057f-34a21e3feed3_1770211294022_rect_diag.pb"
        )
        payload = bundle_to_simulation_payload(load_path_bundle(bundle_path))

        self.assertEqual(payload["timestep_seconds"], 0.001)
        self.assertEqual(payload["max_simulation_time_seconds"], 600.0)
        self.assertEqual(payload["max_velocity"], 220.0)
        self.assertEqual(payload["max_acceleration"], 600.0)
        self.assertEqual(payload["triggers"], [])
        self.assertIn("io_delays", payload)
        self.assertIn("valve", payload)
        self.assertEqual(len(payload["segments"]), 5)
        self.assertTrue(all(segment["type"] in {"LINE", "ARC"} for segment in payload["segments"]))

        dumped = json.dumps(payload, ensure_ascii=True)
        reloaded = json.loads(dumped)
        self.assertEqual(reloaded["segments"][0]["start"]["x"], 0.0)
        self.assertEqual(reloaded["segments"][1]["end"]["y"], 100.0)

    def test_trigger_points_are_projected_to_global_path_length(self) -> None:
        bundle_path = (
            self.project_root
            / "tests"
            / "fixtures"
            / "imported"
            / "uploads-dxf"
            / "live"
            / "d1a4fe0c-664e-4177-057f-34a21e3feed3_1770211294022_rect_diag.pb"
        )
        with tempfile.TemporaryDirectory() as tmp_dir:
            csv_path = Path(tmp_dir) / "triggers.csv"
            csv_path.write_text(
                "x_mm,y_mm,channel,state\n"
                "50,0,DO_VALVE,true\n"
                "100,50,DO_VALVE,false\n",
                encoding="utf-8",
            )

            trigger_points = load_trigger_points_csv(csv_path, default_channel="DO_CAMERA_TRIGGER", default_state=True)
            payload = bundle_to_simulation_payload(
                load_path_bundle(bundle_path),
                trigger_points=trigger_points,
            )

        self.assertEqual(len(payload["triggers"]), 2)
        self.assertAlmostEqual(payload["triggers"][0]["trigger_position"], 50.0, places=6)
        self.assertAlmostEqual(payload["triggers"][1]["trigger_position"], 150.0, places=6)
        self.assertTrue(payload["triggers"][0]["state"])
        self.assertFalse(payload["triggers"][1]["state"])

    def test_r12_and_fallback_primitive_paths_are_classified_separately(self) -> None:
        line_primitive = pb.Primitive(type=pb.PRIMITIVE_LINE)
        line_primitive.line.start.x = 0.0
        line_primitive.line.start.y = 0.0
        line_primitive.line.end.x = 10.0
        line_primitive.line.end.y = 0.0

        spline_primitive = pb.Primitive(type=pb.PRIMITIVE_SPLINE)
        spline_primitive.spline.control_points.add(x=0.0, y=0.0)
        spline_primitive.spline.control_points.add(x=10.0, y=5.0)

        self.assertIsNotNone(primitive_to_segments_r12(line_primitive, max_seg=1.0))
        self.assertIsNone(primitive_to_segments_r12(spline_primitive, max_seg=1.0))
        self.assertIsNone(primitive_to_segments_fallback(line_primitive, max_seg=1.0))
        self.assertIsNotNone(primitive_to_segments_fallback(spline_primitive, max_seg=1.0))

    def test_collect_export_notes_mentions_fallback_primitives(self) -> None:
        bundle = pb.PathBundle()

        line_primitive = bundle.primitives.add()
        line_primitive.type = pb.PRIMITIVE_LINE
        line_primitive.line.start.x = 0.0
        line_primitive.line.start.y = 0.0
        line_primitive.line.end.x = 5.0
        line_primitive.line.end.y = 0.0

        spline_primitive = bundle.primitives.add()
        spline_primitive.type = pb.PRIMITIVE_SPLINE
        spline_primitive.spline.control_points.add(x=0.0, y=0.0)
        spline_primitive.spline.control_points.add(x=5.0, y=5.0)

        self.assertTrue(bundle_contains_fallback_primitives(bundle))

        notes = collect_export_notes(bundle)

        self.assertTrue(any("Used non-R12 fallback primitive conversions" in note for note in notes))
        self.assertTrue(any("Spline primitives were linearized" in note for note in notes))


if __name__ == "__main__":
    unittest.main()
