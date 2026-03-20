import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.contracts.observer_status import ObserverState  # noqa: E402
from hmi_client.features.sim_observer.contracts.observer_types import GroupBasis, ObjectId  # noqa: E402
from hmi_client.features.sim_observer.rules.rule_r2_minimize_group import (  # noqa: E402
    minimize_candidate_group,
)


class RuleR2MinimizeGroupTest(unittest.TestCase):
    def test_empty_candidates_return_mapping_missing(self) -> None:
        result = minimize_candidate_group(tuple(), GroupBasis.ISSUE_RELATED)
        self.assertEqual(result.state, ObserverState.MAPPING_MISSING)
        self.assertEqual(result.group_cardinality, 0)

    def test_removable_members_keep_group_not_minimized(self) -> None:
        result = minimize_candidate_group(
            (ObjectId("segment:0"), ObjectId("segment:1")),
            GroupBasis.ISSUE_RELATED,
            removable_object_ids=(ObjectId("segment:1"),),
        )
        self.assertEqual(result.state, ObserverState.GROUP_NOT_MINIMIZED)
        self.assertFalse(result.is_minimized)

    def test_minimal_group_resolves_deterministically(self) -> None:
        result = minimize_candidate_group(
            (ObjectId("segment:0"), ObjectId("segment:1")),
            GroupBasis.ISSUE_RELATED,
        )
        self.assertEqual(result.state, ObserverState.RESOLVED)
        self.assertTrue(result.is_minimized)
        self.assertEqual(result.group_id, ObjectId("group:issue_related:segment:0,segment:1"))


if __name__ == "__main__":
    unittest.main()
