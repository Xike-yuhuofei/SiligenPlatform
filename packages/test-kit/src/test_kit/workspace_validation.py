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


WORKSPACE_ROOT = Path(__file__).resolve().parents[4]
DEFAULT_SUITES = ("apps", "packages", "integration", "protocol-compatibility", "simulation")
CONTROL_APPS_BUILD_ROOT = Path(
    os.getenv(
        "SILIGEN_CONTROL_APPS_BUILD_ROOT",
        str(Path(os.getenv("LOCALAPPDATA", str(WORKSPACE_ROOT))) / "SiligenSuite" / "control-apps-build"),
    )
)


def _default_report_dir() -> Path:
    return WORKSPACE_ROOT / "integration" / "reports"


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


def _normalize_suites(requested: list[str] | None) -> list[str]:
    if not requested or "all" in requested:
        return list(DEFAULT_SUITES)

    normalized: list[str] = []
    for suite in requested:
        if suite not in normalized:
            normalized.append(suite)
    return normalized


def build_cases(profile: str, suites: list[str], *, include_hardware_smoke: bool = False) -> list[ValidationCase]:
    cases: list[ValidationCase] = []
    local_profile = profile == "local"

    if "apps" in suites:
        cases.extend(
            [
                ValidationCase(
                    name="control-runtime-dryrun",
                    layer="apps",
                    description="control-runtime canonical 入口 dry-run",
                    command=_powershell_file_command(
                        WORKSPACE_ROOT / "apps" / "control-runtime" / "run.ps1",
                        "-DryRun",
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="control-tcp-server-dryrun",
                    layer="apps",
                    description="control-tcp-server canonical 入口 dry-run",
                    command=_powershell_file_command(
                        WORKSPACE_ROOT / "apps" / "control-tcp-server" / "run.ps1",
                        "-DryRun",
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="control-cli-dryrun",
                    layer="apps",
                    description="control-cli canonical 入口 dry-run",
                    command=_powershell_file_command(
                        WORKSPACE_ROOT / "apps" / "control-cli" / "run.ps1",
                        "-DryRun",
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="hmi-app-unit",
                    layer="apps",
                    description="hmi-app 单元测试",
                    command=_powershell_file_command(
                        WORKSPACE_ROOT / "apps" / "hmi-app" / "scripts" / "test.ps1",
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
            ]
        )

    if "packages" in suites:
        cases.extend(
            [
                ValidationCase(
                    name="application-contracts",
                    layer="packages",
                    description="application-contracts 兼容性测试",
                    command=python_command(
                        WORKSPACE_ROOT / "packages" / "application-contracts" / "tests" / "test_protocol_compatibility.py"
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="engineering-contracts",
                    layer="packages",
                    description="engineering-contracts 兼容性测试",
                    command=python_command(
                        WORKSPACE_ROOT / "packages" / "engineering-contracts" / "tests" / "test_engineering_contracts.py"
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="engineering-data",
                    layer="packages",
                    description="engineering-data 兼容性测试",
                    command=python_command(
                        WORKSPACE_ROOT / "packages" / "engineering-data" / "tests" / "test_engineering_data_compatibility.py"
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="transport-gateway",
                    layer="packages",
                    description="transport-gateway 兼容性测试",
                    command=python_command(
                        WORKSPACE_ROOT / "packages" / "transport-gateway" / "tests" / "test_transport_gateway_compatibility.py"
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="device-shared-boundary",
                    layer="packages",
                    description="device/shared package 负向依赖校验",
                    command=python_command(WORKSPACE_ROOT / "tools" / "migration" / "validate_device_split.py"),
                    cwd=WORKSPACE_ROOT,
                ),
            ]
        )

        if local_profile:
            cases.extend(
                [
                    ValidationCase(
                        name="process-runtime-core-unit",
                        layer="packages",
                        description="process-runtime-core 主单测集（经 canonical control-apps build root 构建产物暴露）",
                        command=[str(_control_apps_executable("siligen_unit_tests.exe"))],
                        cwd=WORKSPACE_ROOT,
                        allow_missing=True,
                    ),
                    ValidationCase(
                        name="process-runtime-core-pr1",
                        layer="packages",
                        description="process-runtime-core PR1 冒烟单测集（经 canonical control-apps build root 构建产物暴露）",
                        command=[str(_control_apps_executable("siligen_pr1_tests.exe"))],
                        cwd=WORKSPACE_ROOT,
                        allow_missing=True,
                    ),
                ]
            )

    if "integration" in suites:
        cases.append(
            ValidationCase(
                name="engineering-regression",
                layer="integration",
                description="engineering-data -> engineering-contracts 场景回归",
                command=python_command(WORKSPACE_ROOT / "integration" / "scenarios" / "run_engineering_regression.py"),
                cwd=WORKSPACE_ROOT,
            )
        )

        if local_profile or include_hardware_smoke:
            cases.append(
                ValidationCase(
                    name="hardware-smoke",
                    layer="integration",
                    description="control-tcp-server 最小硬件冒烟入口",
                    command=python_command(WORKSPACE_ROOT / "integration" / "hardware-in-loop" / "run_hardware_smoke.py"),
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
                description="application/engineering/transport 契约兼容性",
                command=python_command(WORKSPACE_ROOT / "integration" / "protocol-compatibility" / "run_protocol_compatibility.py"),
                cwd=WORKSPACE_ROOT,
            )
        )

    if "simulation" in suites:
        allow_missing = local_profile
        cases.extend(
            [
                ValidationCase(
                    name="simulation-engine-smoke",
                    layer="simulation",
                    description="simulation-engine smoke test",
                    command=[str(WORKSPACE_ROOT / "packages" / "simulation-engine" / "build" / "Debug" / "simulation_engine_smoke_test.exe")],
                    cwd=WORKSPACE_ROOT,
                    allow_missing=allow_missing,
                ),
                ValidationCase(
                    name="simulation-engine-json-io",
                    layer="simulation",
                    description="simulation-engine json io test",
                    command=[str(WORKSPACE_ROOT / "packages" / "simulation-engine" / "build" / "Debug" / "simulation_engine_json_io_test.exe")],
                    cwd=WORKSPACE_ROOT,
                    allow_missing=allow_missing,
                ),
                ValidationCase(
                    name="simulated-line",
                    layer="simulation",
                    description="simulation-engine 对 canonical fixture 的仿真回归",
                    command=python_command(WORKSPACE_ROOT / "integration" / "simulated-line" / "run_simulated_line.py"),
                    cwd=WORKSPACE_ROOT,
                    known_failure_exit_codes=(KNOWN_FAILURE_EXIT_CODE,),
                    skipped_exit_codes=(SKIPPED_EXIT_CODE,),
                ),
            ]
        )

    return cases


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run workspace validation suites and write a report.")
    parser.add_argument("--report-dir", default=str(_default_report_dir()))
    parser.add_argument("--profile", choices=("local", "ci"), default="local")
    parser.add_argument(
        "--suite",
        action="append",
        choices=("all", *DEFAULT_SUITES),
        help="One or more suites to run. Default is all.",
    )
    parser.add_argument("--include-hardware-smoke", action="store_true")
    parser.add_argument("--fail-on-known-failure", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir)
    report = ValidationReport(
        generated_at=datetime.now(timezone.utc).isoformat(),
        workspace_root=str(WORKSPACE_ROOT),
    )

    suites = _normalize_suites(args.suite)
    for case in build_cases(
        args.profile,
        suites,
        include_hardware_smoke=bool(args.include_hardware_smoke),
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
