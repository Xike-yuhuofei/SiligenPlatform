from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path


CATALOG_RELATIVE_PATH = Path("scripts") / "migration" / "legacy_fact_catalog.json"


@dataclass(frozen=True)
class CatalogMeta:
    catalog_id: str
    catalog_version: str
    repo_id: str
    baseline_date: str
    status: str
    scope_statement: str
    non_scope_statement: str
    source_of_truth_refs: tuple[str, ...]
    review_cycle: str
    change_rule: str


@dataclass(frozen=True)
class RootEntry:
    root_id: str
    path: str
    classification: str
    presence_policy: str
    impl_owner_policy: str
    entry_role: str
    content_constraint: str
    notes: str


@dataclass(frozen=True)
class ZoneEntry:
    zone_id: str
    zone_class: str
    path_prefixes: tuple[str, ...]
    root_level_files: tuple[str, ...]
    default_scan_policy: str
    notes: str


@dataclass(frozen=True)
class Matcher:
    matcher_kind: str
    message: str
    paths: tuple[str, ...] = ()
    snippets: tuple[str, ...] = ()
    patterns: tuple[str, ...] = ()
    target_glob: str | None = None
    target_set: str | None = None


@dataclass(frozen=True)
class ResidualRule:
    residual_id: str
    rule: str
    residual_kind: str
    decision: str
    severity: str
    reference_ids: tuple[str, ...]
    matchers: tuple[Matcher, ...]


@dataclass(frozen=True)
class ExceptionEntry:
    exception_id: str
    status: str
    linked_residual_ids: tuple[str, ...]
    allowed_locations: tuple[str, ...]
    owner_scope: str
    justification: str
    exit_condition: str
    reference_ids: tuple[str, ...]


@dataclass(frozen=True)
class ReferenceEntry:
    reference_id: str
    source_type: str
    source_path: str
    statement_summary: str
    authority_level: str
    effective_from: str


@dataclass(frozen=True)
class GateProfile:
    profile_id: str
    included_zone_ids: tuple[str, ...]
    scan_roots: tuple[str, ...]
    root_level_files: tuple[str, ...]
    file_suffixes: tuple[str, ...]
    skip_path_prefixes: tuple[str, ...]
    allowlisted_paths: tuple[str, ...]
    fail_on_decisions: tuple[str, ...]


@dataclass(frozen=True)
class LegacyFactCatalog:
    meta: CatalogMeta
    root_registry: dict[str, RootEntry]
    zone_registry: dict[str, ZoneEntry]
    residual_registry: dict[str, ResidualRule]
    exception_registry: dict[str, ExceptionEntry]
    reference_registry: dict[str, ReferenceEntry]
    gate_profile_registry: dict[str, GateProfile]

    def classify_zone(self, relative_path: str) -> str | None:
        normalized = relative_path.replace("\\", "/").strip("/")
        for zone in self.zone_registry.values():
            if normalized in zone.root_level_files:
                return zone.zone_id
            for prefix in zone.path_prefixes:
                normalized_prefix = prefix.replace("\\", "/").strip("/")
                if normalized == normalized_prefix or normalized.startswith(normalized_prefix + "/"):
                    return zone.zone_id
        return None

    def active_exceptions(self) -> tuple[ExceptionEntry, ...]:
        return tuple(item for item in self.exception_registry.values() if item.status == "active")

    def gate_profile(self, profile_id: str) -> GateProfile:
        return self.gate_profile_registry[profile_id]

    def root_paths_for_rule(self, rule_name: str) -> tuple[str, ...]:
        paths: list[str] = []
        for residual in self.residual_registry.values():
            if residual.rule != rule_name:
                continue
            for matcher in residual.matchers:
                if matcher.matcher_kind == "path-exists":
                    paths.extend(matcher.paths)
        return tuple(dict.fromkeys(paths))

    def snippets_for_rule(self, rule_name: str, *, target_glob: str | None = None) -> tuple[str, ...]:
        snippets: list[str] = []
        for residual in self.residual_registry.values():
            if residual.rule != rule_name:
                continue
            for matcher in residual.matchers:
                if matcher.matcher_kind != "line-contains":
                    continue
                if target_glob is not None and matcher.target_glob != target_glob:
                    continue
                snippets.extend(matcher.snippets)
        return tuple(dict.fromkeys(snippets))


def _catalog_path(workspace_root: Path) -> Path:
    return workspace_root / CATALOG_RELATIVE_PATH


def _require(value: object, field_name: str) -> object:
    if value is None:
        raise ValueError(f"missing required field: {field_name}")
    return value


def _to_tuple(values: object) -> tuple[str, ...]:
    if values is None:
        return ()
    if not isinstance(values, list):
        raise ValueError(f"expected list value, got {type(values).__name__}")
    return tuple(str(item) for item in values)


