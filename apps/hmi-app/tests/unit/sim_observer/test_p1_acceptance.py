import copy
import os
import sys
import unittest
from pathlib import Path


os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PyQt5.QtWidgets import QApplication


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.state.observer_store import ObserverStore  # noqa: E402
from hmi_client.features.sim_observer.ui.p1_workspace import SimObserverP1Workspace  # noqa: E402
from hmi_client.features.sim_observer.ui.sim_observer_workspace import SimObserverWorkspace  # noqa: E402


def _base_payload() -> dict:
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
                    "alert_id": "alarm-window",
                    "alert_time": 2.5,
                    "label": "Alarm Window",
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
                "field": "window-unresolved-anchor",
                "tick": {"time_ns": 2_000_000_000},
                "primary_object_id": "segment:1",
                "key_window_unresolved": True,
            },
            {
                "field": "insufficient-anchor",
                "tick": {"time_ns": 3_000_000_000},
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


class SimObserverP1AcceptanceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.app = QApplication.instance() or QApplication([])

    def _load_p1_workspace(self, payload: dict) -> SimObserverP1Workspace:
        workspace = SimObserverP1Workspace()
        workspace.load_store(ObserverStore.from_mapping(copy.deepcopy(payload)))
        self.app.processEvents()
        return workspace

    def _load_container(self, payload: dict) -> SimObserverWorkspace:
        workspace = SimObserverWorkspace()
        workspace.load_mapping(copy.deepcopy(payload))
        self.app.processEvents()
        return workspace

    def test_p1_s4_01_alert_entry_shows_resolved_context_and_key_window(self) -> None:
        workspace = self._load_p1_workspace(_base_payload())

        workspace._alert_list.setCurrentRow(0)
        self.app.processEvents()

        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("selection.kind: alert", detail_text)
        self.assertIn("context.state: resolved", detail_text)
        self.assertIn("key.state: resolved", detail_text)
        self.assertIn("context.range: 2.0 -> 3.0", detail_text)
        self.assertIn("backlink.level: sequence_range", detail_text)
        self.assertTrue(workspace._backlink_button.isEnabled())

    def test_p1_s4_02_alert_fallback_is_explicit_and_not_a_fake_full_window(self) -> None:
        workspace = self._load_p1_workspace(_base_payload())

        workspace._alert_list.setCurrentRow(1)
        self.app.processEvents()

        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("selection.kind: alert", detail_text)
        self.assertIn("context.state: mapping_insufficient", detail_text)
        self.assertIn("context.basis: anchor_only_fallback", detail_text)
        self.assertIn("context.fallback: single_anchor", detail_text)
        self.assertIn("backlink.level: unavailable", detail_text)
        self.assertFalse(workspace._backlink_button.isEnabled())

    def test_p1_s4_03_alert_backlink_returns_to_p0_with_consistent_result(self) -> None:
        workspace = self._load_container(_base_payload())

        workspace._show_p1_button.click()
        self.app.processEvents()
        workspace._p1_workspace._alert_list.setCurrentRow(0)
        self.app.processEvents()
        workspace._p1_workspace._backlink_button.click()
        self.app.processEvents()

        self.assertIs(workspace._stack.currentWidget(), workspace._p0_workspace)
        detail_text = workspace._p0_workspace._detail_text.toPlainText()
        self.assertIn("P1 回接", detail_text)
        self.assertIn("external.sequence: segment:0, segment:1", detail_text)

    def test_p1_s5_01_p0_stays_default_first_screen_and_p1_defaults_to_time_anchor(self) -> None:
        workspace = self._load_container(_base_payload())

        self.assertIs(workspace._stack.currentWidget(), workspace._p0_workspace)
        workspace._show_p1_button.click()
        self.app.processEvents()

        self.assertIs(workspace._stack.currentWidget(), workspace._p1_workspace)
        self.assertIn("selection=time_anchor", workspace._p1_workspace._status_label.text())
        self.assertIn("selection.kind: time_anchor", workspace._p1_workspace._detail_text.toPlainText())

    def test_p1_s5_02_time_anchor_resolves_with_key_window(self) -> None:
        workspace = self._load_p1_workspace(_base_payload())

        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("selection.kind: time_anchor", detail_text)
        self.assertIn("time.mapping.level: single_object", detail_text)
        self.assertIn("time.object: segment:2", detail_text)
        self.assertIn("key.state: resolved", detail_text)
        self.assertIn("backlink.level: single_object", detail_text)

    def test_p1_s5_03_mapping_insufficient_disables_backlink_and_explains_failure(self) -> None:
        workspace = self._load_p1_workspace(_base_payload())

        workspace._time_anchor_list.setCurrentRow(2)
        self.app.processEvents()

        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("time.mapping.level: mapping_insufficient", detail_text)
        self.assertIn("failure.reason: mapping_insufficient", detail_text)
        self.assertIn("backlink.level: unavailable", detail_text)
        self.assertFalse(workspace._backlink_button.isEnabled())

    def test_p1_f_01_window_not_resolved_keeps_explained_failure_and_return_path(self) -> None:
        workspace = self._load_container(_base_payload())

        workspace._show_p1_button.click()
        self.app.processEvents()
        workspace._p1_workspace._time_anchor_list.setCurrentRow(1)
        self.app.processEvents()

        detail_text = workspace._p1_workspace._detail_text.toPlainText()
        self.assertIn("key.state: window_not_resolved", detail_text)
        self.assertIn("failure.reason: window_not_resolved", detail_text)
        workspace._p1_workspace._return_button.click()
        self.app.processEvents()
        self.assertIs(workspace._stack.currentWidget(), workspace._p0_workspace)

    def test_p1_r_01_invalid_render_state_is_explicitly_intercepted(self) -> None:
        workspace = self._load_p1_workspace(_base_payload())

        object.__setattr__(workspace._current_selection.time_mapping, "mapping_state", "bad_state")

        with self.assertRaisesRegex(ValueError, "Unsupported time mapping state"):
            workspace._render_from_store()


if __name__ == "__main__":
    unittest.main()
