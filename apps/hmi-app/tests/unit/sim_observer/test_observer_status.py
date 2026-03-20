import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.contracts.observer_status import (  # noqa: E402
    FailureReason,
    ObserverState,
    REASON_DIRECT_OBJECT_ANCHOR,
    REASON_NO_DIRECT_RELATION,
    is_explained_empty,
    is_failure_state,
    is_terminal_state,
)


class ObserverStatusTest(unittest.TestCase):
    def test_observer_state_enum_contains_expected_values(self) -> None:
        self.assertEqual(ObserverState.RESOLVED.value, "resolved")
        self.assertEqual(ObserverState.EMPTY_BUT_EXPLAINED.value, "empty_but_explained")

    def test_failure_reason_enum_contains_expected_values(self) -> None:
        self.assertEqual(FailureReason.ANCHOR_MISSING.value, "anchor_missing")
        self.assertEqual(FailureReason.WINDOW_NOT_RESOLVED.value, "window_not_resolved")

    def test_is_failure_state(self) -> None:
        self.assertTrue(is_failure_state(ObserverState.MAPPING_MISSING))
        self.assertFalse(is_failure_state(ObserverState.EMPTY_BUT_EXPLAINED))

    def test_is_terminal_state(self) -> None:
        self.assertTrue(is_terminal_state(ObserverState.RESOLVED))
        self.assertTrue(is_terminal_state(ObserverState.EMPTY_BUT_EXPLAINED))
        self.assertFalse(is_terminal_state(ObserverState.GROUP_NOT_MINIMIZED))

    def test_reason_code_constants_exist(self) -> None:
        self.assertEqual(REASON_DIRECT_OBJECT_ANCHOR, "direct_object_anchor")
        self.assertEqual(REASON_NO_DIRECT_RELATION, "no_direct_relation")
        self.assertTrue(is_explained_empty(ObserverState.EMPTY_BUT_EXPLAINED))


if __name__ == "__main__":
    unittest.main()