def _load_meta(data: dict[str, object]) -> CatalogMeta:
    return CatalogMeta(
        catalog_id=str(_require(data.get("catalog_id"), "catalog_meta.catalog_id")),
        catalog_version=str(_require(data.get("catalog_version"), "catalog_meta.catalog_version")),
        repo_id=str(_require(data.get("repo_id"), "catalog_meta.repo_id")),
        baseline_date=str(_require(data.get("baseline_date"), "catalog_meta.baseline_date")),
        status=str(_require(data.get("status"), "catalog_meta.status")),
        scope_statement=str(_require(data.get("scope_statement"), "catalog_meta.scope_statement")),
        non_scope_statement=str(_require(data.get("non_scope_statement"), "catalog_meta.non_scope_statement")),
        source_of_truth_refs=_to_tuple(data.get("source_of_truth_refs")),
        review_cycle=str(_require(data.get("review_cycle"), "catalog_meta.review_cycle")),
        change_rule=str(_require(data.get("change_rule"), "catalog_meta.change_rule")),
    )


def _load_roots(entries: list[dict[str, object]]) -> dict[str, RootEntry]:
    roots: dict[str, RootEntry] = {}
    for entry in entries:
        root = RootEntry(
            root_id=str(_require(entry.get("root_id"), "root_registry.root_id")),
            path=str(_require(entry.get("path"), "root_registry.path")),
            classification=str(_require(entry.get("classification"), "root_registry.classification")),
            presence_policy=str(_require(entry.get("presence_policy"), "root_registry.presence_policy")),
            impl_owner_policy=str(_require(entry.get("impl_owner_policy"), "root_registry.impl_owner_policy")),
            entry_role=str(_require(entry.get("entry_role"), "root_registry.entry_role")),
            content_constraint=str(_require(entry.get("content_constraint"), "root_registry.content_constraint")),
            notes=str(entry.get("notes", "")),
        )
        if root.root_id in roots:
            raise ValueError(f"duplicate root_id: {root.root_id}")
        roots[root.root_id] = root
    return roots


def _load_zones(entries: list[dict[str, object]]) -> dict[str, ZoneEntry]:
    zones: dict[str, ZoneEntry] = {}
    for entry in entries:
        zone = ZoneEntry(
            zone_id=str(_require(entry.get("zone_id"), "zone_registry.zone_id")),
            zone_class=str(_require(entry.get("zone_class"), "zone_registry.zone_class")),
            path_prefixes=_to_tuple(entry.get("path_prefixes")),
            root_level_files=_to_tuple(entry.get("root_level_files")),
            default_scan_policy=str(_require(entry.get("default_scan_policy"), "zone_registry.default_scan_policy")),
            notes=str(entry.get("notes", "")),
        )
        if zone.zone_id in zones:
            raise ValueError(f"duplicate zone_id: {zone.zone_id}")
        zones[zone.zone_id] = zone
    return zones


def _load_matchers(entries: list[dict[str, object]]) -> tuple[Matcher, ...]:
    matchers: list[Matcher] = []
    for entry in entries:
        matchers.append(
            Matcher(
                matcher_kind=str(_require(entry.get("matcher_kind"), "matcher.matcher_kind")),
                message=str(_require(entry.get("message"), "matcher.message")),
                paths=_to_tuple(entry.get("paths")),
                snippets=_to_tuple(entry.get("snippets")),
                patterns=_to_tuple(entry.get("patterns")),
                target_glob=str(entry["target_glob"]) if entry.get("target_glob") is not None else None,
                target_set=str(entry["target_set"]) if entry.get("target_set") is not None else None,
            )
        )
    return tuple(matchers)


def _load_residuals(entries: list[dict[str, object]]) -> dict[str, ResidualRule]:
    residuals: dict[str, ResidualRule] = {}
    for entry in entries:
        residual = ResidualRule(
            residual_id=str(_require(entry.get("residual_id"), "residual_registry.residual_id")),
            rule=str(_require(entry.get("rule"), "residual_registry.rule")),
            residual_kind=str(_require(entry.get("residual_kind"), "residual_registry.residual_kind")),
            decision=str(_require(entry.get("decision"), "residual_registry.decision")),
            severity=str(_require(entry.get("severity"), "residual_registry.severity")),
            reference_ids=_to_tuple(entry.get("reference_ids")),
            matchers=_load_matchers(entry.get("matchers", [])),
        )
        if residual.residual_id in residuals:
            raise ValueError(f"duplicate residual_id: {residual.residual_id}")
        residuals[residual.residual_id] = residual
    return residuals


