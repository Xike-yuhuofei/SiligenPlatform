from __future__ import annotations

import sys
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[4]
TOOLS_ROOT = WORKSPACE_ROOT / "apps" / "planner-cli" / "tools"
FIXTURE_ROOT = WORKSPACE_ROOT / "shared" / "contracts" / "engineering" / "fixtures" / "cases" / "rect_diag"
WORKSPACE_PREVIEW_SCRIPT = WORKSPACE_ROOT / "scripts" / "engineering-data" / "generate_preview.py"


def ensure_tools_root() -> None:
    tools_root = str(TOOLS_ROOT)
    if tools_root not in sys.path:
        sys.path.insert(0, tools_root)


ensure_tools_root()
