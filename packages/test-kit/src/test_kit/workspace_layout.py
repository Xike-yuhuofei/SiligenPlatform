from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class WorkspaceLayoutEntry:
    key: str
    relative: str
    absolute: Path


def load_workspace_layout(workspace_root: Path) -> dict[str, WorkspaceLayoutEntry]:
    layout_file = workspace_root / "cmake" / "workspace-layout.env"
    if not layout_file.exists():
        raise FileNotFoundError(f"workspace layout file not found: {layout_file}")

    entries: dict[str, WorkspaceLayoutEntry] = {}
    for raw_line in layout_file.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, relative = line.split("=", 1)
        key = key.strip()
        relative = relative.strip()
        absolute = (workspace_root / relative).resolve() if not Path(relative).is_absolute() else Path(relative)
        entries[key] = WorkspaceLayoutEntry(key=key, relative=relative, absolute=absolute)

    return entries
