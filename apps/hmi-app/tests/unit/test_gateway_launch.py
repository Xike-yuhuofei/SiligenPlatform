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


class GatewayLaunchSpecEnvTest(unittest.TestCase):
    def test_explicit_spec_file_takes_precedence_over_legacy_env_hint(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir).resolve()
            spec_exe = temp_root / "gateway-from-spec.exe"
            env_exe = temp_root / "gateway-from-env.exe"
            spec_exe.write_text("", encoding="utf-8")
            env_exe.write_text("", encoding="utf-8")
            spec_path = temp_root / "gateway-launch.json"
            spec_path.write_text(
                json.dumps(
                    {
                        "executable": str(spec_exe),
                        "args": ["--port", "10001"],
                    }
                ),
                encoding="utf-8",
            )

            with _EnvGuard(
                SILIGEN_GATEWAY_LAUNCH_SPEC=str(spec_path),
                SILIGEN_GATEWAY_EXE=str(env_exe),
                SILIGEN_GATEWAY_ARGS="--port 20002",
            ):
                spec = load_gateway_launch_spec()

        self.assertIsNotNone(spec)
        assert spec is not None
        self.assertEqual(spec.executable, spec_exe)
        self.assertEqual(spec.args, ("--port", "10001"))

    def test_default_spec_file_takes_precedence_over_env_contract(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir).resolve()
            spec_exe = temp_root / "gateway-from-default.exe"
            env_exe = temp_root / "gateway-from-env.exe"
            spec_exe.write_text("", encoding="utf-8")
            env_exe.write_text("", encoding="utf-8")
            default_spec = temp_root / "gateway-launch.json"
            default_spec.write_text(
                json.dumps(
                    {
                        "executable": str(spec_exe),
                        "args": ["--mode", "online"],
                    }
                ),
                encoding="utf-8",
            )

            with mock.patch.object(gateway_launch_module, "_DEFAULT_SPEC_PATH", default_spec):
                with _EnvGuard(
                    SILIGEN_GATEWAY_LAUNCH_SPEC=None,
                    SILIGEN_GATEWAY_EXE=str(env_exe),
                    SILIGEN_GATEWAY_ARGS="--mode dry-run",
                ):
                    spec = load_gateway_launch_spec()

        self.assertIsNotNone(spec)
        assert spec is not None
        self.assertEqual(spec.executable, spec_exe)
        self.assertEqual(spec.args, ("--mode", "online"))

    def test_returns_none_when_default_and_explicit_spec_missing(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir).resolve()

            with mock.patch.object(gateway_launch_module, "_DEFAULT_SPEC_PATH", temp_root / "__missing_default__.json"):
                with _EnvGuard(
                    SILIGEN_GATEWAY_LAUNCH_SPEC=None,
                    SILIGEN_GATEWAY_EXE=None,
                ):
                    spec = load_gateway_launch_spec()

        self.assertIsNone(spec)

    def test_returns_none_when_only_legacy_gateway_exe_env_is_set(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir).resolve()
            exe_path = temp_root / "gateway.exe"
            exe_path.write_text("", encoding="utf-8")

            with _EnvGuard(
                SILIGEN_GATEWAY_LAUNCH_SPEC=None,
                SILIGEN_GATEWAY_EXE=str(exe_path),
            ):
                with mock.patch.object(gateway_launch_module, "_DEFAULT_SPEC_PATH", temp_root / "__missing_default__.json"):
                    spec = load_gateway_launch_spec()

        self.assertIsNone(spec)

    def test_load_gateway_launch_spec_requires_executable_in_spec_file(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir).resolve()
            spec_path = temp_root / "gateway-launch.json"
            spec_path.write_text(json.dumps({"args": ["--port", "9527"]}), encoding="utf-8")

            with _EnvGuard(
                SILIGEN_GATEWAY_LAUNCH_SPEC=str(spec_path),
                SILIGEN_GATEWAY_EXE=None,
            ):
                with self.assertRaisesRegex(ValueError, "gateway launch spec 缺少 executable"):
                    load_gateway_launch_spec()
