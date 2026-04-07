from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.legacy_fact_catalog import load_legacy_fact_catalog
from test_kit.asset_catalog import advisory_baseline_governance_issues, baseline_governance_summary, blocking_baseline_governance_issues
from test_kit.workspace_layout import load_workspace_layout


REQUIRED_KEYS = (
    "SILIGEN_APPS_ROOT",
    "SILIGEN_MODULES_ROOT",
    "SILIGEN_SHARED_ROOT",
    "SILIGEN_DOCS_ROOT",
    "SILIGEN_SAMPLES_ROOT",
    "SILIGEN_TESTS_ROOT",
    "SILIGEN_SCRIPTS_ROOT",
    "SILIGEN_CONFIG_ROOT",
    "SILIGEN_DATA_ROOT",
    "SILIGEN_DEPLOY_ROOT",
    "SILIGEN_THIRD_PARTY_DIR",
    "SILIGEN_MACHINE_CONFIG_FILE",
    "SILIGEN_RECIPES_DIR",
    "SILIGEN_RECIPE_SCHEMAS_DIR",
    "SILIGEN_FORMAL_FREEZE_DOCSET_DIR",
    "SILIGEN_FORMAL_FREEZE_INDEX_FILE",
)

LEGACY_LAYOUT_KEYS = (
    "SILIGEN_MULTICARD_VENDOR_DIR",
    "SILIGEN_RUNTIME_HOST_DIR",
    "SILIGEN_PROCESS_RUNTIME_CORE_DIR",
    "SILIGEN_SHARED_KERNEL_DIR",
    "SILIGEN_TRANSPORT_GATEWAY_DIR",
    "SILIGEN_TRACEABILITY_OBSERVABILITY_DIR",
    "SILIGEN_DEVICE_ADAPTERS_DIR",
    "SILIGEN_DEVICE_CONTRACTS_DIR",
    "SILIGEN_SIMULATION_ENGINE_DIR",
)

LEGACY_FACT_CATALOG = load_legacy_fact_catalog(ROOT)
LEGACY_ROOTS = LEGACY_FACT_CATALOG.root_paths_for_rule("legacy-root-presence")

MODULES = (
    "workflow",
    "job-ingest",
    "dxf-geometry",
    "topology-feature",
    "process-planning",
    "coordinate-alignment",
    "process-path",
    "motion-planning",
    "dispense-packaging",
    "runtime-execution",
    "trace-diagnostics",
    "hmi-application",
)

MODULE_REQUIRED_DIRS = (
    "contracts",
    "domain",
    "services",
    "application",
    "adapters",
    "tests",
    "examples",
)

MODULE_REQUIRED_TEST_DIRS = (
    "tests/unit",
    "tests/contract",
    "tests/integration",
    "tests/regression",
)

BRIDGE_ROOTS = LEGACY_FACT_CATALOG.root_paths_for_rule("bridge-root-presence")
FORBIDDEN_MODULE_YAML_SNIPPETS = LEGACY_FACT_CATALOG.snippets_for_rule(
    "bridge-metadata",
    target_glob="modules/**/module.yaml",
)
FORBIDDEN_CMAKE_SNIPPETS = LEGACY_FACT_CATALOG.snippets_for_rule(
    "bridge-metadata",
    target_glob="modules/**/CMakeLists.txt",
)

def _contains(path: Path, snippet: str) -> bool:
    if not path.exists():
        return False
    return snippet in path.read_text(encoding="utf-8", errors="ignore")


def _extract_cmake_property_value(cmake_text: str, property_name: str) -> str | None:
    pattern = rf"{property_name}\s+\"([^\"]+)\""
    match = re.search(pattern, cmake_text, re.MULTILINE | re.DOTALL)
    if not match:
        return None
    return match.group(1).strip()


def _split_cmake_list(value: str) -> tuple[str, ...]:
    return tuple(item.strip() for item in value.split(";") if item.strip())


