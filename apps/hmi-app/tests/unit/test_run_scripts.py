import os
import subprocess
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
POWERSHELL = "powershell"
OFFICIAL_RUNNER = PROJECT_ROOT / "run.ps1"
INTERNAL_RUNNER = PROJECT_ROOT / "scripts" / "run.ps1"


class HmiRunScriptContractTest(unittest.TestCase):
    def test_internal_online_runner_requires_official_entrypoint_or_explicit_contract(self) -> None:
        env = os.environ.copy()
        env.pop("SILIGEN_GATEWAY_LAUNCH_SPEC", None)
        env.pop("SILIGEN_GATEWAY_EXE", None)
        env.pop("SILIGEN_HMI_OFFICIAL_ENTRYPOINT", None)
        env["SILIGEN_GATEWAY_AUTOSTART"] = "1"

        completed = subprocess.run(
            [
                POWERSHELL,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(INTERNAL_RUNNER),
                "--mode",
                "online",
            ],
            cwd=str(PROJECT_ROOT),
            env=env,
            capture_output=True,
            text=True,
        )

        self.assertNotEqual(completed.returncode, 0)
        output = f"{completed.stdout}\n{completed.stderr}"
        self.assertIn("Use apps/hmi-app/run.ps1", output)

    def test_internal_online_runner_rejects_legacy_gateway_exe_env_without_spec(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            env = os.environ.copy()
            env.pop("SILIGEN_GATEWAY_LAUNCH_SPEC", None)
            env.pop("SILIGEN_HMI_OFFICIAL_ENTRYPOINT", None)
            env["SILIGEN_GATEWAY_AUTOSTART"] = "1"
            env["SILIGEN_GATEWAY_EXE"] = str(Path(temp_dir) / "legacy-gateway.exe")

            completed = subprocess.run(
                [
                    POWERSHELL,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(INTERNAL_RUNNER),
                    "--mode",
                    "online",
                ],
                cwd=str(PROJECT_ROOT),
                env=env,
                capture_output=True,
                text=True,
            )

        self.assertNotEqual(completed.returncode, 0)
        output = f"{completed.stdout}\n{completed.stderr}"
        self.assertIn("Use apps/hmi-app/run.ps1", output)

    def test_official_runner_dryrun_generates_explicit_contract_from_canonical_gateway(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            build_root = temp_root / "control-apps-build"
            exe_path = build_root / "bin" / "Debug" / "siligen_runtime_gateway.exe"
            exe_path.parent.mkdir(parents=True, exist_ok=True)
            exe_path.write_text("", encoding="utf-8")

            env = os.environ.copy()
            env["SILIGEN_CONTROL_APPS_BUILD_ROOT"] = str(build_root)
            env.pop("SILIGEN_GATEWAY_LAUNCH_SPEC", None)
            env.pop("SILIGEN_GATEWAY_EXE", None)

            completed = subprocess.run(
                [
                    POWERSHELL,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(OFFICIAL_RUNNER),
                    "-DryRun",
                    "-BuildConfig",
                    "Debug",
                ],
                cwd=str(PROJECT_ROOT),
                env=env,
                capture_output=True,
                text=True,
            )

        self.assertEqual(completed.returncode, 0, msg=completed.stderr)
        self.assertIn("gateway contract source: generated-dev", completed.stdout)
        self.assertIn(str(exe_path), completed.stdout)

    def test_contract_builder_dryrun_parses_runtime_gateway_output_without_path_format_error(self) -> None:
        contract_builder = PROJECT_ROOT / "scripts" / "new-gateway-launch-contract.ps1"
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            build_root = temp_root / "control-apps-build"
            exe_path = build_root / "bin" / "Debug" / "siligen_runtime_gateway.exe"
            exe_path.parent.mkdir(parents=True, exist_ok=True)
            exe_path.write_text("", encoding="utf-8")

            env = os.environ.copy()
            env["SILIGEN_CONTROL_APPS_BUILD_ROOT"] = str(build_root)

            completed = subprocess.run(
                [
                    POWERSHELL,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(contract_builder),
                    "-DryRun",
                    "-BuildConfig",
                    "Debug",
                ],
                cwd=str(PROJECT_ROOT),
                env=env,
                capture_output=True,
                text=True,
            )

        self.assertEqual(completed.returncode, 0, msg=completed.stderr)
        self.assertIn(str(exe_path), completed.stdout)

    def test_official_runner_dryrun_reports_app_default_when_autostart_disabled(self) -> None:
        completed = subprocess.run(
            [
                POWERSHELL,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(OFFICIAL_RUNNER),
                "-DryRun",
                "-DisableGatewayAutostart",
            ],
            cwd=str(PROJECT_ROOT),
            capture_output=True,
            text=True,
        )

        self.assertEqual(completed.returncode, 0, msg=completed.stderr)
        self.assertIn("gateway contract source: app-default", completed.stdout)
        self.assertIn(str(PROJECT_ROOT / "config" / "gateway-launch.json"), completed.stdout)
