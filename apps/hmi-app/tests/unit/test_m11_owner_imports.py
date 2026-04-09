import importlib.util
import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
WORKSPACE_ROOT = PROJECT_ROOT.parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "src"))
sys.path.insert(0, str(WORKSPACE_ROOT / "modules" / "hmi-application" / "application"))

from hmi_application.preview_gate import DispensePreviewGate as OwnerPreviewGate
from hmi_application.launch_state import LaunchUiState as OwnerLaunchUiState
from hmi_application.launch_supervision_session import SupervisorSession as OwnerSupervisorSession
from hmi_application.startup import LaunchResult as OwnerLaunchResult
from hmi_application.adapters.qt_workers import RecoveryWorker as OwnerRecoveryWorker
from hmi_application.adapters.qt_workers import StartupWorker as OwnerStartupWorker
from hmi_client.client.launch_state import LaunchUiState as CompatLaunchUiState
from hmi_client.client import RecoveryWorker as CompatRecoveryWorker
from hmi_client.client import StartupWorker as CompatStartupWorker
from hmi_client.client.launch_supervision_session import SupervisorSession as CompatLaunchSupervisorSession
from hmi_client.client.startup_sequence import LaunchResult as CompatLaunchResult
from hmi_client.features.dispense_preview_gate.preview_gate import DispensePreviewGate as CompatPreviewGate


class M11OwnerImportCompatibilityTest(unittest.TestCase):
    def test_startup_compat_module_reexports_owner_type(self) -> None:
        self.assertIs(CompatLaunchResult, OwnerLaunchResult)

    def test_launch_supervision_compat_module_reexports_owner_type(self) -> None:
        self.assertIs(CompatLaunchSupervisorSession, OwnerSupervisorSession)

    def test_preview_gate_compat_module_reexports_owner_type(self) -> None:
        self.assertIs(CompatPreviewGate, OwnerPreviewGate)

    def test_launch_state_compat_module_reexports_owner_type(self) -> None:
        self.assertIs(CompatLaunchUiState, OwnerLaunchUiState)

    def test_preview_session_compat_module_is_removed(self) -> None:
        self.assertIsNone(importlib.util.find_spec("hmi_client.client.preview_session"))

    def test_supervisor_compat_modules_are_removed(self) -> None:
        self.assertIsNone(importlib.util.find_spec("hmi_client.client.supervisor_contract"))
        self.assertIsNone(importlib.util.find_spec("hmi_client.client.supervisor_session"))

    def test_client_package_does_not_reexport_preview_session_owner(self) -> None:
        import hmi_client.client as client_package

        self.assertFalse(hasattr(client_package, "PreviewSessionOwner"))

    def test_startup_root_does_not_reexport_workers(self) -> None:
        import hmi_application.startup as startup_module

        self.assertFalse(hasattr(startup_module, "StartupWorker"))
        self.assertFalse(hasattr(startup_module, "RecoveryWorker"))

    def test_startup_sequence_compat_does_not_reexport_workers(self) -> None:
        import hmi_client.client.startup_sequence as startup_sequence_module

        self.assertFalse(hasattr(startup_sequence_module, "StartupWorker"))
        self.assertFalse(hasattr(startup_sequence_module, "RecoveryWorker"))

    def test_client_package_reexports_adapter_workers(self) -> None:
        self.assertIs(CompatStartupWorker, OwnerStartupWorker)
        self.assertIs(CompatRecoveryWorker, OwnerRecoveryWorker)

    def test_owner_root_package_requires_explicit_submodule_imports(self) -> None:
        import hmi_application as owner_package

        self.assertEqual(owner_package.__all__, [])
        self.assertFalse(hasattr(owner_package, "LaunchResult"))
        self.assertFalse(hasattr(owner_package, "SessionSnapshot"))
        self.assertFalse(hasattr(owner_package, "SupervisorSession"))
        self.assertFalse(hasattr(owner_package, "PreviewSessionOwner"))
        self.assertFalse(hasattr(owner_package, "StartupWorker"))
        self.assertFalse(hasattr(owner_package, "RecoveryWorker"))


if __name__ == "__main__":
    unittest.main()
