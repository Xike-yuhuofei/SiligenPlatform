import importlib.util
import io
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from types import SimpleNamespace


PROJECT_ROOT = Path(__file__).resolve().parents[2]
UI_QTEST = PROJECT_ROOT / "src" / "hmi_client" / "tools" / "ui_qtest.py"
SPEC = importlib.util.spec_from_file_location("ui_qtest_module", UI_QTEST)
assert SPEC is not None and SPEC.loader is not None
ui_qtest = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ui_qtest)


def _operator_context(**overrides):
    base = {
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


def _make_runner(context):
    runner = object.__new__(ui_qtest.GuiContractRunner)
    runner.failed = False
    runner.timed_out = False
    runner.timeout_ms = 1000
    runner.window = SimpleNamespace(build_operator_context_snapshot=lambda: dict(context))
    runner._switch_to_production_tab = lambda: None
    runner._click_button = lambda _testid: None
    runner._wait_for = lambda *_args, **_kwargs: True
    runner._status_message = lambda: ""
    runner._capture_screenshot = lambda *_args, **_kwargs: None
    return runner


class UiQtestContractTest(unittest.TestCase):
    def _run_qtest(self, *extra_args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                sys.executable,
                str(UI_QTEST),
                *extra_args,
            ],
            cwd=tempfile.gettempdir(),
            capture_output=True,
            text=True,
        )

    def test_operator_preview_requires_explicit_dxf_browse_path(self) -> None:
        completed = self._run_qtest(
            "--mode",
            "online",
            "--no-mock",
            "--exercise-runtime-actions",
            "--runtime-action-profile",
            "operator_preview",
        )

        output = f"{completed.stdout}\n{completed.stderr}"
        self.assertEqual(completed.returncode, 10, msg=output)
        self.assertIn("--runtime-action-profile operator_preview requires --dxf-browse-path", output)

    def test_operator_production_requires_explicit_dxf_browse_path(self) -> None:
        completed = self._run_qtest(
            "--mode",
            "online",
            "--no-mock",
            "--exercise-runtime-actions",
            "--runtime-action-profile",
            "operator_production",
        )

        output = f"{completed.stdout}\n{completed.stderr}"
        self.assertEqual(completed.returncode, 10, msg=output)
        self.assertIn("--runtime-action-profile operator_production requires --dxf-browse-path", output)

    def test_emit_operator_context_reads_main_window_export(self) -> None:
        runner = _make_runner(
            _operator_context(
                completed_count="1 / 1",
                global_progress_percent=100,
                current_operation="等待 下一批",
            )
        )

        with io.StringIO() as buffer, redirect_stdout(buffer):
            ui_qtest.GuiContractRunner._emit_operator_context(runner, "next-job-ready")
            output = buffer.getvalue().strip()

        self.assertIn("OPERATOR_CONTEXT stage=next-job-ready", output)
        self.assertIn("artifact_id=artifact-1", output)
        self.assertIn("preview_kind=glue_points", output)
        self.assertIn("completed_count=1/1", output)
        self.assertIn("global_progress_percent=100", output)
        self.assertIn("current_operation=等待_下一批", output)
        self.assertIn("preview_confirmed=true", output)

    def test_operator_preview_journey_fail_closes_before_stage_emission(self) -> None:
        runner = _make_runner(_operator_context())
        emitted_stages: list[str] = []
        captured_stages: list[str] = []

        def _failing_wait(*_args, **_kwargs):
            runner.failed = True
            return False

        runner._wait_for = _failing_wait
        runner._emit_operator_context = lambda stage: emitted_stages.append(stage)
        runner._capture_screenshot = lambda *_args, **kwargs: captured_stages.append(kwargs.get("stage_name", ""))

        ui_qtest.GuiContractRunner._run_operator_preview_journey(runner)

        self.assertTrue(runner.failed)
        self.assertEqual(emitted_stages, [])
        self.assertEqual(captured_stages, [])


if __name__ == "__main__":
    unittest.main()
