from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.legacy_fact_catalog import GateProfile, LegacyFactCatalog, Matcher, load_legacy_fact_catalog


LEGACY_FACT_CATALOG = load_legacy_fact_catalog(ROOT)
LEGACY_EXIT_PROFILE = LEGACY_FACT_CATALOG.gate_profile("legacy-exit")


@dataclass(frozen=True)
class Finding:
    residual_id: str
    rule: str
    decision: str
    message: str
    file: str | None = None
    line: int | None = None
    zone: str | None = None
    category: str | None = None


@dataclass(frozen=True)
class ScanResult:
    findings: tuple[Finding, ...]
    scanned_files: tuple[str, ...]


def _normalize_rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def _is_skipped_path(rel_posix: str, profile: GateProfile) -> bool:
    for prefix in profile.skip_path_prefixes:
        normalized = prefix.strip("/").replace("\\", "/")
        if rel_posix == normalized or rel_posix.startswith(normalized + "/"):
            return True
    return False


def _iter_profile_code_files(profile: GateProfile) -> list[Path]:
    files: list[Path] = []
    seen: set[str] = set()

    for root_name in profile.scan_roots:
        base = ROOT / root_name
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if not path.is_file():
                continue
            rel_posix = _normalize_rel(path)
            if _is_skipped_path(rel_posix, profile):
                continue
            if rel_posix in profile.allowlisted_paths:
                continue
            if path.name == "CMakeLists.txt" or path.suffix.lower() in profile.file_suffixes:
                if rel_posix not in seen:
                    seen.add(rel_posix)
                    files.append(path)

    for file_name in profile.root_level_files:
        path = ROOT / file_name
        if not path.exists() or not path.is_file():
            continue
        rel_posix = _normalize_rel(path)
        if rel_posix in profile.allowlisted_paths or rel_posix in seen:
            continue
        seen.add(rel_posix)
        files.append(path)

    return files


def _iter_active_doc_files() -> list[Path]:
    files: list[Path] = []
    seen: set[str] = set()
    for relative in ("README.md", "WORKSPACE.md"):
        path = ROOT / relative
        if path.exists() and path.is_file():
            rel_posix = _normalize_rel(path)
            seen.add(rel_posix)
            files.append(path)

    docs_root = ROOT / "docs"
    if docs_root.exists():
        for path in docs_root.rglob("*.md"):
            if not path.is_file():
                continue
            rel_posix = _normalize_rel(path)
            if rel_posix.startswith("docs/_archive/"):
                continue
            if rel_posix not in seen:
                seen.add(rel_posix)
                files.append(path)
    return files


def _matcher_files(
    matcher: Matcher,
    profile: GateProfile,
    file_cache: dict[tuple[str, str], list[Path]],
) -> list[Path]:
    if matcher.target_set == "active-source-files":
        key = ("target_set", matcher.target_set)
        if key not in file_cache:
            file_cache[key] = _iter_profile_code_files(profile)
        return file_cache[key]

    if matcher.target_glob:
        key = ("target_glob", matcher.target_glob)
        if key not in file_cache:
            file_cache[key] = sorted(
                [path for path in ROOT.glob(matcher.target_glob) if path.is_file()],
                key=lambda item: item.as_posix(),
            )
        return file_cache[key]

    return []


def _decision_for(
    residual_id: str,
    rel_posix: str | None,
    default_decision: str,
    catalog: LegacyFactCatalog,
) -> str:
    if rel_posix is None:
        return default_decision
    normalized_rel = rel_posix.replace("\\", "/")
    if normalized_rel in {"README.md", "WORKSPACE.md"} or normalized_rel.startswith("docs/"):
        return "report-only"
    for exception in catalog.active_exceptions():
        if residual_id not in exception.linked_residual_ids:
            continue
        if rel_posix in exception.allowed_locations:
            return "controlled-exception"
    return default_decision


