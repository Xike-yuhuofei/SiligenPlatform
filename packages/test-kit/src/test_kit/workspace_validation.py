from __future__ import annotations

import argparse
import os
import shutil
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


WORKSPACE_ROOT = Path(__file__).resolve().parents[4]
DEFAULT_SUITES = ("apps", "packages", "integration", "protocol-compatibility", "simulation")
WORKSPACE_LAYOUT = load_workspace_layout(WORKSPACE_ROOT)
CONTROL_APPS_BUILD_ROOT = Path(
    os.getenv(
        "SILIGEN_CONTROL_APPS_BUILD_ROOT",
        str(Path(os.getenv("LOCALAPPDATA", str(WORKSPACE_ROOT))) / "SiligenSuite" / "control-apps-build"),
    )
)
SIMULATION_ENGINE_BUILD_ROOT = Path(
    os.getenv(
        "SILIGEN_SIMULATION_ENGINE_BUILD_ROOT",
        str(WORKSPACE_ROOT / "build" / "simulation-engine"),
    )
)


def _default_report_dir() -> Path:
    return WORKSPACE_ROOT / "integration" / "reports"


def _layout_absolute_path(key: str) -> Path:
    return WORKSPACE_LAYOUT[key].absolute


def _control_apps_cmake_cache_file() -> Path:
    return CONTROL_APPS_BUILD_ROOT / "CMakeCache.txt"


def _read_cmake_cache_home_directory(cache_file: Path) -> str:
    if not cache_file.exists():
        return ""

    for raw_line in cache_file.read_text(encoding="utf-8", errors="ignore").splitlines():
        if raw_line.startswith("CMAKE_HOME_DIRECTORY:"):
            _, _, value = raw_line.partition("=")
            return value.strip()
    return ""


def _workspace_validation_metadata() -> dict[str, str]:
    control_apps_cache = _control_apps_cmake_cache_file()
    return {
        "workspace_layout_file": str((WORKSPACE_ROOT / "cmake" / "workspace-layout.env").resolve()),
        "workspace_root": str(WORKSPACE_ROOT.resolve()),
        "control_apps_build_root": str(CONTROL_APPS_BUILD_ROOT.resolve()),
        "control_apps_cmake_cache_file": str(control_apps_cache.resolve()),
        "control_apps_cmake_home_directory": _read_cmake_cache_home_directory(control_apps_cache),
    }


def _archive_nested_hil_reports(hil_report_dir: Path) -> None:
    nested_dir = hil_report_dir / "hil-controlled-test"
    if not nested_dir.exists() or not nested_dir.is_dir():
        return

    archive_root = hil_report_dir / "_legacy-nested"
    archive_root.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    archive_dir = archive_root / f"hil-controlled-test-{stamp}"
    suffix = 1
    while archive_dir.exists():
        archive_dir = archive_root / f"hil-controlled-test-{stamp}-{suffix}"
        suffix += 1
    shutil.move(str(nested_dir), str(archive_dir))


def _resolve_hil_report_dir(base_report_dir: Path) -> Path:
    resolved_report_dir = base_report_dir.resolve()
    hil_report_dir = (
        resolved_report_dir
        if resolved_report_dir.name == "hil-controlled-test"
        else resolved_report_dir / "hil-controlled-test"
    )
    _archive_nested_hil_reports(hil_report_dir)
    return hil_report_dir


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


