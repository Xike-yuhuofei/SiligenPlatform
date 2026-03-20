import json
import os
import sys
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

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
    def test_load_gateway_launch_spec_from_env_contract(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir).resolve()
            exe_path = temp_root / "gateway.exe"
            exe_path.write_text("", encoding="utf-8")
            cwd_path = temp_root / "cwd"
            path_a = temp_root / "binA"
            path_b = temp_root / "binB"
            env_payload = {"SILIGEN_TCP_SERVER_PORT": "9527", "MODE": "test"}

            with _EnvGuard(
                SILIGEN_GATEWAY_LAUNCH_SPEC=None,
                SILIGEN_GATEWAY_EXE=str(exe_path),
                SILIGEN_GATEWAY_ARGS="--port 9527 --mode dry-run",
                SILIGEN_GATEWAY_CWD=str(cwd_path),
                SILIGEN_GATEWAY_PATH=os.pathsep.join((str(path_a), str(path_b))),
                SILIGEN_GATEWAY_ENV_JSON=json.dumps(env_payload),
            ):
                spec = load_gateway_launch_spec()

        self.assertIsNotNone(spec)
        assert spec is not None
        self.assertEqual(spec.executable, exe_path)
        self.assertEqual(spec.args, ("--port", "9527", "--mode", "dry-run"))
        self.assertEqual(spec.cwd, cwd_path)
        self.assertEqual(spec.path_entries, (path_a, path_b))
        self.assertEqual(spec.env, env_payload)

    def test_load_gateway_launch_spec_rejects_non_object_env_json(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir).resolve()
            exe_path = temp_root / "gateway.exe"
            exe_path.write_text("", encoding="utf-8")

            with _EnvGuard(
                SILIGEN_GATEWAY_LAUNCH_SPEC=None,
                SILIGEN_GATEWAY_EXE=str(exe_path),
                SILIGEN_GATEWAY_ENV_JSON='["bad"]',
            ):
                with self.assertRaisesRegex(ValueError, "SILIGEN_GATEWAY_ENV_JSON 必须是 JSON 对象"):
                    load_gateway_launch_spec()
