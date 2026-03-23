from __future__ import annotations

import os
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = ROOT / "packages" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.workspace_layout import load_workspace_layout  # noqa: E402


REQUIRED_KEYS = (
    "SILIGEN_MULTICARD_VENDOR_DIR",
    "SILIGEN_RUNTIME_HOST_DIR",
    "SILIGEN_PROCESS_RUNTIME_CORE_DIR",
    "SILIGEN_SHARED_KERNEL_DIR",
    "SILIGEN_TRANSPORT_GATEWAY_DIR",
    "SILIGEN_TRACEABILITY_OBSERVABILITY_DIR",
    "SILIGEN_DEVICE_ADAPTERS_DIR",
    "SILIGEN_DEVICE_CONTRACTS_DIR",
    "SILIGEN_SIMULATION_ENGINE_DIR",
    "SILIGEN_THIRD_PARTY_DIR",
    "SILIGEN_MACHINE_CONFIG_FILE",
    "SILIGEN_RECIPES_DIR",
    "SILIGEN_RECIPE_SCHEMAS_DIR",
)


def _contains(path: Path, snippet: str) -> bool:
    return snippet in path.read_text(encoding="utf-8")


def _control_apps_build_root() -> Path:
    env_root = os.getenv("SILIGEN_CONTROL_APPS_BUILD_ROOT", "").strip()
    if env_root:
        return Path(env_root).resolve()

    local_app_data = os.getenv("LOCALAPPDATA", "").strip()
    if local_app_data:
        return (Path(local_app_data) / "SiligenSuite" / "control-apps-build").resolve()

    return (ROOT / "build" / "control-apps").resolve()


def _read_cmake_home_directory(cache_file: Path) -> Path | None:
    if not cache_file.exists():
        return None

    for raw_line in cache_file.read_text(encoding="utf-8", errors="ignore").splitlines():
        if raw_line.startswith("CMAKE_HOME_DIRECTORY:"):
            _, _, value = raw_line.partition("=")
            resolved = value.strip()
            if not resolved:
                return None
            return Path(resolved).resolve()

    return None


def main() -> int:
    layout = load_workspace_layout(ROOT)
    control_apps_build_root = _control_apps_build_root()
    control_apps_cache = control_apps_build_root / "CMakeCache.txt"
    control_apps_cmake_home = _read_cmake_home_directory(control_apps_cache)

    missing_keys = [key for key in REQUIRED_KEYS if key not in layout]
    missing_paths = [key for key in REQUIRED_KEYS if key in layout and not layout[key].absolute.exists()]
    wiring_checks = {
        "root_cmake_uses_loader": _contains(ROOT / "CMakeLists.txt", "LoadWorkspaceLayout.cmake"),
        "root_cmake_pins_workspace_root": _contains(
            ROOT / "CMakeLists.txt",
            'set(SILIGEN_WORKSPACE_ROOT "${SILIGEN_CMAKE_SOURCE_ROOT}")',
        ),
        "root_cmake_rejects_noncanonical_source_dir": _contains(
            ROOT / "CMakeLists.txt",
            "workspace root must be canonical superbuild source root",
        ),
        "tests_cmake_uses_loader": _contains(ROOT / "tests" / "CMakeLists.txt", "LoadWorkspaceLayout.cmake"),
        "tests_cmake_pins_workspace_root_candidate": _contains(
            ROOT / "tests" / "CMakeLists.txt",
            'set(SILIGEN_WORKSPACE_ROOT "${SILIGEN_WORKSPACE_ROOT_CANDIDATE}")',
        ),
        "tests_cmake_rejects_noncanonical_source_dir": _contains(
            ROOT / "tests" / "CMakeLists.txt",
            "tests root must resolve to canonical superbuild source root",
        ),
        "build_validation_uses_layout_script": _contains(
            ROOT / "tools" / "build" / "build-validation.ps1",
            "get-workspace-layout.ps1",
        ),
        "build_validation_reports_workspace_root": _contains(
            ROOT / "tools" / "build" / "build-validation.ps1",
            "workspace root:",
        ),
        "workspace_validation_uses_layout_module": _contains(
            ROOT / "packages" / "test-kit" / "src" / "test_kit" / "workspace_validation.py",
            "workspace_layout",
        ),
        "workspace_validation_reports_workspace_root": _contains(
            ROOT / "packages" / "test-kit" / "src" / "test_kit" / "workspace_validation.py",
            '"workspace_root"',
        ),
        "workspace_validation_reports_cmake_home": _contains(
            ROOT / "packages" / "test-kit" / "src" / "test_kit" / "workspace_validation.py",
            "control_apps_cmake_home_directory",
        ),
    }
    provenance_checks = {
        "control_apps_cache_home_directory_present_when_cache_exists": (
            (not control_apps_cache.exists()) or control_apps_cmake_home is not None
        ),
        "control_apps_cache_home_directory_matches_workspace_root_when_cache_exists": (
            (not control_apps_cache.exists()) or control_apps_cmake_home == ROOT
        ),
    }

    print("[layout]")
    print(f"  - layout_file={ROOT / 'cmake' / 'workspace-layout.env'}")
    print(f"  - entry_count={len(layout)}")
    for key in REQUIRED_KEYS:
        if key in layout:
            print(f"  - {key}={layout[key].relative}")
    print(f"  - workspace_root={ROOT}")

    print("[provenance]")
    print(f"  - control_apps_build_root={control_apps_build_root}")
    print(f"  - control_apps_cache_file={control_apps_cache}")
    print(f"  - control_apps_cache_exists={str(control_apps_cache.exists()).lower()}")
    print(
        "  - control_apps_cmake_home_directory="
        + (str(control_apps_cmake_home) if control_apps_cmake_home is not None else "missing")
    )
    for key, value in provenance_checks.items():
        print(f"  - {key}={str(value).lower()}")

    print("[wiring]")
    for key, value in wiring_checks.items():
        print(f"  - {key}={str(value).lower()}")

    print("[validation]")
    print(f"  - missing_key_count={len(missing_keys)}")
    print(f"  - missing_path_count={len(missing_paths)}")
    print(f"  - failed_provenance_count={sum(1 for value in provenance_checks.values() if not value)}")
    print(f"  - failed_wiring_count={sum(1 for value in wiring_checks.values() if not value)}")

    issues: list[str] = []
    issues.extend(f"missing key: {key}" for key in missing_keys)
    issues.extend(f"missing path: {key}" for key in missing_paths)
    issues.extend(f"provenance check failed: {key}" for key, value in provenance_checks.items() if not value)
    issues.extend(f"wiring not enabled: {key}" for key, value in wiring_checks.items() if not value)

    if issues:
        print("[issues]")
        for issue in issues:
            print(f"  - {issue}")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
