import os
import sys
import unittest
from dataclasses import dataclass
from pathlib import Path
from typing import Any, cast


os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.tools.ui_qtest import GuiContractRunner


class _FakeWidget:
    def __init__(self, *, enabled: bool = True, count: int = 0) -> None:
        self._enabled = enabled
        self._count = count

    def isEnabled(self) -> bool:
        return self._enabled

    def count(self) -> int:
        return self._count


@dataclass
class _FakeLaunchResult:
    success: bool
    phase: str
    online_ready: bool
    tcp_connected: bool
    hardware_ready: bool
    failure_code: str | None
    failure_stage: str | None


class _FakeWindow:
    def __init__(self, launch_result: _FakeLaunchResult) -> None:
        self._launch_result = launch_result


class _FakeApp:
    def exit(self, _code: int) -> None:
        return None


class _TestRunner(GuiContractRunner):
    def __init__(self, *, window: _FakeWindow) -> None:
        super().__init__(
            app=cast(Any, _FakeApp()),
            window=cast(Any, window),
            launch_mode="online",
            timeout_ms=1000,
            screenshot_path="D:/tmp/online-ready.png",
        )
        self.capture_calls: list[str] = []
        self.runtime_action_chain_calls = 0
        self._label_texts = {
            "label-launch-mode": "Online",
            "label-tcp-state": "已连接",
            "label-hw-state": "就绪",
            "label-current-operation": "空闲",
        }

    def _wait_for(self, _description: str, _predicate, timeout_ms: int | None = None) -> bool:
        return True

    def _label_text(self, testid: str) -> str:
        return self._label_texts[testid]

    def _require_widget(self, testid: str) -> _FakeWidget:
        if testid == "list-alarms":
            return _FakeWidget(count=0)
        return _FakeWidget(enabled=True)

    def _status_message(self) -> str:
        return ""

    def _uses_external_gateway(self) -> bool:
        return True

    def _capture_screenshot(self, description: str) -> None:
        self.capture_calls.append(description)

    def _assert_runtime_action_chain(self) -> None:
        self.runtime_action_chain_calls += 1


class GuiQtestContractTest(unittest.TestCase):
    def test_online_contract_captures_screenshot_on_success_without_runtime_actions(self) -> None:
        launch_result = _FakeLaunchResult(
            success=True,
            phase="ready",
            online_ready=True,
            tcp_connected=True,
            hardware_ready=True,
            failure_code=None,
            failure_stage=None,
        )
        runner = _TestRunner(window=_FakeWindow(launch_result))

        runner._assert_online_contract()

        self.assertEqual(runner.capture_calls, ["online ready screenshot"])
        self.assertEqual(runner.runtime_action_chain_calls, 0)


if __name__ == "__main__":
    unittest.main()
