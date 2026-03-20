import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.adapters.recording_loader import load_recording_from_mapping  # noqa: E402
from hmi_client.features.sim_observer.adapters.recording_normalizer import normalize_recording  # noqa: E402
from hmi_client.features.sim_observer.contracts.observer_rules import GroupMinimizationResult  # noqa: E402
from hmi_client.features.sim_observer.contracts.observer_status import ObserverState  # noqa: E402
from hmi_client.features.sim_observer.contracts.observer_types import (  # noqa: E402
    AlertAnchor,
    GroupBasis,
    ObjectAnchor,
    ObjectId,
)
from hmi_client.features.sim_observer.rules.rule_r1_primary_association import (  # noqa: E402
    resolve_primary_association,
)


def _sample_payload() -> dict:
    return {
        "summary": {
            "issues": [],
            "alerts": [],
        },
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
                    "segment_type": "line",
                    "start": {"x": 10.0, "y": 0.0},
                    "end": {"x": 20.0, "y": 5.0},
                },
            ]
        },
    }


class RuleR1PrimaryAssociationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.normalized = normalize_recording(load_recording_from_mapping(_sample_payload()))

    def test_direct_object_anchor_resolves_single_object(self) -> None:
        result = resolve_primary_association(
            ObjectAnchor(object_id=ObjectId("segment:1")),
            self.normalized,
        )
        self.assertEqual(result.association_level, "single_object")
        self.assertEqual(result.primary_object_id, ObjectId("segment:1"))
        self.assertEqual(result.selection_state, ObserverState.RESOLVED)

    def test_resolved_group_result_resolves_object_group(self) -> None:
        group_result = GroupMinimizationResult(
            group_id=ObjectId("group:test"),
            group_basis=GroupBasis.ISSUE_RELATED,
            member_object_ids=(ObjectId("segment:0"), ObjectId("segment:1")),
            group_cardinality=2,
            is_minimized=True,
            state=ObserverState.RESOLVED,
        )
        result = resolve_primary_association(
            None,
            self.normalized,
            group_result=group_result,
        )
        self.assertEqual(result.association_level, "object_group")
        self.assertEqual(result.primary_group_id, ObjectId("group:test"))

    def test_candidate_single_object_resolves_without_anchor(self) -> None:
        result = resolve_primary_association(
            None,
            self.normalized,
            candidate_object_ids=(ObjectId("segment:0"),),
        )
        self.assertEqual(result.association_level, "single_object")
        self.assertEqual(result.primary_object_id, ObjectId("segment:0"))

    def test_alert_anchor_without_mapping_falls_back_to_summary_only(self) -> None:
        result = resolve_primary_association(
            AlertAnchor(alert_id=ObjectId("alert:missing")),
            self.normalized,
        )
        self.assertEqual(result.association_level, "summary_only")
        self.assertEqual(result.selection_state, ObserverState.MAPPING_INSUFFICIENT)

    def test_missing_anchor_and_candidates_returns_mapping_missing(self) -> None:
        result = resolve_primary_association(None, self.normalized)
        self.assertEqual(result.association_level, "summary_only")
        self.assertEqual(result.selection_state, ObserverState.MAPPING_MISSING)


if __name__ == "__main__":
    unittest.main()
