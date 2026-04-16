from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATHS = (
    ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_hil_closed_loop.py",
    ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_case_matrix.py",
    ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_hardware_smoke.py",
    ROOT / "tests" / "integration" / "scenarios" / "first-layer" / "run_tcp_estop_chain.py",
    ROOT / "tests" / "integration" / "scenarios" / "first-layer" / "run_tcp_jog_focus_smoke.py",
    ROOT / "tests" / "integration" / "scenarios" / "first-layer" / "run_tcp_manual_motion_matrix.py",
    ROOT / "tests" / "integration" / "scenarios" / "first-layer" / "run_tcp_precondition_matrix.py",
)


def _write_matching_cmake_cache(build_root: Path) -> Path:
    cache_path = build_root / "CMakeCache.txt"
    cache_path.write_text(
        f"CMAKE_HOME_DIRECTORY:INTERNAL={ROOT}\n",
        encoding="utf-8",
    )
    return cache_path


def _probe_resolved_executable(script_path: Path, fake_file_name: str, localappdata_root: Path) -> Path:
    probe_code = "\n".join(
        (
            "import importlib.util",
            "import sys",
            "from pathlib import Path",
            f"script_path = r'{script_path.as_posix()}'",
            "sys.path.insert(0, str(Path(script_path).parent))",
            "spec = importlib.util.spec_from_file_location('codex_probe_module', script_path)",
            "module = importlib.util.module_from_spec(spec)",
            "assert spec is not None and spec.loader is not None",
            "sys.modules[spec.name] = module",
            "spec.loader.exec_module(module)",
            f"print(module.resolve_default_exe('{fake_file_name}'))",
        )
    )
    env = {**os.environ, "LOCALAPPDATA": str(localappdata_root)}
    env.pop("SILIGEN_CONTROL_APPS_BUILD_ROOT", None)
    completed = subprocess.run(
        [sys.executable, "-c", probe_code],
        cwd=str(ROOT),
        env=env,
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
    assert completed.returncode == 0, completed.stderr
    return Path(completed.stdout.strip())


def test_gateway_resolution_consumers_prefer_workspace_build_and_ignore_localappdata(
    tmp_path: Path,
) -> None:
    fake_file_name = "codex_gateway_resolution_consumer_probe.exe"
    workspace_build_exe = ROOT / "build" / "bin" / "Debug" / fake_file_name
    workspace_hmi_fix_exe = ROOT / "build" / "hmi-home-fix" / "bin" / "Debug" / fake_file_name
    legacy_localappdata = tmp_path / "localappdata"
    legacy_exe = legacy_localappdata / "SiligenSuite" / "control-apps-build" / "bin" / "Debug" / fake_file_name

    for path in (workspace_build_exe, workspace_hmi_fix_exe, legacy_exe):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("", encoding="utf-8")
    workspace_build_cache = _write_matching_cmake_cache(ROOT / "build")
    legacy_cache = _write_matching_cmake_cache(legacy_localappdata / "SiligenSuite" / "control-apps-build")

    try:
        for script_path in SCRIPT_PATHS:
            resolved = _probe_resolved_executable(script_path, fake_file_name, legacy_localappdata)
            assert resolved == workspace_build_exe, f"{script_path.name} resolved {resolved}"
    finally:
        for path in (workspace_build_exe, workspace_hmi_fix_exe):
            if path.exists():
                path.unlink()
        for cache_path in (workspace_build_cache, legacy_cache):
            if cache_path.exists():
                cache_path.unlink()
