#!/usr/bin/env python3
"""PyQt5 QTest UI smoke test for Siligen HMI."""
from __future__ import annotations

import argparse
import sys
import threading
from pathlib import Path
from typing import Optional, Tuple

from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtTest import QTest
from PyQt5.QtWidgets import QApplication, QDialog, QWidget

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ui.main_window import MainWindow, LoginDialog  # noqa: E402
from tools.mock_server import MockState, MockTcpServer, MockRequestHandler  # noqa: E402


def find_by_testid(root: QWidget, testid: str) -> Optional[QWidget]:
    if root.property("data-testid") == testid:
        return root
    for widget in root.findChildren(QWidget):
        if widget.property("data-testid") == testid:
            return widget
    return None


def set_spinbox_value(spinbox, value: str):
    line = spinbox.lineEdit()
    line.setFocus()
    line.selectAll()
    QTest.keyClicks(line, value)
    QTest.keyClick(line, Qt.Key_Enter)


class UiTestRunner:
    def __init__(self, app: QApplication, window: MainWindow, host: str, port: int):
        self.app = app
        self.window = window
        self.host = host
        self.port = port
        self.failed = False

    def _fail(self, message: str):
        self.failed = True
        print(f"FAIL: {message}")

    def _expect(self, condition: bool, message: str):
        if not condition:
            self._fail(message)

    def _activate_tab(self, title: str):
        tabs = find_by_testid(self.window, "main-tabs")
        self._expect(tabs is not None, "Main tabs missing")
        if not tabs:
            return
        for i in range(tabs.count()):
            if tabs.tabText(i) == title:
                tabs.setCurrentIndex(i)
                QTest.qWait(100)
                return
        self._fail(f"Tab '{title}' not found")

    def _exercise_login_dialog(self, username: str, pin: str):
        dialog = LoginDialog(self.window)
        dialog.show()
        QTest.qWait(100)
        user_input = find_by_testid(dialog, "input-username")
        pin_input = find_by_testid(dialog, "input-password")
        login_btn = find_by_testid(dialog, "btn-login")
        self._expect(all([user_input, pin_input, login_btn]), "Login dialog widgets missing")
        if user_input and pin_input and login_btn:
            user_input.setText(username)
            pin_input.setText(pin)
            QTest.mouseClick(login_btn, Qt.LeftButton)
            QTest.qWait(100)
            self._expect(dialog.result() == QDialog.Accepted, "Login dialog not accepted")
        dialog.close()

    def _connect(self):
        self._activate_tab("设置")
        self.window._client.host = self.host
        self.window._client.port = self.port
        self.window._on_connect()
        QTest.qWait(200)

    def _exercise_motion(self):
        self._activate_tab("设置")
        move_x = find_by_testid(self.window, "input-move-X")
        move_y = find_by_testid(self.window, "input-move-Y")
        move_speed = find_by_testid(self.window, "input-move-speed")
        move_btn = find_by_testid(self.window, "btn-move-to-position")
        self._expect(all([move_x, move_y, move_speed, move_btn]), "Move widgets missing")
        set_spinbox_value(move_x, "12.3")
        set_spinbox_value(move_y, "-4.5")
        set_spinbox_value(move_speed, "20")
        QTest.mouseClick(move_btn, Qt.LeftButton)

        jog_btn = find_by_testid(self.window, "btn-jog-X-plus")
        self._expect(jog_btn is not None, "Jog button missing")
        QTest.mousePress(jog_btn, Qt.LeftButton)
        QTest.qWait(100)
        QTest.mouseRelease(jog_btn, Qt.LeftButton)

    def _exercise_dispenser(self):
        self._activate_tab("设置")
        disp_count = find_by_testid(self.window, "input-dispenser-count")
        disp_interval = find_by_testid(self.window, "input-dispenser-interval")
        disp_duration = find_by_testid(self.window, "input-dispenser-duration")
        disp_start = find_by_testid(self.window, "btn-dispenser-start")
        disp_pause = find_by_testid(self.window, "btn-dispenser-pause")
        disp_resume = find_by_testid(self.window, "btn-dispenser-resume")
        disp_stop = find_by_testid(self.window, "btn-dispenser-stop")
        self._expect(
            all([disp_count, disp_interval, disp_duration, disp_start, disp_pause, disp_resume, disp_stop]),
            "Dispenser widgets missing",
        )
        set_spinbox_value(disp_count, "2")
        set_spinbox_value(disp_interval, "200")
        set_spinbox_value(disp_duration, "120")
        QTest.mouseClick(disp_start, Qt.LeftButton)
        QTest.mouseClick(disp_pause, Qt.LeftButton)
        QTest.mouseClick(disp_resume, Qt.LeftButton)
        QTest.mouseClick(disp_stop, Qt.LeftButton)

        supply_open = find_by_testid(self.window, "btn-supply-open")
        supply_close = find_by_testid(self.window, "btn-supply-close")
        self._expect(all([supply_open, supply_close]), "Supply widgets missing")
        QTest.mouseClick(supply_open, Qt.LeftButton)
        QTest.mouseClick(supply_close, Qt.LeftButton)

        purge_duration = find_by_testid(self.window, "input-purge-duration")
        purge_btn = find_by_testid(self.window, "btn-purge")
        self._expect(all([purge_duration, purge_btn]), "Purge widgets missing")
        set_spinbox_value(purge_duration, "500")
        QTest.mouseClick(purge_btn, Qt.LeftButton)

    def _exercise_dxf(self):
        self._activate_tab("配置")
        self.window._dxf_filepath = "C:/mock/test.dxf"
        dxf_path = find_by_testid(self.window, "input-dxf-path")
        if dxf_path is not None:
            dxf_path.setText(self.window._dxf_filepath)
        dxf_load = find_by_testid(self.window, "btn-dxf-load")
        dxf_preview = find_by_testid(self.window, "btn-dxf-preview")
        dxf_execute = find_by_testid(self.window, "btn-dxf-execute")
        dxf_stop = find_by_testid(self.window, "btn-dxf-stop")
        self._expect(all([dxf_load, dxf_preview, dxf_execute, dxf_stop]), "DXF widgets missing")
        QTest.mouseClick(dxf_load, Qt.LeftButton)
        QTest.mouseClick(dxf_preview, Qt.LeftButton)
        QTest.mouseClick(dxf_execute, Qt.LeftButton)
        QTest.qWait(300)
        QTest.mouseClick(dxf_stop, Qt.LeftButton)

    def _exercise_alarms(self):
        self._activate_tab("报警")
        alarm_list = find_by_testid(self.window, "list-alarms")
        ack_btn = find_by_testid(self.window, "btn-alarm-ack")
        self._expect(all([alarm_list, ack_btn]), "Alarm widgets missing")
        QTest.qWait(300)
        if alarm_list.count() > 0:
            alarm_list.setCurrentRow(0)
            before = alarm_list.count()
            QTest.mouseClick(ack_btn, Qt.LeftButton)
            QTest.qWait(300)
            after = alarm_list.count()
            self._expect(after < before, "Alarm acknowledge did not reduce list")

    def run(self):
        try:
            original_send = self.window._client.send_request

            def send_request_fast(method, params=None, timeout=1.0):
                return original_send(method, params, timeout=1.0)

            self.window._client.send_request = send_request_fast

            print("STEP: login dialog", flush=True)
            self._exercise_login_dialog("engineer", "9999")
            ok, _ = self.window._auth.login("engineer", "9999")
            self._expect(ok, "Auth login failed")
            if ok:
                self.window._user_label.setText(self.window._auth.current_user.role)
                self.window._apply_permissions()

            print("STEP: connect", flush=True)
            self._connect()

            hw_btn = find_by_testid(self.window, "btn-hw-init")
            if hw_btn and hw_btn.isEnabled():
                print("STEP: hw init", flush=True)
                QTest.mouseClick(hw_btn, Qt.LeftButton)

            print("STEP: motion", flush=True)
            self._exercise_motion()
            print("STEP: dispenser", flush=True)
            self._exercise_dispenser()
            print("STEP: dxf", flush=True)
            self._exercise_dxf()
            print("STEP: alarms", flush=True)
            self._exercise_alarms()

            prod_start = find_by_testid(self.window, "btn-production-start")
            prod_pause = find_by_testid(self.window, "btn-production-pause")
            prod_stop = find_by_testid(self.window, "btn-production-stop")
            if all([prod_start, prod_pause, prod_stop]):
                self._activate_tab("生产")
                print("STEP: production", flush=True)
                QTest.mouseClick(prod_start, Qt.LeftButton)
                QTest.qWait(200)
                QTest.mouseClick(prod_pause, Qt.LeftButton)
                QTest.qWait(100)
                QTest.mouseClick(prod_stop, Qt.LeftButton)
        finally:
            self.window.close()
            self.app.exit(1 if self.failed else 0)


def start_mock_server(host: str, port: int, verbose: bool) -> Tuple[MockTcpServer, int]:
    state = MockState(seed_alarms=True)
    server = MockTcpServer((host, port), MockRequestHandler, state, verbose)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server, server.server_address[1]


def main() -> int:
    parser = argparse.ArgumentParser(description="Run PyQt5 UI QTest smoke tests")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0, help="0 to auto-pick a free port")
    parser.add_argument("--no-mock", action="store_true", help="Use existing server")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--timeout-ms", type=int, default=20000)
    args = parser.parse_args()

    server: Optional[MockTcpServer] = None
    port = args.port
    if not args.no_mock:
        server, port = start_mock_server(args.host, args.port, args.verbose)

    app = QApplication([])
    window = MainWindow()
    window.show()

    runner = UiTestRunner(app, window, args.host, port)
    QTimer.singleShot(0, runner.run)
    QTimer.singleShot(args.timeout_ms, lambda: app.exit(1))
    exit_code = app.exec_()

    if server:
        server.shutdown()
        server.server_close()

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
