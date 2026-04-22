from __future__ import annotations

import sys
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
HMI_APPLICATION_ROOT = WORKSPACE_ROOT / "modules" / "hmi-application" / "application"
TEST_KIT_ROOT = WORKSPACE_ROOT / "shared" / "testing" / "test-kit" / "src"


def ensure_hmi_application_test_paths() -> None:
    application_root = str(HMI_APPLICATION_ROOT)
    test_kit_root = str(TEST_KIT_ROOT)
    if application_root not in sys.path:
        sys.path.insert(0, application_root)
    if test_kit_root not in sys.path:
        sys.path.insert(0, test_kit_root)
