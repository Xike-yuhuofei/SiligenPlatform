from __future__ import annotations

from pathlib import Path
import sys


def _bootstrap_runtime_execution() -> None:
    workspace_root = Path(__file__).resolve().parents[5]
    runtime_root = workspace_root / "modules" / "runtime-execution" / "application"
    runtime_root_str = str(runtime_root)
    if runtime_root_str not in sys.path:
        sys.path.insert(0, runtime_root_str)


_bootstrap_runtime_execution()

from runtime_execution.export_simulation_input import main  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
