from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class LayoutEntry:
    key: str
    relative: str
    absolute: Path


def _layout_file(workspace_root: Path) -> Path:
    return workspace_root / "cmake" / "workspace-layout.env"


def load_workspace_layout(workspace_root: Path) -> dict[str, LayoutEntry]:
    layout_file = _layout_file(workspace_root)
    if not layout_file.exists():
        raise FileNotFoundError(f"workspace layout file missing: {layout_file}")

    entries: dict[str, LayoutEntry] = {}
    for raw_line in layout_file.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        key, value = line.split("=", 1)
        relative = value.strip()
        absolute = Path(relative) if Path(relative).is_absolute() else (workspace_root / relative)
        entries[key.strip()] = LayoutEntry(
            key=key.strip(),
            relative=relative,
            absolute=absolute.resolve(),
        )
    return entries