def _simulation_engine_build_root() -> Path:
    candidates = (
        SIMULATION_ENGINE_BUILD_ROOT,
        _layout_absolute_path("SILIGEN_SIMULATION_ENGINE_DIR") / "build",
    )
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def _simulation_engine_executable(name: str) -> Path:
    build_root = _simulation_engine_build_root()
    candidates = (
        build_root / "Debug" / name,
        build_root / "Release" / name,
        build_root / "RelWithDebInfo" / name,
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


def _powershell_inline_command(command: str) -> list[str]:
    return [
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-Command",
        command,
    ]


def _normalize_suites(requested: list[str] | None) -> list[str]:
    if not requested or "all" in requested:
        return list(DEFAULT_SUITES)

    normalized: list[str] = []
    for suite in requested:
        if suite not in normalized:
            normalized.append(suite)
    return normalized


def build_cases(
    profile: str,
    suites: list[str],
    *,
    include_hardware_smoke: bool = False,
    include_hil_closed_loop: bool = False,
    report_dir: Path | None = None,
) -> list[ValidationCase]:
    cases: list[ValidationCase] = []
    local_profile = profile == "local"
    machine_config = _layout_absolute_path("SILIGEN_MACHINE_CONFIG_FILE")
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
                    name="control-runtime-version",
                    layer="apps",
                    description="control-runtime 真实 canonical 可执行物版本探测",
                    command=[str(_control_apps_executable("siligen_control_runtime.exe")), "--version"],
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="control-tcp-server-help",
                    layer="apps",
                    description="control-tcp-server 真实 canonical 可执行物帮助输出",
                    command=[str(_control_apps_executable("siligen_tcp_server.exe")), "--help"],
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="control-cli-bootstrap-check",
                    layer="apps",
                    description="control-cli 最小真实命令面 bootstrap-check",
                    command=[
                        str(_control_apps_executable("siligen_cli.exe")),
                        "bootstrap-check",
                        "--config",
                        str(machine_config),
                    ],
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="control-cli-help",
                    layer="apps",
                    description="control-cli 完整 canonical 命令面帮助输出",
                    command=[str(_control_apps_executable("siligen_cli.exe")), "--help"],
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="control-cli-recipe-list",
                    layer="apps",
                    description="control-cli canonical recipe list 离线入口",
                    command=[
                        str(_control_apps_executable("siligen_cli.exe")),
                        "recipe",
                        "list",
                        "--config",
                        str(machine_config),
                    ],
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="control-cli-dxf-plan",
                    layer="apps",
                    description="control-cli canonical DXF plan 离线入口",
                    command=[
                        str(_control_apps_executable("siligen_cli.exe")),
                        "dxf-plan",
                        "--file",
                        str(WORKSPACE_ROOT / "examples" / "dxf" / "rect_diag.dxf"),
                        "--config",
                        str(machine_config),
                    ],
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="control-cli-dxf-preview",
                    layer="apps",
                    description="control-cli canonical DXF preview 离线入口",
                    command=[
                        str(_control_apps_executable("siligen_cli.exe")),
                        "dxf-preview",
                        "--file",
                        str(WORKSPACE_ROOT / "examples" / "dxf" / "rect_diag.dxf"),
                        "--preview-max-points",
                        "200",
                        "--json",
                        "--config",
                        str(machine_config),
                    ],
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="hmi-app-dryrun",
                    layer="apps",
                    description="hmi-app canonical 入口 dry-run（仅允许显式 gateway 契约）",
                    command=_powershell_file_command(
                        WORKSPACE_ROOT / "apps" / "hmi-app" / "run.ps1",
                        "-DryRun",
                        "-DisableGatewayAutostart",
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
                ValidationCase(
                    name="hmi-app-offline-smoke",
                    layer="apps",
                    description="hmi-app 离线 GUI 契约 smoke",
                    command=_powershell_file_command(
                        WORKSPACE_ROOT / "apps" / "hmi-app" / "scripts" / "offline-smoke.ps1",
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="hmi-app-online-smoke",
                    layer="apps",
                    description="hmi-app 在线 GUI 契约 smoke",
                    command=_powershell_file_command(
                        WORKSPACE_ROOT / "apps" / "hmi-app" / "scripts" / "online-smoke.ps1",
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="hmi-app-online-ready-timeout",
                    layer="apps",
                    description="hmi-app 在线 ready-timeout 失败注入验证",
                    command=_powershell_file_command(
                        WORKSPACE_ROOT / "apps" / "hmi-app" / "scripts" / "verify-online-ready-timeout.ps1",
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="hmi-app-online-recovery-loop",
                    layer="apps",
                    description="hmi-app 在线恢复链路（supervisor recovery loop）验证",
                    command=_powershell_file_command(
                        WORKSPACE_ROOT / "apps" / "hmi-app" / "scripts" / "verify-online-recovery-loop.ps1",
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
                        name="shared-kernel-unit",
                        layer="packages",
                        description="shared-kernel 基础能力单测（经 canonical control-apps build root 构建产物暴露）",
                        command=[str(_control_apps_executable("siligen_shared_kernel_tests.exe"))],
                        cwd=WORKSPACE_ROOT,
                        allow_missing=True,
                    ),
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

        if include_hil_closed_loop:
            resolved_report_dir = (report_dir if report_dir is not None else _default_report_dir()).resolve()
            hil_report_dir = _resolve_hil_report_dir(resolved_report_dir)
            hil_duration_seconds = int(os.getenv("SILIGEN_HIL_DURATION_SECONDS", "300"))
            hil_pause_resume_cycles = int(os.getenv("SILIGEN_HIL_PAUSE_RESUME_CYCLES", "3"))
            hil_dispenser_count = int(os.getenv("SILIGEN_HIL_DISPENSER_COUNT", "300"))
            hil_dispenser_interval_ms = int(os.getenv("SILIGEN_HIL_DISPENSER_INTERVAL_MS", "200"))
            hil_dispenser_duration_ms = int(os.getenv("SILIGEN_HIL_DISPENSER_DURATION_MS", "80"))
            hil_state_wait_timeout_seconds = float(os.getenv("SILIGEN_HIL_STATE_WAIT_TIMEOUT_SECONDS", "8"))
            hil_allow_skip = os.getenv("SILIGEN_HIL_ALLOW_SKIP_ON_MISSING", "").strip().lower() in ("1", "true", "yes")
            cases.append(
                ValidationCase(
                    name="hil-closed-loop",
                    layer="sim_hil",
                    description="HIL 闭环动作与长稳探针（TCP 驱动）",
                    command=[
                        *python_command(WORKSPACE_ROOT / "integration" / "hardware-in-loop" / "run_hil_closed_loop.py"),
                        "--report-dir",
                        str(hil_report_dir),
                        "--duration-seconds",
                        str(hil_duration_seconds),
                        "--pause-resume-cycles",
                        str(hil_pause_resume_cycles),
                        "--dispenser-count",
                        str(hil_dispenser_count),
                        "--dispenser-interval-ms",
                        str(hil_dispenser_interval_ms),
                        "--dispenser-duration-ms",
                        str(hil_dispenser_duration_ms),
                        "--state-wait-timeout-seconds",
                        str(hil_state_wait_timeout_seconds),
                        *(["--allow-skip-on-missing-gateway", "--allow-skip-on-missing-cli"] if hil_allow_skip else []),
                    ],
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
        resolved_report_dir = report_dir if report_dir is not None else _default_report_dir()
        simulated_line_report_dir = resolved_report_dir / "simulation" / "simulated-line"
        simulation_build_root = _simulation_engine_build_root()
        simulation_bin_root = simulation_build_root / "Debug"
        simulation_ruckig_root = simulation_build_root / "third_party" / "ruckig" / "Debug"
        scheme_c_regex = (
            "simulation_engine_scheme_c_"
            "(baseline|runtime_bridge_shim|runtime_bridge_core_integration|recording_export|"
            "application_runner|virtual_devices|virtual_time)_test"
        )
        runtime_contract_regex = (
            "process_runtime_core_"
            "(deterministic_path_execution|motion_runtime_assembly|pb_path_source_adapter)_test"
        )
        scheme_c_validation_command = (
            f"$buildRoot = '{simulation_build_root}'; "
            f"if (-not (Test-Path $buildRoot)) {{ Write-Error \"simulation-engine build missing: $buildRoot\"; exit {KNOWN_FAILURE_EXIT_CODE}; }} "
            f"$env:PATH = '{simulation_bin_root};{simulation_ruckig_root};' + $env:PATH; "
            f"ctest --test-dir $buildRoot -C Debug --output-on-failure -R '{scheme_c_regex}'; "
            "exit $LASTEXITCODE"
        )
        runtime_contract_validation_command = (
            f"$buildRoot = '{simulation_build_root}'; "
            f"if (-not (Test-Path $buildRoot)) {{ Write-Error \"simulation-engine build missing: $buildRoot\"; exit {KNOWN_FAILURE_EXIT_CODE}; }} "
            f"$env:PATH = '{simulation_bin_root};{simulation_ruckig_root};' + $env:PATH; "
            f"ctest --test-dir $buildRoot -C Debug --output-on-failure -R '{runtime_contract_regex}'; "
            "exit $LASTEXITCODE"
        )
        cases.extend(
            [
                ValidationCase(
                    name="simulation-engine-smoke",
                    layer="simulation",
                    description="simulation-engine smoke test",
                    command=[str(_simulation_engine_executable("simulation_engine_smoke_test.exe"))],
                    cwd=WORKSPACE_ROOT,
                    allow_missing=allow_missing,
                ),
                ValidationCase(
                    name="simulation-engine-json-io",
                    layer="simulation",
                    description="simulation-engine json io test",
                    command=[str(_simulation_engine_executable("simulation_engine_json_io_test.exe"))],
                    cwd=WORKSPACE_ROOT,
                    allow_missing=allow_missing,
                ),
                ValidationCase(
                    name="simulation-engine-scheme-c",
                    layer="simulation",
                    description="simulation-engine scheme C package tests and bridge regression",
                    command=_powershell_inline_command(scheme_c_validation_command),
                    cwd=WORKSPACE_ROOT,
                    known_failure_exit_codes=(KNOWN_FAILURE_EXIT_CODE,),
                ),
                ValidationCase(
                    name="simulation-runtime-core-contracts",
                    layer="simulation",
                    description="process-runtime-core 在 simulation standalone 构建下的执行与降级合同测试",
                    command=_powershell_inline_command(runtime_contract_validation_command),
                    cwd=WORKSPACE_ROOT,
                    known_failure_exit_codes=(KNOWN_FAILURE_EXIT_CODE,),
                ),
                ValidationCase(
                    name="simulated-line",
                    layer="simulation",
                    description="simulation-engine compat + scheme C 对 canonical fixture 的仿真回归",
                    command=[
                        *python_command(WORKSPACE_ROOT / "integration" / "simulated-line" / "run_simulated_line.py"),
                        "--report-dir",
                        str(simulated_line_report_dir),
                    ],
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
