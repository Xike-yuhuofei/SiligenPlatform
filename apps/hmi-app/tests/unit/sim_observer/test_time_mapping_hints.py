import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.adapters.recording_loader import load_recording_from_mapping  # noqa: E402
from hmi_client.features.sim_observer.adapters.recording_normalizer import normalize_recording  # noqa: E402
from hmi_client.features.sim_observer.state.time_mapping_hints import with_time_mapping_hint  # noqa: E402


def _hint_payload() -> dict:
    return {
        "summary": {"issues": [], "alerts": []},
        "snapshots": [
            {
                "tick": {"time_ns": 1_000_000_000},
                "axes": [
                    {"axis": "X", "position_mm": 1.0},
                    {"axis": "Y", "position_mm": 0.0},
                ],
            },
            {
                "tick": {"time_ns": 2_000_000_000},
                "axes": [
                    {"axis": "X", "position_mm": 10.0},
                    {"axis": "Y", "position_mm": 0.0},
                ],
            },
            {
                "tick": {"time_ns": 3_000_000_000},
                "axes": [
                    {"axis": "X", "position_mm": 0.0},
                    {"axis": "Y", "position_mm": 0.0},
                ],
            },
        ],
        "timeline": [],
        "trace": [
            {"field": "unique-hit", "tick": {"time_ns": 1_000_000_000}},
            {"field": "contiguous-hit", "tick": {"time_ns": 2_000_000_000}},
            {"field": "non-contiguous-hit", "tick": {"time_ns": 3_000_000_000}},
            {"field": "no-snapshot", "tick": {"time_ns": 4_000_000_000}},
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
                    "end": {"x": 20.0, "y": 0.0},
                },
                {
                    "segment_index": 2,
                    "segment_type": "line",
                    "start": {"x": 0.0, "y": 0.0},
                    "end": {"x": 0.0, "y": 10.0},
                },
            ]
        },
    }


def _arc_hint_payload() -> dict:
    return {
        "summary": {"issues": [], "alerts": []},
        "snapshots": [
            {
                "tick": {"time_ns": 1_000_000_000},
                "axes": [
                    {"axis": "X", "position_mm": 7.0710678119},
                    {"axis": "Y", "position_mm": 7.0710678119},
                ],
            }
        ],
        "timeline": [],
        "trace": [{"field": "arc-distance-hit", "tick": {"time_ns": 1_000_000_000}}],
        "motion_profile": {
            "segments": [
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
                },
                {
                    "segment_index": 1,
                    "segment_type": "line",
                    "start": {"x": -20.0, "y": -20.0},
                    "end": {"x": -10.0, "y": -10.0},
                },
            ]
        },
    }


class TimeMappingHintsTest(unittest.TestCase):
    def setUp(self) -> None:
        self.payload = _hint_payload()
        self.normalized = normalize_recording(load_recording_from_mapping(self.payload))

    def test_snapshot_hint_resolves_unique_primary_object(self) -> None:
        hinted = with_time_mapping_hint(
            self.normalized.time_anchors[0],
            self.payload["trace"][0],
            self.normalized,
        )

        self.assertEqual(hinted["primary_object_id"], "segment:0")
        self.assertEqual(hinted["time_mapping_hint_source"], "snapshot_position")
        self.assertEqual(hinted["time_mapping_hint_reason_code"], "time_mapping_from_snapshot_position")

    def test_snapshot_hint_resolves_contiguous_sequence_members(self) -> None:
        hinted = with_time_mapping_hint(
            self.normalized.time_anchors[1],
            self.payload["trace"][1],
            self.normalized,
        )

        self.assertEqual(hinted["segment_member_ids"], ["segment:0", "segment:1"])
        self.assertEqual(hinted["time_mapping_hint_reason_code"], "time_mapping_from_snapshot_sequence")
        self.assertNotIn("concurrent_candidates_present", hinted)

    def test_snapshot_hint_marks_non_contiguous_candidates_as_ambiguous(self) -> None:
        hinted = with_time_mapping_hint(
            self.normalized.time_anchors[2],
            self.payload["trace"][2],
            self.normalized,
        )

        self.assertEqual(hinted["segment_member_ids"], ["segment:0", "segment:2"])
        self.assertTrue(hinted["concurrent_candidates_present"])
        self.assertEqual(hinted["time_mapping_hint_reason_code"], "time_mapping_snapshot_ambiguous")

    def test_no_snapshot_match_keeps_mapping_unmodified(self) -> None:
        hinted = with_time_mapping_hint(
            self.normalized.time_anchors[3],
            self.payload["trace"][3],
            self.normalized,
        )

        self.assertEqual(hinted, self.payload["trace"][3])

    def test_arc_geometry_distance_enables_unique_hint_when_points_are_sparse(self) -> None:
        payload = _arc_hint_payload()
        normalized = normalize_recording(load_recording_from_mapping(payload))

        hinted = with_time_mapping_hint(
            normalized.time_anchors[0],
            payload["trace"][0],
            normalized,
        )

        self.assertEqual(hinted["primary_object_id"], "segment:0")
        self.assertEqual(hinted["time_mapping_hint_reason_code"], "time_mapping_from_snapshot_position")


if __name__ == "__main__":
    unittest.main()