def _validate_module_skeleton(root: Path) -> list[str]:
    issues: list[str] = []
    modules_root = root / "modules"

    for module_name in MODULES:
        module_root = modules_root / module_name
        if not module_root.exists():
            issues.append(f"missing module root: modules/{module_name}")
            continue

        for relative in MODULE_REQUIRED_DIRS:
            path = module_root / relative
            if not path.exists():
                issues.append(f"missing canonical module directory: modules/{module_name}/{relative}")

        for relative in MODULE_REQUIRED_TEST_DIRS:
            path = module_root / relative
            if not path.exists():
                issues.append(f"missing canonical test directory: modules/{module_name}/{relative}")

        module_yaml = module_root / "module.yaml"
        if not module_yaml.exists():
            issues.append(f"missing module metadata: modules/{module_name}/module.yaml")
        else:
            yaml_text = module_yaml.read_text(encoding="utf-8", errors="ignore")
            for snippet in FORBIDDEN_MODULE_YAML_SNIPPETS:
                if snippet in yaml_text:
                    issues.append(f"bridge metadata still active: modules/{module_name}/module.yaml -> {snippet}")

        cmake_path = module_root / "CMakeLists.txt"
        if not cmake_path.exists():
            issues.append(f"missing module build entry: modules/{module_name}/CMakeLists.txt")
            continue

        cmake_text = cmake_path.read_text(encoding="utf-8", errors="ignore")
        for snippet in FORBIDDEN_CMAKE_SNIPPETS:
            if snippet in cmake_text:
                issues.append(f"bridge CMake metadata still active: modules/{module_name}/CMakeLists.txt -> {snippet}")

        owner_root = _extract_cmake_property_value(cmake_text, "SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT")
        implementation_roots = _extract_cmake_property_value(cmake_text, "SILIGEN_TARGET_IMPLEMENTATION_ROOTS")
        if owner_root != f"modules/{module_name}":
            issues.append(f"owner root mismatch: modules/{module_name}/CMakeLists.txt -> {owner_root!r}")
        if implementation_roots is None:
            issues.append(f"missing implementation roots metadata: modules/{module_name}/CMakeLists.txt")
        else:
            values = _split_cmake_list(implementation_roots)
            if not values:
                issues.append(f"empty implementation roots metadata: modules/{module_name}/CMakeLists.txt")
            if not any(item.startswith(f"modules/{module_name}/") for item in values):
                issues.append(f"implementation roots missing canonical module paths: modules/{module_name}/CMakeLists.txt")

    return issues


def _validate_bridge_roots_absent(root: Path) -> list[str]:
    issues: list[str] = []
    for relative in BRIDGE_ROOTS:
        if (root / relative).exists():
            issues.append(f"bridge root must be physically deleted: {relative}")
    return issues