def _load_exceptions(entries: list[dict[str, object]]) -> dict[str, ExceptionEntry]:
    exceptions: dict[str, ExceptionEntry] = {}
    for entry in entries:
        exception = ExceptionEntry(
            exception_id=str(_require(entry.get("exception_id"), "exception_registry.exception_id")),
            status=str(_require(entry.get("status"), "exception_registry.status")),
            linked_residual_ids=_to_tuple(entry.get("linked_residual_ids")),
            allowed_locations=_to_tuple(entry.get("allowed_locations")),
            owner_scope=str(_require(entry.get("owner_scope"), "exception_registry.owner_scope")),
            justification=str(_require(entry.get("justification"), "exception_registry.justification")),
            exit_condition=str(_require(entry.get("exit_condition"), "exception_registry.exit_condition")),
            reference_ids=_to_tuple(entry.get("reference_ids")),
        )
        if exception.exception_id in exceptions:
            raise ValueError(f"duplicate exception_id: {exception.exception_id}")
        exceptions[exception.exception_id] = exception
    return exceptions


def _load_references(entries: list[dict[str, object]]) -> dict[str, ReferenceEntry]:
    references: dict[str, ReferenceEntry] = {}
    for entry in entries:
        reference = ReferenceEntry(
            reference_id=str(_require(entry.get("reference_id"), "reference_registry.reference_id")),
            source_type=str(_require(entry.get("source_type"), "reference_registry.source_type")),
            source_path=str(_require(entry.get("source_path"), "reference_registry.source_path")),
            statement_summary=str(_require(entry.get("statement_summary"), "reference_registry.statement_summary")),
            authority_level=str(_require(entry.get("authority_level"), "reference_registry.authority_level")),
            effective_from=str(_require(entry.get("effective_from"), "reference_registry.effective_from")),
        )
        if reference.reference_id in references:
            raise ValueError(f"duplicate reference_id: {reference.reference_id}")
        references[reference.reference_id] = reference
    return references


def _load_gate_profiles(entries: list[dict[str, object]]) -> dict[str, GateProfile]:
    profiles: dict[str, GateProfile] = {}
    for entry in entries:
        profile = GateProfile(
            profile_id=str(_require(entry.get("profile_id"), "gate_profile_registry.profile_id")),
            included_zone_ids=_to_tuple(entry.get("included_zone_ids")),
            scan_roots=_to_tuple(entry.get("scan_roots")),
            root_level_files=_to_tuple(entry.get("root_level_files")),
            file_suffixes=_to_tuple(entry.get("file_suffixes")),
            skip_path_prefixes=_to_tuple(entry.get("skip_path_prefixes")),
            allowlisted_paths=_to_tuple(entry.get("allowlisted_paths")),
            fail_on_decisions=_to_tuple(entry.get("fail_on_decisions")),
        )
        if profile.profile_id in profiles:
            raise ValueError(f"duplicate profile_id: {profile.profile_id}")
        profiles[profile.profile_id] = profile
    return profiles


def _validate_catalog(catalog: LegacyFactCatalog) -> None:
    for residual in catalog.residual_registry.values():
        missing_refs = [ref_id for ref_id in residual.reference_ids if ref_id not in catalog.reference_registry]
        if missing_refs:
            raise ValueError(f"{residual.residual_id} references missing refs: {missing_refs}")

    for exception in catalog.exception_registry.values():
        missing_rules = [rule_id for rule_id in exception.linked_residual_ids if rule_id not in catalog.residual_registry]
        if missing_rules:
            raise ValueError(f"{exception.exception_id} references missing residuals: {missing_rules}")
        missing_refs = [ref_id for ref_id in exception.reference_ids if ref_id not in catalog.reference_registry]
        if missing_refs:
            raise ValueError(f"{exception.exception_id} references missing refs: {missing_refs}")

    for profile in catalog.gate_profile_registry.values():
        missing_zones = [zone_id for zone_id in profile.included_zone_ids if zone_id not in catalog.zone_registry]
        if missing_zones:
            raise ValueError(f"{profile.profile_id} references missing zones: {missing_zones}")


def load_legacy_fact_catalog(workspace_root: Path) -> LegacyFactCatalog:
    catalog_path = _catalog_path(workspace_root)
    if not catalog_path.exists():
        raise FileNotFoundError(f"legacy fact catalog missing: {catalog_path}")

    raw = json.loads(catalog_path.read_text(encoding="utf-8"))
    catalog = LegacyFactCatalog(
        meta=_load_meta(raw["catalog_meta"]),
        root_registry=_load_roots(raw["root_registry"]),
        zone_registry=_load_zones(raw["zone_registry"]),
        residual_registry=_load_residuals(raw["residual_registry"]),
        exception_registry=_load_exceptions(raw.get("exception_registry", [])),
        reference_registry=_load_references(raw["reference_registry"]),
        gate_profile_registry=_load_gate_profiles(raw["gate_profile_registry"]),
    )
    _validate_catalog(catalog)
    return catalog
