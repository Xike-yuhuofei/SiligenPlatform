from __future__ import annotations

import re
import sys
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]

TEXT_SUFFIXES = {
    ".cmake",
    ".cpp",
    ".h",
    ".hpp",
    ".inl",
    ".ps1",
    ".py",
    ".txt",
}


def iter_text_files(base: Path):
    for path in base.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() in TEXT_SUFFIXES:
            yield path


def scan(base: Path, patterns: list[tuple[str, re.Pattern[str]]]) -> list[dict[str, str]]:
    results: list[dict[str, str]] = []
    for path in iter_text_files(base):
        try:
            content = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            content = path.read_text(encoding="utf-8", errors="ignore")
        for line_no, line in enumerate(content.splitlines(), start=1):
            for label, pattern in patterns:
                if pattern.search(line):
                    results.append(
                        {
                            "rule": label,
                            "file": str(path.relative_to(ROOT)),
                            "line": str(line_no),
                            "text": line.strip(),
                        }
                    )
    return results


def print_section(title: str):
    print(f"[{title}]")


def print_entries(entries: list[dict[str, str]], sample_limit: int = 5):
    if not entries:
        print("  - none")
        return
    grouped: dict[str, list[dict[str, str]]] = defaultdict(list)
    for entry in entries:
        grouped[entry["rule"]].append(entry)
    for rule, items in sorted(grouped.items()):
        print(f"  - {rule}: {len(items)}")
        for entry in items[:sample_limit]:
            print(f"    sample | {entry['file']}:{entry['line']} | {entry['text']}")


def main() -> int:
    shared_kernel_dir = ROOT / "shared" / "kernel"
    contracts_dir = ROOT / "modules" / "runtime-execution" / "device-contracts"
    adapters_dir = ROOT / "modules" / "runtime-execution" / "device-adapters"

    legacy_shared_kernel_pattern = re.compile(r"modules[/\\]shared-kernel")
    legacy_device_hal_pattern = re.compile(r"modules[/\\]device-hal")

    shared_kernel_illegal = scan(
        shared_kernel_dir,
        [
            ("shared-kernel->legacy-shared-kernel", legacy_shared_kernel_pattern),
            ("shared-kernel->device", re.compile(r"siligen/device|device-contracts|device-adapters")),
            ("shared-kernel->process-runtime-core", re.compile(r"siligen/process|process-runtime-core|siligen_process_core")),
        ],
    )

    contracts_illegal = scan(
        contracts_dir,
        [
            ("contracts->adapters", re.compile(r"device-adapters|siligen_device_adapters")),
            ("contracts->process-runtime-core", re.compile(r"process-runtime-core|siligen_process_core")),
            ("contracts->legacy-shared-kernel", legacy_shared_kernel_pattern),
            ("contracts->legacy-device-hal", legacy_device_hal_pattern),
        ],
    )

    adapters_illegal = scan(
        adapters_dir,
        [
            ("adapters->legacy-shared-kernel", legacy_shared_kernel_pattern),
            ("adapters->legacy-device-hal", legacy_device_hal_pattern),
        ],
    )

    shared_kernel_complete = all(
        (shared_kernel_dir / path).exists()
        for path in [
            Path("CMakeLists.txt"),
            Path("include/siligen/shared/numeric_types.h"),
            Path("include/siligen/shared/axis_types.h"),
            Path("include/siligen/shared/basic_types.h"),
            Path("include/siligen/shared/point2d.h"),
            Path("include/siligen/shared/error.h"),
            Path("include/siligen/shared/result.h"),
            Path("include/siligen/shared/result_types.h"),
            Path("include/siligen/shared/strings/string_manipulator.h"),
            Path("src/strings/string_manipulator.cpp"),
        ]
    )
    contracts_complete = all(
        (contracts_dir / "include" / "siligen" / "device" / "contracts" / part).exists()
        for part in ["commands", "state", "capabilities", "faults", "events", "ports"]
    )
    adapters_complete = all(
        (adapters_dir / path).exists()
        for path in [
            Path("include/siligen/device/adapters/motion/multicard_motion_device.h"),
            Path("include/siligen/device/adapters/io/multicard_digital_io_adapter.h"),
            Path("include/siligen/device/adapters/fake/fake_motion_device.h"),
            Path("include/siligen/device/adapters/fake/fake_dispenser_device.h"),
            Path("vendor/multicard/MultiCard.lib"),
        ]
    )

    print_section("boundary")
    print(f"  - shared_kernel_files_complete={shared_kernel_complete}")
    print(f"  - contracts_dirs_complete={contracts_complete}")
    print(f"  - adapters_dirs_complete={adapters_complete}")

    print_section("illegal-dependencies")
    print("shared-kernel")
    print_entries(shared_kernel_illegal)
    print("device-contracts")
    print_entries(contracts_illegal)
    print("device-adapters")
    print_entries(adapters_illegal)

    print_section("validation")
    print(f"  - shared_kernel_illegal_count={len(shared_kernel_illegal)}")
    print(f"  - contracts_illegal_count={len(contracts_illegal)}")
    print(f"  - adapters_illegal_count={len(adapters_illegal)}")

    return 0 if shared_kernel_complete and not shared_kernel_illegal and not contracts_illegal and not adapters_illegal else 1


if __name__ == "__main__":
    sys.exit(main())
