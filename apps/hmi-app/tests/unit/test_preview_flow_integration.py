import os
import sys
import unittest
from pathlib import Path
from typing import Any, cast


os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PyQt5.QtWidgets import QApplication, QWidget


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))
sys.path.insert(0, str(PROJECT_ROOT / "src" / "hmi_client"))

from hmi_client.ui import main_window as main_window_module


class _DummyPage:
    def setBackgroundColor(self, *_args, **_kwargs) -> None:
        return None


class _FakePreviewView(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.html = ""
        self._page = _DummyPage()

    def page(self):
        return self._page

    def setHtml(self, html: str) -> None:
        self.html = html


class _Signal:
    def __init__(self) -> None:
        self._callbacks = []

    def connect(self, callback) -> None:
        self._callbacks.append(callback)

    def emit(self, *args) -> None:
        for callback in list(self._callbacks):
            callback(*args)


class _FakePreviewSnapshotWorker:
    created = []

    def __init__(
        self,
        *,
        host: str,
        port: int,
        artifact_id: str,
        speed_mm_s: float,
        dry_run: bool,
        dry_run_speed_mm_s: float,
    ) -> None:
        self.host = host
        self.port = port
        self.artifact_id = artifact_id
        self.speed_mm_s = speed_mm_s
        self.dry_run = dry_run
        self.dry_run_speed_mm_s = dry_run_speed_mm_s
        self.completed = _Signal()
        self.finished = _Signal()
        type(self).created.append(self)

    def start(self) -> None:
        payload = {
            "snapshot_id": "snapshot-it",
            "snapshot_hash": "hash-it",
            "plan_id": "plan-it",
            "preview_source": "planned_glue_snapshot",
            "preview_kind": "glue_points",
            "segment_count": 2,
            "glue_point_count": 2,
            "glue_points": [
                {"x": 0.0, "y": 0.0},
                {"x": 10.0, "y": 0.0},
            ],
            "execution_polyline_source_point_count": 8,
            "execution_polyline_point_count": 2,
            "execution_polyline": [
                {"x": 0.0, "y": 0.0},
                {"x": 10.0, "y": 0.0},
            ],
            "motion_preview": {
                "source": "execution_trajectory_snapshot",
                "kind": "polyline",
                "source_point_count": 8,
                "point_count": 2,
                "is_sampled": True,
                "sampling_strategy": "fixed_spacing_corner_preserving",
                "polyline": [
                    {"x": 0.0, "y": 0.0},
                    {"x": 10.0, "y": 0.0},
                ],
            },
            "total_length_mm": 10.0,
            "estimated_time_s": 0.5,
            "generated_at": "2026-04-02T00:00:00Z",
            "dry_run": False,
        }
        self.completed.emit(True, payload, "")
        self.finished.emit()

    def deleteLater(self) -> None:
        return None


class _FakeProtocol:
    def __init__(self) -> None:
        self.calls = []

    def dxf_create_artifact(self, filepath: str):
        self.calls.append(("dxf.artifact.create", filepath))
        return True, {"artifact_id": "artifact-it", "segment_count": 7}, ""

    def dxf_get_info(self):
        self.calls.append(("dxf.info",))
        return {"total_length": 12.5, "total_segments": 7}


class _FakeClient:
    def __init__(self, host: str = "127.0.0.1", port: int = 9527) -> None:
        self.host = host
        self.port = port


class PreviewFlowIntegrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self) -> None:
        _FakePreviewSnapshotWorker.created.clear()
        self._original_web_view = getattr(main_window_module, "QWebEngineView", None)
        self._original_web_engine_flag = getattr(main_window_module, "WEB_ENGINE_AVAILABLE", False)
        self._original_worker = main_window_module.PreviewSnapshotWorker
        main_window_module.QWebEngineView = _FakePreviewView
        main_window_module.WEB_ENGINE_AVAILABLE = True
        main_window_module.PreviewSnapshotWorker = _FakePreviewSnapshotWorker
        self.window = main_window_module.MainWindow(launch_mode="offline")

    def tearDown(self) -> None:
        self.window.close()
        self.window.deleteLater()
        main_window_module.QWebEngineView = self._original_web_view
        main_window_module.WEB_ENGINE_AVAILABLE = self._original_web_engine_flag
        main_window_module.PreviewSnapshotWorker = self._original_worker

    def test_on_dxf_load_runs_import_to_preview_update_chain(self) -> None:
        runtime_window = cast(Any, self.window)
        runtime_window._require_online_mode = lambda _capability: True
        runtime_window._protocol = _FakeProtocol()
        runtime_window._client = _FakeClient()
        runtime_window._connected = True
        runtime_window._mode_production.setChecked(True)
        runtime_window._mode_dryrun.setChecked(False)
        runtime_window._dxf_filepath = str(PROJECT_ROOT.parent.parent / "samples" / "dxf" / "rect_diag.dxf")

        runtime_window._on_dxf_load()

        self.assertTrue(runtime_window._dxf_loaded)
        self.assertEqual(runtime_window._dxf_artifact_id, "artifact-it")
        self.assertEqual(runtime_window._current_plan_id, "plan-it")
        self.assertEqual(runtime_window._current_plan_fingerprint, "hash-it")
        self.assertEqual(runtime_window._preview_source, "planned_glue_snapshot")
        self.assertEqual(runtime_window._preview_session.state.preview_kind, "glue_points")
        self.assertEqual(runtime_window._preview_session.state.glue_point_count, 2)
        self.assertEqual(runtime_window._preview_session.state.current_plan_id, "plan-it")
        self.assertEqual(runtime_window._preview_session.state.current_plan_fingerprint, "hash-it")
        self.assertEqual(runtime_window._preview_gate.snapshot.snapshot_hash, "hash-it")
        self.assertEqual(runtime_window.statusBar().currentMessage(), "胶点预览已更新，启动前需确认")
        self.assertEqual(runtime_window._dxf_filename_display.text(), "rect_diag.dxf")
        self.assertIn("规划胶点主预览", runtime_window._dxf_view.html)
        self.assertIn("hash-it", runtime_window._dxf_view.html)
        self.assertEqual(
            runtime_window._protocol.calls,
            [
                ("dxf.artifact.create", runtime_window._dxf_filepath),
                ("dxf.info",),
            ],
        )
        self.assertEqual(len(_FakePreviewSnapshotWorker.created), 1)
        worker = _FakePreviewSnapshotWorker.created[0]
        self.assertEqual(worker.host, "127.0.0.1")
        self.assertEqual(worker.port, 9527)
        self.assertEqual(worker.artifact_id, "artifact-it")
        self.assertEqual(worker.speed_mm_s, runtime_window._dxf_speed.value())
        self.assertFalse(worker.dry_run)
        self.assertEqual(worker.dry_run_speed_mm_s, runtime_window._dxf_speed.value())
        self.assertIsNone(runtime_window._preview_snapshot_worker)
        self.assertFalse(runtime_window._preview_refresh_inflight)


if __name__ == "__main__":
    unittest.main()
