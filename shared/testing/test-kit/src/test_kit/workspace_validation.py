from __future__ import annotations

import argparse
import os
from datetime import datetime, timezone
from pathlib import Path

from .runner import (
    KNOWN_FAILURE_EXIT_CODE,
    SKIPPED_EXIT_CODE,
    ValidationCase,
    ValidationReport,
    python_command,
    report_to_json,
    report_to_markdown,
    run_case,
)
from .workspace_layout import load_workspace_layout


WORKSPACE_ROOT = Path(__file__).resolve().parents[5]
DEFAULT_SUITES = ("apps", "contracts", "e2e", "protocol-compatibility")
WORKSPACE_LAYOUT = load_workspace_layout(WORKSPACE_ROOT)
CONTROL_APPS_BUILD_ROOT = Path(
    os.getenv(
        "SILIGEN_CONTROL_APPS_BUILD_ROOT",
        str(Path(os.getenv("LOCALAPPDATA", str(WORKSPACE_ROOT))) / "SiligenSuite" / "control-apps-build"),
    )
)


def _default_report_dir() -> Path:
    return WORKSPACE_ROOT / "tests" / "reports"


def _layout_absolute_path(key: str) -> Path:
    return WORKSPACE_LAYOUT[key].absolute


def _control_apps_executable(name: str) -> Path:
    candidates = (
        CONTROL_APPS_BUILD_ROOT / "bin" / name,
        CONTROL_APPS_BUILD_ROOT / "bin" / "Debug" / name,
        CONTROL_APPS_BUILD_ROOT / "bin" / "Release" / name,
        CONTROL_APPS_BUILD_ROOT / "bin" / "RelWithDebInfo" / name,
    )
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def _powershell_file_command(script_path: Path, *script_args: str) -> list[str]:
    return [
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        str(script_path),
        *script_args,
    ]


def _resolve_freeze_report_dir(base_report_dir: Path) -> Path:
    configured = os.getenv("SILIGEN_FREEZE_DOCSET_REPORT_DIR", "").strip()
    if configured:
        return Path(configured).resolve()
    resolved_report_dir = base_report_dir.resolve()
    return (
        resolved_report_dir
        if resolved_report_dir.name == "dsp-e2e-spec-docset"
        else resolved_report_dir / "dsp-e2e-spec-docset"
    )


def _resolve_legacy_exit_report_dir(base_report_dir: Path) -> Path:
    configured = os.getenv("SILIGEN_LEGACY_EXIT_REPORT_DIR", "").strip()
    if configured:
        return Path(configured).resolve()
    resolved_report_dir = base_report_dir.resolve()
    return resolved_report_dir if resolved_report_dir.name == "legacy-exit" else resolved_report_dir / "legacy-exit"


def _freeze_evidence_cases() -> tuple[str, ...]:
    configured = os.getenv("SILIGEN_FREEZE_EVIDENCE_CASES", "").strip()
    if not configured:
        return ("success", "block", "rollback", "recovery", "archive")
    requested = tuple(item.strip() for item in configured.split(",") if item.strip())
    supported = ("success", "block", "rollback", "recovery", "archive")
    return tuple(item for item in requested if item in supported) or supported


def _freeze_validator_command(
    *,
    report_dir: Path,
    report_stem: str,
    scenario_classes: tuple[str, ...] = (),
) -> list[str]:
    command = [
        *python_command(WORKSPACE_ROOT / "scripts" / "migration" / "validate_dsp_e2e_spec_docset.py"),
        "--report-dir",
        str(report_dir),
        "--report-stem",
        report_stem,
    ]
    for scenario_class in scenario_classes:
        command.extend(["--scenario-class", scenario_class])
    return command


def _normalize_suites(requested: list[str] | None) -> list[str]:
    if not requested or "all" in requested:
        return list(DEFAULT_SUITES)

    normalized: list[str] = []
    for suite in requested:
        if suite not in normalized:
            normalized.append(suite)
    return normalized


