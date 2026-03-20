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


def _workspace_payload() -> dict:
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
                }
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


class SimObserverP1WorkspaceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.app = QApplication.instance() or QApplication([])

    def test_workspace_defaults_to_time_anchor_selection(self) -> None:
        workspace = SimObserverP1Workspace()
        workspace.load_store(ObserverStore.from_mapping(_workspace_payload()))
        self.app.processEvents()

        self.assertIn("selection=time_anchor", workspace._status_label.text())
        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("selection.kind: time_anchor", detail_text)
        self.assertIn("time.object: segment:2", detail_text)
        self.assertTrue(workspace._backlink_button.isEnabled())

    def test_selecting_alert_refreshes_detail_and_backlink_state(self) -> None:
        workspace = SimObserverP1Workspace()
        workspace.load_store(ObserverStore.from_mapping(_workspace_payload()))
        self.app.processEvents()

        workspace._alert_list.setCurrentRow(0)
        self.app.processEvents()

        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("selection.kind: alert", detail_text)
        self.assertIn("context.state: resolved", detail_text)
        self.assertIn("backlink.level: sequence_range", detail_text)
        self.assertTrue(workspace._backlink_button.isEnabled())

    def test_failed_time_anchor_disables_backlink(self) -> None:
        workspace = SimObserverP1Workspace()
        workspace.load_store(ObserverStore.from_mapping(_workspace_payload()))
        self.app.processEvents()

        workspace._time_anchor_list.setCurrentRow(1)
        self.app.processEvents()

        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("failure.reason: mapping_insufficient", detail_text)
        self.assertIn("backlink.level: unavailable", detail_text)
        self.assertFalse(workspace._backlink_button.isEnabled())


class SimObserverWorkspaceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.app = QApplication.instance() or QApplication([])

    def test_backlink_from_p1_returns_to_p0(self) -> None:
        workspace = SimObserverWorkspace()
        workspace.load_mapping(_workspace_payload())
        self.app.processEvents()

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


if __name__ == "__main__":
    unittest.main()
