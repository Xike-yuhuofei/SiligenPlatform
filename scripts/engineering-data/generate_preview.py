from __future__ import annotations

"""Workspace formal preview CLI entry."""

from pathlib import Path
import sys


def _bootstrap() -> None:
    tools_root = Path(__file__).resolve().parents[2] / "apps" / "planner-cli" / "tools"
    tools_root_str = str(tools_root)
    if tools_root_str not in sys.path:
        sys.path.insert(0, tools_root_str)


_bootstrap()

from planner_cli_preview.generate_preview import main  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
