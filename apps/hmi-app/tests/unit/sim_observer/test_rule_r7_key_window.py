import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.contracts.observer_rules import ContextWindowRef  # noqa: E402
from hmi_client.features.sim_observer.contracts.observer_status import ObserverState  # noqa: E402
from hmi_client.features.sim_observer.rules.rule_r7_key_window import resolve_key_window  # noqa: E402


class RuleR7KeyWindowTest(unittest.TestCase):
    def test_explicit_key_window_wins(self) -> None:
        result = resolve_key_window(
            {
                "key_window_start": 1.25,
                "key_window_end": 1.75,
            },
            anchor_time=1.5,
        )
        self.assertEqual(result.window_state, ObserverState.RESOLVED)
        self.assertEqual(result.window_basis, "key_window")
        self.assertEqual((result.key_window_start, result.key_window_end), (1.25, 1.75))

    def test_unresolved_flag_reports_window_not_resolved(self) -> None:
        result = resolve_key_window(
            {
                "key_window_unresolved": True,
            },
            anchor_time=2.0,
        )
        self.assertEqual(result.window_state, ObserverState.WINDOW_NOT_RESOLVED)
        self.assertEqual(result.window_basis, "key_window")
        self.assertIsNone(result.key_window_start)
        self.assertIsNone(result.key_window_end)

    def test_resolved_context_window_is_reused(self) -> None:
        context_window = ContextWindowRef(
            window_start=3.0,
            window_end=4.0,
            anchor_time=3.5,
            window_basis="alert_context",
            window_state=ObserverState.RESOLVED,
        )
        result = resolve_key_window({}, anchor_time=3.5, context_window=context_window)
        self.assertEqual(result.window_state, ObserverState.RESOLVED)
        self.assertEqual(result.window_basis, "context_window")
        self.assertEqual((result.key_window_start, result.key_window_end), (3.0, 4.0))

    def test_default_anchor_range_is_used_when_available(self) -> None:
        result = resolve_key_window(
            {},
            anchor_time=5.0,
            default_start=4.5,
            default_end=5.25,
            default_basis="time_anchor",
        )
        self.assertEqual(result.window_state, ObserverState.RESOLVED)
        self.assertEqual(result.window_basis, "time_anchor")
        self.assertEqual((result.key_window_start, result.key_window_end), (4.5, 5.25))


if __name__ == "__main__":
    unittest.main()
