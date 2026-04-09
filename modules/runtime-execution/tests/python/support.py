from __future__ import annotations

import os
import sys
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[4]
MODULE_ROOT = WORKSPACE_ROOT / "modules" / "runtime-execution"
PACKAGE_ROOT = MODULE_ROOT / "application"
DXF_GEOMETRY_PACKAGE_ROOT = WORKSPACE_ROOT / "modules" / "dxf-geometry" / "application"
FIXTURE_ROOT = WORKSPACE_ROOT / "shared" / "contracts" / "engineering" / "fixtures" / "cases" / "rect_diag"
PACKAGE_ROOTS = (
    PACKAGE_ROOT,
    DXF_GEOMETRY_PACKAGE_ROOT,
)


def ensure_package_roots() -> None:
    for package_root in PACKAGE_ROOTS:
        package_root_str = str(package_root)
        if package_root_str not in sys.path:
            sys.path.insert(0, package_root_str)


def pythonpath_env() -> dict[str, str]:
    env = os.environ.copy()
    existing = env.get("PYTHONPATH")
    package_roots = [str(package_root) for package_root in PACKAGE_ROOTS]
    env["PYTHONPATH"] = os.pathsep.join(package_roots + ([existing] if existing else []))
    return env


ensure_package_roots()
