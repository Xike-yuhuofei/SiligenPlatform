from __future__ import annotations

import hashlib
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence


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


@dataclass(frozen=True)
class RequiredArtifactProbe:
    artifact_name: str
    resolved_path: Path | None
    searched_paths: tuple[Path, ...]


@dataclass(frozen=True)
class ControlAppsBuildReadiness:
    ready: bool
    selected_probe: BuildRootProbe | None
    required_artifacts: tuple[str, ...]
    artifact_probes: tuple[RequiredArtifactProbe, ...]
    missing_artifacts: tuple[str, ...]
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


def _matches_workspace(cmake_home_directory: str, workspace_root: Path) -> bool:
    if not cmake_home_directory:
        return False
    try:
        return Path(cmake_home_directory).resolve() == workspace_root.resolve()
    except OSError:
        return False


def _artifact_candidates(build_root: Path, artifact_name: str) -> tuple[Path, ...]:
    return (
        build_root / "bin" / artifact_name,
        build_root / "bin" / "Debug" / artifact_name,
        build_root / "bin" / "Release" / artifact_name,
        build_root / "bin" / "RelWithDebInfo" / artifact_name,
    )


def _probe_required_artifact(probe: BuildRootProbe, artifact_name: str) -> RequiredArtifactProbe:
    candidates = _artifact_candidates(probe.root, artifact_name)
    resolved_path = next((candidate for candidate in candidates if candidate.exists()), None)
    return RequiredArtifactProbe(
        artifact_name=artifact_name,
        resolved_path=resolved_path,
        searched_paths=candidates,
    )


def _normalized_required_artifacts(required_artifacts: Sequence[str]) -> tuple[str, ...]:
    normalized: list[str] = []
    for artifact_name in required_artifacts:
        value = str(artifact_name).strip()
        if value and value not in normalized:
            normalized.append(value)
    return tuple(normalized)


def _probe_build_root(root: Path, *, source: str, workspace_root: Path, allow_stale: bool) -> BuildRootProbe:
    resolved_root = root.resolve()
    cache_path = resolved_root / "CMakeCache.txt"
    cmake_home_directory = _read_cmake_home_directory(cache_path)
    matches_workspace = _matches_workspace(cmake_home_directory, workspace_root)

    if allow_stale:
        accepted = True
        reason = ""
    elif not cache_path.exists():
        accepted = False
        reason = "missing-cmake-cache"
    elif not cmake_home_directory:
        accepted = False
        reason = "missing-cmake-home"
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
        return tuple(probes)

    add_probe(workspace_root / "build" / "ca", source="workspace-build-ca")
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


def probe_control_apps_build_readiness(
    workspace_root: Path,
    *,
    required_artifacts: Sequence[str],
    explicit_build_root: str | None = None,
) -> ControlAppsBuildReadiness:
    normalized_artifacts = _normalized_required_artifacts(required_artifacts)
    probes = control_apps_build_root_probes(workspace_root, explicit_build_root=explicit_build_root)
    selected_probe = preferred_control_apps_build_root(workspace_root, explicit_build_root=explicit_build_root)

    if explicit_build_root and not selected_probe.exists:
        return ControlAppsBuildReadiness(
            ready=False,
            selected_probe=selected_probe,
            required_artifacts=normalized_artifacts,
            artifact_probes=tuple(),
            missing_artifacts=normalized_artifacts,
            reason="explicit-build-root-missing",
        )

    if not selected_probe.accepted:
        return ControlAppsBuildReadiness(
            ready=False,
            selected_probe=selected_probe,
            required_artifacts=normalized_artifacts,
            artifact_probes=tuple(),
            missing_artifacts=normalized_artifacts,
            reason=selected_probe.reason or "build-root-rejected",
        )

    if not selected_probe.exists:
        return ControlAppsBuildReadiness(
            ready=False,
            selected_probe=selected_probe,
            required_artifacts=normalized_artifacts,
            artifact_probes=tuple(),
            missing_artifacts=normalized_artifacts,
            reason="build-root-missing",
        )

    artifact_probes = tuple(_probe_required_artifact(selected_probe, artifact_name) for artifact_name in normalized_artifacts)
    missing_artifacts = tuple(
        artifact_probe.artifact_name for artifact_probe in artifact_probes if artifact_probe.resolved_path is None
    )
    if missing_artifacts:
        return ControlAppsBuildReadiness(
            ready=False,
            selected_probe=selected_probe,
            required_artifacts=normalized_artifacts,
            artifact_probes=artifact_probes,
            missing_artifacts=missing_artifacts,
            reason="missing-required-artifacts",
        )

    return ControlAppsBuildReadiness(
        ready=True,
        selected_probe=selected_probe,
        required_artifacts=normalized_artifacts,
        artifact_probes=artifact_probes,
        missing_artifacts=tuple(),
        reason="",
    )
