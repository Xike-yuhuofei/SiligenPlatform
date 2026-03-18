#!/usr/bin/env python3
import argparse
import re
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


EXTENSIONS = {".cpp", ".cc", ".c", ".h", ".hpp", ".hh", ".hxx", ".cxx", ".inl"}
FUNCTION_START_RE = re.compile(
    r"^[\w\s:<>,~*&]+\b([A-Za-z_~][\w:]*)\s*\([^;]*\)\s*(const\s*)?(noexcept\s*)?(->[^\\{]+)?\{\s*$"
)
CONTROL_KEYWORDS = {"if", "for", "while", "switch", "catch"}
DEFAULT_PATTERNS = {
    "null_guard_position_control": r"if\s*\(\s*!\s*position_control_port_\s*\)",
    "port_not_initialized": r"PORT_NOT_INITIALIZED",
    "validate_axis": r"\bValidateAxis\s*\(",
    "wait_for_motion_complete": r"\bWaitForMotionComplete\s*\(",
    "sdk_clear_status": r"\bMC_ClrSts\s*\(",
    "sdk_axis_on": r"\bMC_AxisOn\s*\(",
}


@dataclass
class FileStat:
    path: Path
    line_count: int


@dataclass
class DirectoryStat:
    path: Path
    file_count: int
    line_count: int


@dataclass
class FunctionStat:
    path: Path
    name: str
    start_line: int
    line_count: int


@dataclass
class PatternHit:
    label: str
    path: Path
    count: int


@dataclass
class DuplicateChunk:
    occurrences: int
    file_count: int
    snippet: str
    locations: list[tuple[Path, int]]


def iter_source_files(root: Path) -> Iterable[Path]:
    for path in root.rglob("*"):
        if path.is_file() and path.suffix.lower() in EXTENSIONS:
            yield path


def safe_read_lines(path: Path) -> list[str]:
    return path.read_text(encoding="utf-8", errors="ignore").splitlines()


def collect_file_stats(source_root: Path) -> list[FileStat]:
    stats = []
    for path in iter_source_files(source_root):
        stats.append(FileStat(path=path, line_count=len(safe_read_lines(path))))
    return stats


def collect_directory_stats(source_root: Path, file_stats: list[FileStat]) -> list[DirectoryStat]:
    grouped: dict[Path, list[FileStat]] = defaultdict(list)
    for stat in file_stats:
        grouped[stat.path.parent].append(stat)
    results = []
    for directory, files in grouped.items():
        try:
            rel_dir = directory.relative_to(source_root.parent)
        except ValueError:
            rel_dir = directory
        results.append(
            DirectoryStat(path=rel_dir, file_count=len(files), line_count=sum(item.line_count for item in files))
        )
    return results


def extract_functions(source_root: Path) -> list[FunctionStat]:
    functions: list[FunctionStat] = []
    for path in iter_source_files(source_root):
        lines = safe_read_lines(path)
        index = 0
        while index < len(lines):
            stripped = lines[index].strip()
            if stripped.startswith(("//", "#", "*")):
                index += 1
                continue
            match = FUNCTION_START_RE.match(stripped)
            if not match:
                index += 1
                continue
            name = match.group(1)
            if name in CONTROL_KEYWORDS:
                index += 1
                continue

            start_line = index + 1
            depth = stripped.count("{") - stripped.count("}")
            cursor = index + 1
            while cursor < len(lines) and depth > 0:
                depth += lines[cursor].count("{") - lines[cursor].count("}")
                cursor += 1

            functions.append(
                FunctionStat(path=path, name=name, start_line=start_line, line_count=cursor - index)
            )
            index = cursor
    return functions


def collect_pattern_hits(source_root: Path, patterns: dict[str, str]) -> list[PatternHit]:
    compiled = {label: re.compile(regex) for label, regex in patterns.items()}
    hits: list[PatternHit] = []
    for path in iter_source_files(source_root):
        text = path.read_text(encoding="utf-8", errors="ignore")
        for label, regex in compiled.items():
            count = len(regex.findall(text))
            if count:
                hits.append(PatternHit(label=label, path=path, count=count))
    return hits


def collect_duplicate_chunks(
    source_root: Path,
    chunk_size: int,
    min_files: int,
    max_results: int,
) -> list[DuplicateChunk]:
    windows: dict[str, list[tuple[Path, int]]] = defaultdict(list)
    for path in iter_source_files(source_root):
        lines = safe_read_lines(path)
        normalized_lines = []
        line_map = []
        for line_number, line in enumerate(lines, start=1):
            stripped = line.strip()
            if not stripped or stripped.startswith(("//", "#")) or stripped in {"{", "}"}:
                continue
            normalized_lines.append(" ".join(stripped.split()))
            line_map.append(line_number)

        for index in range(0, max(len(normalized_lines) - chunk_size + 1, 0)):
            chunk = "\n".join(normalized_lines[index : index + chunk_size])
            windows[chunk].append((path, line_map[index]))

    results = []
    for snippet, locations in windows.items():
        files = sorted({str(path) for path, _ in locations})
        if len(files) < min_files:
            continue
        results.append(
            DuplicateChunk(
                occurrences=len(locations),
                file_count=len(files),
                snippet=snippet,
                locations=locations[:5],
            )
        )
    results.sort(key=lambda item: (item.occurrences, item.file_count, item.snippet), reverse=True)
    return results[:max_results]


