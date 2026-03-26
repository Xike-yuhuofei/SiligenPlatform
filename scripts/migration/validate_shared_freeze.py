from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = Path(__file__).with_name("shared_freeze_manifest.json")
CODE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".inl", ".py"}
FORBIDDEN_CONTRACT_DIRS = {"src", "runtime", "adapters", "vendor", "tools"}


def iter_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if "__pycache__" in path.parts or path.suffix.lower() == ".pyc":
            continue
        files.append(path)
    return files


def collect_code_like_files(root: Path) -> set[str]:
    return {
        path.relative_to(root).as_posix()
        for path in iter_files(root)
        if path.suffix.lower() in CODE_SUFFIXES or path.name in {"CMakeLists.txt", "pyproject.toml"}
    }


def validate_contract_package(package_root: Path, *, allowed_source_roots: set[str]) -> list[str]:
    issues: list[str] = []
    for path in iter_files(package_root):
        relative = path.relative_to(package_root)
        if any(part in FORBIDDEN_CONTRACT_DIRS for part in relative.parts):
            issues.append(f"{relative.as_posix()} in forbidden expansion directory")
            continue
        if path.suffix.lower() in CODE_SUFFIXES and relative.parts[0] not in allowed_source_roots:
            issues.append(
                f"{relative.as_posix()} is code-like content outside allowed roots {sorted(allowed_source_roots)}"
            )
    return issues


def validate_manifest(package_root: Path, manifest: set[str], *, label: str) -> list[str]:
    issues: list[str] = []
    actual = collect_code_like_files(package_root)
    extra = sorted(actual - manifest)
    missing = sorted(manifest - actual)
    for rel in extra:
        issues.append(f"{label}: unexpected file {rel}")
    for rel in missing:
        issues.append(f"{label}: missing file {rel}")
    return issues


def load_manifest() -> dict[str, set[str]]:
    data = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    return {key: set(value) for key, value in data.items()}


def main() -> int:
    manifest = load_manifest()
    shared_root = ROOT / "shared" / "kernel"
    test_kit_root = ROOT / "shared" / "testing" / "test-kit"
    application_contracts_root = ROOT / "shared" / "contracts" / "application"
    device_contracts_root = ROOT / "shared" / "contracts" / "device"
    engineering_contracts_root = ROOT / "shared" / "contracts" / "engineering"

    print("[boundary]")
    print("  - shared/contracts/application: contract-only target freeze")
    print("  - shared/contracts/device: contract-only target freeze")
    print("  - shared/contracts/engineering: contract-only target freeze")
    print("  - shared/kernel: manifest-frozen shared target freeze")
    print("  - shared/testing/test-kit: manifest-frozen shared target freeze")

    issues: list[str] = []
    issues.extend(
        f"shared/contracts/application: {issue}"
        for issue in validate_contract_package(application_contracts_root, allowed_source_roots={"tests"})
    )
    issues.extend(
        f"shared/contracts/device: {issue}"
        for issue in validate_contract_package(device_contracts_root, allowed_source_roots={"include"})
    )
    issues.extend(
        f"shared/contracts/engineering: {issue}"
        for issue in validate_contract_package(engineering_contracts_root, allowed_source_roots={"tests"})
    )
    issues.extend(
        f"shared/kernel: {issue}"
        for issue in validate_manifest(shared_root, manifest["shared-kernel"], label="shared-kernel")
    )
    issues.extend(
        f"shared/testing/test-kit: {issue}"
        for issue in validate_manifest(test_kit_root, manifest["test-kit"], label="test-kit")
    )

    print("[illegal-expansion]")
    if issues:
        for issue in issues:
            print(f"  - {issue}")
    else:
        print("  - none")

    print("[validation]")
    print(f"  - illegal_expansion_count={len(issues)}")
    print(f"  - shared_kernel_manifest_count={len(manifest['shared-kernel'])}")
    print(f"  - test_kit_manifest_count={len(manifest['test-kit'])}")
    print("  - contract_surfaces_scanned=3")

    return 0 if not issues else 1


if __name__ == "__main__":
    raise SystemExit(main())
