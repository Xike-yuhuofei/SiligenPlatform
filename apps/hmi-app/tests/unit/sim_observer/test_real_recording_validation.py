import os
import sys
import unittest
from pathlib import Path


os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PyQt5.QtWidgets import QApplication
from PyQt5.QtWidgets import QGraphicsPathItem


PROJECT_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.features.sim_observer.state.observer_store import ObserverStore  # noqa: E402
from hmi_client.features.sim_observer.ui.p0_workspace import SimObserverP0Workspace  # noqa: E402
from hmi_client.features.sim_observer.ui.sim_observer_workspace import SimObserverWorkspace  # noqa: E402


REPO_ROOT = PROJECT_ROOT.parents[1]
REPLAY_ROOT = REPO_ROOT / "examples" / "replay-data"
SAMPLE_TRAJECTORY = REPLAY_ROOT / "sample_trajectory.scheme_c.recording.json"
RECT_DIAG = REPLAY_ROOT / "rect_diag.scheme_c.recording.json"


class RealRecordingValidationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.app = QApplication.instance() or QApplication([])

    def test_sample_trajectory_real_recording_loads_path_segments(self) -> None:
        store = ObserverStore.from_file(SAMPLE_TRAJECTORY)

        self.assertEqual(len(store.normalized_data.path_segments), 3)
        self.assertEqual(tuple(segment.segment_type for segment in store.normalized_data.path_segments), ("line", "arc", "line"))
        self.assertEqual(store.issue_ids, tuple())
        arc_segment = store.normalized_data.path_segments[1]
        self.assertAlmostEqual(arc_segment.start_point.x, 120.0, places=3)
        self.assertAlmostEqual(arc_segment.start_point.y, 0.0, places=3)
        self.assertAlmostEqual(arc_segment.end_point.x, 180.0, places=3)
        self.assertAlmostEqual(arc_segment.end_point.y, 60.0, places=3)
        self.assertIsNotNone(arc_segment.arc_geometry)
        self.assertAlmostEqual(arc_segment.arc_geometry.center.x, 120.0, places=3)
        self.assertAlmostEqual(arc_segment.arc_geometry.center.y, 60.0, places=3)
        self.assertAlmostEqual(arc_segment.arc_geometry.radius, 60.0, places=3)

    def test_rect_diag_real_recording_loads_path_segments(self) -> None:
        store = ObserverStore.from_file(RECT_DIAG)

        self.assertEqual(len(store.normalized_data.path_segments), 5)
        self.assertTrue(all(segment.segment_type == "line" for segment in store.normalized_data.path_segments))
        self.assertEqual(store.issue_ids, tuple())

    def test_p0_workspace_handles_real_recording_without_issues(self) -> None:
        workspace = SimObserverP0Workspace()
        workspace.load_store(ObserverStore.from_file(SAMPLE_TRAJECTORY))
        self.app.processEvents()

        self.assertEqual(workspace._summary_list.count(), 1)
        self.assertEqual(workspace._structure_list.count(), 3)
        self.assertIn("issues=0", workspace._status_label.text())
        self.assertIn("segments=3", workspace._status_label.text())
        self.assertIn("mode=empty", workspace._status_label.text())
        self.assertIn("无 P0 issue", workspace._canvas_hint.text())
        self.assertIn("没有可用于 P0 的 issue 入口", workspace._detail_text.toPlainText())
        path_item_count = sum(
            1 for item in workspace._canvas_view.scene().items() if isinstance(item, QGraphicsPathItem)
        )
        self.assertGreaterEqual(path_item_count, 1)

    def test_p0_workspace_auto_focuses_first_segment_without_issues(self) -> None:
        store = ObserverStore.from_file(SAMPLE_TRAJECTORY)
        workspace = SimObserverP0Workspace()
        workspace.load_store(store)
        self.app.processEvents()

        first_segment = store.normalized_data.path_segments[0]
        self.assertEqual(workspace._current_structure_object_id, first_segment.object_id)
        detail_text = workspace._detail_text.toPlainText()
        self.assertIn(f"structure.focus: {first_segment.object_id}", detail_text)
        self.assertIn("focused.segment: 0 | line | point_count=2", detail_text)

    def test_p0_workspace_switches_structure_focus_without_issues(self) -> None:
        workspace = SimObserverP0Workspace()
        workspace.load_store(ObserverStore.from_file(RECT_DIAG))
        self.app.processEvents()

        workspace._structure_list.setCurrentRow(4)
        self.app.processEvents()

        self.assertEqual(workspace._current_structure_object_id, "segment:4")
        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("structure.focus: segment:4", detail_text)
        self.assertIn("focused.segment: 4 | line | point_count=2", detail_text)
        self.assertIn("structure.note: 当前 recording 无 issue，结构焦点已切换到路径事实。", detail_text)

    def test_p0_workspace_reports_arc_render_mode_when_focusing_arc_segment(self) -> None:
        workspace = SimObserverP0Workspace()
        workspace.load_store(ObserverStore.from_file(SAMPLE_TRAJECTORY))
        self.app.processEvents()

        workspace._structure_list.setCurrentRow(1)
        self.app.processEvents()

        detail_text = workspace._detail_text.toPlainText()
        self.assertIn("focused.segment: 1 | arc | point_count=9", detail_text)
        self.assertIn("segment.render.mode: arc_primitive", detail_text)

    def test_real_recording_flow_can_enter_p1_and_return_to_p0(self) -> None:
        workspace = SimObserverWorkspace()
        workspace.load_store(ObserverStore.from_file(SAMPLE_TRAJECTORY))
        self.app.processEvents()

        initial_focus = workspace._p0_workspace._current_structure_object_id
        self.assertEqual(initial_focus, "segment:0")
        workspace._show_p1_button.click()
        self.app.processEvents()

        self.assertIs(workspace._stack.currentWidget(), workspace._p1_workspace)
        self.assertIn("selection=time_anchor", workspace._p1_workspace._status_label.text())
        self.assertIn("mode=resolved", workspace._p1_workspace._status_label.text())
        p1_detail = workspace._p1_workspace._detail_text.toPlainText()
        self.assertIn("selection.id: time_anchor:trace:0", p1_detail)
        self.assertIn("time.mapping.state: resolved", p1_detail)
        self.assertIn("failure.reason: -", p1_detail)
        self.assertIn("time.object: segment:0", p1_detail)
        self.assertIn("time.reason.code: time_mapping_from_snapshot_position", p1_detail)
        self.assertIn("backlink.level: single_object", p1_detail)

        workspace._p1_workspace._return_button.click()
        self.app.processEvents()

        self.assertIs(workspace._stack.currentWidget(), workspace._p0_workspace)
        self.assertEqual(workspace._p0_workspace._current_structure_object_id, initial_focus)
        self.assertIn(
            f"structure.focus: {initial_focus}",
            workspace._p0_workspace._detail_text.toPlainText(),
        )

    def test_rect_diag_default_time_anchor_keeps_mapping_insufficient(self) -> None:
        workspace = SimObserverWorkspace()
        workspace.load_store(ObserverStore.from_file(RECT_DIAG))
        self.app.processEvents()

        workspace._show_p1_button.click()
        self.app.processEvents()

        self.assertIn("selection=time_anchor", workspace._p1_workspace._status_label.text())
        self.assertIn("mode=failed", workspace._p1_workspace._status_label.text())
        p1_detail = workspace._p1_workspace._detail_text.toPlainText()
        self.assertIn("selection.id: time_anchor:trace:0", p1_detail)
        self.assertIn("failure.reason: mapping_insufficient", p1_detail)
        self.assertIn("time.reason.code: time_mapping_snapshot_ambiguous", p1_detail)
        self.assertIn("backlink.level: unavailable", p1_detail)


if __name__ == "__main__":
    unittest.main()
