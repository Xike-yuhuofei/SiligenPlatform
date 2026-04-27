from __future__ import annotations

import argparse
import datetime as _dt
import fnmatch
import json
import os
import re
from collections import Counter
from pathlib import Path
from typing import Any


TARGET_RE = re.compile(r"\b(?:add_library|add_executable|add_custom_target)\s*\(\s*([A-Za-z0-9_.:+-]+)", re.I)
LINK_BLOCK_RE = re.compile(r"\b(?:target_link_libraries|add_dependencies)\s*\((.*?)\)", re.I | re.S)
PATH_DEPENDENCY_BLOCK_RE = re.compile(
    r"\b(?:add_subdirectory|target_include_directories|target_sources)\s*\((.*?)\)",
    re.I | re.S,
)
INCLUDE_RE = re.compile(r"^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]", re.M)
SILIGEN_TOKEN_RE = re.compile(r"\bsiligen_[A-Za-z0-9_]+\b")
SKIP_LINK_TOKENS = {
    "PUBLIC",
    "PRIVATE",
    "INTERFACE",
    "LINK_PUBLIC",
    "LINK_PRIVATE",
    "debug",
    "optimized",
    "general",
}


def utc_now() -> str:
    return _dt.datetime.now(_dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def norm_path(value: str | Path) -> str:
    text = str(value).replace("\\", "/").strip()
    while text.startswith("./"):
        text = text[2:]
    return text.strip("/")


def path_key(value: str | Path) -> str:
    return norm_path(value).casefold()


def is_under(path: str, root: str) -> bool:
    p = path_key(path)
    r = path_key(root)
    return p == r or p.startswith(r + "/")


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(path: Path, payload: Any) -> None:
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


class AuditLog:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_text("", encoding="utf-8")

    def write(self, message: str) -> None:
        with self.path.open("a", encoding="utf-8") as handle:
            handle.write(f"{utc_now()} {message}\n")


class BoundaryModel:
    def __init__(self, workspace: Path, boundary: dict[str, Any], bridges: dict[str, Any], policy_file: dict[str, Any], mode: str) -> None:
        self.workspace = workspace
        self.boundary = boundary
        self.modules = boundary.get("modules", [])
        self.external_roots = [norm_path(item) for item in boundary.get("external_roots", [])]
        self.bridges = bridges.get("bridges", [])
        self.policy = self._resolve_policy(policy_file, mode)
        self.mode = mode
        self.coverage = self.policy.get("coverage_requirements", {})
        self.module_by_id = {module["id"]: module for module in self.modules}
        self.module_roots: list[tuple[str, str]] = []
        for module in self.modules:
            for root in module.get("roots", []):
                self.module_roots.append((norm_path(root), module["id"]))
        self.module_roots.sort(key=lambda item: len(item[0]), reverse=True)
        self.public_targets = self._surface_targets("public_surfaces")
        self.public_paths = self._surface_paths("public_surfaces")
        self.private_paths = self._surface_paths("private_surfaces")
        self.bridge_targets = self._bridge_targets()
        self.bridge_by_target = self._bridge_by_target()

    def _resolve_policy(self, policy_file: dict[str, Any], mode: str) -> dict[str, Any]:
        policies = {policy["mode"]: policy for policy in policy_file.get("policies", [])}
        if mode not in policies:
            raise ValueError(f"missing boundary policy for mode: {mode}")
        selected = json.loads(json.dumps(policies[mode]))
        inherit = selected.get("coverage_requirements", {}).get("inherit_from")
        if inherit:
            base = json.loads(json.dumps(policies[inherit].get("coverage_requirements", {})))
            base.update({k: v for k, v in selected["coverage_requirements"].items() if k != "inherit_from"})
            selected["coverage_requirements"] = base
        return selected

    def _surface_targets(self, field: str) -> dict[str, str]:
        result: dict[str, str] = {}
        for module in self.modules:
            for surface in module.get(field, []):
                text = str(surface)
                if "/" not in text and "\\" not in text and "." not in text:
                    result[text] = module["id"]
        return result

    def _surface_paths(self, field: str) -> list[tuple[str, str]]:
        result: list[tuple[str, str]] = []
        for module in self.modules:
            for surface in module.get(field, []):
                text = norm_path(str(surface))
                if "/" in text or "\\" in str(surface):
                    result.append((text, module["id"]))
        result.sort(key=lambda item: len(item[0]), reverse=True)
        return result

    def _bridge_targets(self) -> set[str]:
        targets: set[str] = set()
        for bridge in self.bridges:
            targets.update(str(item) for item in bridge.get("allowed_targets", []))
        return targets

    def _bridge_by_target(self) -> dict[str, list[dict[str, Any]]]:
        result: dict[str, list[dict[str, Any]]] = {}
        for bridge in self.bridges:
            for target in bridge.get("allowed_targets", []):
                result.setdefault(str(target), []).append(bridge)
        return result

    def owner_for_path(self, rel_path: str) -> str:
        rel = norm_path(rel_path)
        for root, owner in self.module_roots:
            if is_under(rel, root):
                return owner
        for root in self.external_roots:
            if is_under(rel, root):
                return f"external:{root}"
        return "unknown"

    def is_external_owner(self, owner: str) -> bool:
        return owner.startswith("external:")

    def dependency_allowed(self, source_owner: str, target_owner: str) -> bool:
        if source_owner == target_owner:
            return True
        if source_owner not in self.module_by_id:
            return False
        allowed = self.module_by_id[source_owner].get("allowed_dependencies", [])
        return any(self._dependency_pattern_matches(pattern, target_owner) for pattern in allowed)

    def dependency_forbidden(self, source_owner: str, target_owner: str) -> bool:
        if source_owner not in self.module_by_id:
            return False
        forbidden = self.module_by_id[source_owner].get("forbidden_dependencies", [])
        return any(self._dependency_pattern_matches(pattern, target_owner) for pattern in forbidden)

    def _dependency_pattern_matches(self, pattern: str, target_owner: str) -> bool:
        return fnmatch.fnmatchcase(target_owner, pattern)

    def bridge_allowed_for_file(self, target: str, rel_file: str) -> dict[str, Any] | None:
        for bridge in self.bridge_by_target.get(target, []):
            if any(path_key(rel_file) == path_key(item) for item in bridge.get("allowed_files", [])):
                return bridge
        return None

    def target_is_registered_bridge(self, target: str) -> bool:
        return target in self.bridge_targets


def validate_model(model: BoundaryModel) -> list[dict[str, Any]]:
    findings: list[dict[str, Any]] = []
    module_ids = set()
    roots = set()
    for module in model.modules:
        missing = [
            key
            for key in (
                "id",
                "owner",
                "roots",
                "public_surfaces",
                "private_surfaces",
                "allowed_dependencies",
                "forbidden_dependencies",
            )
            if key not in module
        ]
        if missing:
            findings.append(finding("model_error", "module-schema", "", module.get("id", ""), f"missing module fields: {missing}"))
        if module.get("id") in module_ids:
            findings.append(finding("model_error", "duplicate-module-id", "", module.get("id", ""), "duplicate module id"))
        module_ids.add(module.get("id"))
        for root in module.get("roots", []):
            root_key = path_key(root)
            if root_key in roots:
                findings.append(finding("model_error", "duplicate-module-root", norm_path(root), module.get("id", ""), "duplicate module root"))
            roots.add(root_key)
            if model.owner_for_path(root) != module.get("id"):
                findings.append(
                    finding("owner_mapping_missing", "module-root-owner-mapping", norm_path(root), module.get("id", ""), "module root does not map to its owner")
                )
    for bridge in model.bridges:
        missing_bridge = [
            key
            for key in (
                "id",
                "owner",
                "consumer",
                "reason",
                "allowed_files",
                "allowed_targets",
                "expiry_condition",
                "cleanup_ticket",
            )
            if key not in bridge or bridge.get(key) in ("", [], None)
        ]
        if missing_bridge:
            findings.append(finding("model_error", "bridge-schema", "", bridge.get("id", ""), f"missing bridge fields: {missing_bridge}"))
    return findings


def finding(kind: str, rule_id: str, file_path: str, owner: str, detail: str, **extra: Any) -> dict[str, Any]:
    payload = {
        "kind": kind,
        "rule_id": rule_id,
        "file": norm_path(file_path) if file_path else "",
        "owner": owner,
        "detail": detail,
    }
    payload.update(extra)
    return payload


def is_supported(rel_path: str, supported: list[str]) -> bool:
    path = Path(rel_path)
    if path.name == "CMakeLists.txt" and "CMakeLists.txt" in supported:
        return True
    suffix = path.suffix
    return suffix in supported


def iter_repository_files(workspace: Path, supported: list[str]) -> list[str]:
    roots = ["apps", "modules", "shared", "scripts", ".github"]
    excluded = {
        ".git",
        "build",
        "tests/reports",
        ".pytest_cache",
        "__pycache__",
    }
    files: list[str] = []
    for root in roots:
        base = workspace / root
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if not path.is_file():
                continue
            rel = norm_path(path.relative_to(workspace))
            if any(is_under(rel, item) for item in excluded):
                continue
            if is_supported(rel, supported):
                files.append(rel)
    return sorted(files)


def expand_changed_files(values: list[str]) -> list[str]:
    result: list[str] = []
    for value in values:
        for item in str(value).split(","):
            rel = norm_path(item)
            if rel:
                result.append(rel)
    seen = set()
    unique: list[str] = []
    for item in result:
        key = path_key(item)
        if key not in seen:
            unique.append(item)
            seen.add(key)
    return unique


def collect_scan_files(workspace: Path, model: BoundaryModel, changed_files: list[str]) -> tuple[list[str], list[str]]:
    supported = model.coverage.get("supported_files", [])
    if model.mode == "pr":
        scan_files = [
            rel
            for rel in changed_files
            if is_supported(rel, supported) and (workspace / rel).is_file()
        ]
        return sorted(scan_files), changed_files
    return iter_repository_files(workspace, supported), changed_files


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig", errors="ignore")


def strip_cmake_comments(text: str) -> str:
    lines = []
    for line in text.splitlines():
        if "#" in line:
            line = line.split("#", 1)[0]
        lines.append(line)
    return "\n".join(lines)


def split_cmake_tokens(block: str) -> list[str]:
    raw = re.split(r"[\s\r\n]+", block.replace("(", " ").replace(")", " "))
    tokens: list[str] = []
    for token in raw:
        token = token.strip().strip('"').strip("'")
        if not token or token in SKIP_LINK_TOKENS:
            continue
        if token.startswith("${") or token.startswith("$<"):
            inner = re.search(r":([A-Za-z0-9_.:+-]+)>", token)
            if inner:
                token = inner.group(1)
            else:
                continue
        if token.startswith("-") or "/" in token:
            continue
        tokens.append(token)
    return tokens


def discover_declared_targets(workspace: Path, model: BoundaryModel, log: AuditLog) -> dict[str, str]:
    supported = model.coverage.get("supported_files", [])
    targets: dict[str, str] = dict(model.public_targets)
    for rel in iter_repository_files(workspace, supported):
        if not (rel.endswith(".cmake") or Path(rel).name == "CMakeLists.txt"):
            continue
        text = read_text(workspace / rel)
        owner = model.owner_for_path(rel)
        for match in TARGET_RE.finditer(strip_cmake_comments(text)):
            target = match.group(1)
            if target and not target.startswith("$"):
                targets.setdefault(target, owner)
    log.write(f"declared_targets={len(targets)}")
    return targets


def target_is_public(model: BoundaryModel, target: str) -> bool:
    return target in model.public_targets


def target_is_external(model: BoundaryModel, target: str) -> bool:
    prefixes = model.coverage.get("external_target_prefixes", [])
    return any(target.startswith(prefix) for prefix in prefixes)


def target_is_bridge_like(model: BoundaryModel, target: str) -> bool:
    return any(pattern in target for pattern in model.coverage.get("bridge_like_patterns", []))


def evaluate_required_owner_targets(workspace: Path, model: BoundaryModel, changed_files: list[str]) -> list[dict[str, Any]]:
    findings: list[dict[str, Any]] = []
    changed = {path_key(item) for item in changed_files}
    for requirement in model.coverage.get("required_owner_targets", []):
        rel = norm_path(requirement["path"])
        if model.mode == "pr" and changed and path_key(rel) not in changed:
            continue
        if model.mode == "pr" and not changed:
            continue
        path = workspace / rel
        if not path.is_file():
            findings.append(
                finding(
                    "missing_required_owner_target",
                    requirement["id"],
                    rel,
                    requirement.get("owner", ""),
                    f"required owner target file is missing: {rel}",
                    expected_pattern=requirement.get("pattern", ""),
                )
            )
            continue
        text = read_text(path)
        if requirement.get("pattern", "") not in text:
            findings.append(
                finding(
                    "missing_required_owner_target",
                    requirement["id"],
                    rel,
                    requirement.get("owner", ""),
                    "required owner target pattern is missing",
                    expected_pattern=requirement.get("pattern", ""),
                )
            )
    return findings


def evaluate_retired_surfaces(workspace: Path, model: BoundaryModel, scan_files: list[str], changed_files: list[str]) -> list[dict[str, Any]]:
    findings: list[dict[str, Any]] = []
    changed = {path_key(item) for item in changed_files}
    for retired in model.coverage.get("retired_surfaces", []):
        rel = norm_path(retired["path"])
        should_check_path = model.mode != "pr" or path_key(rel) in changed
        if should_check_path and (workspace / rel).exists():
            findings.append(
                finding(
                    "retired_surface_reintroduced",
                    retired["id"],
                    rel,
                    retired.get("owner", ""),
                    f"retired surface exists: {rel}",
                )
            )
        needles = {rel}
        for item in retired.get("forbidden_references", []):
            needles.add(str(item))
        for scan_file in scan_files:
            text = read_text(workspace / scan_file)
            if any(needle in text for needle in needles):
                findings.append(
                    finding(
                        "retired_surface_reintroduced",
                        retired["id"],
                        scan_file,
                        model.owner_for_path(scan_file),
                        f"scan file references retired surface: {rel}",
                        retired_surface=rel,
                    )
                )
    return findings


def evaluate_private_surface_access(workspace: Path, model: BoundaryModel, scan_file: str, text: str) -> list[dict[str, Any]]:
    findings: list[dict[str, Any]] = []
    source_owner = model.owner_for_path(scan_file)
    is_cmake = scan_file.endswith(".cmake") or Path(scan_file).name == "CMakeLists.txt"
    if not is_cmake:
        return findings
    dependency_text = "\n".join(PATH_DEPENDENCY_BLOCK_RE.findall(strip_cmake_comments(text)))
    for private_surface, target_owner in model.private_paths:
        if target_owner == source_owner:
            continue
        if private_surface in dependency_text:
            findings.append(
                finding(
                    "private_surface_access",
                    "private-surface-access",
                    scan_file,
                    source_owner,
                    f"{scan_file} references private surface {private_surface} owned by {target_owner}",
                    dependency_owner=target_owner,
                    dependency=private_surface,
                )
            )
    return findings


def evaluate_include_dependency(model: BoundaryModel, scan_file: str, include_norm: str, source_owner: str) -> dict[str, Any] | None:
    for public_surface, target_owner in model.public_paths:
        if (
            public_surface.endswith(include_norm)
            or include_norm in public_surface
            or is_under(include_norm, public_surface)
        ):
            if target_owner == source_owner:
                return None
            if model.dependency_forbidden(source_owner, target_owner):
                return finding(
                    "forbidden_dependency",
                    "forbidden-public-include-dependency",
                    scan_file,
                    source_owner,
                    f"{source_owner} is forbidden to include public surface owned by {target_owner}",
                    dependency=include_norm,
                    dependency_owner=target_owner,
                )
            if model.dependency_allowed(source_owner, target_owner):
                return finding(
                    "allowed_dependency",
                    "allowed-public-include-dependency",
                    scan_file,
                    source_owner,
                    f"{source_owner} includes public surface owned by {target_owner}",
                    dependency=include_norm,
                    dependency_owner=target_owner,
                )
            return finding(
                "unknown_dependency",
                "public-include-dependency-not-allowed-by-owner-model",
                scan_file,
                source_owner,
                f"{source_owner} include dependency on {target_owner} is not declared in allowed_dependencies",
                dependency=include_norm,
                dependency_owner=target_owner,
            )
    return None


def bridge_status_for_target(model: BoundaryModel, target: str, scan_file: str) -> tuple[str, dict[str, Any] | None]:
    bridge = model.bridge_allowed_for_file(target, scan_file)
    if bridge is not None:
        return "allowed", bridge
    if model.target_is_registered_bridge(target):
        return "unauthorized", None
    return "unregistered", None


def evaluate_target_reference(
    model: BoundaryModel,
    target_owners: dict[str, str],
    scan_file: str,
    target: str,
    observed_bridges: set[str],
) -> dict[str, Any]:
    source_owner = model.owner_for_path(scan_file)
    forbidden_targets = model.coverage.get("forbidden_targets", [])
    for forbidden in forbidden_targets:
        if forbidden.get("pattern") == target:
            return finding(
                "forbidden_dependency",
                forbidden.get("id", "forbidden-target"),
                scan_file,
                source_owner,
                f"forbidden target reference: {target}",
                dependency=target,
                dependency_owner=forbidden.get("owner", ""),
            )

    direct_targets = set(model.coverage.get("direct_workflow_targets", []))
    if target in direct_targets:
        if target_is_public(model, target) and model.public_targets.get(target) == source_owner:
            return finding(
                "allowed_dependency",
                "owner-public-target-definition",
                scan_file,
                source_owner,
                f"owner references its own public workflow target: {target}",
                dependency=target,
                dependency_owner=source_owner,
            )
        status, bridge = bridge_status_for_target(model, target, scan_file)
        if status == "allowed" and bridge is not None:
            observed_bridges.add(bridge["id"])
            return finding(
                "registered_bridge_observed",
                bridge["id"],
                scan_file,
                source_owner,
                f"registered bridge target observed: {target}",
                dependency=target,
                bridge_id=bridge["id"],
            )
        return finding(
            "direct_workflow_target_reference",
            "direct-workflow-target-reference",
            scan_file,
            source_owner,
            f"direct workflow target is not registered for this file: {target}",
            dependency=target,
        )

    if model.target_is_registered_bridge(target):
        status, bridge = bridge_status_for_target(model, target, scan_file)
        if status == "allowed" and bridge is not None:
            observed_bridges.add(bridge["id"])
            return finding(
                "registered_bridge_observed",
                bridge["id"],
                scan_file,
                source_owner,
                f"registered bridge target observed: {target}",
                dependency=target,
                bridge_id=bridge["id"],
            )
        return finding(
            "unauthorized_bridge",
            "unauthorized-registered-bridge",
            scan_file,
            source_owner,
            f"registered bridge target is not allowed in this file: {target}",
            dependency=target,
        )

    if target_is_external(model, target):
        return finding("external_dependency", "external-target", scan_file, source_owner, f"external target: {target}", dependency=target)

    if target in target_owners:
        dependency_owner = target_owners[target]
        if model.is_external_owner(source_owner) or source_owner == "unknown":
            return finding("allowed_dependency", "external-source", scan_file, source_owner, f"external source target reference: {target}", dependency=target)
        if model.dependency_forbidden(source_owner, dependency_owner):
            return finding(
                "forbidden_dependency",
                "forbidden-module-dependency",
                scan_file,
                source_owner,
                f"{source_owner} is forbidden to depend on {dependency_owner}",
                dependency=target,
                dependency_owner=dependency_owner,
            )
        if source_owner != dependency_owner and not target_is_public(model, target):
            return finding(
                "private_surface_access",
                "private-target-access",
                scan_file,
                source_owner,
                f"{source_owner} references private target {target} owned by {dependency_owner}",
                dependency=target,
                dependency_owner=dependency_owner,
            )
        if model.dependency_allowed(source_owner, dependency_owner):
            return finding(
                "allowed_dependency",
                "allowed-module-dependency",
                scan_file,
                source_owner,
                f"{source_owner} depends on {dependency_owner}",
                dependency=target,
                dependency_owner=dependency_owner,
            )
        return finding(
            "unknown_dependency",
            "dependency-not-allowed-by-owner-model",
            scan_file,
            source_owner,
            f"{source_owner} dependency on {dependency_owner} is not declared in allowed_dependencies",
            dependency=target,
            dependency_owner=dependency_owner,
        )

    if target_is_bridge_like(model, target):
        return finding(
            "unregistered_bridge",
            "unregistered-bridge-reference",
            scan_file,
            source_owner,
            f"bridge-like target is not registered: {target}",
            dependency=target,
        )

    if target.startswith("siligen_"):
        return finding(
            "unknown_dependency",
            "unknown-siligen-target",
            scan_file,
            source_owner,
            f"siligen target is not declared, public, external, forbidden, or registered bridge: {target}",
            dependency=target,
        )

    return finding("external_dependency", "untracked-non-siligen-target", scan_file, source_owner, f"untracked non-siligen target: {target}", dependency=target)


def extract_targets_from_file(workspace: Path, model: BoundaryModel, rel_file: str) -> set[str]:
    text = read_text(workspace / rel_file)
    targets: set[str] = set()
    is_cmake = rel_file.endswith(".cmake") or Path(rel_file).name == "CMakeLists.txt"
    if is_cmake:
        clean = strip_cmake_comments(text)
        for block in LINK_BLOCK_RE.findall(clean):
            tokens = split_cmake_tokens(block)
            if len(tokens) <= 1:
                continue
            for token in tokens[1:]:
                targets.add(token)
    else:
        for forbidden in model.coverage.get("forbidden_targets", []):
            pattern = forbidden.get("pattern", "")
            if pattern and pattern in text:
                targets.add(pattern)
        for direct_target in model.coverage.get("direct_workflow_targets", []):
            if direct_target in text:
                negative_assertion_patterns = (
                    f'find("{direct_target}")',
                    f"find('{direct_target}')",
                    f"NotIn(\"{direct_target}\"",
                    f"NotIn('{direct_target}'",
                    f"not in text",
                )
                if any(pattern in text for pattern in negative_assertion_patterns):
                    continue
                targets.add(direct_target)
    return targets


def evaluate_scan_files(workspace: Path, model: BoundaryModel, scan_files: list[str], target_owners: dict[str, str], log: AuditLog) -> tuple[list[dict[str, Any]], set[str]]:
    findings: list[dict[str, Any]] = []
    observed_bridges: set[str] = set()
    for rel_file in scan_files:
        source_owner = model.owner_for_path(rel_file)
        if source_owner == "unknown" and any(is_under(rel_file, root) for root in ("apps", "modules", "shared")):
            findings.append(finding("owner_mapping_missing", "scan-file-owner-mapping", rel_file, source_owner, "scan file under canonical root has no owner mapping"))
        text = read_text(workspace / rel_file)
        findings.extend(evaluate_private_surface_access(workspace, model, rel_file, text))
        for include in INCLUDE_RE.findall(text):
            include_norm = norm_path(include)
            include_dependency = evaluate_include_dependency(model, rel_file, include_norm, source_owner)
            if include_dependency is not None:
                findings.append(include_dependency)
            for private_surface, target_owner in model.private_paths:
                if (
                    private_surface.endswith(include_norm)
                    or include_norm in private_surface
                    or is_under(include_norm, private_surface)
                ):
                    if target_owner != source_owner:
                        findings.append(
                            finding(
                                "private_surface_access",
                                "private-include-access",
                                rel_file,
                                source_owner,
                                f"include references private surface {private_surface} owned by {target_owner}",
                                dependency=include_norm,
                                dependency_owner=target_owner,
                            )
                        )
        targets = extract_targets_from_file(workspace, model, rel_file)
        for bridge_target in model.bridge_targets:
            if rel_file.endswith(".cmake") or Path(rel_file).name == "CMakeLists.txt":
                continue
            if "/" not in bridge_target:
                continue
            if bridge_target in text:
                targets.add(bridge_target)
        for target in sorted(targets):
            findings.append(evaluate_target_reference(model, target_owners, rel_file, target, observed_bridges))
    log.write(f"scan_files={len(scan_files)} findings={len(findings)} observed_bridges={len(observed_bridges)}")
    return findings, observed_bridges


def classify_findings(findings: list[dict[str, Any]], policy: dict[str, Any]) -> tuple[list[dict[str, Any]], list[dict[str, Any]], list[dict[str, Any]]]:
    blocking_kinds = set(policy.get("blocking_findings", []))
    advisory_kinds = set(policy.get("advisory_findings", []))
    blocking: list[dict[str, Any]] = []
    advisory: list[dict[str, Any]] = []
    observations: list[dict[str, Any]] = []
    for item in findings:
        kind = item["kind"]
        if kind in blocking_kinds:
            blocking.append(item)
        elif kind in advisory_kinds:
            advisory.append(item)
        else:
            observations.append(item)
    return blocking, advisory, observations


def build_bridge_status(model: BoundaryModel, observed_bridges: set[str]) -> dict[str, Any]:
    bridge_items = []
    for bridge in model.bridges:
        expiry = bridge.get("expiry_condition", "")
        status = "expired" if str(expiry).lower().startswith("expired:") else "active"
        bridge_items.append(
            {
                "id": bridge["id"],
                "owner": bridge["owner"],
                "consumer": bridge["consumer"],
                "cleanup_ticket": bridge["cleanup_ticket"],
                "expiry_condition": expiry,
                "status": status,
                "observed": bridge["id"] in observed_bridges,
                "allowed_files": bridge.get("allowed_files", []),
                "allowed_targets": bridge.get("allowed_targets", []),
            }
        )
    return {
        "schemaVersion": 1,
        "generated_at": utc_now(),
        "registered_count": len(bridge_items),
        "observed_count": sum(1 for item in bridge_items if item["observed"]),
        "expired_count": sum(1 for item in bridge_items if item["status"] == "expired"),
        "bridges": bridge_items,
    }


def build_coverage(model: BoundaryModel, changed_files: list[str], scan_files: list[str], findings: list[dict[str, Any]]) -> dict[str, Any]:
    owners = Counter(model.owner_for_path(path) for path in scan_files)
    kinds = Counter(item["kind"] for item in findings)
    module_roots = []
    for module in model.modules:
        for root in module.get("roots", []):
            module_roots.append(
                {
                    "root": norm_path(root),
                    "owner": module["id"],
                    "mapped_owner": model.owner_for_path(root),
                    "exists": (model.workspace / norm_path(root)).exists(),
                }
            )
    return {
        "schemaVersion": 1,
        "generated_at": utc_now(),
        "mode": model.mode,
        "changed_file_count": len(changed_files),
        "scanned_file_count": len(scan_files),
        "owner_file_counts": dict(sorted(owners.items())),
        "finding_counts": dict(sorted(kinds.items())),
        "module_root_coverage": module_roots,
    }


def write_markdown(path: Path, audit: dict[str, Any], blocking: list[dict[str, Any]], advisory: list[dict[str, Any]]) -> None:
    lines = [
        "# Module Boundary Audit",
        "",
        f"- generated_at: {audit['generated_at']}",
        f"- mode: {audit['mode']}",
        f"- status: {audit['status']}",
        f"- changed_files: {len(audit['changed_files'])}",
        f"- scanned_files: {len(audit['scanned_files'])}",
        f"- blocking_findings: {len(blocking)}",
        f"- advisory_findings: {len(advisory)}",
        "",
        "## Blocking Findings",
        "",
    ]
    if not blocking:
        lines.append("- none")
    else:
        for item in blocking:
            lines.append(f"- {item['kind']} `{item['rule_id']}` {item.get('file', '')}: {item['detail']}")
    lines.extend(["", "## Advisory Findings", ""])
    if not advisory:
        lines.append("- none")
    else:
        for item in advisory:
            lines.append(f"- {item['kind']} `{item['rule_id']}` {item.get('file', '')}: {item['detail']}")
    lines.extend(["", "## Reports", ""])
    for name in (
        "module-boundary-audit.json",
        "module-boundary-coverage.json",
        "bridge-registry-status.json",
    ):
        lines.append(f"- `{name}`")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def classify_triage_category(item: dict[str, Any]) -> str:
    kind = item.get("kind", "")
    dependency = str(item.get("dependency", ""))
    file_path = str(item.get("file", ""))
    if "quarantine" in dependency or "quarantine" in file_path:
        return "quarantine_exit"
    if kind in {"unknown_dependency", "owner_mapping_missing", "model_error", "missing_required_owner_target"}:
        return "model_gap"
    if kind == "direct_workflow_target_reference":
        return "workflow_legacy"
    if "/tests/" in file_path or file_path.endswith("/tests/CMakeLists.txt"):
        return "test_infra_gap"
    return "real_violation"


def build_triage(blocking: list[dict[str, Any]]) -> dict[str, Any]:
    items = []
    counts: Counter[str] = Counter()
    for index, item in enumerate(blocking, start=1):
        category = classify_triage_category(item)
        counts[category] += 1
        items.append(
            {
                "index": index,
                "category": category,
                "kind": item.get("kind", ""),
                "rule_id": item.get("rule_id", ""),
                "file": item.get("file", ""),
                "owner": item.get("owner", ""),
                "dependency": item.get("dependency", ""),
                "dependency_owner": item.get("dependency_owner", ""),
                "detail": item.get("detail", ""),
            }
        )
    return {
        "schemaVersion": 1,
        "generated_at": utc_now(),
        "blocking_count": len(blocking),
        "category_counts": dict(sorted(counts.items())),
        "items": items,
    }


def write_triage_markdown(path: Path, triage: dict[str, Any]) -> None:
    lines = [
        "# Boundary Findings Triage",
        "",
        f"- generated_at: {triage['generated_at']}",
        f"- blocking_count: {triage['blocking_count']}",
        "",
        "## Category Counts",
        "",
    ]
    if not triage["category_counts"]:
        lines.append("- none")
    else:
        for category, count in triage["category_counts"].items():
            lines.append(f"- `{category}`: {count}")
    lines.extend(["", "## Items", ""])
    if not triage["items"]:
        lines.append("- none")
    else:
        for item in triage["items"]:
            lines.append(
                f"- #{item['index']} `{item['category']}` `{item['kind']}` "
                f"{item['file']}: {item['detail']}"
            )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def run(args: argparse.Namespace) -> int:
    workspace = Path(args.workspace_root).resolve()
    report_dir = Path(args.report_dir).resolve()
    report_dir.mkdir(parents=True, exist_ok=True)
    logs_dir = report_dir / "logs"
    logs_dir.mkdir(parents=True, exist_ok=True)

    load_log = AuditLog(logs_dir / "model-loader.log")
    extract_log = AuditLog(logs_dir / "dependency-extractor.log")
    eval_log = AuditLog(logs_dir / "policy-evaluator.log")

    boundary = load_json(Path(args.boundary_file))
    bridges = load_json(Path(args.bridge_file))
    policy_file = load_json(Path(args.policy_file))
    model = BoundaryModel(workspace, boundary, bridges, policy_file, args.mode)
    changed_files = expand_changed_files(args.changed_file)

    load_log.write(f"workspace={workspace}")
    load_log.write(f"mode={args.mode}")
    load_log.write(f"modules={len(model.modules)} bridges={len(model.bridges)}")

    scan_files, changed_files = collect_scan_files(workspace, model, changed_files)
    target_owners = discover_declared_targets(workspace, model, extract_log)

    findings: list[dict[str, Any]] = []
    findings.extend(validate_model(model))
    if args.mode == "pr" and not scan_files:
        findings.append(
            finding(
                "no_boundary_relevant_changed_files",
                "no-boundary-relevant-changed-files",
                "",
                "",
                "PR audit received no changed CMake/C++/PowerShell files to scan",
            )
        )
    findings.extend(evaluate_required_owner_targets(workspace, model, changed_files))
    findings.extend(evaluate_retired_surfaces(workspace, model, scan_files, changed_files))
    scan_findings, observed_bridges = evaluate_scan_files(workspace, model, scan_files, target_owners, extract_log)
    findings.extend(scan_findings)

    bridge_status = build_bridge_status(model, observed_bridges)
    if args.mode == "nightly":
        for bridge in bridge_status["bridges"]:
            if bridge["status"] == "expired":
                findings.append(
                    finding(
                        "expired_bridge",
                        bridge["id"],
                        "",
                        bridge["owner"],
                        f"registered bridge is expired: {bridge['expiry_condition']}",
                        bridge_id=bridge["id"],
                    )
                )
            if not bridge["observed"]:
                findings.append(
                    finding(
                        "unused_registered_bridge",
                        bridge["id"],
                        "",
                        bridge["owner"],
                        "registered bridge was not observed in nightly repository scan",
                        bridge_id=bridge["id"],
                    )
                )

    blocking, advisory, observations = classify_findings(findings, model.policy)
    status = "passed" if not blocking else "failed"
    eval_log.write(f"status={status} blocking={len(blocking)} advisory={len(advisory)} observations={len(observations)}")

    audit = {
        "schemaVersion": 1,
        "generated_at": utc_now(),
        "mode": args.mode,
        "status": status,
        "workspace_root": str(workspace),
        "changed_files": changed_files,
        "scanned_files": scan_files,
        "blocking_findings": blocking,
        "advisory_findings": advisory,
        "observations": observations,
        "policy": {
            "blocking_findings": model.policy.get("blocking_findings", []),
            "advisory_findings": model.policy.get("advisory_findings", []),
        },
        "reports": {
            "coverage": "module-boundary-coverage.json",
            "bridge_registry_status": "bridge-registry-status.json",
            "triage": "boundary-findings-triage.json",
            "logs": "logs/",
        },
    }
    coverage = build_coverage(model, changed_files, scan_files, findings)
    triage = build_triage(blocking)

    write_json(report_dir / "module-boundary-audit.json", audit)
    write_markdown(report_dir / "module-boundary-audit.md", audit, blocking, advisory)
    write_json(report_dir / "module-boundary-coverage.json", coverage)
    write_json(report_dir / "bridge-registry-status.json", bridge_status)
    write_json(report_dir / "boundary-findings-triage.json", triage)
    write_triage_markdown(report_dir / "boundary-findings-triage.md", triage)

    return 0 if status == "passed" else 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Model-driven module boundary audit")
    parser.add_argument("--mode", choices=("pr", "full", "nightly"), required=True)
    parser.add_argument("--workspace-root", required=True)
    parser.add_argument("--report-dir", required=True)
    parser.add_argument("--boundary-file", required=True)
    parser.add_argument("--bridge-file", required=True)
    parser.add_argument("--policy-file", required=True)
    parser.add_argument("--changed-file", action="append", default=[])
    return parser.parse_args()


if __name__ == "__main__":
    raise SystemExit(run(parse_args()))
