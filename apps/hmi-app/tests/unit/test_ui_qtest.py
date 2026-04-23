import os
import sys
import unittest
from dataclasses import dataclass
import io
from contextlib import redirect_stdout
from pathlib import Path
from types import SimpleNamespace
from typing import Any, cast


os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.tools.ui_qtest import GuiContractRunner, resolve_runtime_action_request


def _operator_context(**overrides: object) -> dict[str, object]:
    base: dict[str, object] = {
        "artifact_id": "artifact-1",
        "plan_id": "plan-1",
        "preview_source": "planned_glue_snapshot",
        "preview_kind": "glue_points",
        "glue_point_count": 2,
        "snapshot_hash": "hash-1",
        "confirmed_snapshot_hash": "hash-1",
        "snapshot_ready": True,
        "preview_confirmed": True,
        "job_id": "job-1",
        "target_count": 1,
        "completed_count": "0 / 1",
        "global_progress_percent": 0,
        "current_operation": "生产 运行中",
    }
    base.update(overrides)
    return base


def _make_export_runner(context: dict[str, object]) -> GuiContractRunner:
    runner = object.__new__(GuiContractRunner)
    runner.failed = False
    runner.timed_out = False
    runner.timeout_ms = 1000
    runner.window = SimpleNamespace(
        build_operator_context_snapshot=lambda: dict(context),
        _current_plan_fingerprint="fp-1",
        _preview_gate=SimpleNamespace(last_error_message="", state="ready"),
    )
    runner._switch_to_production_tab = lambda: None
    runner._click_button = lambda _testid: None
    runner._wait_for = lambda *_args, **_kwargs: True
    runner._status_message = lambda: ""
    runner._capture_screenshot = lambda *_args, **_kwargs: None
    return cast(GuiContractRunner, runner)


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

    def _capture_screenshot(self, description: str, *, stage_name: str = "") -> None:
        self.capture_calls.append(description)

    def _assert_runtime_action_chain(self) -> None:
        self.runtime_action_chain_calls += 1


class _PreviewJourneyRunner(GuiContractRunner):
    def __init__(self, *, wait_results: list[bool]) -> None:
        fake_window = SimpleNamespace(
            _preview_gate=SimpleNamespace(last_error_message=""),
            _current_plan_fingerprint="fp-1",
        )
        super().__init__(
            app=cast(Any, _FakeApp()),
            window=cast(Any, fake_window),
            launch_mode="online",
            timeout_ms=1000,
            runtime_action_profile="operator_preview",
        )
        self._wait_results = list(wait_results)
        self.emitted_stages: list[str] = []
        self.captured_stages: list[str] = []
        self._context = _operator_context()
        self.window.build_operator_context_snapshot = self._build_operator_context_snapshot

    def _build_operator_context_snapshot(self) -> dict[str, object]:
        return dict(self._context)

    def _switch_to_production_tab(self) -> None:
        return None

    def _click_button(self, _testid: str) -> None:
        return None

    def _wait_for(self, description: str, _predicate, timeout_ms: int | None = None) -> bool:
        if not self._wait_results:
            raise AssertionError(f"unexpected wait: {description}")
        result = self._wait_results.pop(0)
        if not result:
            self._fail(f"Timed out waiting for {description}")
        return result

    def _status_message(self) -> str:
        return ""

    def _emit_operator_context(self, stage: str) -> None:
        self.emitted_stages.append(stage)

    def _capture_screenshot(self, description: str, *, stage_name: str = "") -> None:
        self.captured_stages.append(stage_name or description)


class _RuntimeActionProfileRunner(GuiContractRunner):
    def __init__(self, *, runtime_action_profile: str) -> None:
        super().__init__(
            app=cast(Any, _FakeApp()),
            window=cast(Any, SimpleNamespace()),
            launch_mode="online",
            timeout_ms=1000,
            exercise_runtime_actions=True,
            runtime_action_profile=runtime_action_profile,
            screenshot_path="D:/tmp/runtime-action.png",
        )
        self.capture_calls: list[str] = []
        self.operator_production_calls = 0
        self.operator_preview_calls = 0

    def _cached_status(self):
        return object()

    def _assert_operator_preview_action(self) -> None:
        self.operator_preview_calls += 1

    def _assert_operator_production_action(self) -> None:
        self.operator_production_calls += 1

    def _assert_runtime_move_to_action(self) -> None:
        return None

    def _assert_runtime_jog_action(self) -> None:
        return None

    def _assert_runtime_supply_dispenser_action(self) -> None:
        return None

    def _assert_runtime_estop_reset_action(self) -> None:
        return None

    def _assert_runtime_door_interlock_action(self) -> None:
        return None

    def _capture_screenshot(self, description: str, *, stage_name: str = "") -> None:
        self.capture_calls.append(stage_name or description)


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

    def test_operator_preview_requires_explicit_dxf_browse_path(self) -> None:
        with self.assertRaisesRegex(ValueError, "--runtime-action-profile operator_preview requires --dxf-browse-path"):
            resolve_runtime_action_request(
                exercise_runtime_actions=True,
                runtime_action_profile="operator_preview",
                preview_payload_path="",
                dxf_browse_path="",
            )

    def test_operator_production_requires_explicit_dxf_browse_path(self) -> None:
        with self.assertRaisesRegex(ValueError, "--runtime-action-profile operator_production requires --dxf-browse-path"):
            resolve_runtime_action_request(
                exercise_runtime_actions=True,
                runtime_action_profile="operator_production",
                preview_payload_path="",
                dxf_browse_path="",
            )

    def test_emit_operator_context_reads_main_window_export(self) -> None:
        runner = _make_export_runner(
            _operator_context(
                completed_count="1 / 1",
                global_progress_percent=100,
                current_operation="等待 下一批",
            )
        )

        with io.StringIO() as buffer, redirect_stdout(buffer):
            GuiContractRunner._emit_operator_context(runner, "next-job-ready")
            output = buffer.getvalue().strip()

        self.assertIn("OPERATOR_CONTEXT stage=next-job-ready", output)
        self.assertIn("artifact_id=artifact-1", output)
        self.assertIn("plan_fingerprint=fp-1", output)
        self.assertIn("preview_kind=glue_points", output)
        self.assertIn("completed_count=1/1", output)
        self.assertIn("global_progress_percent=100", output)
        self.assertIn("current_operation=等待_下一批", output)
        self.assertIn("preview_confirmed=true", output)

    def test_preview_journey_fail_closed_does_not_emit_preview_ready_on_timeout(self) -> None:
        runner = _PreviewJourneyRunner(wait_results=[True, False])

        result = runner._run_operator_preview_journey()

        self.assertFalse(result)
        self.assertTrue(runner.failed)
        self.assertEqual(runner.emitted_stages, ["preview-ready-failed"])
        self.assertEqual(runner.captured_stages, ["preview-ready-failed"])

    def test_operator_production_runtime_chain_skips_trailing_generic_screenshot(self) -> None:
        runner = _RuntimeActionProfileRunner(runtime_action_profile="operator_production")

        runner._assert_runtime_action_chain()

        self.assertEqual(runner.operator_production_calls, 1)
        self.assertEqual(runner.capture_calls, [])

    def test_full_runtime_chain_keeps_trailing_generic_screenshot(self) -> None:
        runner = _RuntimeActionProfileRunner(runtime_action_profile="full")

        runner._assert_runtime_action_chain()

        self.assertEqual(runner.operator_preview_calls, 1)
        self.assertEqual(runner.capture_calls, ["runtime action profile full"])


if __name__ == "__main__":
    unittest.main()
