import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.adapters.recording_loader import load_recording_from_mapping  # noqa: E402
from hmi_client.features.sim_observer.adapters.recording_normalizer import normalize_recording  # noqa: E402
from hmi_client.features.sim_observer.contracts.observer_status import ObserverState  # noqa: E402
from hmi_client.features.sim_observer.contracts.observer_types import GroupBasis, ObjectId  # noqa: E402
from hmi_client.features.sim_observer.rules.rule_r5_time_mapping import resolve_time_mapping  # noqa: E402


def _sample_payload() -> dict:
    return {
        "summary": {
            "issues": [],
            "alerts": [],
        },
        "snapshots": [],
        "timeline": [],
        "trace": [
            {
                "field": "direct-anchor",
                "tick": {"time_ns": 1_000_000_000},
                "primary_object_id": "segment:1",
            },
            {
                "field": "sequence-anchor",
                "tick": {"time_ns": 2_000_000_000},
                "start_segment_id": "segment:0",
                "end_segment_id": "segment:2",
            },
            {
                "field": "concurrent-anchor",
                "tick": {"time_ns": 3_000_000_000},
                "candidate_segment_indices": [0, 2],
                "concurrent_candidates_present": True,
            },
            {
                "field": "missing-anchor",
                "tick": {"time_ns": 4_000_000_000},
                "segment_index": 99,
            },
            {
                "field": "insufficient-anchor",
                "tick": {"time_ns": 5_000_000_000},
            },
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
                {
                    "segment_index": 2,
                    "segment_type": "line",
                    "start": {"x": 20.0, "y": 5.0},
                    "end": {"x": 30.0, "y": 5.0},
                },
            ]
        },
    }


class RuleR5TimeMappingTest(unittest.TestCase):
    def setUp(self) -> None:
        self.payload = _sample_payload()
        self.normalized = normalize_recording(load_recording_from_mapping(self.payload))

    def test_direct_object_mapping_resolves_single_object(self) -> None:
        result = resolve_time_mapping(
            self.normalized.time_anchors[0],
            self.payload["trace"][0],
            self.normalized,
        )
        self.assertEqual(result.association_level, "single_object")
        self.assertEqual(result.object_id, ObjectId("segment:1"))
        self.assertEqual(result.mapping_state, ObserverState.RESOLVED)

    def test_contiguous_segments_resolve_sequence_range(self) -> None:
        result = resolve_time_mapping(
            self.normalized.time_anchors[1],
            self.payload["trace"][1],
            self.normalized,
        )
        self.assertEqual(result.association_level, "sequence_range")
        self.assertEqual(result.mapping_state, ObserverState.RESOLVED)
        self.assertEqual(
            result.sequence_range_ref.member_segment_ids,
            (
                ObjectId("segment:0"),
                ObjectId("segment:1"),
                ObjectId("segment:2"),
            ),
        )

    def test_concurrent_candidates_report_mapping_insufficient(self) -> None:
        result = resolve_time_mapping(
            self.normalized.time_anchors[2],
            self.payload["trace"][2],
            self.normalized,
        )
        self.assertEqual(result.association_level, "mapping_insufficient")
        self.assertEqual(result.mapping_state, ObserverState.MAPPING_INSUFFICIENT)

    def test_missing_normalized_targets_reports_mapping_missing(self) -> None:
        result = resolve_time_mapping(
            self.normalized.time_anchors[3],
            self.payload["trace"][3],
            self.normalized,
        )
        self.assertEqual(result.association_level, "mapping_missing")
        self.assertEqual(result.mapping_state, ObserverState.MAPPING_MISSING)

    def test_no_mapping_hints_report_mapping_insufficient(self) -> None:
        result = resolve_time_mapping(
            self.normalized.time_anchors[4],
            self.payload["trace"][4],
            self.normalized,
            default_group_basis=GroupBasis.TIME_MAPPED,
        )
        self.assertEqual(result.association_level, "mapping_insufficient")
        self.assertEqual(result.mapping_state, ObserverState.MAPPING_INSUFFICIENT)


if __name__ == "__main__":
    unittest.main()
