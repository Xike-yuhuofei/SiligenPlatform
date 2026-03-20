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
                    "issue_id": "single-target",
                    "issue_category": "coverage",
                    "issue_level": "warning",
                    "label": "Single target issue",
                    "primary_object_id": "segment:1",
                },
                {
                    "issue_id": "group-target",
                    "issue_category": "ordering",
                    "issue_level": "warning",
                    "label": "Group target issue",
                    "candidate_object_ids": ["segment:0", "segment:1"],
                    "removable_object_ids": ["segment:1"],
                },
                {
                    "issue_id": "summary-only",
                    "issue_category": "coverage",
                    "issue_level": "warning",
                    "label": "Summary only issue",
                },
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
    }


class ObserverStoreTest(unittest.TestCase):
    def setUp(self) -> None:
        self.store = ObserverStore.from_mapping(_store_payload())

    def test_default_issue_prefers_resolved_issue(self) -> None:
        payload = _store_payload()
        payload["summary"]["issues"] = [
            payload["summary"]["issues"][2],
            payload["summary"]["issues"][1],
            payload["summary"]["issues"][0],
        ]
        store = ObserverStore.from_mapping(payload)
        self.assertEqual(store.default_issue_id, ObjectId("issue:single-target:2"))

    def test_issue_selection_resolves_direct_object(self) -> None:
        selection = self.store.select_issue(ObjectId("issue:single-target:0"))
        self.assertEqual(selection.primary_association.association_level, "single_object")
        self.assertEqual(selection.primary_association.primary_object_id, ObjectId("segment:1"))
        self.assertIsNone(selection.failure_reason)
        self.assertEqual(self.store.current_primary_object.object_id, ObjectId("segment:1"))

    def test_select_default_issue_updates_current_selection(self) -> None:
        selection = self.store.select_default_issue()
        self.assertIsNotNone(selection)
        self.assertEqual(selection.selected_id, self.store.default_issue_id)
        self.assertEqual(self.store.current_selection.selected_id, self.store.default_issue_id)

    def test_issue_selection_preserves_group_not_minimized_failure(self) -> None:
        selection = self.store.select_issue(ObjectId("issue:group-target:1"))
        self.assertEqual(selection.primary_association.association_level, "summary_only")
        self.assertEqual(selection.failure_reason, FailureReason.GROUP_NOT_MINIMIZED)
        self.assertIsNotNone(selection.group_result)
        self.assertEqual(selection.group_result.state.value, "group_not_minimized")

    def test_issue_without_mapping_reports_anchor_missing(self) -> None:
        selection = self.store.select_issue(ObjectId("issue:summary-only:2"))
        self.assertEqual(selection.primary_association.association_level, "summary_only")
        self.assertEqual(selection.failure_reason, FailureReason.ANCHOR_MISSING)

    def test_alert_without_object_mapping_reports_mapping_insufficient(self) -> None:
        selection = self.store.select_alert(ObjectId("alert:summary:alarm-1:0"))
        self.assertEqual(selection.primary_association.association_level, "summary_only")
        self.assertEqual(selection.failure_reason, FailureReason.MAPPING_INSUFFICIENT)

    def test_selecting_same_issue_twice_is_stable(self) -> None:
        left = self.store.select_issue(ObjectId("issue:single-target:0"))
        right = self.store.select_issue(ObjectId("issue:single-target:0"))
        self.assertEqual(left.primary_association.primary_object_id, right.primary_association.primary_object_id)

    def test_find_issue_ids_for_object_matches_candidates(self) -> None:
        matched_issue_ids = self.store.find_issue_ids_for_object(ObjectId("segment:1"))
        self.assertEqual(
            matched_issue_ids,
            (
                ObjectId("issue:single-target:0"),
                ObjectId("issue:group-target:1"),
            ),
        )


if __name__ == "__main__":
    unittest.main()