def _workspace_validation_metadata() -> dict[str, str]:
    control_apps_cache = CONTROL_APPS_BUILD_ROOT / "CMakeCache.txt"
    cmake_home = ""
    if control_apps_cache.exists():
        for raw_line in control_apps_cache.read_text(encoding="utf-8", errors="ignore").splitlines():
            if raw_line.startswith("CMAKE_HOME_DIRECTORY:"):
                _, _, value = raw_line.partition("=")
                cmake_home = value.strip()
                break

    metadata = {
        "workspace_layout_file": str((WORKSPACE_ROOT / "cmake" / "workspace-layout.env").resolve()),
        "workspace_root": str(WORKSPACE_ROOT.resolve()),
        "tests_root": str((WORKSPACE_ROOT / "tests").resolve()),
        "scripts_root": str((WORKSPACE_ROOT / "scripts").resolve()),
        "samples_root": str((WORKSPACE_ROOT / "samples").resolve()),
        "control_apps_build_root": str(CONTROL_APPS_BUILD_ROOT.resolve()),
        "control_apps_cmake_home_directory": cmake_home,
    }

    for key in ("SILIGEN_FORMAL_FREEZE_DOCSET_DIR", "SILIGEN_FORMAL_FREEZE_INDEX_FILE"):
        if key in WORKSPACE_LAYOUT:
            metadata[key.lower()] = str(_layout_absolute_path(key))
    return metadata


