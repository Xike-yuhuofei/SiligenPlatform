import os
import sys
import time
import unittest
from dataclasses import replace
from pathlib import Path


os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PyQt5.QtWidgets import QApplication, QWidget


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src" / "hmi_client"))

import ui.main_window as main_window_module
from client import SessionSnapshot, launch_result_from_snapshot
from client.protocol import AxisStatus, EffectiveInterlocks, IOStatus, MachineStatus, SupervisionStatus


class FakeProtocol:
    def __init__(self, status: MachineStatus):
        self.status = status
        self.status_calls = 0
        self.home_calls = []
        self.home_auto_calls = []
        self.jog_calls = []
        self.stop_calls = 0
        self.emergency_stop_calls = 0
        self.estop_reset_calls = 0
        self.start_job_calls = []
        self.emergency_stop_result = (True, "E-Stop")
        self.estop_reset_result = (True, "E-Stop reset")
        self.start_job_response = (
            True,
            {
                "job_id": "job-1",
                "performance_profile": {
                    "execution_cache_hit": False,
                    "execution_joined_inflight": False,
                    "execution_wait_ms": 0,
                    "motion_plan_ms": 0,
                    "assembly_ms": 0,
                    "export_ms": 0,
                    "execution_total_ms": 0,
                },
            },
            "",
        )

    def get_status(self) -> MachineStatus:
        self.status_calls += 1
        return self.status

    def get_alarms(self) -> list:
        return []

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

    def emergency_stop(self):
        self.emergency_stop_calls += 1
        return self.emergency_stop_result

    def estop_reset(self):
        self.estop_reset_calls += 1
        return self.estop_reset_result

    def dxf_start_job(self, plan_id: str, target_count: int = 1, plan_fingerprint: str = ""):
        self.start_job_calls.append((plan_id, target_count, plan_fingerprint))
        return self.start_job_response

    def get_alarms(self):
        return []


class _FakePreviewView:
    def __init__(self) -> None:
        self.html = ""

    def setHtml(self, html: str) -> None:
        self.html = html


class _CancellableWorker:
    def __init__(self) -> None:
        self.cancel_called = False

    def cancel(self) -> None:
        self.cancel_called = True


class _FakeSignal:
    def __init__(self) -> None:
        self._callbacks = []

    def connect(self, callback) -> None:
        self._callbacks.append(callback)

    def emit(self, *args, **kwargs) -> None:
        for callback in list(self._callbacks):
            callback(*args, **kwargs)


class _FakeHomeAutoWorker:
    instances = []

    def __init__(
        self,
        *,
        host: str,
        port: int,
        axes,
        force_rehome: bool,
        timeout_ms: int = 0,
    ) -> None:
        self.host = host
        self.port = port
        self.axes = list(axes) if axes else None
        self.force_rehome = force_rehome
        self.timeout_ms = timeout_ms
        self.completed = _FakeSignal()
        self.finished = _FakeSignal()
        self.start_called = False
        self.cancel_called = False
        self.delete_later_called = False
        self.wait_calls = []
        self._running = False
        type(self).instances.append(self)

    def start(self) -> None:
        self.start_called = True
        self._running = True

    def cancel(self) -> None:
        self.cancel_called = True
        self._running = False

    def isRunning(self) -> bool:
        return self._running

    def wait(self, timeout_ms: int = 0) -> bool:
        self.wait_calls.append(timeout_ms)
        self._running = False
        return True

    def deleteLater(self) -> None:
        self.delete_later_called = True

    def complete(self, ok: bool, message: str) -> None:
        self.completed.emit(ok, message)
        self._running = False
        self.finished.emit()


class MainWindowTabsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self) -> None:
        self._original_web_view = getattr(main_window_module, "QWebEngineView", None)
        self._original_web_engine_flag = getattr(main_window_module, "WEB_ENGINE_AVAILABLE", False)
        self._original_home_auto_worker = getattr(main_window_module, "HomeAutoWorker")
        main_window_module.QWebEngineView = None
        main_window_module.WEB_ENGINE_AVAILABLE = False
        _FakeHomeAutoWorker.instances = []
        main_window_module.HomeAutoWorker = _FakeHomeAutoWorker
        self.window = main_window_module.MainWindow(launch_mode="offline")
        QApplication.clipboard().clear()

    def tearDown(self) -> None:
        self.window.close()
        self.window.deleteLater()
        main_window_module.QWebEngineView = self._original_web_view
        main_window_module.WEB_ENGINE_AVAILABLE = self._original_web_engine_flag
        main_window_module.HomeAutoWorker = self._original_home_auto_worker

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

    def _set_launch_connectivity(self, *, connected: bool, hardware_connected: bool | None = None) -> None:
        ui_state = self.window._launch_ui_state
        self.assertIsNotNone(ui_state)
        if hardware_connected is None:
            hardware_connected = connected
        assert ui_state is not None
        self.window._launch_ui_state = replace(
            ui_state,
            connected=connected,
            hardware_connected=hardware_connected,
        )

    def _arm_confirmed_preview(
        self,
        *,
        preview_source: str = "planned_glue_snapshot",
        preview_kind: str = "glue_points",
        glue_points=None,
        plan_id: str = "plan-1",
        snapshot_hash: str = "hash-1",
        preview_validation_classification: str = "pass",
        preview_exception_reason: str = "",
        preview_failure_reason: str = "",
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
                "motion_preview": {
                    "source": "execution_trajectory_snapshot",
                    "kind": "polyline",
                    "source_point_count": 8,
                    "point_count": 2,
                    "is_sampled": True,
                    "sampling_strategy": "fixed_spacing_corner_preserving",
                    "polyline": [
                        {"x": 0.0, "y": 0.0},
                        {"x": 12.0, "y": 3.0},
                    ],
                },
                "preview_validation_classification": preview_validation_classification,
                "preview_exception_reason": preview_exception_reason,
                "preview_failure_reason": preview_failure_reason,
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

    def test_alarm_panel_exposes_status_log_view_and_copy_action(self) -> None:
        testids = self._collect_testids()

        self.assertIn("status-log-view", testids)
        self.assertIn("btn-status-log-copy", testids)

    def test_status_bar_messages_are_recorded_in_alarm_panel(self) -> None:
        self.window.statusBar().showMessage("测试状态栏归档")
        QApplication.processEvents()

        entries = self.window._status_log_view.toPlainText().splitlines()

        self.assertTrue(any(line.endswith("状态初始化中") for line in entries))
        self.assertTrue(any(line.endswith("Offline 模式已启用，本次启动不会尝试连接 gateway。") for line in entries))
        self.assertFalse(any(line.endswith("系统就绪") for line in entries))
        self.assertTrue(any(line.endswith("测试状态栏归档") for line in entries))

    def test_copy_status_log_copies_recorded_messages(self) -> None:
        self.window.statusBar().showMessage("复制测试消息")
        QApplication.processEvents()

        self.window._on_copy_status_log()
        QApplication.processEvents()
        copied = QApplication.clipboard().text()

        self.assertIn("状态初始化中", copied)
        self.assertIn("Offline 模式已启用，本次启动不会尝试连接 gateway。", copied)
        self.assertNotIn("系统就绪", copied)
        self.assertIn("复制测试消息", copied)

    def test_offline_window_uses_snapshot_backed_launch_result(self) -> None:
        self.assertIsNotNone(self.window._launch_result)
        assert self.window._launch_result is not None
        self.assertIsNotNone(self.window._launch_result.session_snapshot)
        assert self.window._launch_result.session_snapshot is not None
        self.assertEqual(self.window._launch_result.session_snapshot.mode, "offline")
        self.assertEqual(self.window._launch_result.session_snapshot.session_state, "idle")

    def test_online_window_does_not_report_system_ready_before_first_snapshot(self) -> None:
        original_auto_startup = main_window_module.MainWindow._auto_startup
        main_window_module.MainWindow._auto_startup = lambda self: None
        online_window = None
        try:
            online_window = main_window_module.MainWindow(launch_mode="online")
            self.assertIsNone(online_window._launch_result)
            self.assertIsNone(online_window._session_snapshot)
            self.assertEqual(online_window._operation_status.text(), "启动中")
            self.assertEqual(online_window.statusBar().currentMessage(), "启动中，等待 Supervisor 首个快照")
        finally:
            if online_window is not None:
                online_window.close()
                online_window.deleteLater()
            main_window_module.MainWindow._auto_startup = original_auto_startup

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

    def test_on_home_starts_background_worker_and_updates_status_on_completion(self) -> None:
        status = self._make_status(x_homed=True, y_homed=True)
        fake_protocol = FakeProtocol(status)
        self.window._protocol = fake_protocol
        self.window._check_home_preconditions = lambda: True

        self.window._on_home(["X", "Y"])
        worker = _FakeHomeAutoWorker.instances[-1]

        self.assertEqual(fake_protocol.home_calls, [])
        self.assertEqual(fake_protocol.home_auto_calls, [])
        self.assertEqual(len(_FakeHomeAutoWorker.instances), 1)
        self.assertTrue(worker.start_called)
        self.assertTrue(worker.isRunning())
        self.assertEqual(worker.host, self.window._client.host)
        self.assertEqual(worker.port, self.window._client.port)
        self.assertEqual(worker.axes, ["X", "Y"])
        self.assertFalse(worker.force_rehome)
        self.assertEqual(self.window.statusBar().currentMessage(), "回零进行中...")

        worker.complete(True, "Axes ready at zero")

        self.assertIsNone(self.window._home_auto_worker)
        self.assertFalse(worker.isRunning())
        self.assertTrue(worker.delete_later_called)
        self.assertEqual(self.window.statusBar().currentMessage(), "回零: Axes ready at zero")

    def test_on_home_completion_requalifies_recoverable_runtime_failure(self) -> None:
        status = self._make_status(x_homed=True, y_homed=True)
        fake_protocol = FakeProtocol(status)
        failed_snapshot = SessionSnapshot(
            mode="online",
            session_state="failed",
            backend_state="ready",
            tcp_state="ready",
            hardware_state="failed",
            failure_code="SUP_HARDWARE_CONNECT_FAILED",
            failure_stage="hardware_ready",
            recoverable=True,
            last_error_message="运行中硬件状态不可用，在线能力已收敛。",
            updated_at="2026-04-01T07:58:49Z",
        )
        self.window._requested_launch_mode = "online"
        self.window._protocol = fake_protocol
        self.window._session_snapshot = failed_snapshot
        self.window._launch_result = launch_result_from_snapshot("online", failed_snapshot)
        self.window._home_request_generation = 1

        self.window._on_home_auto_completed(True, "Axes ready at zero", request_token=1)

        self.assertEqual(fake_protocol.status_calls, 1)
        self.assertTrue(self.window._is_online_ready())
        self.assertIsNotNone(self.window._launch_result)
        assert self.window._launch_result is not None
        self.assertTrue(self.window._launch_result.online_ready)
        self.assertIsNone(self.window._launch_result.failure_code)
        self.assertEqual(self.window.statusBar().currentMessage(), "回零: Axes ready at zero")
        self.assertTrue(
            any(
                getattr(event, "event_type", "") == "stage_succeeded"
                and getattr(event, "stage", "") == "online_ready"
                for event in self.window._supervisor_stage_events
            )
        )

    def test_on_home_rejects_duplicate_request_while_worker_is_running(self) -> None:
        status = self._make_status(x_homed=False, y_homed=True)
        fake_protocol = FakeProtocol(status)
        self.window._protocol = fake_protocol
        self.window._check_home_preconditions = lambda: True

        self.window._on_home(["X"])
        worker = _FakeHomeAutoWorker.instances[-1]
        self.window._on_home(["Y"])

        self.assertEqual(fake_protocol.home_calls, [])
        self.assertEqual(fake_protocol.home_auto_calls, [])
        self.assertEqual(len(_FakeHomeAutoWorker.instances), 1)
        self.assertTrue(worker.isRunning())
        self.assertEqual(self.window.statusBar().currentMessage(), "回零进行中，请稍候")

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

    def test_update_status_prefers_runtime_supervision_state_over_compat_machine_state(self) -> None:
        status = self._make_status()
        status.machine_state = "Idle"
        status.machine_state_reason = "idle"
        status.supervision.current_state = "Running"
        status.supervision.state_reason = "job_running"
        fake_protocol = FakeProtocol(status)
        self.window._protocol = fake_protocol
        self.window._is_online_ready = lambda: True

        self.window._update_status()

        self.assertEqual(self.window._state_label.text(), "Running")

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

    def test_on_stop_cancels_active_home_worker_and_sends_stop(self) -> None:
        status = self._make_status()
        fake_protocol = FakeProtocol(status)
        self.window._protocol = fake_protocol
        self.window._check_home_preconditions = lambda: True
        self.window._on_home(["X"])

        worker = _FakeHomeAutoWorker.instances[-1]
        self.window._on_stop("manual")

        self.assertTrue(worker.cancel_called)
        self.assertIsNone(self.window._home_auto_worker)
        self.assertEqual(fake_protocol.stop_calls, 1)

    def test_on_estop_cancels_active_home_worker_and_uses_runtime_command_channel_gate(self) -> None:
        status = self._make_status()
        fake_protocol = FakeProtocol(status)
        self.window._protocol = fake_protocol
        self.window._check_home_preconditions = lambda: True
        self.window._require_runtime_command_channel = lambda capability: True
        self.window._on_home(["X"])

        worker = _FakeHomeAutoWorker.instances[-1]
        self.window._on_estop()

        self.assertTrue(worker.cancel_called)
        self.assertIsNone(self.window._home_auto_worker)
        self.assertEqual(fake_protocol.emergency_stop_calls, 1)
        self.assertEqual(self.window.statusBar().currentMessage(), "急停: E-Stop")

    def test_on_estop_ignores_stale_home_worker_completion(self) -> None:
        status = self._make_status()
        fake_protocol = FakeProtocol(status)
        self.window._protocol = fake_protocol
        self.window._check_home_preconditions = lambda: True
        self.window._require_runtime_command_channel = lambda capability: True
        self.window._on_home(["X"])

        worker = _FakeHomeAutoWorker.instances[-1]
        self.window._on_estop()
        worker.complete(True, "Axes ready at zero")

        self.assertEqual(fake_protocol.emergency_stop_calls, 1)
        self.assertEqual(self.window.statusBar().currentMessage(), "急停: E-Stop")

    def test_on_estop_reset_uses_runtime_command_channel_gate(self) -> None:
        status = self._make_status()
        fake_protocol = FakeProtocol(status)
        self.window._protocol = fake_protocol
        self.window._require_runtime_command_channel = lambda capability: True

        self.window._on_estop_reset()

        self.assertEqual(fake_protocol.estop_reset_calls, 1)
        self.assertEqual(self.window.statusBar().currentMessage(), "急停复位: E-Stop reset")

    def test_system_panel_does_not_render_home_buttons(self) -> None:
        testids = self._collect_testids()

        self.assertNotIn("btn-home-all", testids)
        self.assertNotIn("btn-home-X", testids)
        self.assertNotIn("btn-home-Y", testids)
        self.assertIn("btn-production-home-all", testids)

    def test_system_panel_renders_recovery_region(self) -> None:
        testids = self._collect_testids()

        self.assertIn("panel-session-recovery", testids)
        self.assertIn("btn-recovery-retry", testids)
        self.assertIn("btn-recovery-restart", testids)
        self.assertIn("btn-recovery-stop", testids)

    def test_runtime_hardware_degradation_enables_recovery_buttons(self) -> None:
        failed_snapshot = SessionSnapshot(
            mode="online",
            session_state="failed",
            backend_state="ready",
            tcp_state="ready",
            hardware_state="failed",
            failure_code="SUP_HARDWARE_CONNECT_FAILED",
            failure_stage="hardware_ready",
            recoverable=True,
            last_error_message="运行中硬件状态不可用，在线能力已收敛。",
            updated_at="2026-04-01T07:58:49Z",
        )
        self.window._requested_launch_mode = "online"
        self.window._session_snapshot = failed_snapshot
        self.window._launch_result = launch_result_from_snapshot("online", failed_snapshot)

        self.window._refresh_launch_status_ui()
        self.window._apply_mode_capabilities()

        self.assertTrue(self.window._retry_stage_btn.isEnabled())
        self.assertTrue(self.window._restart_session_btn.isEnabled())
        self.assertTrue(self.window._stop_session_btn.isEnabled())

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

    def test_render_runtime_preview_html_renders_exception_banner(self) -> None:
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
            preview_warning="",
            preview_validation_classification="pass_with_exception",
            preview_exception_reason="短闭环按例外保留，仍允许确认与启动。",
            preview_failure_reason="",
        )

        self.assertIn("非阻断提示", html)
        self.assertIn("短闭环按例外保留", html)

    def test_confirmation_summary_includes_pass_with_exception_reason(self) -> None:
        self._arm_confirmed_preview(
            preview_validation_classification="pass_with_exception",
            preview_exception_reason="短闭环按例外保留，仍允许确认与启动。",
        )

        summary = self.window._preview_session.build_confirmation_summary()

        self.assertIn("非阻断提示", summary)
        self.assertIn("短闭环按例外保留", summary)

    def test_check_production_preconditions_rejects_mock_preview_source(self) -> None:
        status = self._make_status(x_homed=True, y_homed=True)
        self.window._require_online_mode = lambda capability: True
        self.window._is_online_ready = lambda: True
        self.window._protocol = FakeProtocol(status)
        self._set_launch_connectivity(connected=True, hardware_connected=True)
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
        self._set_launch_connectivity(connected=True, hardware_connected=True)
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
        self._set_launch_connectivity(connected=True, hardware_connected=True)
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

    def test_check_production_preconditions_rejects_failed_preview_gate(self) -> None:
        status = self._make_status(x_homed=True, y_homed=True)
        self.window._require_online_mode = lambda capability: True
        self.window._is_online_ready = lambda: True
        self.window._protocol = FakeProtocol(status)
        self._set_launch_connectivity(connected=True, hardware_connected=True)
        self.window._runtime_status_fault = False
        self.window._preview_session.state.current_plan_id = "plan-failed"
        self.window._preview_session.state.current_plan_fingerprint = "hash-failed"
        self.window._preview_session.state.preview_plan_dry_run = False
        self.window._preview_session.state.preview_source = "planned_glue_snapshot"
        self.window._preview_session.state.preview_kind = "glue_points"
        self.window._preview_session.state.glue_point_count = 2
        self.window._preview_session.gate.preview_failed("preview authority is not shared with execution")
        self.window._sync_preview_session_fields()
        warnings = []
        self.window._show_preflight_warning = warnings.append

        result = self.window._check_production_preconditions(dry_run=False)

        self.assertFalse(result)
        self.assertEqual(
            warnings[-1],
            "胶点预览失败: preview authority is not shared with execution",
        )

    def test_check_production_preconditions_blocks_fail_preview_reason_from_metadata(self) -> None:
        status = self._make_status(x_homed=True, y_homed=True)
        self.window._require_online_mode = lambda capability: True
        self.window._is_online_ready = lambda: True
        self.window._protocol = FakeProtocol(status)
        self._set_launch_connectivity(connected=True, hardware_connected=True)
        self.window._runtime_status_fault = False
        self._arm_confirmed_preview(
            preview_validation_classification="fail",
            preview_failure_reason="authority layout missing for preview/execution share check",
        )
        warnings = []
        self.window._show_preflight_warning = warnings.append

        result = self.window._check_production_preconditions(dry_run=False)

        self.assertFalse(result)
        self.assertEqual(
            warnings[-1],
            "authority layout missing for preview/execution share check",
        )

    def test_start_production_logs_job_start_performance_profile(self) -> None:
        status = self._make_status(x_homed=True, y_homed=True)
        fake_protocol = FakeProtocol(status)
        fake_protocol.start_job_response = (
            True,
            {
                "job_id": "job-42",
                "performance_profile": {
                    "execution_cache_hit": True,
                    "execution_joined_inflight": False,
                    "execution_wait_ms": 3,
                    "motion_plan_ms": 12,
                    "assembly_ms": 8,
                    "export_ms": 2,
                    "execution_total_ms": 22,
                },
            },
            "",
        )
        self.window._protocol = fake_protocol
        self.window._require_online_mode = lambda capability: True
        self._set_launch_connectivity(connected=True, hardware_connected=True)
        self.window._runtime_status_fault = False
        self.window._dxf_loaded = True
        self.window._target_count = 2
        self.window._check_production_preconditions = lambda dry_run=False: True
        self._arm_confirmed_preview()
        self._set_cached_status(status)

        with self.assertLogs(main_window_module._UI_LOGGER.name, level="INFO") as captured:
            self.window._start_production_process(dry_run=False)

        self.assertEqual(fake_protocol.start_job_calls, [("plan-1", 2, "hash-1")])
        self.assertEqual(self.window._current_job_id, "job-42")
        log_text = "\n".join(captured.output)
        self.assertIn("job_start_performance_profile", log_text)
        self.assertIn("execution_total_ms=22", log_text)
        self.assertIn("job_started", log_text)
        self.window._production_running = False
        self.window._current_job_id = ""

    def test_preview_snapshot_success_renders_authority_payload_without_timeout_side_effects(self) -> None:
        messages = []
        self.window._dxf_view = _FakePreviewView()
        self.window._set_preview_message_html = lambda title, detail="", is_error=False: messages.append((title, detail, is_error))

        self.window._on_preview_snapshot_completed(
            True,
            {
                "snapshot_id": "snapshot-300s",
                "snapshot_hash": "hash-300s",
                "plan_id": "plan-300s",
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
                "generated_at": "2026-03-29T00:00:00Z",
                "dry_run": False,
            },
            "",
        )

        self.assertEqual(messages, [])
        self.assertEqual(self.window._preview_source, "planned_glue_snapshot")
        self.assertEqual(self.window._current_plan_id, "plan-300s")
        self.assertEqual(self.window._current_plan_fingerprint, "hash-300s")
        self.assertIsNotNone(self.window._preview_gate.snapshot)
        self.assertEqual(self.window._preview_gate.snapshot.snapshot_hash, "hash-300s")
        self.assertEqual(self.window.statusBar().currentMessage(), "胶点预览已更新，启动前需确认")
        self.assertIn("规划胶点主预览", self.window._dxf_view.html)
        self.assertIn("运动轨迹来源</td><td>执行轨迹快照</td>", self.window._dxf_view.html)
        self.assertIn("运动轨迹采样策略</td><td>fixed_spacing_corner_preserving</td>", self.window._dxf_view.html)
        self.assertIn("hash-300s", self.window._dxf_view.html)

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

        self.assertEqual(self.window._preview_gate.last_error_message, "旧版 runtime_snapshot 预览契约仍在返回")
        self.assertTrue(messages)
        self.assertEqual(messages[-1][0], "胶点预览生成失败")
        self.assertIn("当前 HMI 连接的 runtime-gateway 很可能还是旧构建", messages[-1][1])
        self.assertIn("plan_id=plan-legacy", messages[-1][1])
        self.assertTrue(messages[-1][2])

    def test_preview_snapshot_worker_error_preserves_timeout_detail(self) -> None:
        messages = []
        self.window._set_preview_message_html = lambda title, detail="", is_error=False: messages.append((title, detail, is_error))

        self.window._on_preview_snapshot_completed(False, {}, "Request timed out (300.0s)")

        self.assertEqual(self.window._preview_gate.last_error_message, "Request timed out (300.0s)")
        self.assertTrue(messages)
        self.assertEqual(messages[-1][0], "胶点预览生成失败")
        self.assertEqual(messages[-1][1], "Request timed out (300.0s)")
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

    def test_preview_snapshot_worker_error_preserves_binding_failure_detail(self) -> None:
        messages = []
        self.window._set_preview_message_html = lambda title, detail="", is_error=False: messages.append((title, detail, is_error))

        self.window._on_preview_snapshot_completed(False, {}, "authority trigger binding unavailable")

        self.assertEqual(
            self.window._preview_gate.last_error_message,
            "preview binding unavailable (authority trigger binding unavailable)",
        )
        self.assertTrue(messages)
        self.assertEqual(messages[-1][0], "胶点预览生成失败")
        self.assertEqual(
            messages[-1][1],
            "preview binding unavailable (authority trigger binding unavailable)",
        )
        self.assertTrue(messages[-1][2])

    def test_invalidate_preview_plan_cancels_active_worker(self) -> None:
        worker = _CancellableWorker()
        self.window._preview_snapshot_worker = worker
        self.window._preview_session.state.current_plan_id = "plan-1"
        self.window._preview_session.state.current_plan_fingerprint = "fp-1"
        self.window._set_preview_refresh_inflight(True)

        self.window._invalidate_preview_plan()

        self.assertTrue(worker.cancel_called)
        self.assertEqual(self.window._preview_session.state.current_plan_id, "")
        self.assertEqual(self.window._preview_session.state.current_plan_fingerprint, "")
        self.assertFalse(self.window._preview_session.state.preview_refresh_inflight)

    def test_mode_toggle_auto_regenerates_preview_when_runtime_preview_is_available(self) -> None:
        generated = []
        self.window._dxf_loaded = True
        self.window._dxf_artifact_id = "artifact-1"
        self._set_launch_connectivity(connected=True)
        self.window._dxf_view = _FakePreviewView()
        self.window._is_offline_mode = lambda: False
        main_window_module.WEB_ENGINE_AVAILABLE = True
        self.window._preview_session.state.current_plan_id = "plan-1"
        self.window._preview_session.state.current_plan_fingerprint = "fp-1"
        self.window._generate_dxf_preview = lambda: generated.append("preview")

        self.window._on_mode_toggled(True)

        self.assertEqual(generated, ["preview"])
        self.assertEqual(self.window._preview_session.state.current_plan_id, "")
        self.assertEqual(self.window._preview_session.state.current_plan_fingerprint, "")

    def test_mode_toggle_prompts_manual_refresh_when_auto_preview_is_unavailable(self) -> None:
        self.window._dxf_loaded = True
        self.window._dxf_artifact_id = ""
        self._set_launch_connectivity(connected=True)
        self.window._preview_session.state.current_plan_id = "plan-1"
        self.window._preview_session.state.current_plan_fingerprint = "fp-1"

        self.window._on_mode_toggled(True)

        self.assertEqual(self.window.statusBar().currentMessage(), "运行模式已变更，请刷新预览并重新确认")
        self.assertEqual(self.window._preview_session.state.current_plan_id, "")
        self.assertEqual(self.window._preview_session.state.current_plan_fingerprint, "")

    def test_preview_snapshot_ignores_stale_worker_completion(self) -> None:
        messages = []
        self.window._dxf_view = _FakePreviewView()
        self.window._set_preview_message_html = lambda title, detail="", is_error=False: messages.append((title, detail, is_error))
        self.window._preview_request_generation = 4
        self.window._set_preview_refresh_inflight(True)

        self.window._on_preview_snapshot_completed(
            True,
            {
                "snapshot_id": "snapshot-stale",
                "snapshot_hash": "hash-stale",
                "plan_id": "plan-stale",
                "preview_source": "planned_glue_snapshot",
                "preview_kind": "glue_points",
                "glue_point_count": 2,
                "glue_points": [{"x": 0.0, "y": 0.0}, {"x": 10.0, "y": 0.0}],
                "execution_polyline": [{"x": 0.0, "y": 0.0}, {"x": 10.0, "y": 0.0}],
            },
            "",
            request_token=3,
        )

        self.assertEqual(messages, [])
        self.assertEqual(self.window._current_plan_id, "")
        self.assertIsNone(self.window._preview_gate.snapshot)
        self.assertTrue(self.window._preview_session.state.preview_refresh_inflight)


if __name__ == "__main__":
    unittest.main()
