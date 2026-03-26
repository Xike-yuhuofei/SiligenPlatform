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
            trajectory_points=[(0.0, 0.0), (6.0, 0.0), (12.0, 3.0)],
        )

        self.assertIn("当前为 Mock 模拟轨迹", html)
        self.assertIn("非真实几何", html)
        self.assertIn("来源</td><td>Mock模拟</td>", html)


if __name__ == "__main__":
    unittest.main()