def _validate_root_wiring(root: Path) -> list[str]:
    issues: list[str] = []
    required_paths = (
        root / "docs" / "architecture" / "migration-alignment-clearance-matrix.md",
        root / "docs" / "architecture" / "bridge-exit-closeout.md",
        root / "shared" / "contracts" / "CMakeLists.txt",
        root / "scripts" / "build" / "build-validation.ps1",
        root / "scripts" / "validation" / "run-local-validation-gate.ps1",
        root / "ci.ps1",
        root / "tests" / "contracts" / "test_bridge_exit_contract.py",
    )
    for path in required_paths:
        if not path.exists():
            issues.append(f"missing closeout asset: {path.relative_to(root).as_posix()}")

    if not _contains(root / "CMakeLists.txt", "workspace root must be canonical superbuild source root"):
        issues.append("CMakeLists.txt 缺少 canonical superbuild root 断言")
    if not _contains(root / "tests" / "CMakeLists.txt", "tests root must resolve to canonical superbuild source root"):
        issues.append("tests/CMakeLists.txt 缺少 canonical tests root 断言")
    if not _contains(root / "scripts" / "build" / "build-validation.ps1", "validate_workspace_layout.py"):
        issues.append("build-validation 未执行 workspace layout gate")
    if not _contains(root / "scripts" / "validation" / "run-local-validation-gate.ps1", "legacy-exit-checks.py"):
        issues.append("run-local-validation-gate 未执行 legacy-exit gate")
    if not _contains(root / "ci.ps1", "legacy-exit-checks.py"):
        issues.append("ci.ps1 未执行 legacy-exit gate")
    if not (root / "scripts" / "validation" / "check_no_loose_mock.py").exists():
        issues.append("scripts/validation 缺少 check_no_loose_mock.py")

    shared_contracts = root / "shared" / "contracts" / "CMakeLists.txt"
    if shared_contracts.exists():
        text = shared_contracts.read_text(encoding="utf-8", errors="ignore")
        if 'set(SILIGEN_DEVICE_CONTRACTS_CANONICAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/device")' not in text:
            issues.append("shared/contracts 未从 shared/contracts/device canonical root 解析设备契约")
        if "modules/runtime-execution/device-contracts" in text:
            issues.append("shared/contracts 仍引用 runtime device-contracts bridge 根")
        if "modules/runtime-execution/contracts/device" in text:
            issues.append("shared/contracts 仍把 runtime-execution/contracts/device 当作 canonical device contracts 根")

    shared_device_contracts = root / "shared" / "contracts" / "device" / "CMakeLists.txt"
    if not shared_device_contracts.exists():
        issues.append("missing closeout asset: shared/contracts/device/CMakeLists.txt")

    workflow_application = root / "modules" / "workflow" / "application" / "CMakeLists.txt"
    if workflow_application.exists():
        workflow_application_text = workflow_application.read_text(encoding="utf-8", errors="ignore")
        for required in (
            "siligen_job_ingest_application_public",
            "siligen_dxf_geometry_application_public",
            "siligen_runtime_execution_application_public",
        ):
            if required not in workflow_application_text:
                issues.append(f"workflow application owner wiring missing: {required}")

    forbidden_reverse_mutations = (
        (
            root / "modules" / "job-ingest" / "CMakeLists.txt",
            (
                "siligen_application_dispensing",
                "siligen_process_runtime_core_application",
                "siligen_workflow_application_public",
            ),
            "job-ingest must not mutate workflow targets",
        ),
        (
            root / "modules" / "dxf-geometry" / "CMakeLists.txt",
            (
                "siligen_application_dispensing",
                "siligen_process_runtime_core_application",
                "siligen_workflow_application_public",
            ),
            "dxf-geometry must not mutate workflow targets",
        ),
        (
            root / "modules" / "runtime-execution" / "application" / "CMakeLists.txt",
            (
                "siligen_workflow_application_headers",
                "siligen_application_dispensing",
                "siligen_process_runtime_core_application",
            ),
            "runtime-execution/application must not mutate workflow targets",
        ),
    )
    for path, snippets, message in forbidden_reverse_mutations:
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        for snippet in snippets:
            if snippet in text:
                issues.append(f"{message}: {path.relative_to(root).as_posix()} -> {snippet}")

    workflow_dxf_wrapper = (
        root
        / "modules"
        / "workflow"
        / "application"
        / "include"
        / "application"
        / "services"
        / "dxf"
        / "DxfPbPreparationService.h"
    )
    if workflow_dxf_wrapper.exists():
        wrapper_text = workflow_dxf_wrapper.read_text(encoding="utf-8", errors="ignore")
        if "dxf_geometry/application/services/dxf/DxfPbPreparationService.h" not in wrapper_text:
            issues.append("workflow dxf wrapper must forward to dxf_geometry/application/services/dxf/DxfPbPreparationService.h")

    return issues


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate canonical workspace layout and zero-bridge closeout.")
    parser.add_argument("--wave", default="Bridge Exit Closeout")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    layout = load_workspace_layout(ROOT)

    missing_keys: list[str] = []
    missing_paths: list[str] = []
    present_legacy_keys: list[str] = []
    present_legacy_roots: list[str] = []

    for key in REQUIRED_KEYS:
        if key not in layout:
            missing_keys.append(key)
            continue
        entry = layout[key]
        if not entry.absolute.exists():
            missing_paths.append(f"{key} -> {entry.relative}")

    for key in LEGACY_LAYOUT_KEYS:
        if key in layout:
            present_legacy_keys.append(key)

    for root_name in LEGACY_ROOTS:
        if (ROOT / root_name).exists():
            present_legacy_roots.append(root_name)

    module_failures = _validate_module_skeleton(ROOT)
    bridge_root_failures = _validate_bridge_roots_absent(ROOT)
    root_wiring_failures = _validate_root_wiring(ROOT)
    baseline_governance = baseline_governance_summary(ROOT)
    baseline_blocking_failures = blocking_baseline_governance_issues(baseline_governance)
    baseline_advisory_failures = advisory_baseline_governance_issues(baseline_governance)

    print("[wave]")
    print(f"  - requested_wave={args.wave}")

    print("[layout]")
    print(f"  - layout_file={ROOT / 'cmake' / 'workspace-layout.env'}")
    print(f"  - entry_count={len(layout)}")

    print("[validation]")
    print(f"  - missing_key_count={len(missing_keys)}")
    print(f"  - missing_path_count={len(missing_paths)}")
    print(f"  - legacy_layout_key_count={len(present_legacy_keys)}")
    print(f"  - legacy_root_present_count={len(present_legacy_roots)}")
    print(f"  - module_failure_count={len(module_failures)}")
    print(f"  - bridge_root_failure_count={len(bridge_root_failures)}")
    print(f"  - root_wiring_failure_count={len(root_wiring_failures)}")
    print(f"  - baseline_blocking_issue_count={len(baseline_blocking_failures)}")
    print(f"  - baseline_advisory_issue_count={len(baseline_advisory_failures)}")

    issues: list[str] = []
    issues.extend(f"missing key: {item}" for item in missing_keys)
    issues.extend(f"missing path: {item}" for item in missing_paths)
    issues.extend(f"legacy layout key must be removed: {item}" for item in present_legacy_keys)
    issues.extend(f"legacy root must be physically deleted: {item}" for item in present_legacy_roots)
    issues.extend(module_failures)
    issues.extend(bridge_root_failures)
    issues.extend(root_wiring_failures)
    issues.extend(baseline_blocking_failures)

    print("[issues]")
    if issues:
        for issue in issues:
            print(f"  - {issue}")
        return 1

    print("  - none")
    print("[advisory-issues]")
    if baseline_advisory_failures:
        for issue in baseline_advisory_failures:
            print(f"  - {issue}")
    else:
        print("  - none")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
