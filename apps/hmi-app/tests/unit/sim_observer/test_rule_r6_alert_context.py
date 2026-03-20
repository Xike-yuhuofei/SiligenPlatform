import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.adapters.recording_loader import load_recording_from_mapping  # noqa: E402
from hmi_client.features.sim_observer.adapters.recording_normalizer import normalize_recording  # noqa: E402
from hmi_client.features.sim_observer.contracts.observer_status import ObserverState  # noqa: E402
from hmi_client.features.sim_observer.rules.rule_r6_alert_context import resolve_alert_context_window  # noqa: E402


def _sample_payload() -> dict:
    return {
        "summary": {
            "issues": [],
            "alerts": [
                {
                    "alert_id": "explicit-window",
                    "alert_time": 2.0,
                    "label": "Explicit Window",
                    "window_start": 1.0,
                    "window_end": 3.5,
                },
                {
                    "alert_id": "second-window",
                    "alert_time": 5.0,
                    "label": "Seconds Window",
                    "pre_window_s": 0.5,
                    "post_window_s": 1.25,
                },
                {
                    "alert_id": "fallback-window",
                    "alert_time": 8.0,
                    "label": "Fallback Window",
                },
            ],
        },
        "snapshots": [],
        "timeline": [],
        "trace": [],
        "motion_profile": {"segments": []},
    }


class RuleR6AlertContextTest(unittest.TestCase):
    def setUp(self) -> None:
        self.payload = _sample_payload()
        self.normalized = normalize_recording(load_recording_from_mapping(self.payload))

    def test_explicit_window_resolves_context(self) -> None:
        result = resolve_alert_context_window(
            self.normalized.alerts[0],
            self.payload["summary"]["alerts"][0],
        )
        self.assertEqual(result.window_state, ObserverState.RESOLVED)
        self.assertEqual(result.window_basis, "alert_context")
        self.assertEqual((result.window_start, result.window_end), (1.0, 3.5))

    def test_pre_and_post_seconds_resolve_context(self) -> None:
        result = resolve_alert_context_window(
            self.normalized.alerts[1],
            self.payload["summary"]["alerts"][1],
        )
        self.assertEqual(result.window_state, ObserverState.RESOLVED)
        self.assertEqual((result.window_start, result.window_end), (4.5, 6.25))

    def test_missing_context_falls_back_to_single_anchor(self) -> None:
        result = resolve_alert_context_window(
            self.normalized.alerts[2],
            self.payload["summary"]["alerts"][2],
        )
        self.assertEqual(result.window_state, ObserverState.MAPPING_INSUFFICIENT)
        self.assertEqual(result.window_basis, "anchor_only_fallback")
        self.assertEqual(result.fallback_mode, "single_anchor")
        self.assertIsNone(result.window_start)
        self.assertIsNone(result.window_end)


if __name__ == "__main__":
    unittest.main()
