import shutil
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
POWERSHELL = "powershell"
HIL_RUNNER = ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_hil_controlled_test.ps1"
CONTROLLED_PRODUCTION_RUNNER = ROOT / "tests" / "e2e" / "simulated-line" / "run_controlled_production_test.ps1"


class ControlledRunnerPositionalBindingContractTest(unittest.TestCase):
    def _assert_timestamp_switch_rejects_boolean_literal(self, script_path: Path, script_marker: str) -> None:
        true_root = ROOT / "True"
        had_true_root = true_root.exists()
        try:
            completed = subprocess.run(
                [
                    POWERSHELL,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(script_path),
                    "-UseTimestampedReportDir",
                    "True",
                    "-PublishLatestOnPass:$false",
                ],
                cwd=str(ROOT),
                capture_output=True,
                text=True,
            )
        finally:
            if not had_true_root and true_root.exists():
                shutil.rmtree(true_root, ignore_errors=True)

        output = f"{completed.stdout}\n{completed.stderr}"
        self.assertNotEqual(completed.returncode, 0, msg=output)
        self.assertRegex(
            output.lower(),
            r"cannot process argument transformation|parameterargumenttransformationerror|"
            r"positional parameter|parameter cannot be found",
        )
        self.assertNotIn(script_marker, output, msg=output)

    def test_hil_controlled_runner_rejects_boolean_literal_after_timestamp_switch(self) -> None:
        self._assert_timestamp_switch_rejects_boolean_literal(
            HIL_RUNNER,
            "hil controlled test: parameter snapshot",
        )

    def test_hil_controlled_runner_accepts_explicit_false_publish_argument(self) -> None:
        completed = subprocess.run(
            [
                POWERSHELL,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(HIL_RUNNER),
                "-HilDurationSeconds",
                "1",
                "-PublishLatestOnPass:$false",
            ],
            cwd=str(ROOT),
            capture_output=True,
            text=True,
        )

        output = f"{completed.stdout}\n{completed.stderr}"
        self.assertNotEqual(completed.returncode, 0, msg=output)
        self.assertIn("publish_latest_on_pass=False", output, msg=output)
        self.assertIn("HilDurationSeconds", output, msg=output)
        self.assertNotIn("ParameterArgumentTransformationError", output, msg=output)

    def test_hil_controlled_runner_rejects_explicit_true_publish_argument(self) -> None:
        completed = subprocess.run(
            [
                POWERSHELL,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(HIL_RUNNER),
                "-PublishLatestOnPass:$true",
            ],
            cwd=str(ROOT),
            capture_output=True,
            text=True,
        )

        output = f"{completed.stdout}\n{completed.stderr}"
        self.assertNotEqual(completed.returncode, 0, msg=output)
        self.assertIn("publish_latest_on_pass=True", output, msg=output)
        self.assertIn("PublishLatestOnPass", output, msg=output)
        self.assertIn("必须为 false", output, msg=output)
        self.assertNotIn("ParameterArgumentTransformationError", output, msg=output)

    def test_controlled_production_runner_rejects_boolean_literal_after_timestamp_switch(self) -> None:
        self._assert_timestamp_switch_rejects_boolean_literal(
            CONTROLLED_PRODUCTION_RUNNER,
            "controlled-production-test: parameter snapshot",
        )


if __name__ == "__main__":
    unittest.main()
