import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
WORKSPACE_ROOT = PROJECT_ROOT.parents[1]
POWERSHELL = "powershell"
OFFICIAL_RUNNER = PROJECT_ROOT / "run.ps1"
INTERNAL_RUNNER = PROJECT_ROOT / "scripts" / "run.ps1"
RUNTIME_RUNNER = WORKSPACE_ROOT / "apps" / "runtime-gateway" / "run.ps1"


class HmiRunScriptContractTest(unittest.TestCase):
    def _write_matching_cmake_cache(self, build_root: Path) -> None:
        (build_root / "CMakeCache.txt").write_text(
            f"CMAKE_HOME_DIRECTORY:INTERNAL={WORKSPACE_ROOT}\n",
            encoding="utf-8",
        )

    def _ensure_workspace_gateway_exe(self) -> tuple[Path, bool, bool]:
        exe_path = WORKSPACE_ROOT / "build" / "ca" / "bin" / "Debug" / "siligen_runtime_gateway.exe"
        cache_path = WORKSPACE_ROOT / "build" / "ca" / "CMakeCache.txt"
        if exe_path.exists():
            created_cache = False
            if not cache_path.exists():
                self._write_matching_cmake_cache(WORKSPACE_ROOT / "build" / "ca")
                created_cache = True
            return exe_path, False, created_cache

        exe_path.parent.mkdir(parents=True, exist_ok=True)
        exe_path.write_text("", encoding="utf-8")
        created_cache = False
        if not cache_path.exists():
            self._write_matching_cmake_cache(WORKSPACE_ROOT / "build" / "ca")
            created_cache = True
        return exe_path, True, created_cache

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

    def test_runtime_gateway_runner_dryrun_prefers_workspace_build_over_localappdata_by_default(self) -> None:
        workspace_exe, created_workspace_exe, created_workspace_cache = self._ensure_workspace_gateway_exe()
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                temp_root = Path(temp_dir)
                local_app_data = temp_root / "localappdata"
                localappdata_exe = local_app_data / "SiligenSuite" / "control-apps-build" / "bin" / "Debug" / "siligen_runtime_gateway.exe"
                localappdata_exe.parent.mkdir(parents=True, exist_ok=True)
                localappdata_exe.write_text("", encoding="utf-8")
                self._write_matching_cmake_cache(local_app_data / "SiligenSuite" / "control-apps-build")

                env = os.environ.copy()
                env.pop("SILIGEN_CONTROL_APPS_BUILD_ROOT", None)
                env["LOCALAPPDATA"] = str(local_app_data)

                completed = subprocess.run(
                    [
                        POWERSHELL,
                        "-NoProfile",
                        "-ExecutionPolicy",
                        "Bypass",
                        "-File",
                        str(RUNTIME_RUNNER),
                        "-DryRun",
                        "-BuildConfig",
                        "Debug",
                    ],
                    cwd=str(WORKSPACE_ROOT),
                    env=env,
                    capture_output=True,
                    text=True,
                )
        finally:
            if created_workspace_exe and workspace_exe.exists():
                workspace_exe.unlink()
            if created_workspace_cache:
                cache_path = WORKSPACE_ROOT / "build" / "ca" / "CMakeCache.txt"
                if cache_path.exists():
                    cache_path.unlink()

        self.assertEqual(completed.returncode, 0, msg=completed.stderr)
        self.assertIn(str(workspace_exe), completed.stdout)
        self.assertNotIn(str(localappdata_exe), completed.stdout)

    def test_runtime_gateway_runner_dryrun_does_not_fallback_when_workspace_root_missing_matching_cache(self) -> None:
        workspace_exe = WORKSPACE_ROOT / "build" / "ca" / "bin" / "Debug" / "siligen_runtime_gateway.exe"
        cache_path = WORKSPACE_ROOT / "build" / "ca" / "CMakeCache.txt"
        created_workspace_exe = False
        removed_cache = False
        original_cache = None
        try:
            if not workspace_exe.exists():
                workspace_exe.parent.mkdir(parents=True, exist_ok=True)
                workspace_exe.write_text("", encoding="utf-8")
                created_workspace_exe = True
            if cache_path.exists():
                original_cache = cache_path.read_text(encoding="utf-8")
                cache_path.unlink()
                removed_cache = True

            with tempfile.TemporaryDirectory() as temp_dir:
                temp_root = Path(temp_dir)
                local_app_data = temp_root / "localappdata"
                localappdata_build_root = local_app_data / "SiligenSuite" / "control-apps-build"
                localappdata_exe = localappdata_build_root / "bin" / "Debug" / "siligen_runtime_gateway.exe"
                localappdata_exe.parent.mkdir(parents=True, exist_ok=True)
                localappdata_exe.write_text("", encoding="utf-8")
                self._write_matching_cmake_cache(localappdata_build_root)

                env = os.environ.copy()
                env.pop("SILIGEN_CONTROL_APPS_BUILD_ROOT", None)
                env["LOCALAPPDATA"] = str(local_app_data)

                completed = subprocess.run(
                    [
                        POWERSHELL,
                        "-NoProfile",
                        "-ExecutionPolicy",
                        "Bypass",
                        "-File",
                        str(RUNTIME_RUNNER),
                        "-DryRun",
                        "-BuildConfig",
                        "Debug",
                    ],
                    cwd=str(WORKSPACE_ROOT),
                    env=env,
                    capture_output=True,
                    text=True,
                )
        finally:
            if created_workspace_exe and workspace_exe.exists():
                workspace_exe.unlink()
            if removed_cache and original_cache is not None:
                cache_path.write_text(original_cache, encoding="utf-8")

        output = f"{completed.stdout}\n{completed.stderr}"
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("current-workspace control-apps build root not ready for runtime-", output)
        self.assertIn("gateway", output)
        self.assertNotIn(str(localappdata_exe), output)

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

    def test_official_runner_dryrun_prefers_workspace_build_over_localappdata_by_default(self) -> None:
        workspace_exe, created_workspace_exe, created_workspace_cache = self._ensure_workspace_gateway_exe()
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                temp_root = Path(temp_dir)
                local_app_data = temp_root / "localappdata"
                localappdata_exe = local_app_data / "SiligenSuite" / "control-apps-build" / "bin" / "Debug" / "siligen_runtime_gateway.exe"
                localappdata_exe.parent.mkdir(parents=True, exist_ok=True)
                localappdata_exe.write_text("", encoding="utf-8")
                self._write_matching_cmake_cache(local_app_data / "SiligenSuite" / "control-apps-build")

                env = os.environ.copy()
                env.pop("SILIGEN_CONTROL_APPS_BUILD_ROOT", None)
                env.pop("SILIGEN_GATEWAY_LAUNCH_SPEC", None)
                env.pop("SILIGEN_GATEWAY_EXE", None)
                env["LOCALAPPDATA"] = str(local_app_data)

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
        finally:
            if created_workspace_exe and workspace_exe.exists():
                workspace_exe.unlink()
            if created_workspace_cache:
                cache_path = WORKSPACE_ROOT / "build" / "ca" / "CMakeCache.txt"
                if cache_path.exists():
                    cache_path.unlink()

        self.assertEqual(completed.returncode, 0, msg=completed.stderr)
        self.assertIn("gateway contract source: generated-dev", completed.stdout)
        self.assertIn(str(workspace_exe), completed.stdout)
        self.assertNotIn(str(localappdata_exe), completed.stdout)

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

    def test_verify_online_ready_timeout_handles_python_path_with_spaces(self) -> None:
        verify_script = PROJECT_ROOT / "scripts" / "verify-online-ready-timeout.ps1"
        python_exe = Path(sys.executable).resolve()

        completed = subprocess.run(
            [
                POWERSHELL,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(verify_script),
                "-MockStartupTimeoutMs",
                "2000",
                "-PythonExe",
                str(python_exe),
            ],
            cwd=str(PROJECT_ROOT),
            capture_output=True,
            text=True,
        )

        output = f"{completed.stdout}\n{completed.stderr}"
        self.assertEqual(completed.returncode, 0, msg=output)
        self.assertIn("observed exit_code=21", output)
        self.assertNotIn("Cannot process argument transformation on parameter 'TimeoutMs'", output)

    def test_online_smoke_writes_screenshot_without_runtime_action_mode(self) -> None:
        smoke_script = PROJECT_ROOT / "scripts" / "online-smoke.ps1"
        python_exe = Path(sys.executable).resolve()

        with tempfile.TemporaryDirectory() as temp_dir:
            screenshot_path = Path(temp_dir) / "online-smoke.png"
            completed = subprocess.run(
                [
                    POWERSHELL,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(smoke_script),
                    "-PythonExe",
                    str(python_exe),
                    "-TimeoutMs",
                    "20000",
                    "-ScreenshotPath",
                    str(screenshot_path),
                ],
                cwd=str(PROJECT_ROOT),
                capture_output=True,
                text=True,
            )

            output = f"{completed.stdout}\n{completed.stderr}"
            self.assertEqual(completed.returncode, 0, msg=output)
            self.assertTrue(screenshot_path.exists(), msg=output)