def build_cases(
    profile: str,
    suites: list[str],
    *,
    include_hardware_smoke: bool = False,
    include_hil_closed_loop: bool = False,
    report_dir: Path,
) -> list[ValidationCase]:
    local_profile = profile == "local"
    resolved_report_dir = report_dir.resolve()
    freeze_report_dir = _resolve_freeze_report_dir(resolved_report_dir)
    legacy_exit_report_dir = _resolve_legacy_exit_report_dir(resolved_report_dir)
    freeze_evidence_cases = _freeze_evidence_cases()

    cases: list[ValidationCase] = []

    if "apps" in suites:
        cases.extend(
            [
                ValidationCase(
                    name="runtime-service-dry-run",
                    layer="apps",
                    description="runtime-service dry-run",
                    command=_powershell_file_command(WORKSPACE_ROOT / "apps" / "runtime-service" / "run.ps1", "-DryRun"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="runtime-gateway-dry-run",
                    layer="apps",
                    description="runtime-gateway dry-run",
                    command=_powershell_file_command(WORKSPACE_ROOT / "apps" / "runtime-gateway" / "run.ps1", "-DryRun"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="planner-cli-dry-run",
                    layer="apps",
                    description="planner-cli dry-run",
                    command=_powershell_file_command(WORKSPACE_ROOT / "apps" / "planner-cli" / "run.ps1", "-DryRun"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="hmi-app-offline-smoke",
                    layer="apps",
                    description="hmi-app 离线 smoke",
                    command=_powershell_file_command(WORKSPACE_ROOT / "apps" / "hmi-app" / "scripts" / "offline-smoke.ps1"),
                    cwd=WORKSPACE_ROOT,
                ),
            ]
        )

    if "contracts" in suites:
        cases.extend(
            [
                ValidationCase(
                    name="application-contracts",
                    layer="contracts",
                    description="application contracts compatibility",
                    command=python_command(WORKSPACE_ROOT / "tests" / "contracts" / "test_protocol_compatibility.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="engineering-contracts",
                    layer="contracts",
                    description="engineering contracts compatibility",
                    command=python_command(WORKSPACE_ROOT / "tests" / "contracts" / "test_engineering_contracts.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="engineering-data",
                    layer="contracts",
                    description="engineering data compatibility",
                    command=python_command(WORKSPACE_ROOT / "tests" / "contracts" / "test_engineering_data_compatibility.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="freeze-docset-contract",
                    layer="contracts",
                    description="formal freeze docset contract",
                    command=python_command(WORKSPACE_ROOT / "tests" / "contracts" / "test_freeze_docset_contract.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="migration-alignment-matrix",
                    layer="contracts",
                    description="migration alignment matrix contract",
                    command=python_command(WORKSPACE_ROOT / "tests" / "contracts" / "test_migration_alignment_matrix.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="bridge-exit-contract",
                    layer="contracts",
                    description="bridge exit contract",
                    command=python_command(WORKSPACE_ROOT / "tests" / "contracts" / "test_bridge_exit_contract.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="legacy-exit",
                    layer="contracts",
                    description="legacy and bridge exit checks",
                    command=[
                        *python_command(WORKSPACE_ROOT / "scripts" / "migration" / "legacy-exit-checks.py"),
                        "--profile",
                        profile,
                        "--report-dir",
                        str(legacy_exit_report_dir),
                    ],
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="workspace-layout-boundary",
                    layer="contracts",
                    description="workspace layout gate",
                    command=python_command(WORKSPACE_ROOT / "scripts" / "migration" / "validate_workspace_layout.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="dsp-e2e-spec-docset",
                    layer="freeze",
                    description="dsp-e2e-spec docset gate",
                    command=_freeze_validator_command(report_dir=freeze_report_dir, report_stem="dsp-e2e-spec-docset"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="shared-freeze-boundary",
                    layer="contracts",
                    description="shared freeze boundary gate",
                    command=python_command(WORKSPACE_ROOT / "scripts" / "migration" / "validate_shared_freeze.py"),
                    cwd=WORKSPACE_ROOT,
                ),
            ]
        )

        for scenario_class in freeze_evidence_cases:
            cases.append(
                ValidationCase(
                    name=f"dsp-e2e-spec-{scenario_class}",
                    layer="freeze",
                    description=f"dsp-e2e-spec {scenario_class} evidence case",
                    command=_freeze_validator_command(
                        report_dir=freeze_report_dir,
                        report_stem=f"dsp-e2e-spec-{scenario_class}",
                        scenario_classes=(scenario_class,),
                    ),
                    cwd=WORKSPACE_ROOT,
                )
            )

        if local_profile:
            cases.extend(
                [
                    ValidationCase(
                        name="shared-kernel-unit",
                        layer="unit",
                        description="shared-kernel unit tests",
                        command=[str(_control_apps_executable("siligen_shared_kernel_tests.exe"))],
                        cwd=WORKSPACE_ROOT,
                        allow_missing=True,
                    ),
                    ValidationCase(
                        name="runtime-host-unit",
                        layer="unit",
                        description="runtime-host unit tests",
                        command=[str(_control_apps_executable("siligen_runtime_host_unit_tests.exe"))],
                        cwd=WORKSPACE_ROOT,
                        allow_missing=True,
                    ),
                    ValidationCase(
                        name="dxf-geometry-unit",
                        layer="unit",
                        description="dxf-geometry unit tests",
                        command=[str(_control_apps_executable("siligen_dxf_geometry_unit_tests.exe"))],
                        cwd=WORKSPACE_ROOT,
                        allow_missing=True,
                    ),
                    ValidationCase(
                        name="job-ingest-unit",
                        layer="unit",
                        description="job-ingest unit tests",
                        command=[str(_control_apps_executable("siligen_job_ingest_unit_tests.exe"))],
                        cwd=WORKSPACE_ROOT,
                        allow_missing=True,
                    ),
                    ValidationCase(
                        name="workflow-unit",
                        layer="unit",
                        description="workflow core unit tests",
                        command=[str(_control_apps_executable("siligen_unit_tests.exe"))],
                        cwd=WORKSPACE_ROOT,
                        allow_missing=True,
                    ),
                ]
            )

    if "e2e" in suites:
        cases.append(
            ValidationCase(
                name="engineering-regression",
                layer="e2e",
                description="engineering regression scenarios",
                command=python_command(WORKSPACE_ROOT / "tests" / "e2e" / "scenarios" / "run_engineering_regression.py"),
                cwd=WORKSPACE_ROOT,
            )
        )

        if local_profile or include_hardware_smoke:
            cases.append(
                ValidationCase(
                    name="hardware-smoke",
                    layer="e2e",
                    description="hardware smoke",
                    command=python_command(WORKSPACE_ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_hardware_smoke.py"),
                    cwd=WORKSPACE_ROOT,
                    known_failure_exit_codes=(KNOWN_FAILURE_EXIT_CODE,),
                    skipped_exit_codes=(SKIPPED_EXIT_CODE,),
                )
            )

        if include_hil_closed_loop:
            cases.append(
                ValidationCase(
                    name="hil-closed-loop",
                    layer="e2e",
                    description="HIL 闭环动作与长稳探针",
                    command=python_command(WORKSPACE_ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_hil_closed_loop.py"),
                    cwd=WORKSPACE_ROOT,
                    known_failure_exit_codes=(KNOWN_FAILURE_EXIT_CODE,),
                    skipped_exit_codes=(SKIPPED_EXIT_CODE,),
                )
            )

    if "protocol-compatibility" in suites:
        cases.append(
            ValidationCase(
                name="protocol-compatibility",
                layer="protocol-compatibility",
                description="application/engineering protocol compatibility",
                command=python_command(
                    WORKSPACE_ROOT / "tests" / "integration" / "protocol-compatibility" / "run_protocol_compatibility.py"
                ),
                cwd=WORKSPACE_ROOT,
            )
        )

    if "performance" in suites:
        cases.append(
            ValidationCase(
                name="performance-baseline-collection",
                layer="performance",
                description="collect performance baselines",
                command=python_command(WORKSPACE_ROOT / "tests" / "performance" / "collect_baselines.py"),
                cwd=WORKSPACE_ROOT,
            )
        )

    return cases


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run workspace validation suites and write a report.")
    parser.add_argument("--report-dir", default=str(_default_report_dir()))
    parser.add_argument("--profile", choices=("local", "ci"), default="local")
    parser.add_argument(
        "--suite",
        action="append",
        choices=("all", "apps", "contracts", "e2e", "protocol-compatibility", "performance"),
        help="One or more suites to run. Default is all.",
    )
    parser.add_argument("--include-hardware-smoke", action="store_true")
    parser.add_argument("--include-hil-closed-loop", action="store_true")
    parser.add_argument("--fail-on-known-failure", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir).resolve()
    report = ValidationReport(
        generated_at=datetime.now(timezone.utc).isoformat(),
        workspace_root=str(WORKSPACE_ROOT),
        metadata=_workspace_validation_metadata(),
    )

    suites = _normalize_suites(args.suite)
    for case in build_cases(
        args.profile,
        suites,
        include_hardware_smoke=bool(args.include_hardware_smoke),
        include_hil_closed_loop=bool(args.include_hil_closed_loop),
        report_dir=report_dir,
    ):
        report.results.append(run_case(case))

    json_path = report_dir / "workspace-validation.json"
    md_path = report_dir / "workspace-validation.md"
    report_to_json(report, json_path)
    report_to_markdown(report, md_path)

    counts = report.counts()
    print(
        "workspace validation complete: "
        f"profile={args.profile} "
        f"suites={','.join(suites)} "
        f"passed={counts.get('passed', 0)} "
        f"failed={counts.get('failed', 0)} "
        f"known_failure={counts.get('known_failure', 0)} "
        f"skipped={counts.get('skipped', 0)}"
    )
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")
    return report.exit_code(fail_on_known_failure=bool(args.fail_on_known_failure))


if __name__ == "__main__":
    raise SystemExit(main())
