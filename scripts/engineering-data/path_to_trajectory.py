from __future__ import annotations

"""Workspace formal trajectory CLI entry."""

from pathlib import Path
import sys


def _bootstrap() -> None:
    src_root = Path(__file__).resolve().parents[2] / "modules" / "motion-planning" / "application"
    src_root_str = str(src_root)
    if src_root_str not in sys.path:
        sys.path.insert(0, src_root_str)


_bootstrap()

from motion_planning.trajectory_generation import main  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