def format_path(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root.parent).as_posix()
    except ValueError:
        return path.as_posix()


def print_section(title: str) -> None:
    print()
    print(f"== {title} ==")


def main() -> int:
    parser = argparse.ArgumentParser(description="Scan C/C++ sources for micro-level redundancy signals.")
    parser.add_argument("--source-dir", default="src", help="Source directory to scan. Default: src")
    parser.add_argument("--top-files", type=int, default=20, help="How many large files to print")
    parser.add_argument("--top-dirs", type=int, default=15, help="How many dense directories to print")
    parser.add_argument("--top-functions", type=int, default=20, help="How many long functions to print")
    parser.add_argument("--file-threshold", type=int, default=400, help="Large file threshold in lines")
    parser.add_argument("--function-threshold", type=int, default=80, help="Long function threshold in lines")
    parser.add_argument("--pattern", action="append", default=[], help="Extra scan pattern in label=regex format")
    parser.add_argument("--duplicates", action="store_true", help="Enable normalized duplicate chunk scan")
    parser.add_argument("--duplicate-lines", type=int, default=8, help="Window size for duplicate chunk scan")
    parser.add_argument("--duplicate-min-files", type=int, default=2, help="Minimum files for a duplicate chunk")
    parser.add_argument("--duplicate-top", type=int, default=10, help="How many duplicate chunks to print")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    source_root = (repo_root / args.source_dir).resolve()
    if not source_root.exists():
        print(f"Source directory not found: {source_root}")
        return 2

    patterns = dict(DEFAULT_PATTERNS)
    for item in args.pattern:
        if "=" not in item:
            print(f"Invalid pattern: {item}. Use label=regex")
            return 2
        label, regex = item.split("=", 1)
        patterns[label.strip()] = regex.strip()

    file_stats = collect_file_stats(source_root)
    directory_stats = collect_directory_stats(source_root, file_stats)
    function_stats = extract_functions(source_root)
    pattern_hits = collect_pattern_hits(source_root, patterns)

    total_lines = sum(item.line_count for item in file_stats)
    large_file_count = sum(1 for item in file_stats if item.line_count >= args.file_threshold)
    long_function_count = sum(1 for item in function_stats if item.line_count >= args.function_threshold)

    print(f"Source root: {source_root}")
    print(f"Files scanned: {len(file_stats)}")
    print(f"Total lines: {total_lines}")
    print(f"Files >= {args.file_threshold} lines: {large_file_count}")
    print(f"Functions >= {args.function_threshold} lines: {long_function_count}")

    print_section(f"Top {args.top_files} large files")
    for stat in sorted(file_stats, key=lambda item: item.line_count, reverse=True)[: args.top_files]:
        print(f"{stat.line_count:5d}  {format_path(stat.path, source_root)}")

    print_section(f"Top {args.top_dirs} dense directories")
    for stat in sorted(directory_stats, key=lambda item: item.line_count, reverse=True)[: args.top_dirs]:
        print(f"{stat.line_count:5d} lines  {stat.file_count:3d} files  {stat.path.as_posix()}")

    print_section(f"Top {args.top_functions} long functions")
    for stat in sorted(function_stats, key=lambda item: item.line_count, reverse=True)[: args.top_functions]:
        print(
            f"{stat.line_count:5d}  {format_path(stat.path, source_root)}:{stat.start_line}  {stat.name}"
        )

    if pattern_hits:
        print_section("High-frequency patterns by file")
        grouped: dict[str, list[PatternHit]] = defaultdict(list)
        totals = Counter()
        for hit in pattern_hits:
            grouped[hit.label].append(hit)
            totals[hit.label] += hit.count
        for label in sorted(grouped, key=lambda item: totals[item], reverse=True):
            print(f"{label}: total={totals[label]}")
            for hit in sorted(grouped[label], key=lambda item: item.count, reverse=True)[:10]:
                print(f"  {hit.count:3d}  {format_path(hit.path, source_root)}")

    if args.duplicates:
        duplicates = collect_duplicate_chunks(
            source_root=source_root,
            chunk_size=args.duplicate_lines,
            min_files=args.duplicate_min_files,
            max_results=args.duplicate_top,
        )
        if duplicates:
            print_section("Normalized duplicate chunks")
            for item in duplicates:
                first_line = item.snippet.splitlines()[0][:120]
                print(
                    f"occurrences={item.occurrences} files={item.file_count} snippet={first_line}"
                )
                for path, line in item.locations:
                    print(f"  {format_path(path, source_root)}:{line}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
