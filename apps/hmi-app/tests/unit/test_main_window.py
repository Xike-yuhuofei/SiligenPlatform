import os
import sys
import time
import unittest
from pathlib import Path


os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PyQt5.QtWidgets import QApplication, QWidget


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src" / "hmi_client"))

import ui.main_window as main_window_module
from client.protocol import AxisStatus, EffectiveInterlocks, IOStatus, MachineStatus, SupervisionStatus


class FakeProtocol:
    def __init__(self, status: MachineStatus):
        self.status = status
        self.home_calls = []
        self.home_auto_calls = []
        self.jog_calls = []
        self.stop_calls = 0

    def get_status(self) -> MachineStatus:
        return self.status

    def home(self, axes=None, force=False):
        self.home_calls.append((axes, force))
        return True, "Homed"

    def home_auto(self, axes=None, force: bool = False, wait_for_completion: bool = True, timeout_ms: int = 0):
        self.home_auto_calls.append((axes, force, wait_for_completion, timeout_ms))
        return True, "Axes ready at zero"

    def jog(self, axis: str, direction: int, speed: float):
        self.jog_calls.append((axis, direction, speed))
        return True, "Jogging"

    def stop(self) -> bool:
        self.stop_calls += 1
        return True


class _FakePreviewView:
    def __init__(self) -> None:
        self.html = ""

    def setHtml(self, html: str) -> None:
        self.html = html


class MainWindowTabsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self) -> None:
        self._original_web_view = getattr(main_window_module, "QWebEngineView", None)
        self._original_web_engine_flag = getattr(main_window_module, "WEB_ENGINE_AVAILABLE", False)
        main_window_module.QWebEngineView = None
        main_window_module.WEB_ENGINE_AVAILABLE = False
        self.window = main_window_module.MainWindow(launch_mode="offline")

    def tearDown(self) -> None:
        self.window.close()
        self.window.deleteLater()
        main_window_module.QWebEngineView = self._original_web_view
        main_window_module.WEB_ENGINE_AVAILABLE = self._original_web_engine_flag

    def _make_status(
        self,
        *,
        connected: bool = True,
        connection_state: str = "connected",
        estop: bool = False,
        door: bool = False,
        estop_known: bool = True,
        door_known: bool = True,
        effective_estop: bool | None = None,
        effective_door: bool | None = None,
        x_homed: bool = False,
        y_homed: bool = False,
    ) -> MachineStatus:
        return MachineStatus(
            connected=connected,
            connection_state=connection_state,
            machine_state="Idle",
            machine_state_reason="idle",
            supervision=SupervisionStatus(current_state="Idle", requested_state="Idle", state_reason="idle"),
            axes={
                "X": AxisStatus(enabled=True, homed=x_homed, homing_state="homed" if x_homed else "not_homed"),
                "Y": AxisStatus(enabled=True, homed=y_homed, homing_state="homed" if y_homed else "not_homed"),
            },
            effective_interlocks=EffectiveInterlocks(
                estop_active=estop if effective_estop is None else effective_estop,
                estop_known=estop_known,
                door_open_active=door if effective_door is None else effective_door,
                door_open_known=door_known,
            ),
            io=IOStatus(
                estop=estop,
                estop_known=estop_known,
                door=door,
                door_known=door_known,
            ),
        )

    def _set_cached_status(self, status: MachineStatus) -> None:
        self.window._last_status = status
        self.window._last_status_ts = time.monotonic()

    def _arm_confirmed_preview(
        self,
        *,
        preview_source: str = "planned_glue_snapshot",
        preview_kind: str = "glue_points",
        glue_points=None,
        plan_id: str = "plan-1",
        snapshot_hash: str = "hash-1",
        expect_ok: bool = True,
    ):
        glue_point_payload = (
            [
                {"x": 0.0, "y": 0.0},
                {"x": 6.0, "y": 0.0},
                {"x": 12.0, "y": 3.0},
            ]
            if glue_points is None
            else glue_points
        )
        result = self.window._preview_session.process_snapshot_payload(
            {
                "snapshot_id": "snapshot-1",
                "snapshot_hash": snapshot_hash,
                "plan_id": plan_id,
                "preview_source": preview_source,
                "preview_kind": preview_kind,
                "segment_count": 2,
                "glue_point_count": len(glue_point_payload),
                "glue_points": glue_point_payload,
                "execution_polyline": [
                    {"x": 0.0, "y": 0.0},
                    {"x": 12.0, "y": 3.0},
                ],
                "total_length_mm": 12.0,
                "estimated_time_s": 1.5,
                "generated_at": "2026-03-26T00:00:00Z",
                "dry_run": False,
            },
            current_dry_run=False,
        )
        self.assertEqual(result.ok, expect_ok)
        if expect_ok:
            self.window._preview_session.gate.confirm_current_snapshot()
            self.window._sync_preview_session_fields()
        return result

    def _collect_testids(self) -> set[str]:
        return {
            testid
            for widget in self.window.findChildren(QWidget)
            if (testid := widget.property("data-testid"))
        }

    def test_main_tabs_do_not_expose_sim_observer_entry(self) -> None:
        labels = [
            self.window._main_tabs.tabText(index)
            for index in range(self.window._main_tabs.count())
        ]

        self.assertEqual(labels, ["生产", "设置", "配置", "报警"])
        self.assertNotIn("仿真观察", labels)

    def test_check_home_preconditions_rejects_estop(self) -> None:
        self.window._require_online_mode = lambda capability: True
        self._set_cached_status(self._make_status(estop=True))

        result = self.window._check_home_preconditions()

        self.assertFalse(result)
        self.assertEqual(self.window.statusBar().currentMessage(), "急停未解除，无法回零")

    def test_check_home_preconditions_rejects_open_door(self) -> None:
        self.window._require_online_mode = lambda capability: True
        self._set_cached_status(self._make_status(door=True))

        result = self.window._check_home_preconditions()

        self.assertFalse(result)
        self.assertEqual(self.window.statusBar().currentMessage(), "安全门打开，无法回零")

    def test_on_home_uses_home_auto_when_target_axes_are_already_homed(self) -> None:
        status = self._make_status(x_homed=True, y_homed=True)
        fake_protocol = FakeProtocol(status)
        self.window._protocol = fake_protocol
        self.window._check_home_preconditions = lambda: True

        self.window._on_home(["X", "Y"])

        self.assertEqual(fake_protocol.home_calls, [])
        self.assertEqual(fake_protocol.home_auto_calls, [(["X", "Y"], False, True, 0)])
        self.assertEqual(self.window.statusBar().currentMessage(), "回零: Axes ready at zero")

    def test_on_home_uses_home_auto_when_target_axis_is_not_homed(self) -> None:
        status = self._make_status(x_homed=False, y_homed=True)
        fake_protocol = FakeProtocol(status)
        self.window._protocol = fake_protocol
        self.window._check_home_preconditions = lambda: True

        self.window._on_home(["X"])

        self.assertEqual(fake_protocol.home_calls, [])
        self.assertEqual(fake_protocol.home_auto_calls, [(["X"], False, True, 0)])
        self.assertEqual(self.window.statusBar().currentMessage(), "回零: Axes ready at zero")

    def test_check_motion_preconditions_rejects_degraded_connection(self) -> None:
        self.window._require_online_mode = lambda capability: True
        self.window._protocol = FakeProtocol(self._make_status(connection_state="degraded"))

        result = self.window._check_motion_preconditions()

        self.assertFalse(result)
        self.assertEqual(self.window.statusBar().currentMessage(), "硬件连接已降级，无法点动，请重新连接")

    def test_check_motion_preconditions_prefers_effective_door_interlock(self) -> None:
        self.window._require_online_mode = lambda capability: True
        self.window._protocol = FakeProtocol(self._make_status(door=False, effective_door=True))

        result = self.window._check_motion_preconditions()

        self.assertFalse(result)
        self.assertEqual(self.window.statusBar().currentMessage(), "安全门打开，无法点动")

    def test_on_jog_sends_protocol_request_when_preconditions_pass(self) -> None:
        status = self._make_status(x_homed=True, y_homed=True)
        fake_protocol = FakeProtocol(status)
        self.window._protocol = fake_protocol
        self.window._check_motion_preconditions = lambda: True
        self.window._jog_speed = 12.0

        self.window._on_jog("X", 1)

        self.assertEqual(fake_protocol.jog_calls, [("X", 1, 12.0)])
        self.assertIsNotNone(self.window._jog_press_time)
        self.assertEqual(self.window._last_jog_context["axis"], "X")
        self.assertEqual(self.window._last_jog_context["direction"], 1)
        self.assertEqual(self.window._last_jog_context["speed"], 12.0)

    def test_on_jog_release_requests_stop(self) -> None:
        status = self._make_status(x_homed=True, y_homed=True)
        fake_protocol = FakeProtocol(status)
        self.window._protocol = fake_protocol
        self.window._jog_press_time = time.monotonic()
        self.window._last_jog_context = {"axis": "X", "direction": 1, "speed": 10.0}

        self.window._on_jog_release()

        self.assertEqual(fake_protocol.stop_calls, 1)
        self.assertIsNone(self.window._jog_press_time)

    def test_system_panel_does_not_render_home_buttons(self) -> None:
        testids = self._collect_testids()

        self.assertNotIn("btn-home-all", testids)
        self.assertNotIn("btn-home-X", testids)
        self.assertNotIn("btn-home-Y", testids)
        self.assertIn("btn-production-home-all", testids)

    def test_system_panel_does_not_render_recovery_region(self) -> None:
        testids = self._collect_testids()

        self.assertNotIn("panel-session-recovery", testids)
        self.assertNotIn("btn-recovery-retry", testids)
        self.assertNotIn("btn-recovery-restart", testids)
        self.assertNotIn("btn-recovery-stop", testids)

    def test_render_runtime_preview_html_marks_mock_source_as_non_real_geometry(self) -> None:
        snapshot = main_window_module.PreviewSnapshotMeta(
            snapshot_id="snapshot-1",
            snapshot_hash="hash-1",
            segment_count=2,
            point_count=3,
            total_length_mm=12.0,
            estimated_time_s=1.5,
            generated_at="2026-03-26T00:00:00Z",
        )

        html = self.window._render_runtime_preview_html(
            snapshot=snapshot,
            speed_mm_s=12.0,
            dry_run=False,
            preview_source="mock_synthetic",
            glue_points=[(0.0, 0.0), (6.0, 0.0), (12.0, 3.0)],
            execution_polyline=[(0.0, 0.0), (12.0, 3.0)],
            preview_kind="glue_points",
        )

        self.assertIn("当前为 Mock 模拟轨迹", html)
        self.assertIn("非真实几何", html)
        self.assertIn("来源</td><td>Mock模拟</td>", html)

    def test_render_runtime_preview_html_renders_sampling_warning_banner(self) -> None:
        snapshot = main_window_module.PreviewSnapshotMeta(
            snapshot_id="snapshot-1",
            snapshot_hash="hash-1",
            segment_count=2,
            point_count=3,
            total_length_mm=12.0,
            estimated_time_s=1.5,
            generated_at="2026-03-26T00:00:00Z",
        )

        html = self.window._render_runtime_preview_html(
            snapshot=snapshot,
            speed_mm_s=12.0,
            dry_run=False,
            preview_source="planned_glue_snapshot",
            glue_points=[(0.0, 0.0), (6.0, 0.0), (12.0, 3.0)],
            execution_polyline=[(0.0, 0.0), (12.0, 3.0)],
            preview_kind="glue_points",
            preview_warning="胶点预览疑似退化为轨迹采样点（胶点数 950，执行轨迹源点 1000）。",
        )

        self.assertIn("预览质量告警", html)
        self.assertIn("胶点预览疑似退化为轨迹采样点", html)

    def test_check_production_preconditions_rejects_mock_preview_source(self) -> None:
        status = self._make_status(x_homed=True, y_homed=True)
        self.window._require_online_mode = lambda capability: True
        self.window._is_online_ready = lambda: True
        self.window._protocol = FakeProtocol(status)
        self.window._connected = True
        self.window._hw_connected = True
        self.window._runtime_status_fault = False
        self._arm_confirmed_preview()
        self.window._preview_session.state.preview_source = "mock_synthetic"
        self.window._sync_preview_session_fields()
        warnings = []
        self.window._show_preflight_warning = warnings.append

        result = self.window._check_production_preconditions(dry_run=False)

        self.assertFalse(result)
        self.assertEqual(
            warnings[-1],
            "当前预览来源为 mock_synthetic，不能作为真实在线预览通过依据",
        )

    def test_check_production_preconditions_rejects_invalid_preview_kind(self) -> None:
        status = self._make_status(x_homed=True, y_homed=True)
        self.window._require_online_mode = lambda capability: True
        self.window._is_online_ready = lambda: True
        self.window._protocol = FakeProtocol(status)
        self.window._connected = True
        self.window._hw_connected = True
        self.window._runtime_status_fault = False
        self._arm_confirmed_preview()
        self.window._preview_session.state.preview_kind = "trajectory_polyline"
        self.window._sync_preview_session_fields()
        warnings = []
        self.window._show_preflight_warning = warnings.append

        result = self.window._check_production_preconditions(dry_run=False)

        self.assertFalse(result)
        self.assertEqual(
            warnings[-1],
            "当前预览语义为 trajectory_polyline，不是 glue_points，不能作为真实在线预览通过依据",
        )

    def test_check_production_preconditions_rejects_authority_hash_mismatch(self) -> None:
        status = self._make_status(x_homed=True, y_homed=True)
        self.window._require_online_mode = lambda capability: True
        self.window._is_online_ready = lambda: True
        self.window._protocol = FakeProtocol(status)
        self.window._connected = True
        self.window._hw_connected = True
        self.window._runtime_status_fault = False
        self._arm_confirmed_preview()
        self.window._preview_session.state.current_plan_fingerprint = "hash-2"
        self.window._sync_preview_session_fields()
        warnings = []
        self.window._show_preflight_warning = warnings.append

        result = self.window._check_production_preconditions(dry_run=False)

        self.assertFalse(result)
        self.assertEqual(
            warnings[-1],
            "预览快照与执行快照不一致，请重新生成并确认",
        )

    def test_preview_snapshot_success_renders_authority_payload_without_timeout_side_effects(self) -> None:
        messages = []
        self.window._dxf_view = _FakePreviewView()
        self.window._set_preview_message_html = lambda title, detail="", is_error=False: messages.append((title, detail, is_error))

        self.window._on_preview_snapshot_completed(
            True,
            {
                "snapshot_id": "snapshot-100s",
                "snapshot_hash": "hash-100s",
                "plan_id": "plan-100s",
                "preview_source": "planned_glue_snapshot",
                "preview_kind": "glue_points",
                "segment_count": 2,
                "glue_point_count": 2,
                "glue_points": [
                    {"x": 0.0, "y": 0.0},
                    {"x": 10.0, "y": 0.0},
                ],
                "execution_polyline": [
                    {"x": 0.0, "y": 0.0},
                    {"x": 10.0, "y": 0.0},
                ],
                "total_length_mm": 10.0,
                "estimated_time_s": 0.5,
                "generated_at": "2026-03-29T00:00:00Z",
                "dry_run": False,
            },
            "",
        )

        self.assertEqual(messages, [])
        self.assertEqual(self.window._preview_source, "planned_glue_snapshot")
        self.assertEqual(self.window._current_plan_id, "plan-100s")
        self.assertEqual(self.window._current_plan_fingerprint, "hash-100s")
        self.assertIsNotNone(self.window._preview_gate.snapshot)
        self.assertEqual(self.window._preview_gate.snapshot.snapshot_hash, "hash-100s")
        self.assertEqual(self.window.statusBar().currentMessage(), "胶点预览已更新，启动前需确认")
        self.assertIn("规划胶点主预览", self.window._dxf_view.html)
        self.assertIn("hash-100s", self.window._dxf_view.html)

    def test_preview_snapshot_rejects_non_authoritative_preview_source(self) -> None:
        messages = []
        self.window._set_preview_message_html = lambda title, detail="", is_error=False: messages.append((title, detail, is_error))

        self.window._on_preview_snapshot_completed(
            True,
            {
                "snapshot_id": "snapshot-invalid-source",
                "snapshot_hash": "hash-invalid-source",
                "plan_id": "plan-invalid-source",
                "preview_source": "mock_synthetic",
                "preview_kind": "glue_points",
                "glue_point_count": 2,
                "glue_points": [
                    {"x": 0.0, "y": 0.0},
                    {"x": 10.0, "y": 0.0},
                ],
                "execution_polyline": [
                    {"x": 0.0, "y": 0.0},
                    {"x": 10.0, "y": 0.0},
                ],
            },
            "",
        )

        self.assertEqual(
            self.window._preview_gate.last_error_message,
            "当前预览来源为 mock_synthetic，不能作为真实在线预览通过依据",
        )
        self.assertTrue(messages)
        self.assertEqual(messages[-1][0], "胶点预览生成失败")
        self.assertIn("preview_source=mock_synthetic", messages[-1][1])
        self.assertTrue(messages[-1][2])

    def test_preview_snapshot_rejects_non_glue_points_preview_kind(self) -> None:
        messages = []
        self.window._set_preview_message_html = lambda title, detail="", is_error=False: messages.append((title, detail, is_error))

        self.window._on_preview_snapshot_completed(
            True,
            {
                "snapshot_id": "snapshot-invalid-kind",
                "snapshot_hash": "hash-invalid-kind",
                "plan_id": "plan-invalid-kind",
                "preview_source": "planned_glue_snapshot",
                "preview_kind": "trajectory_polyline",
                "glue_point_count": 2,
                "glue_points": [
                    {"x": 0.0, "y": 0.0},
                    {"x": 10.0, "y": 0.0},
                ],
                "execution_polyline": [
                    {"x": 0.0, "y": 0.0},
                    {"x": 10.0, "y": 0.0},
                ],
            },
            "",
        )

        self.assertEqual(
            self.window._preview_gate.last_error_message,
            "当前预览语义为 trajectory_polyline，不是 glue_points，不能作为真实在线预览通过依据",
        )
        self.assertTrue(messages)
        self.assertEqual(messages[-1][0], "胶点预览生成失败")
        self.assertIn("preview_kind=trajectory_polyline", messages[-1][1])
        self.assertTrue(messages[-1][2])

    def test_preview_snapshot_rejects_empty_authority_glue_points(self) -> None:
        messages = []
        self.window._set_preview_message_html = lambda title, detail="", is_error=False: messages.append((title, detail, is_error))

        self.window._on_preview_snapshot_completed(
            True,
            {
                "snapshot_id": "snapshot-empty",
                "snapshot_hash": "hash-empty",
                "plan_id": "plan-empty",
                "preview_source": "planned_glue_snapshot",
                "preview_kind": "glue_points",
                "glue_point_count": 0,
                "glue_points": [],
                "execution_polyline": [
                    {"x": 0.0, "y": 0.0},
                    {"x": 10.0, "y": 0.0},
                ],
            },
            "",
        )

        self.assertEqual(
            self.window._preview_gate.last_error_message,
            "当前预览缺少非空 glue_points，不能作为真实在线预览通过依据",
        )
        self.assertTrue(messages)
        self.assertEqual(messages[-1][0], "胶点预览生成失败")
        self.assertIn("缺少非空 glue_points", messages[-1][1])
        self.assertTrue(messages[-1][2])

    def test_preview_snapshot_reports_legacy_backend_contract_when_glue_points_missing(self) -> None:
        messages = []
        self.window._set_preview_message_html = lambda title, detail="", is_error=False: messages.append((title, detail, is_error))

        self.window._on_preview_snapshot_completed(
            True,
            {
                "snapshot_id": "snapshot-legacy",
                "snapshot_hash": "hash-legacy",
                "plan_id": "plan-legacy",
                "preview_source": "runtime_snapshot",
                "trajectory_polyline": [
                    {"x": 0.0, "y": 0.0},
                    {"x": 10.0, "y": 0.0},
                ],
            },
            "",
        )

        self.assertEqual(self.window._preview_gate.last_error_message, "运行时仍返回旧版轨迹预览契约")
        self.assertTrue(messages)
        self.assertEqual(messages[-1][0], "胶点预览生成失败")
        self.assertIn("当前 HMI 连接的 runtime-gateway 很可能还是旧构建", messages[-1][1])
        self.assertTrue(messages[-1][2])

    def test_preview_snapshot_worker_error_preserves_timeout_detail(self) -> None:
        messages = []
        self.window._set_preview_message_html = lambda title, detail="", is_error=False: messages.append((title, detail, is_error))

        self.window._on_preview_snapshot_completed(False, {}, "Request timed out (100.0s)")

        self.assertEqual(self.window._preview_gate.last_error_message, "Request timed out (100.0s)")
        self.assertTrue(messages)
        self.assertEqual(messages[-1][0], "胶点预览生成失败")
        self.assertEqual(messages[-1][1], "Request timed out (100.0s)")
        self.assertTrue(messages[-1][2])

    def test_preview_snapshot_worker_error_preserves_non_timeout_detail(self) -> None:
        messages = []
        self.window._set_preview_message_html = lambda title, detail="", is_error=False: messages.append((title, detail, is_error))

        self.window._on_preview_snapshot_completed(False, {}, "plan not found")

        self.assertEqual(self.window._preview_gate.last_error_message, "plan not found")
        self.assertTrue(messages)
        self.assertEqual(messages[-1][0], "胶点预览生成失败")
        self.assertEqual(messages[-1][1], "plan not found")
        self.assertTrue(messages[-1][2])


if __name__ == "__main__":
    unittest.main()
