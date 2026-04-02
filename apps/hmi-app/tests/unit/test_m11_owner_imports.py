import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
WORKSPACE_ROOT = PROJECT_ROOT.parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "src"))
sys.path.insert(0, str(WORKSPACE_ROOT / "modules" / "hmi-application" / "application"))

from hmi_application.preview_gate import DispensePreviewGate as OwnerPreviewGate
from hmi_application.launch_state import LaunchUiState as OwnerLaunchUiState
from hmi_application.preview_session import PreviewSessionOwner as OwnerPreviewSessionOwner
from hmi_application.startup import LaunchResult as OwnerLaunchResult
from hmi_application.supervisor_session import SupervisorSession as OwnerSupervisorSession
from hmi_client.client.launch_state import LaunchUiState as CompatLaunchUiState
from hmi_client.client.preview_session import PreviewSessionOwner as CompatPreviewSessionOwner
from hmi_client.client.startup_sequence import LaunchResult as CompatLaunchResult
from hmi_client.client.supervisor_session import SupervisorSession as CompatSupervisorSession
from hmi_client.features.dispense_preview_gate.preview_gate import DispensePreviewGate as CompatPreviewGate


class M11OwnerImportCompatibilityTest(unittest.TestCase):
    def test_startup_compat_module_reexports_owner_type(self) -> None:
        self.assertIs(CompatLaunchResult, OwnerLaunchResult)

    def test_supervisor_compat_module_reexports_owner_type(self) -> None:
        self.assertIs(CompatSupervisorSession, OwnerSupervisorSession)

    def test_preview_gate_compat_module_reexports_owner_type(self) -> None:
        self.assertIs(CompatPreviewGate, OwnerPreviewGate)

    def test_launch_state_compat_module_reexports_owner_type(self) -> None:
        self.assertIs(CompatLaunchUiState, OwnerLaunchUiState)

    def test_preview_session_compat_module_reexports_owner_type(self) -> None:
        self.assertIs(CompatPreviewSessionOwner, OwnerPreviewSessionOwner)


if __name__ == "__main__":
    unittest.main()
