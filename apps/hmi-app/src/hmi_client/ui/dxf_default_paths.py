from __future__ import annotations

import os
from pathlib import Path


def _normalize_candidate(path: Path) -> Path:
    return path.expanduser().resolve()


def _resolve_workspace_root(current_file: Path) -> Path | None:
    workspace_override = os.getenv("SILIGEN_WORKSPACE_ROOT", "").strip()
    if workspace_override:
        return _normalize_candidate(Path(workspace_override))

    for candidate in current_file.resolve().parents:
        if (candidate / "WORKSPACE.md").exists() and (candidate / "cmake" / "workspace-layout.env").exists():
            return candidate
    return None


def build_default_dxf_candidates(current_file: Path) -> list[Path]:
    candidates: list[Path] = []
    seen: set[Path] = set()
    default_dir_override = os.getenv("SILIGEN_DXF_DEFAULT_DIR", "").strip()
    if default_dir_override:
        candidate = _normalize_candidate(Path(default_dir_override))
        candidates.append(candidate)
        seen.add(candidate)

    workspace_root = _resolve_workspace_root(current_file)
    if workspace_root is not None:
        for candidate in (
            workspace_root / "uploads" / "dxf",
            workspace_root / "shared" / "contracts" / "engineering" / "fixtures" / "cases" / "rect_diag",
        ):
            normalized = _normalize_candidate(candidate)
            if normalized in seen:
                continue
            candidates.append(normalized)
            seen.add(normalized)
    return candidates
