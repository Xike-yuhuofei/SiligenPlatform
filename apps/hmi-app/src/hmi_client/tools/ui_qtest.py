#!/usr/bin/env python3
"""Mode-aware PyQt5 GUI contract smoke test for Siligen HMI."""
from __future__ import annotations

import argparse
import os
import sys
import threading
import time
from contextlib import ExitStack, contextmanager
from pathlib import Path
from typing import Callable, Iterator, Optional, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from qt_env import configure_qt_environment

configure_qt_environment(headless=True)

from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtTest import QTest
from PyQt5.QtWidgets import QApplication, QWidget

import ui.main_window as main_window_module  # noqa: E402
from tools.mock_server import MockRequestHandler, MockState, MockTcpServer  # noqa: E402


MainWindow = main_window_module.MainWindow

EXIT_SUCCESS = 0
EXIT_ASSERTION_FAILED = 10
EXIT_TIMEOUT = 11


def find_by_testid(root: QWidget, testid: str) -> Optional[QWidget]:
    if root.property("data-testid") == testid:
        return root
    for widget in root.findChildren(QWidget):
        if widget.property("data-testid") == testid:
            return widget
    return None


@contextmanager
def patch_launch_endpoints(host: str, port: int) -> Iterator[None]:
    original_tcp_client = main_window_module.TcpClient
    original_backend_manager = main_window_module.BackendManager
    main_window_module.TcpClient = lambda: original_tcp_client(host=host, port=port)
    main_window_module.BackendManager = lambda: original_backend_manager(host=host, port=port)
    try:
        yield
    finally:
        main_window_module.TcpClient = original_tcp_client
        main_window_module.BackendManager = original_backend_manager
@contextmanager
def patch_headless_preview() -> Iterator[None]:
    original_web_view = getattr(main_window_module, "QWebEngineView", None)
    original_flag = getattr(main_window_module, "WEB_ENGINE_AVAILABLE", False)
    main_window_module.QWebEngineView = None
    main_window_module.WEB_ENGINE_AVAILABLE = False
    try:
        yield
    finally:
        main_window_module.QWebEngineView = original_web_view
        main_window_module.WEB_ENGINE_AVAILABLE = original_flag


@contextmanager
def patch_modal_dialogs() -> Iterator[None]:
    original_critical = main_window_module.QMessageBox.critical

    def _critical_noop(*_args, **_kwargs):
        return main_window_module.QMessageBox.Ok

    main_window_module.QMessageBox.critical = _critical_noop
    try:
        yield
    finally:
        main_window_module.QMessageBox.critical = original_critical


def start_mock_server(host: str, port: int, verbose: bool) -> Tuple[MockTcpServer, int]:
    state = MockState(seed_alarms=True)
    server = MockTcpServer((host, port), MockRequestHandler, state, verbose)
    server.daemon_threads = True
    if hasattr(server, "block_on_close"):
        server.block_on_close = False
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server, server.server_address[1]


