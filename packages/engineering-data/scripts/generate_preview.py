from __future__ import annotations

from pathlib import Path
import sys


def _bootstrap() -> None:
    src_root = Path(__file__).resolve().parents[1] / "src"
    src_root_str = str(src_root)
    if src_root_str not in sys.path:
        sys.path.insert(0, src_root_str)


_bootstrap()

from engineering_data.cli.generate_preview import main  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
