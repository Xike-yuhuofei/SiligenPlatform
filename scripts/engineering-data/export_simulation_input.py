from __future__ import annotations

"""Workspace formal engineering_data CLI entry."""

from pathlib import Path
import sys


def _bootstrap() -> None:
    workspace_root = Path(__file__).resolve().parents[2]
    package_roots = (
        workspace_root / "modules" / "runtime-execution" / "application",
        workspace_root / "modules" / "dxf-geometry" / "application",
    )
    for package_root in package_roots:
        package_root_str = str(package_root)
        if package_root_str not in sys.path:
            sys.path.insert(0, package_root_str)


_bootstrap()

from runtime_execution.export_simulation_input import main  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
