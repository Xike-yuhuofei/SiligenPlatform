import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.adapters.recording_loader import load_recording_from_mapping  # noqa: E402
from hmi_client.features.sim_observer.adapters.recording_normalizer import normalize_recording  # noqa: E402
from hmi_client.features.sim_observer.contracts.observer_types import ObjectId  # noqa: E402


class RecordingNormalizerTest(unittest.TestCase):
    def _sample_payload(self):
        return {
            "summary": {
                "issues": [
                    {
                        "issue_id": "coverage-1",
                        "issue_category": "coverage",
                        "issue_level": "warning",
                        "label": "Coverage issue",
                    }
                ],
                "alerts": [
                    {
                        "alert_id": "alarm-1",
                        "alert_time": 1.25,
                        "label": "Alarm one",
                    }
                ],
            },
            "snapshots": [],
            "timeline": [
                {
                    "kind": "alert",
                    "message": "Alarm one",
                    "tick": {"time_ns": 1250000000},
                }
            ],
            "trace": [
                {
                    "field": "running",
                    "tick": {"time_ns": 1000000000},
                }
            ],
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
                        "segment_type": "line",
                        "start": {"x": 10.0, "y": 0.0},
                        "end": {"x": 20.0, "y": 5.0},
                    },
                ]
            },
            "regions": [
                {
                    "region_id": "r1",
                    "label": "Region 1",
                    "region_type": "target",
                    "geometry_refs": ["region:r1"],
                    "rule_source": "fixture",
                    "bbox": {"min_x": 0.0, "min_y": 0.0, "max_x": 20.0, "max_y": 5.0},
                }
            ],
        }

    def test_normalize_recording_builds_expected_objects(self) -> None:
        normalized = normalize_recording(load_recording_from_mapping(self._sample_payload()))
        self.assertEqual(len(normalized.path_segments), 2)
        self.assertEqual(len(normalized.path_points), 4)
        self.assertEqual(len(normalized.issues), 1)
        self.assertEqual(len(normalized.alerts), 2)
        self.assertEqual(len(normalized.time_anchors), 3)
        self.assertEqual(len(normalized.regions), 1)
        self.assertIn(ObjectId("segment:0"), normalized.objects_by_id)

    def test_object_ids_are_stable_for_same_payload(self) -> None:
        payload = load_recording_from_mapping(self._sample_payload())
        left = normalize_recording(payload)
        right = normalize_recording(payload)
        self.assertEqual(tuple(left.objects_by_id.keys()), tuple(right.objects_by_id.keys()))

    def test_duplicate_segment_ids_raise(self) -> None:
        payload = self._sample_payload()
        payload["motion_profile"]["segments"] = [
            {
                "segment_index": 0,
                "segment_type": "line",
                "start": {"x": 0.0, "y": 0.0},
                "end": {"x": 1.0, "y": 1.0},
            },
            {
                "segment_index": 0,
                "segment_type": "line",
                "start": {"x": 1.0, "y": 1.0},
                "end": {"x": 2.0, "y": 2.0},
            },
        ]
        with self.assertRaises(ValueError):
            normalize_recording(load_recording_from_mapping(payload))

    def test_missing_regions_is_allowed(self) -> None:
        payload = self._sample_payload()
        payload.pop("regions")
        normalized = normalize_recording(load_recording_from_mapping(payload))
        self.assertEqual(normalized.regions, tuple())

    def test_arc_segment_preserves_arc_geometry_and_bbox(self) -> None:
        payload = self._sample_payload()
        payload["motion_profile"]["segments"] = [
            {
                "segment_index": 0,
                "segment_type": "arc",
                "start": {"x": 10.0, "y": 0.0},
                "end": {"x": 0.0, "y": 10.0},
                "arc_geometry": {
                    "center": {"x": 0.0, "y": 0.0},
                    "radius": 10.0,
                    "start_angle_rad": 0.0,
                    "end_angle_rad": 1.57079632679,
                },
            }
        ]

        normalized = normalize_recording(load_recording_from_mapping(payload))

        segment = normalized.path_segments[0]
        self.assertIsNotNone(segment.arc_geometry)
        self.assertAlmostEqual(segment.arc_geometry.center.x, 0.0, places=3)
        self.assertAlmostEqual(segment.arc_geometry.center.y, 0.0, places=3)
        self.assertAlmostEqual(segment.arc_geometry.radius, 10.0, places=3)
        self.assertAlmostEqual(segment.bbox.min_x, 0.0, places=3)
        self.assertAlmostEqual(segment.bbox.min_y, 0.0, places=3)
        self.assertAlmostEqual(segment.bbox.max_x, 10.0, places=3)
        self.assertAlmostEqual(segment.bbox.max_y, 10.0, places=3)


if __name__ == "__main__":
    unittest.main()
