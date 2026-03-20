import os
import sys
import unittest
from pathlib import Path


os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PyQt5.QtWidgets import QApplication


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.state.observer_store import ObserverStore  # noqa: E402
from hmi_client.features.sim_observer.contracts.observer_types import ObjectId  # noqa: E402
from hmi_client.features.sim_observer.ui.p0_workspace import SimObserverP0Workspace  # noqa: E402


def _workspace_payload() -> dict:
    return {
        "summary": {
            "issues": [
                {
                    "issue_id": "summary-only",
                    "issue_category": "coverage",
                    "issue_level": "warning",
                    "label": "Summary only issue",
                },
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
            ],
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


class SimObserverP0WorkspaceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.app = QApplication.instance() or QApplication([])

    def test_workspace_populates_summary_structure_and_detail(self) -> None:
        workspace = SimObserverP0Workspace()
        workspace.load_store(ObserverStore.from_mapping(_workspace_payload()))
        self.app.processEvents()

        self.assertEqual(workspace._summary_list.count(), 3)
        self.assertEqual(workspace._structure_list.count(), 2)
        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("Single target issue", detail_text)
        self.assertIn("primary.object: segment:1", detail_text)

    def test_workspace_uses_resolved_issue_as_default_entry(self) -> None:
        workspace = SimObserverP0Workspace()
        workspace.load_store(ObserverStore.from_mapping(_workspace_payload()))
        self.app.processEvents()

        self.assertEqual(workspace._summary_list.currentItem().data(0x0100), ObjectId("issue:single-target:1"))

    def test_structure_reverse_select_does_not_switch_when_multiple_issues_match(self) -> None:
        workspace = SimObserverP0Workspace()
        workspace.load_store(ObserverStore.from_mapping(_workspace_payload()))
        self.app.processEvents()

        workspace._summary_list.setCurrentRow(0)
        self.app.processEvents()
        workspace._structure_list.setCurrentRow(1)
        self.app.processEvents()

        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("多个 issue", detail_text)
        self.assertIn("Summary only issue", detail_text)

    def test_summary_only_issue_does_not_fake_canvas_highlight(self) -> None:
        workspace = SimObserverP0Workspace()
        workspace.load_store(ObserverStore.from_mapping(_workspace_payload()))
        self.app.processEvents()

        workspace._summary_list.setCurrentRow(0)
        self.app.processEvents()

        self.assertIn("失败/降级态", workspace._canvas_hint.text())
        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("failure.reason: anchor_missing", detail_text)


if __name__ == "__main__":
    unittest.main()
