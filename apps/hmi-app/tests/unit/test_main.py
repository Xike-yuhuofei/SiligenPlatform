import os
import sys
import unittest
from pathlib import Path
from unittest import mock


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "src"))

import hmi_client.main as hmi_main


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


class HmiMainContractGuardTest(unittest.TestCase):
    def test_direct_online_launch_requires_official_entrypoint_or_explicit_contract(self) -> None:
        with _EnvGuard(
            SILIGEN_HMI_OFFICIAL_ENTRYPOINT=None,
            SILIGEN_GATEWAY_LAUNCH_SPEC=None,
            SILIGEN_GATEWAY_EXE=None,
            SILIGEN_GATEWAY_AUTOSTART="1",
        ):
            with mock.patch.object(hmi_main, "load_gateway_launch_spec", return_value=None):
                with self.assertRaisesRegex(SystemExit, "apps/hmi-app/run.ps1"):
                    hmi_main._validate_online_launch_contract("online")

    def test_online_launch_allows_explicit_contract(self) -> None:
        with _EnvGuard(
            SILIGEN_HMI_OFFICIAL_ENTRYPOINT=None,
            SILIGEN_GATEWAY_AUTOSTART="1",
        ):
            with mock.patch.object(hmi_main, "load_gateway_launch_spec", return_value=object()):
                hmi_main._validate_online_launch_contract("online")

    def test_offline_launch_does_not_require_gateway_contract(self) -> None:
        with _EnvGuard(
            SILIGEN_HMI_OFFICIAL_ENTRYPOINT=None,
            SILIGEN_GATEWAY_LAUNCH_SPEC=None,
            SILIGEN_GATEWAY_EXE=None,
            SILIGEN_GATEWAY_AUTOSTART="1",
        ):
            with mock.patch.object(hmi_main, "load_gateway_launch_spec", return_value=None):
                hmi_main._validate_online_launch_contract("offline")
