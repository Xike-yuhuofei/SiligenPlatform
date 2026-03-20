#!/usr/bin/env python3
"""Offline launch smoke test for the canonical HMI."""
from __future__ import annotations

import sys
from pathlib import Path

from PyQt5.QtCore import QTimer
from PyQt5.QtTest import QTest
from PyQt5.QtWidgets import QApplication, QWidget

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ui.main_window import MainWindow  # noqa: E402


def find_by_testid(root: QWidget, testid: str):
    if root.property("data-testid") == testid:
        return root
    for widget in root.findChildren(QWidget):
        if widget.property("data-testid") == testid:
            return widget
    return None


class OfflineSmokeRunner:
    def __init__(self, app: QApplication, window: MainWindow):
        self.app = app
        self.window = window
        self.failed = False

    def _check(self, condition: bool, message: str) -> None:
        if not condition:
            self.failed = True
            print(f"FAIL: {message}", flush=True)

    def _expect_status(self, expected: str, message: str) -> None:
        actual = self.window.statusBar().currentMessage()
        self._check(expected in actual, f"{message}; actual={actual!r}")

    def run(self) -> None:
        try:
            print("STEP: launch labels", flush=True)
            launch_mode = find_by_testid(self.window, "label-launch-mode")
            self._check(launch_mode is not None, "Missing launch mode label")
            if launch_mode is not None:
                self._check(launch_mode.text() == "Offline", "Launch mode label should be Offline")
            self._check(self.window._operation_status.text() == "离线模式", "Operation status should be 离线模式")

            print("STEP: offline capability gating", flush=True)
            self._check(not self.window._production_tab.isEnabled(), "Production tab should be disabled")
            self._check(not self.window._system_panel.isEnabled(), "System panel should be disabled")
            self._check(not self.window._motion_control_panel.isEnabled(), "Motion panel should be disabled")
            self._check(not self.window._dispenser_control_panel.isEnabled(), "Dispenser panel should be disabled")
            self._check(not self.window._recipe_tab.isEnabled(), "Recipe tab should be disabled")

            print("STEP: neutral offline messages", flush=True)
            self.window._on_move_to()
            QTest.qWait(50)
            self._expect_status("Offline 模式下不可用: 移动控制", "Move action should not report success offline")

            self.window._dxf_filepath = "C:/mock/test.dxf"
            self.window._on_dxf_load()
            QTest.qWait(50)
            self._expect_status("Offline 模式下不可用: DXF加载", "DXF load should be blocked offline")

            self.window._start_production_process(dry_run=False)
            QTest.qWait(50)
            self._expect_status("Offline 模式下不可用: 生产控制", "Production should be blocked offline")

            self.window._on_alarm_clear()
            QTest.qWait(50)
            self._expect_status("Offline 模式下不可用: 报警控制", "Alarm clear should be blocked offline")

            print("STEP: launch result", flush=True)
            self._check(self.window._launch_result is not None, "Launch result should be present")
            if self.window._launch_result is not None:
                self._check(self.window._launch_result.effective_mode == "offline", "Effective mode should be offline")
                self._check(self.window._launch_result.success, "Offline launch should be successful")
        finally:
            self.window.close()
            self.app.exit(1 if self.failed else 0)


def main() -> int:
    app = QApplication([])
    window = MainWindow(launch_mode="offline")
    window.show()
    runner = OfflineSmokeRunner(app, window)
    QTimer.singleShot(0, runner.run)
    return app.exec_()


if __name__ == "__main__":
    raise SystemExit(main())
