from __future__ import annotations

from pathlib import Path
import sys


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
CODE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".inl", ".py"}
SKIP_DIRS = {
    ".git",
    ".venv",
    "__pycache__",
    "build",
    "tests/reports",
}
SKIP_FILES = {
    "scripts/migration/validate_runtime_asset_cutover.py",
}

ALLOWED_MATCHES = {
    "config/machine_config.ini": set(),
    "templates.json": set(),
}


def _should_skip(path: Path) -> bool:
    relative = path.relative_to(WORKSPACE_ROOT).as_posix()
    if relative in SKIP_FILES:
        return True
    for skip_dir in SKIP_DIRS:
        if relative == skip_dir or relative.startswith(skip_dir + "/"):
            return True
    return False


def _iter_code_files() -> list[Path]:
    files: list[Path] = []
    for path in WORKSPACE_ROOT.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() not in CODE_SUFFIXES:
            continue
        if _should_skip(path):
            continue
        files.append(path)
    return files


def main() -> int:
    files = _iter_code_files()
    issues: list[str] = []
    observed: dict[str, list[str]] = {needle: [] for needle in ALLOWED_MATCHES}

    for path in files:
        relative = path.relative_to(WORKSPACE_ROOT).as_posix()
        try:
            content = path.read_text(encoding="utf-8", errors="ignore")
        except OSError as exc:
            issues.append(f"failed to read {relative}: {exc}")
            continue

        for needle, allowed_paths in ALLOWED_MATCHES.items():
            if needle not in content:
                continue
            observed[needle].append(relative)
            if relative not in allowed_paths:
                issues.append(f"unexpected '{needle}' reference in {relative}")

    print("[runtime-asset-cutover]")
    for needle in sorted(ALLOWED_MATCHES):
        matched = sorted(observed[needle])
        print(f"  - needle={needle}")
        if matched:
            for relative in matched:
                print(f"    * {relative}")
        else:
            print("    * none")

    print("[validation]")
    print(f"  - scanned_file_count={len(files)}")
    print(f"  - issue_count={len(issues)}")

    if issues:
        print("[issues]")
        for issue in issues:
            print(f"  - {issue}")
        return 1

    print("[issues]")
    print("  - none")
    return 0


if __name__ == "__main__":
    sys.exit(main())

