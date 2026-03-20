import copy
import os
import sys
import unittest
from pathlib import Path


os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import QApplication


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.contracts.observer_types import ObjectId  # noqa: E402
from hmi_client.features.sim_observer.state.observer_store import ObserverStore  # noqa: E402
from hmi_client.features.sim_observer.ui.p0_workspace import SimObserverP0Workspace  # noqa: E402


def _base_payload() -> dict:
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


def _group_resolved_payload() -> dict:
    payload = _base_payload()
    payload["summary"]["issues"] = [
        {
            "issue_id": "group-resolved",
            "issue_category": "ordering",
            "issue_level": "warning",
            "label": "Group resolved issue",
            "candidate_object_ids": ["segment:0", "segment:1"],
        }
    ]
    return payload


def _no_issue_payload() -> dict:
    payload = _base_payload()
    payload["summary"]["issues"] = []
    return payload


class SimObserverP0AcceptanceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.app = QApplication.instance() or QApplication([])

    def _load_workspace(self, payload: dict) -> SimObserverP0Workspace:
        workspace = SimObserverP0Workspace()
        workspace.load_store(ObserverStore.from_mapping(copy.deepcopy(payload)))
        self.app.processEvents()
        return workspace

    def test_p0_s1_01_default_entry_prefers_resolved_issue(self) -> None:
        workspace = self._load_workspace(_base_payload())

        self.assertEqual(
            workspace._summary_list.currentItem().data(Qt.UserRole),
            ObjectId("issue:single-target:1"),
        )
        self.assertIn("mode=resolved", workspace._status_label.text())
        self.assertIn("state=resolved", workspace._status_label.text())

    def test_p0_s1_02_summary_switch_refreshes_detail_and_source_refs(self) -> None:
        workspace = self._load_workspace(_base_payload())

        workspace._summary_list.setCurrentRow(0)
        self.app.processEvents()

        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("Summary only issue", detail_text)
        self.assertIn("source.refs:", detail_text)
        self.assertIn("source.ref.count: 1", detail_text)
        self.assertIn("summary.issues[0]", detail_text)

    def test_p0_s2_01_summary_only_does_not_fake_canvas_highlight(self) -> None:
        workspace = self._load_workspace(_base_payload())

        workspace._summary_list.setCurrentRow(0)
        self.app.processEvents()

        self.assertIn("失败/降级态", workspace._canvas_hint.text())
        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("association.level: summary_only", detail_text)
        self.assertIn("failure.reason: anchor_missing", detail_text)
        self.assertIn("primary.object: -", detail_text)

    def test_p0_s2_02_group_issue_stays_as_object_group(self) -> None:
        workspace = self._load_workspace(_group_resolved_payload())

        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("Group resolved issue", detail_text)
        self.assertIn("association.level: object_group", detail_text)
        self.assertIn("group.state: resolved", detail_text)
        self.assertIn("group.members: segment:0, segment:1", detail_text)
        self.assertIn("mode=resolved", workspace._status_label.text())

    def test_p0_s3_01_structure_focus_on_current_object_keeps_summary(self) -> None:
        workspace = self._load_workspace(_base_payload())

        workspace._structure_list.setCurrentRow(1)
        self.app.processEvents()

        self.assertEqual(
            workspace._summary_list.currentItem().data(Qt.UserRole),
            ObjectId("issue:single-target:1"),
        )
        self.assertIn("Single target issue", workspace._detail_text.toPlainText())
        self.assertIn("primary.object: segment:1", workspace._detail_text.toPlainText())

    def test_p0_s3_02_structure_unique_match_switches_summary(self) -> None:
        workspace = self._load_workspace(_base_payload())

        workspace._structure_list.setCurrentRow(0)
        self.app.processEvents()

        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("Group target issue", detail_text)
        self.assertIn("结构反选已回接到唯一 issue", detail_text)

    def test_p0_s3_03_structure_multiple_matches_do_not_switch_summary(self) -> None:
        workspace = self._load_workspace(_base_payload())

        workspace._summary_list.setCurrentRow(0)
        self.app.processEvents()
        workspace._structure_list.setCurrentRow(1)
        self.app.processEvents()

        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("多个 issue", detail_text)
        self.assertIn("Summary only issue", detail_text)

    def test_p0_f_01_global_empty_state_without_recording(self) -> None:
        workspace = SimObserverP0Workspace()
        self.app.processEvents()

        self.assertEqual(workspace._summary_list.count(), 0)
        self.assertEqual(workspace._structure_list.count(), 0)
        self.assertEqual(workspace._canvas_hint.text(), "未加载 recording。")
        self.assertIn("mode=empty", workspace._status_label.text())
        self.assertIn("请先加载 recording", workspace._detail_text.toPlainText())

    def test_p0_f_02_recording_without_issues_shows_explained_empty_state(self) -> None:
        workspace = self._load_workspace(_no_issue_payload())

        self.assertEqual(workspace._summary_list.count(), 1)
        self.assertEqual(workspace._structure_list.count(), 2)
        self.assertIn("issues=0", workspace._status_label.text())
        self.assertIn("mode=empty", workspace._status_label.text())
        self.assertIn("无 P0 issue", workspace._canvas_hint.text())
        self.assertIn("没有可用于 P0 的 issue 入口", workspace._detail_text.toPlainText())

    def test_p0_r_01_invalid_state_is_explicitly_intercepted(self) -> None:
        workspace = self._load_workspace(_base_payload())

        object.__setattr__(workspace._current_selection.primary_association, "selection_state", "bad_state")

        with self.assertRaisesRegex(ValueError, "Unsupported observer state"):
            workspace._render_from_store()


if __name__ == "__main__":
    unittest.main()
