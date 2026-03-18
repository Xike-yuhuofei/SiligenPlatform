from __future__ import annotations

from pathlib import Path
import sys


def ensure_engineering_data_on_path() -> None:
    workspace_root = Path(__file__).resolve().parents[3]
    engineering_data_src = workspace_root / "packages" / "engineering-data" / "src"
    engineering_data_src_str = str(engineering_data_src)
    if engineering_data_src_str not in sys.path:
        sys.path.insert(0, engineering_data_src_str)