def _category_for(rel_posix: str | None) -> str:
    if rel_posix is None:
        return "physical"
    normalized = rel_posix.replace("\\", "/")
    suffix = Path(normalized).suffix.lower()
    name = Path(normalized).name.lower()
    if normalized in {"README.md", "WORKSPACE.md"} or normalized.startswith("docs/"):
        return "documentation"
    if normalized.startswith("deploy/") or "launch" in name or name == "run.ps1":
        return "runtime-entry"
    if normalized.startswith("config/") or suffix in {".json", ".yaml", ".yml", ".toml", ".ini"}:
        return "config"
    if name == "cmakelists.txt" or suffix == ".cmake":
        return "build"
    if suffix == ".ps1":
        return "script"
    if suffix in {".py", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh"}:
        return "source"
    return "other"


def _scan_matcher(
    catalog: LegacyFactCatalog,
    profile: GateProfile,
    residual_id: str,
    rule: str,
    default_decision: str,
    matcher: Matcher,
    file_cache: dict[tuple[str, str], list[Path]],
) -> list[Finding]:
    findings: list[Finding] = []

    if matcher.matcher_kind == "path-exists":
        for relative in matcher.paths:
            target = ROOT / relative
            if not target.exists():
                continue
            findings.append(
                Finding(
                    residual_id=residual_id,
                    rule=rule,
                    decision=_decision_for(residual_id, relative, default_decision, catalog),
                    message=matcher.message.format(path=relative),
                    file=relative,
                    zone=catalog.classify_zone(relative),
                    category=_category_for(relative),
                )
            )
        return findings

    for path in _matcher_files(matcher, profile, file_cache):
        rel_posix = _normalize_rel(path)
        zone = catalog.classify_zone(rel_posix)
        text = path.read_text(encoding="utf-8", errors="ignore")
        for lineno, line in enumerate(text.splitlines(), start=1):
            if matcher.matcher_kind == "line-contains":
                for snippet in matcher.snippets:
                    if snippet in line:
                        findings.append(
                            Finding(
                                residual_id=residual_id,
                                rule=rule,
                                decision=_decision_for(residual_id, rel_posix, default_decision, catalog),
                                message=matcher.message.format(snippet=snippet, path=rel_posix),
                                file=rel_posix,
                                line=lineno,
                                zone=zone,
                                category=_category_for(rel_posix),
                            )
                        )
            elif matcher.matcher_kind == "line-regex":
                for pattern in matcher.patterns:
                    if re.search(pattern, line):
                        findings.append(
                            Finding(
                                residual_id=residual_id,
                                rule=rule,
                                decision=_decision_for(residual_id, rel_posix, default_decision, catalog),
                                message=matcher.message.format(pattern=pattern, path=rel_posix),
                                file=rel_posix,
                                line=lineno,
                                zone=zone,
                                category=_category_for(rel_posix),
                            )
                        )
    return findings


def _collect_findings(catalog: LegacyFactCatalog, profile: GateProfile, *, include_docs: bool = False) -> ScanResult:
    findings: list[Finding] = []
    file_cache: dict[tuple[str, str], list[Path]] = {}
    for residual in catalog.residual_registry.values():
        for matcher in residual.matchers:
            findings.extend(
                _scan_matcher(
                    catalog=catalog,
                    profile=profile,
                    residual_id=residual.residual_id,
                    rule=residual.rule,
                    default_decision=residual.decision,
                    matcher=matcher,
                    file_cache=file_cache,
                )
            )
    if include_docs:
        doc_files = _iter_active_doc_files()
        doc_patterns = tuple(
            pattern
            for residual in catalog.residual_registry.values()
            if residual.rule == "legacy-path-reference"
            for matcher in residual.matchers
            if matcher.matcher_kind == "line-regex"
            for pattern in matcher.patterns
        )
        for path in doc_files:
            rel_posix = _normalize_rel(path)
            text = path.read_text(encoding="utf-8", errors="ignore")
            for lineno, line in enumerate(text.splitlines(), start=1):
                for pattern in doc_patterns:
                    if re.search(pattern, line):
                        findings.append(
                            Finding(
                                residual_id="DOC-LEGACY-PATH",
                                rule="legacy-path-reference",
                                decision="report-only",
                                message="活动文档仍包含 legacy 根路径说明，请确认是否属于历史说明而非运行入口。",
                                file=rel_posix,
                                line=lineno,
                                zone=catalog.classify_zone(rel_posix),
                                category=_category_for(rel_posix),
                            )
                        )
    scanned_files: set[str] = set()
    for paths in file_cache.values():
        scanned_files.update(_normalize_rel(path) for path in paths)
    if include_docs:
        scanned_files.update(_normalize_rel(path) for path in _iter_active_doc_files())
    return ScanResult(findings=tuple(findings), scanned_files=tuple(sorted(scanned_files)))


def _finding_identity(item: dict) -> tuple[str, str, str, int, str]:
    residual_id = str(item.get("residual_id") or item.get("rule_id") or "")
    rule = str(item.get("rule") or item.get("rule_id") or "")
    file = str(item.get("file") or "")
    line = int(item.get("line") or 0)
    message = str(item.get("message") or "")
    return residual_id, rule, file, line, message


def _load_previous_payload(json_path: Path) -> dict | None:
    if not json_path.exists():
        return None
    try:
        return json.loads(json_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return None


def _build_delta(current_findings: list[dict], previous_payload: dict | None) -> dict:
    if previous_payload is None:
        return {
            "previous_report_found": False,
            "previous_finding_count": 0,
            "added_findings": current_findings,
            "removed_findings": [],
        }

    previous_findings = previous_payload.get("findings", [])
    previous_map = {_finding_identity(item): item for item in previous_findings}
    current_map = {_finding_identity(item): item for item in current_findings}
    added_keys = sorted(set(current_map) - set(previous_map))
    removed_keys = sorted(set(previous_map) - set(current_map))

    return {
        "previous_report_found": True,
        "previous_finding_count": int(previous_payload.get("finding_count", len(previous_findings))),
        "added_findings": [current_map[key] for key in added_keys],
        "removed_findings": [previous_map[key] for key in removed_keys],
    }


def _zone_summary(scanned_files: tuple[str, ...], catalog: LegacyFactCatalog) -> dict[str, int]:
    summary: dict[str, int] = {}
    for rel_posix in scanned_files:
        zone = catalog.classify_zone(rel_posix) or "unclassified"
        summary[zone] = summary.get(zone, 0) + 1
    return dict(sorted(summary.items()))


def _decision_summary(findings: list[dict]) -> dict[str, int]:
    summary: dict[str, int] = {}
    for item in findings:
        decision = str(item.get("decision") or "unknown")
        summary[decision] = summary.get(decision, 0) + 1
    return dict(sorted(summary.items()))


def _category_summary(findings: list[dict]) -> dict[str, int]:
    summary: dict[str, int] = {}
    for item in findings:
        category = str(item.get("category") or "uncategorized")
        summary[category] = summary.get(category, 0) + 1
    return dict(sorted(summary.items()))


def _suspended_exceptions(catalog: LegacyFactCatalog) -> list[dict]:
    suspended = []
    for item in catalog.exception_registry.values():
        if item.status != "suspended":
            continue
        suspended.append(
            {
                "exception_id": item.exception_id,
                "linked_residual_ids": list(item.linked_residual_ids),
                "allowed_locations": list(item.allowed_locations),
                "owner_scope": item.owner_scope,
                "justification": item.justification,
                "exit_condition": item.exit_condition,
                "reference_ids": list(item.reference_ids),
            }
        )
    return suspended


def _format_location(item: dict) -> str:
    if not item.get("file"):
        return ""
    return f"{item['file']}:{item.get('line', 0)}"


def _render_findings(lines: list[str], findings: list[dict]) -> None:
    if not findings:
        lines.append("- pass")
        return
    for item in findings:
        location = ""
        if item.get("file"):
            location = f" ({_format_location(item)})"
        decision = f"[{item['decision']}] " if item.get("decision") else ""
        lines.append(f"- `{item.get('rule', item.get('rule_id', 'unknown'))}` {decision}{location}: {item['message']}")


def _render_markdown(payload: dict) -> str:
    findings = payload["findings"]
    lines = [
        "# Legacy Exit Checks",
        "",
        f"- generated_at: `{payload['generated_at']}`",
        f"- profile: `{payload['profile']}`",
        f"- workspace_root: `{payload['workspace_root']}`",
        f"- catalog_id: `{payload['catalog_id']}`",
        f"- catalog_version: `{payload['catalog_version']}`",
        f"- finding_count: `{payload['finding_count']}`",
        f"- blocking_count: `{payload['blocking_count']}`",
        f"- scanned_file_count: `{payload['scanned_file_count']}`",
        f"- include_docs: `{str(payload['include_docs']).lower()}`",
        "",
        "## Scan Summary",
        "",
    ]
    for zone, count in payload["zone_summary"].items():
        lines.append(f"- `{zone}`: `{count}`")
    lines.extend(["", "## Decision Summary", ""])
    for decision, count in payload["decision_summary"].items():
        lines.append(f"- `{decision}`: `{count}`")
    lines.extend(["", "## Category Summary", ""])
    for category, count in payload["category_summary"].items():
        lines.append(f"- `{category}`: `{count}`")

    lines.extend(["", "## Findings", ""])
    _render_findings(lines, findings)

    lines.extend(["", "## Suspended Exceptions", ""])
    if not payload["suspended_exceptions"]:
        lines.append("- none")
    else:
        for item in payload["suspended_exceptions"]:
            lines.append(
                f"- `{item['exception_id']}` owner=`{item['owner_scope']}` residuals=`{', '.join(item['linked_residual_ids'])}`"
            )
            lines.append(f"  locations: `{', '.join(item['allowed_locations'])}`")
            lines.append(f"  exit_condition: {item['exit_condition']}")

    delta = payload["delta"]
    lines.extend(["", "## Delta", ""])
    lines.append(f"- previous_report_found: `{str(delta['previous_report_found']).lower()}`")
    lines.append(f"- previous_finding_count: `{delta['previous_finding_count']}`")
    lines.append(f"- added_finding_count: `{len(delta['added_findings'])}`")
    lines.append(f"- removed_finding_count: `{len(delta['removed_findings'])}`")
    if delta["added_findings"]:
        lines.extend(["", "### Added"])
        _render_findings(lines, delta["added_findings"][:20])
        if len(delta["added_findings"]) > 20:
            lines.append(f"- ... omitted {len(delta['added_findings']) - 20} additional added findings")
    if delta["removed_findings"]:
        lines.extend(["", "### Removed"])
        _render_findings(lines, delta["removed_findings"][:20])
        if len(delta["removed_findings"]) > 20:
            lines.append(f"- ... omitted {len(delta['removed_findings']) - 20} additional removed findings")
    lines.append("")
    return "\n".join(lines)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate physical legacy-root and bridge exit.")
    parser.add_argument("--profile", choices=("local", "ci"), default="local")
    parser.add_argument("--report-dir", default=str(ROOT / "tests" / "reports" / "legacy-exit"))
    parser.add_argument("--include-docs", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir).resolve()
    report_dir.mkdir(parents=True, exist_ok=True)

    json_path = report_dir / "legacy-exit-checks.json"
    previous_payload = _load_previous_payload(json_path)

    scan_result = _collect_findings(LEGACY_FACT_CATALOG, LEGACY_EXIT_PROFILE, include_docs=bool(args.include_docs))
    findings = list(scan_result.findings)
    blocking_count = sum(1 for item in findings if item.decision in LEGACY_EXIT_PROFILE.fail_on_decisions)
    finding_payload = [asdict(item) for item in findings]
    suspended_exceptions = _suspended_exceptions(LEGACY_FACT_CATALOG)
    delta = _build_delta(finding_payload, previous_payload)

    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "profile": args.profile,
        "workspace_root": str(ROOT),
        "catalog_id": LEGACY_FACT_CATALOG.meta.catalog_id,
        "catalog_version": LEGACY_FACT_CATALOG.meta.catalog_version,
        "finding_count": len(findings),
        "blocking_count": blocking_count,
        "scanned_file_count": len(scan_result.scanned_files),
        "include_docs": bool(args.include_docs),
        "zone_summary": _zone_summary(scan_result.scanned_files, LEGACY_FACT_CATALOG),
        "decision_summary": _decision_summary(finding_payload),
        "category_summary": _category_summary(finding_payload),
        "suspended_exception_count": len(suspended_exceptions),
        "suspended_exceptions": suspended_exceptions,
        "delta": delta,
        "findings": finding_payload,
    }

    md_path = report_dir / "legacy-exit-checks.md"
    json_path.write_text(json.dumps(payload, ensure_ascii=True, indent=2), encoding="utf-8")
    md_path.write_text(_render_markdown(payload), encoding="utf-8")

    print("[legacy-exit]")
    print(f"  - profile={args.profile}")
    print(f"  - workspace_root={ROOT}")
    print(f"  - catalog_id={LEGACY_FACT_CATALOG.meta.catalog_id}")
    print(f"  - catalog_version={LEGACY_FACT_CATALOG.meta.catalog_version}")
    print(f"  - finding_count={len(findings)}")
    print(f"  - blocking_count={blocking_count}")
    print(f"  - scanned_file_count={len(scan_result.scanned_files)}")
    print(f"  - include_docs={str(bool(args.include_docs)).lower()}")
    print(f"  - suspended_exception_count={len(suspended_exceptions)}")
    print(f"  - json_report={json_path}")
    print(f"  - markdown_report={md_path}")

    return 1 if blocking_count else 0


if __name__ == "__main__":
    raise SystemExit(main())
