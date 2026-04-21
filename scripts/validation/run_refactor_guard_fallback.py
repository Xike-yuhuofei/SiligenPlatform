from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from ruamel.yaml import YAML
from wcmatch import glob


ROOT = Path(__file__).resolve().parents[2]
GLOB_FLAGS = glob.GLOBSTAR | glob.DOTMATCH | glob.FORCEUNIX


@dataclass(frozen=True)
class RuleSpec:
    rule_id: str
    message: str
    severity: str
    include: tuple[str, ...]
    exclude: tuple[str, ...]
    pattern: re.Pattern[str]


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the refactor-guard fallback scanner.")
    parser.add_argument("--workspace-root", type=Path, default=ROOT)
    parser.add_argument("--config-path", type=Path, required=True)
    parser.add_argument("--report-json", type=Path, required=True)
    parser.add_argument("--scan-target", action="append", default=[])
    return parser.parse_args()


def _normalize_pattern(pattern: str) -> str:
    return str(pattern).strip().lstrip("/")


def _matches_any(path: str, patterns: tuple[str, ...]) -> bool:
    return any(glob.globmatch(path, pattern, flags=GLOB_FLAGS) for pattern in patterns)


def _load_rules(config_path: Path) -> list[RuleSpec]:
    yaml = YAML(typ="safe")
    payload = yaml.load(config_path.read_text(encoding="utf-8"))
    raw_rules = payload.get("rules", []) if isinstance(payload, dict) else []
    loaded: list[RuleSpec] = []
    for raw_rule in raw_rules:
        if not isinstance(raw_rule, dict):
            continue
        languages = tuple(str(language) for language in raw_rule.get("languages", []))
        if "generic" not in languages:
            continue
        paths = raw_rule.get("paths", {}) if isinstance(raw_rule.get("paths", {}), dict) else {}
        include = tuple(_normalize_pattern(item) for item in paths.get("include", []) if str(item).strip())
        exclude = tuple(_normalize_pattern(item) for item in paths.get("exclude", []) if str(item).strip())
        pattern_text = str(raw_rule.get("pattern-regex", "")).strip()
        pattern_text = re.sub(r"\\\\(?=[sbSBdDwW.])", r"\\", pattern_text)
        if not include or not pattern_text:
            continue
        loaded.append(
            RuleSpec(
                rule_id=str(raw_rule.get("id", "")).strip(),
                message=str(raw_rule.get("message", "")).strip(),
                severity=str(raw_rule.get("severity", "ERROR")).strip().upper(),
                include=include,
                exclude=exclude,
                pattern=re.compile(pattern_text, re.MULTILINE),
            )
        )
    return loaded


def _iter_scan_files(workspace_root: Path, scan_targets: list[str]) -> list[Path]:
    files: set[Path] = set()
    targets = scan_targets or ["."]
    for raw_target in targets:
        target = Path(raw_target)
        if not target.is_absolute():
            target = workspace_root / target
        if not target.exists():
            continue
        if target.is_file():
            files.add(target.resolve())
            continue
        for candidate in target.rglob("*"):
            if candidate.is_file():
                files.add(candidate.resolve())
    return sorted(files)


def _is_binary(path: Path) -> bool:
    try:
        return b"\x00" in path.read_bytes()[:4096]
    except OSError:
        return True


def _offset_to_line_col(line_starts: list[int], offset: int) -> tuple[int, int]:
    low = 0
    high = len(line_starts) - 1
    while low <= high:
        mid = (low + high) // 2
        if line_starts[mid] <= offset:
            low = mid + 1
        else:
            high = mid - 1
    line_index = max(0, high)
    line_start = line_starts[line_index]
    return line_index + 1, (offset - line_start) + 1


def _line_starts(text: str) -> list[int]:
    starts = [0]
    for index, char in enumerate(text):
        if char == "\n":
            starts.append(index + 1)
    return starts


def _scan_file(workspace_root: Path, path: Path, rules: list[RuleSpec]) -> list[dict[str, Any]]:
    try:
        relative_path = path.resolve().relative_to(workspace_root.resolve()).as_posix()
    except ValueError:
        return []

    applicable_rules = [
        rule
        for rule in rules
        if _matches_any(relative_path, rule.include) and not (rule.exclude and _matches_any(relative_path, rule.exclude))
    ]
    if not applicable_rules:
        return []

    if _is_binary(path):
        return []

    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return []

    starts = _line_starts(text)
    results: list[dict[str, Any]] = []
    for rule in applicable_rules:
        for match in rule.pattern.finditer(text):
            start_line, start_col = _offset_to_line_col(starts, match.start())
            end_offset = max(match.start(), match.end() - 1)
            end_line, end_col = _offset_to_line_col(starts, end_offset)
            results.append(
                {
                    "check_id": rule.rule_id,
                    "path": relative_path,
                    "start": {"line": start_line, "col": start_col, "offset": match.start()},
                    "end": {"line": end_line, "col": end_col + 1, "offset": match.end()},
                    "extra": {
                        "message": rule.message,
                        "severity": rule.severity,
                        "engine_kind": "refactor-guard-fallback",
                    },
                }
            )
    return results


def main() -> int:
    args = _parse_args()
    workspace_root = args.workspace_root.resolve()
    config_path = args.config_path.resolve()
    report_json = args.report_json.resolve()

    rules = _load_rules(config_path)
    files = _iter_scan_files(workspace_root, list(args.scan_target))
    results: list[dict[str, Any]] = []
    for path in files:
        results.extend(_scan_file(workspace_root, path, rules))

    results.sort(key=lambda item: (str(item["path"]), str(item["check_id"]), int(item["start"]["line"])))
    payload = {
        "version": "refactor-guard-fallback-1",
        "results": results,
        "errors": [],
        "scanner": {
            "engine": "refactor-guard-fallback",
            "config_path": str(config_path),
            "scanned_file_count": len(files),
            "rule_count": len(rules),
        },
    }
    report_json.parent.mkdir(parents=True, exist_ok=True)
    report_json.write_text(json.dumps(payload, ensure_ascii=True, indent=2), encoding="utf-8")

    print(
        "refactor-guard fallback complete: "
        f"rules={len(rules)} scanned_files={len(files)} findings={len(results)} report={report_json}"
    )
    return 1 if results else 0


if __name__ == "__main__":
    raise SystemExit(main())
