from __future__ import annotations

import sys
from pathlib import Path


_WORKSPACE_ROOT = Path(__file__).resolve().parents[4]
_HMI_APPLICATION_ROOT = _WORKSPACE_ROOT / "modules" / "hmi-application" / "application"


def hmi_application_root() -> Path:
    return _HMI_APPLICATION_ROOT


def ensure_hmi_application_path() -> Path:
    root = hmi_application_root()
    root_text = str(root)
    if root_text not in sys.path:
        sys.path.insert(0, root_text)
    return root
