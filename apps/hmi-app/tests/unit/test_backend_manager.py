import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

from hmi_client.client import gateway_launch as gateway_launch_module
from hmi_client.client.gateway_launch import load_gateway_launch_spec
from hmi_client.client.backend_manager import BackendManager


class _EnvGuard:
    def __init__(self, **updates: str | None) -> None:
        self._updates = updates
        self._original: dict[str, str | None] = {}

    def __enter__(self):
        for key, value in self._updates.items():
            self._original[key] = os.environ.get(key)
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value
        return self

    def __exit__(self, exc_type, exc, tb):
        for key, value in self._original.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value


class BackendManagerContractTest(unittest.TestCase):
    def test_load_gateway_launch_spec_from_json(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            exe_path = temp_root / "gateway.exe"
            exe_path.write_text("", encoding="utf-8")
            spec_path = temp_root / "gateway-launch.json"
            spec_path.write_text(
                json.dumps(
                    {
                        "executable": str(exe_path),
                        "cwd": str(temp_root),
                        "args": ["--port", "9527"],
                        "pathEntries": [str(temp_root / "bin")],
                        "env": {"SILIGEN_TCP_SERVER_PORT": "9527"},
                    }
                ),
                encoding="utf-8",
            )

            with _EnvGuard(
                SILIGEN_GATEWAY_LAUNCH_SPEC=str(spec_path),
                SILIGEN_GATEWAY_EXE=None,
            ):
                spec = load_gateway_launch_spec()

        self.assertIsNotNone(spec)
        assert spec is not None
        self.assertEqual(spec.command(), [str(exe_path), "--port", "9527"])
        self.assertEqual(spec.cwd, temp_root)
        self.assertEqual(spec.env["SILIGEN_TCP_SERVER_PORT"], "9527")

    def test_backend_manager_requires_explicit_launch_contract(self) -> None:
        missing_default = PROJECT_ROOT / "tests" / "__missing_gateway_launch_default__.json"
        with mock.patch.object(gateway_launch_module, "_DEFAULT_SPEC_PATH", missing_default):
            with _EnvGuard(
                SILIGEN_GATEWAY_LAUNCH_SPEC=None,
                SILIGEN_GATEWAY_EXE=None,
                SILIGEN_GATEWAY_AUTOSTART="1",
            ):
                manager = BackendManager(host="127.0.0.1", port=1)
                ok, message = manager.start()

        self.assertFalse(ok)
        self.assertIn("SILIGEN_GATEWAY_LAUNCH_SPEC", message)
        self.assertIn("run.ps1 -GatewayExe", message)

    def test_load_gateway_launch_spec_ignores_default_contract_when_executable_missing(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            missing_exe = temp_root / "missing-gateway.exe"
            spec_path = temp_root / "gateway-launch.json"
            spec_path.write_text(
                json.dumps(
                    {
                        "executable": str(missing_exe),
                        "cwd": str(temp_root),
                        "args": [],
                        "pathEntries": [],
                        "env": {},
                    }
                ),
                encoding="utf-8",
            )

            with mock.patch.object(gateway_launch_module, "_DEFAULT_SPEC_PATH", spec_path):
                with _EnvGuard(
                    SILIGEN_GATEWAY_LAUNCH_SPEC=None,
                    SILIGEN_GATEWAY_EXE=None,
                ):
                    spec = load_gateway_launch_spec()

        self.assertIsNone(spec)

    def test_backend_manager_reports_explicit_autostart_disabled_cause(self) -> None:
        with _EnvGuard(
            SILIGEN_GATEWAY_AUTOSTART="0",
            SILIGEN_GATEWAY_LAUNCH_SPEC=None,
            SILIGEN_GATEWAY_EXE=None,
        ):
            manager = BackendManager(host="127.0.0.1", port=1)
            result = manager.start_detailed()

        self.assertFalse(result.ok)
        self.assertEqual(result.failure_code, "SUP_BACKEND_AUTOSTART_DISABLED")
        self.assertIn("SILIGEN_GATEWAY_AUTOSTART=0", result.message)
