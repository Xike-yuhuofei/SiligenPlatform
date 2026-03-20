import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.state.observer_store import ObserverStore  # noqa: E402
from hmi_client.features.sim_observer.ui.path_rendering import (  # noqa: E402
    ArcRenderInstruction,
    PolylineRenderInstruction,
    segment_render_instruction,
    segment_render_points,
)


def _render_payload() -> dict:
    return {
        "summary": {"issues": [], "alerts": []},
        "snapshots": [],
        "timeline": [],
        "trace": [],
        "motion_profile": {
            "segments": [
                {
                    "segment_index": 0,
                    "segment_type": "line",
                    "start": {"x": 0.0, "y": 0.0},
                    "end": {"x": 10.0, "y": 0.0},
                },
                {
                    "segment_index": 1,
                    "segment_type": "arc",
                    "start": {"x": 10.0, "y": 0.0},
                    "end": {"x": 20.0, "y": 10.0},
                    "arc_geometry": {
                        "center": {"x": 10.0, "y": 10.0},
                        "radius": 10.0,
                        "start_angle_rad": -1.57079632679,
                        "end_angle_rad": 0.0,
                    },
                    "points": [
                        {"x": 10.0, "y": 0.0},
                        {"x": 12.5, "y": 2.5},
                        {"x": 15.0, "y": 5.0},
                        {"x": 17.5, "y": 7.5},
                        {"x": 20.0, "y": 10.0},
                    ],
                },
            ]
        },
    }


class PathRenderingTest(unittest.TestCase):
    def setUp(self) -> None:
        self.store = ObserverStore.from_mapping(_render_payload())

    def test_line_segment_falls_back_to_start_and_end_points(self) -> None:
        line_segment = self.store.normalized_data.path_segments[0]

        points = segment_render_points(line_segment, self.store.normalized_data.path_points)

        self.assertEqual(len(points), 2)
        self.assertEqual((points[0].x, points[0].y), (0.0, 0.0))
        self.assertEqual((points[1].x, points[1].y), (10.0, 0.0))

        instruction = segment_render_instruction(line_segment, self.store.normalized_data.path_points)
        self.assertIsInstance(instruction, PolylineRenderInstruction)
        self.assertEqual(instruction.mode, "polyline_fallback")

    def test_multi_point_segment_preserves_ordered_geometry(self) -> None:
        arc_segment = self.store.normalized_data.path_segments[1]

        points = segment_render_points(arc_segment, self.store.normalized_data.path_points)

        self.assertEqual(len(points), 5)
        self.assertEqual((points[0].x, points[0].y), (10.0, 0.0))
        self.assertEqual((points[2].x, points[2].y), (15.0, 5.0))
        self.assertEqual((points[-1].x, points[-1].y), (20.0, 10.0))

        instruction = segment_render_instruction(arc_segment, self.store.normalized_data.path_points)
        self.assertIsInstance(instruction, ArcRenderInstruction)
        self.assertEqual(instruction.mode, "arc_primitive")
        self.assertAlmostEqual(instruction.radius, 10.0, places=3)


if __name__ == "__main__":
    unittest.main()
