from __future__ import annotations

import hashlib
import os
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class BuildRootProbe:
    root: Path
    source: str
    exists: bool
    cache_path: Path
    cache_present: bool
    cmake_home_directory: str
    matches_workspace: bool
    accepted: bool
    reason: str = ""


def workspace_build_token(workspace_root: Path) -> str:
    normalized_root = str(workspace_root.resolve()).lower().encode("utf-8")
    return hashlib.sha256(normalized_root).hexdigest()[:12]


def _read_cmake_home_directory(cache_path: Path) -> str:
    if not cache_path.exists():
        return ""
    for raw_line in cache_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        if raw_line.startswith("CMAKE_HOME_DIRECTORY:"):
            _, _, value = raw_line.partition("=")
            return value.strip()
    return ""


def _probe_build_root(root: Path, *, source: str, workspace_root: Path, allow_stale: bool) -> BuildRootProbe:
    resolved_root = root.resolve()
    cache_path = resolved_root / "CMakeCache.txt"
    cmake_home_directory = _read_cmake_home_directory(cache_path)
    workspace_home = str(workspace_root.resolve())
    matches_workspace = (not cmake_home_directory) or Path(cmake_home_directory).resolve() == workspace_root.resolve()

    if allow_stale:
        accepted = True
        reason = ""
    elif cache_path.exists() and not matches_workspace:
        accepted = False
        reason = f"stale-cmake-home:{cmake_home_directory}"
    else:
        accepted = True
        reason = ""

    return BuildRootProbe(
        root=resolved_root,
        source=source,
        exists=resolved_root.exists(),
        cache_path=cache_path,
        cache_present=cache_path.exists(),
        cmake_home_directory=cmake_home_directory,
        matches_workspace=matches_workspace,
        accepted=accepted,
        reason=reason,
    )


def control_apps_build_root_probes(
    workspace_root: Path,
    *,
    explicit_build_root: str | None = None,
) -> tuple[BuildRootProbe, ...]:
    probes: list[BuildRootProbe] = []
    seen: set[Path] = set()

    def add_probe(root: Path, *, source: str, allow_stale: bool = False) -> None:
        resolved_root = root.resolve()
        if resolved_root in seen:
            return
        seen.add(resolved_root)
        probes.append(
            _probe_build_root(
                resolved_root,
                source=source,
                workspace_root=workspace_root,
                allow_stale=allow_stale,
            )
        )

    if explicit_build_root:
        add_probe(Path(explicit_build_root), source="env", allow_stale=True)

    add_probe(workspace_root / "build" / "ca", source="workspace-build-ca")
    add_probe(workspace_root / "build" / "control-apps", source="workspace-build-control-apps")
    add_probe(workspace_root / "build", source="workspace-build")

    local_app_data = os.getenv("LOCALAPPDATA", "").strip()
    if local_app_data:
        ss_root = Path(local_app_data) / "SS"
        token_root = ss_root / f"cab-{workspace_build_token(workspace_root)}"
        add_probe(token_root, source="workspace-token-build")
        if ss_root.exists():
            for candidate in sorted(ss_root.glob("cab-*")):
                add_probe(candidate, source="workspace-cab-build")
        add_probe(Path(local_app_data) / "SiligenSuite" / "control-apps-build", source="legacy-control-apps-build")
    return tuple(probes)


def valid_control_apps_build_roots(
    workspace_root: Path,
    *,
    explicit_build_root: str | None = None,
) -> tuple[Path, ...]:
    probes = control_apps_build_root_probes(workspace_root, explicit_build_root=explicit_build_root)
    return tuple(probe.root for probe in probes if probe.accepted)


def preferred_control_apps_build_root(
    workspace_root: Path,
    *,
    explicit_build_root: str | None = None,
) -> BuildRootProbe:
    probes = control_apps_build_root_probes(workspace_root, explicit_build_root=explicit_build_root)
    existing_accepted = [probe for probe in probes if probe.accepted and probe.exists]
    if existing_accepted:
        with_matching_cache = [probe for probe in existing_accepted if probe.cache_present and probe.matches_workspace]
        if with_matching_cache:
            return with_matching_cache[0]
        return existing_accepted[0]
    accepted = [probe for probe in probes if probe.accepted]
    if accepted:
        return accepted[0]
    return probes[0]
