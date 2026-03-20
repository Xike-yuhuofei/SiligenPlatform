import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.contracts.observer_status import FailureReason  # noqa: E402
from hmi_client.features.sim_observer.contracts.observer_types import ObjectId  # noqa: E402
from hmi_client.features.sim_observer.state.observer_store import ObserverStore  # noqa: E402


def _store_payload() -> dict:
    return {
        "summary": {
            "issues": [
                {
                    "issue_id": "range-target",
                    "issue_category": "ordering",
                    "issue_level": "warning",
                    "label": "Range target issue",
                    "candidate_object_ids": ["segment:0", "segment:1"],
                },
                {
                    "issue_id": "single-target",
                    "issue_category": "coverage",
                    "issue_level": "warning",
                    "label": "Single target issue",
                    "primary_object_id": "segment:2",
                },
            ],
            "alerts": [
                {
                    "alert_id": "alarm-range",
                    "alert_time": 2.5,
                    "label": "Alarm Range",
                    "start_segment_id": "segment:0",
                    "end_segment_id": "segment:1",
                    "window_start": 2.0,
                    "window_end": 3.0,
                    "key_window_start": 2.2,
                    "key_window_end": 2.7,
                },
                {
                    "alert_id": "alarm-fallback",
                    "alert_time": 4.0,
                    "label": "Alarm Fallback",
                },
            ],
        },
        "snapshots": [],
        "timeline": [],
        "trace": [
            {
                "field": "direct-anchor",
                "tick": {"time_ns": 1_000_000_000},
                "primary_object_id": "segment:2",
                "key_window_start": 0.8,
                "key_window_end": 1.1,
            },
            {
                "field": "uncertain-anchor",
                "tick": {"time_ns": 2_000_000_000},
            },
            {
                "field": "window-unresolved-anchor",
                "tick": {"time_ns": 3_000_000_000},
                "primary_object_id": "segment:1",
                "key_window_unresolved": True,
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


class ObserverStoreP1Test(unittest.TestCase):
    def setUp(self) -> None:
        self.store = ObserverStore.from_mapping(_store_payload())

    def test_default_time_anchor_prefers_trace_anchor(self) -> None:
        self.assertEqual(self.store.default_time_anchor_id, ObjectId("time_anchor:trace:0"))

    def test_select_default_time_anchor_returns_single_object_backlink(self) -> None:
        selection = self.store.select_default_time_anchor()
        self.assertIsNotNone(selection)
        self.assertEqual(selection.selected_kind, "time_anchor")
        self.assertEqual(selection.time_mapping.association_level, "single_object")
        self.assertEqual(selection.time_mapping.object_id, ObjectId("segment:2"))
        self.assertEqual(selection.backlink.backlink_level, "single_object")
        self.assertEqual(selection.backlink.object_id, ObjectId("segment:2"))
        self.assertIsNone(selection.failure_reason)

    def test_select_p1_alert_returns_sequence_range_and_context(self) -> None:
        selection = self.store.select_p1_alert(ObjectId("alert:summary:alarm-range:0"))
        self.assertEqual(selection.selected_kind, "alert")
        self.assertEqual(selection.time_mapping.association_level, "sequence_range")
        self.assertEqual(selection.backlink.backlink_level, "sequence_range")
        self.assertEqual(
            selection.backlink.sequence_range_ref.member_segment_ids,
            (
                ObjectId("segment:0"),
                ObjectId("segment:1"),
            ),
        )
        self.assertEqual(selection.context_window.window_state.value, "resolved")
        self.assertEqual(selection.key_window.window_state.value, "resolved")
        self.assertIsNone(selection.failure_reason)

    def test_select_p1_time_anchor_without_mapping_reports_unavailable_backlink(self) -> None:
        selection = self.store.select_p1_time_anchor(ObjectId("time_anchor:trace:1"))
        self.assertEqual(selection.time_mapping.association_level, "mapping_insufficient")
        self.assertEqual(selection.backlink.backlink_level, "unavailable")
        self.assertEqual(selection.failure_reason, FailureReason.MAPPING_INSUFFICIENT)

    def test_key_window_unresolved_is_exposed_as_window_not_resolved(self) -> None:
        selection = self.store.select_p1_time_anchor(ObjectId("time_anchor:trace:2"))
        self.assertEqual(selection.time_mapping.association_level, "single_object")
        self.assertEqual(selection.time_mapping.object_id, ObjectId("segment:1"))
        self.assertEqual(selection.key_window.window_state.value, "window_not_resolved")
        self.assertEqual(selection.failure_reason, FailureReason.WINDOW_NOT_RESOLVED)


if __name__ == "__main__":
    unittest.main()