class GuiContractRunner:
    def __init__(
        self,
        app: QApplication,
        window: MainWindow,
        launch_mode: str,
        timeout_ms: int,
        expect_failure_code: str = "",
        expect_failure_stage: str = "",
        exercise_recovery: bool = False,
    ):
        self.app = app
        self.window = window
        self.launch_mode = launch_mode
        self.timeout_ms = timeout_ms
        self.expect_failure_code = expect_failure_code.strip()
        self.expect_failure_stage = expect_failure_stage.strip()
        self.exercise_recovery = exercise_recovery
        self.failed = False
        self.timed_out = False
        self.exit_code = EXIT_SUCCESS

    def _fail(self, message: str) -> None:
        self.failed = True
        print(f"FAIL: {message}", flush=True)

    def _expect(self, condition: bool, message: str) -> None:
        if not condition:
            self._fail(message)

    def request_timeout(self) -> None:
        if self.timed_out:
            return
        self.timed_out = True
        self.exit_code = EXIT_TIMEOUT
        print("FAIL: Timed out waiting for GUI contract completion", flush=True)
        self.app.exit(EXIT_TIMEOUT)

    def _wait_for(self, description: str, predicate: Callable[[], bool], timeout_ms: Optional[int] = None) -> bool:
        deadline = time.monotonic() + ((timeout_ms or self.timeout_ms) / 1000.0)
        while time.monotonic() < deadline:
            if predicate():
                return True
            QTest.qWait(50)
        self._fail(f"Timed out waiting for {description}")
        return False

    def _widget(self, testid: str) -> Optional[QWidget]:
        return find_by_testid(self.window, testid)

    def _require_widget(self, testid: str) -> Optional[QWidget]:
        widget = self._widget(testid)
        self._expect(widget is not None, f"Missing widget: {testid}")
        return widget

    def _label_text(self, testid: str) -> str:
        widget = self._require_widget(testid)
        if widget is None:
            return ""
        text = widget.text() if hasattr(widget, "text") else ""
        self._expect(isinstance(text, str), f"Widget does not expose text(): {testid}")
        return text

    def _status_message(self) -> str:
        return self.window.statusBar().currentMessage()

    def _emit_supervisor_diag(self) -> None:
        result = self.window._launch_result
        if result is None:
            print(
                "SUPERVISOR_DIAG "
                "mode=unknown "
                "session_state=none "
                "backend_state=unknown "
                "tcp_state=unknown "
                "hardware_state=unknown "
                "failure_code=null "
                "failure_stage=null "
                "recoverable=false "
                "online_ready=false",
                flush=True,
            )
            return
        snapshot = result.session_snapshot
        mode = snapshot.mode if snapshot is not None else result.effective_mode
        session_state = snapshot.session_state if snapshot is not None else result.session_state
        backend_state = snapshot.backend_state if snapshot is not None else ("ready" if result.backend_started else "stopped")
        tcp_state = snapshot.tcp_state if snapshot is not None else ("ready" if result.tcp_connected else "disconnected")
        hardware_state = snapshot.hardware_state if snapshot is not None else ("ready" if result.hardware_ready else "unavailable")
        failure_code = snapshot.failure_code if snapshot is not None else result.failure_code
        failure_stage = snapshot.failure_stage if snapshot is not None else result.failure_stage
        recoverable = snapshot.recoverable if snapshot is not None else result.recoverable
        online_ready = result.online_ready
        print(
            "SUPERVISOR_DIAG "
            f"mode={mode} "
            f"session_state={session_state} "
            f"backend_state={backend_state} "
            f"tcp_state={tcp_state} "
            f"hardware_state={hardware_state} "
            f"failure_code={failure_code or 'null'} "
            f"failure_stage={failure_stage or 'null'} "
            f"recoverable={str(bool(recoverable)).lower()} "
            f"online_ready={str(bool(online_ready)).lower()}",
            flush=True,
        )

    def _emit_supervisor_stage_events(self) -> None:
        events = getattr(self.window, "_supervisor_stage_events", None) or []
        for event in events:
            failure_code = getattr(event, "failure_code", None) or "null"
            recoverable = getattr(event, "recoverable", None)
            if recoverable is None:
                recoverable_text = "null"
            else:
                recoverable_text = str(bool(recoverable)).lower()
            message = getattr(event, "message", None) or "null"
            message = str(message).replace(" ", "_")
            print(
                "SUPERVISOR_EVENT "
                f"type={getattr(event, 'event_type', 'unknown')} "
                f"session_id={getattr(event, 'session_id', 'unknown')} "
                f"stage={getattr(event, 'stage', 'unknown')} "
                f"failure_code={failure_code} "
                f"recoverable={recoverable_text} "
                f"message={message} "
                f"timestamp={getattr(event, 'timestamp', 'unknown')}",
                flush=True,
            )

    def _assert_gui_shell_present(self) -> None:
        print("STEP: gui shell", flush=True)
        self._expect(self.window.isVisible(), "Main window should be visible")
        for testid in (
            "indicator-global-status",
            "label-launch-mode",
            "label-tcp-state",
            "label-hw-state",
            "main-tabs",
            "tab-production",
            "tab-setup",
            "panel-system",
            "panel-motion-control",
            "panel-dispenser-control",
            "panel-alarms",
        ):
            self._require_widget(testid)

        tabs = self._require_widget("main-tabs")
        if tabs is not None and hasattr(tabs, "count"):
            self._expect(tabs.count() >= 4, "Main tab container should expose the core workspaces")

    def _assert_offline_contract(self) -> None:
        print("STEP: offline contract", flush=True)
        self._wait_for("offline launch result", lambda: self.window._launch_result is not None)
        result = self.window._launch_result
        self._expect(result is not None, "Offline launch result should be available")
        if result is not None:
            self._expect(result.effective_mode == "offline", "Effective mode should stay offline")
            self._expect(result.success, "Offline launch should succeed")
            if result.session_snapshot is not None:
                self._expect(result.session_snapshot.mode == "offline", "Offline snapshot mode should be offline")
                self._expect(result.session_snapshot.failure_code is None, "Offline snapshot should not carry failure code")

        self._expect(self._label_text("label-launch-mode") == "Offline", "Launch mode label should read Offline")
        self._expect(self._label_text("label-tcp-state") == "未连接", "Offline TCP label should stay disconnected")
        self._expect(self._label_text("label-hw-state") == "不可用", "Offline hardware label should stay unavailable")
        self._expect(self._label_text("label-current-operation") == "离线模式", "Offline operation status should be explicit")

        for testid in ("tab-production", "panel-system", "panel-motion-control", "panel-dispenser-control"):
            widget = self._require_widget(testid)
            if widget is not None:
                self._expect(not widget.isEnabled(), f"{testid} should be disabled offline")

        self.window._on_alarm_clear()
        QTest.qWait(50)
        self._expect(
            self._status_message().startswith("Offline 模式下不可用"),
            "Offline guard should block an online-only action",
        )
        self._expect("已连接" not in self._status_message(), "Offline action should not report a connected state")

    def _assert_online_contract(self) -> None:
        print("STEP: online contract", flush=True)
        self._wait_for("online launch result", lambda: self.window._launch_result is not None)
        if self.expect_failure_code:
            self._wait_for(
                f"expected failure_code={self.expect_failure_code}",
                lambda: bool(self.window._launch_result)
                and (self.window._launch_result.failure_code == self.expect_failure_code),
            )
            result = self.window._launch_result
            self._expect(result is not None, "Online failure launch result should be available")
            if result is not None:
                self._expect(not result.online_ready, "Expected failure path must not be online_ready")
                self._expect(result.failure_code == self.expect_failure_code, "Unexpected failure_code")
                if self.expect_failure_stage:
                    self._expect(
                        result.failure_stage == self.expect_failure_stage,
                        f"Unexpected failure_stage, expect {self.expect_failure_stage}",
                    )
            self._expect(self._label_text("label-launch-mode") == "Online", "Launch mode label should read Online")
            return

        self._wait_for(
            "online ready snapshot",
            lambda: bool(self.window._launch_result) and bool(self.window._launch_result.online_ready),
        )
        result = self.window._launch_result
        self._expect(result is not None, "Online launch result should be available")
        if result is not None:
            self._expect(result.success, "Online launch should succeed with the mock server")
            self._expect(result.phase == "ready", "Online launch should reach the ready phase")
            self._expect(result.online_ready, "Online launch should report online_ready")
            self._expect(result.tcp_connected, "Online launch should connect TCP")
            self._expect(result.hardware_ready, "Online launch should initialize hardware")
            self._expect(result.failure_code is None, "Successful online launch should not carry failure_code")
            self._expect(result.failure_stage is None, "Successful online launch should not carry failure_stage")

        self._expect(self._label_text("label-launch-mode") == "Online", "Launch mode label should read Online")
        self._expect(self._label_text("label-tcp-state") == "已连接", "Online TCP label should report connected")
        self._expect(self._label_text("label-hw-state") == "就绪", "Online hardware label should report ready")
        self._expect(self._label_text("label-current-operation") == "空闲", "Online operation status should settle to idle")

        for testid in ("tab-production", "panel-system", "panel-motion-control", "panel-dispenser-control"):
            widget = self._require_widget(testid)
            if widget is not None:
                self._expect(widget.isEnabled(), f"{testid} should remain enabled online")

        alarm_list = self._require_widget("list-alarms")
        if alarm_list is not None:
            self._wait_for("mock alarms to populate", lambda: alarm_list.count() > 0, timeout_ms=3000)
            before = alarm_list.count()
            self._expect(before > 0, "Mock server should seed at least one alarm")
            self.window._on_alarm_clear()
            self._wait_for("alarm clear action", lambda: alarm_list.count() == 0, timeout_ms=3000)
            self._expect(alarm_list.count() == 0, "Online guarded alarm action should be allowed")

        self._expect(
            not self._status_message().startswith("Offline 模式下不可用"),
            "Online actions should not be blocked by the offline guard",
        )
        if self.exercise_recovery:
            self._assert_recovery_cycle()

    def _assert_recovery_cycle(self) -> None:
        print("STEP: recovery cycle", flush=True)
        self.window._apply_runtime_degradation(
            failure_code="SUP_TCP_CONNECT_FAILED",
            failure_stage="tcp_ready",
            message="Injected recovery cycle failure",
            tcp_state="failed",
            hardware_state="unavailable",
        )
        self._wait_for(
            "degraded launch snapshot",
            lambda: bool(self.window._launch_result)
            and (self.window._launch_result.failure_code == "SUP_TCP_CONNECT_FAILED"),
        )

        degraded = self.window._launch_result
        self._expect(degraded is not None, "Recovery scenario should produce a degraded launch result")
        if degraded is not None:
            self._expect(not degraded.online_ready, "Degraded launch result must not stay online_ready")
            self._expect(degraded.failure_stage == "tcp_ready", "Runtime degradation should map to tcp_ready stage")

        retry_btn = self._require_widget("btn-recovery-retry")
        restart_btn = self._require_widget("btn-recovery-restart")
        stop_btn = self._require_widget("btn-recovery-stop")
        if retry_btn is not None:
            self._expect(retry_btn.isEnabled(), "Retry-stage recovery button should be enabled after recoverable failure")
        if restart_btn is not None:
            self._expect(restart_btn.isEnabled(), "Restart-session recovery button should be enabled after recoverable failure")
        if stop_btn is not None:
            self._expect(stop_btn.isEnabled(), "Stop-session recovery button should be enabled after recoverable failure")
        if retry_btn is None:
            return

        QTest.mouseClick(retry_btn, Qt.LeftButton)
        self._wait_for(
            "online ready after retry-stage recovery",
            lambda: bool(self.window._launch_result) and bool(self.window._launch_result.online_ready),
            timeout_ms=8000,
        )
        recovered = self.window._launch_result
        self._expect(recovered is not None, "Recovered launch result should be available")
        if recovered is not None:
            self._expect(recovered.online_ready, "Retry-stage recovery should return system to online_ready")
            self._expect(recovered.failure_code is None, "Recovered launch result must clear failure_code")
            self._expect(recovered.failure_stage is None, "Recovered launch result must clear failure_stage")

        events = getattr(self.window, "_supervisor_stage_events", None) or []
        self._expect(
            any(
                getattr(event, "event_type", "") == "stage_failed"
                and getattr(event, "stage", "") == "tcp_ready"
                and getattr(event, "failure_code", "") == "SUP_TCP_CONNECT_FAILED"
                for event in events
            ),
            "Recovery cycle should emit stage_failed for runtime tcp_ready degradation",
        )
        self._expect(
            any(getattr(event, "event_type", "") == "recovery_invoked" for event in events),
            "Recovery cycle should emit recovery_invoked supervisor event",
        )
        self._expect(
            any(
                getattr(event, "event_type", "") == "stage_succeeded"
                and getattr(event, "stage", "") == "online_ready"
                for event in events
            ),
            "Recovery cycle should emit stage_succeeded for online_ready",
        )

    def run(self) -> None:
        try:
            self._assert_gui_shell_present()
            if self.launch_mode == "offline":
                self._assert_offline_contract()
            else:
                self._assert_online_contract()
        finally:
            self._emit_supervisor_stage_events()
            self._emit_supervisor_diag()
            if hasattr(self.window, "_timer"):
                self.window._timer.stop()
            if getattr(self.window, "_client", None) is not None:
                self.window._client.disconnect()
            self.window.close()
            if self.timed_out:
                self.exit_code = EXIT_TIMEOUT
                self.app.exit(EXIT_TIMEOUT)
            else:
                self.exit_code = EXIT_ASSERTION_FAILED if self.failed else EXIT_SUCCESS
                self.app.exit(self.exit_code)


