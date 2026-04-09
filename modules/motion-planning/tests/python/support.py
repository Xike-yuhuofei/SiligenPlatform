from __future__ import annotations

import os
import sys
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[4]
PACKAGE_ROOT = WORKSPACE_ROOT / "modules" / "motion-planning" / "application"
FIXTURE_ROOT = WORKSPACE_ROOT / "shared" / "contracts" / "engineering" / "fixtures" / "cases" / "rect_diag"


def ensure_package_root() -> None:
    package_root = str(PACKAGE_ROOT)
    if package_root not in sys.path:
        sys.path.insert(0, package_root)


def pythonpath_env() -> dict[str, str]:
    env = os.environ.copy()
    package_root = str(PACKAGE_ROOT)
    existing = env.get("PYTHONPATH")
    env["PYTHONPATH"] = package_root if not existing else os.pathsep.join([package_root, existing])
    return env


ensure_package_root()