def build_window(launch_mode: str, host: str, port: int) -> MainWindow:
    with ExitStack() as stack:
        stack.enter_context(patch_headless_preview())
        stack.enter_context(patch_modal_dialogs())
        if launch_mode == "online":
            stack.enter_context(patch_launch_endpoints(host, port))
        window = MainWindow(launch_mode=launch_mode)
        # Startup failures may trigger modal error dialogs; disable them in smoke runs.
        if hasattr(window, "_show_startup_error"):
            window._show_startup_error = lambda *_args, **_kwargs: None
    return window


def main() -> int:
    parser = argparse.ArgumentParser(description="Run mode-aware GUI contract smoke tests")
    parser.add_argument("--mode", choices=("offline", "online"), default="offline")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0, help="0 to auto-pick a free port when mock mode is enabled")
    parser.add_argument("--no-mock", action="store_true", help="Use an existing TCP server for online mode")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--timeout-ms", type=int, default=15000)
    parser.add_argument("--expect-failure-code", default="")
    parser.add_argument("--expect-failure-stage", default="")
    parser.add_argument("--exercise-recovery", action="store_true")
    args = parser.parse_args()

    server: Optional[MockTcpServer] = None
    port = args.port
    if args.mode == "online" and not args.no_mock:
        server, port = start_mock_server(args.host, args.port, args.verbose)

    app = QApplication([])
    window = build_window(args.mode, args.host, port)
    window.show()

    runner = GuiContractRunner(
        app,
        window,
        args.mode,
        args.timeout_ms,
        expect_failure_code=args.expect_failure_code,
        expect_failure_stage=args.expect_failure_stage,
        exercise_recovery=args.exercise_recovery,
    )
    QTimer.singleShot(0, runner.run)
    QTimer.singleShot(args.timeout_ms, runner.request_timeout)
    app.exec_()

    if server is not None:
        server.shutdown()
        server.server_close()

    return runner.exit_code


if __name__ == "__main__":
    raise SystemExit(main())
