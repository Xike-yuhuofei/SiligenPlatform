from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def _write_matching_cmake_cache(build_root: Path, workspace_root: Path) -> Path:
    cache_path = build_root / "CMakeCache.txt"
    cache_path.write_text(
        f"CMAKE_HOME_DIRECTORY:INTERNAL={workspace_root}\n",
        encoding="utf-8",
    )
    return cache_path


def test_resolve_default_exe_ignores_localappdata_and_falls_back_to_workspace_build_root(tmp_path: Path) -> None:
    fake_file_name = "codex_runtime_gateway_resolution_probe.exe"
    workspace_build_root = ROOT / "build"
    workspace_exe = workspace_build_root / "bin" / "Debug" / fake_file_name
    legacy_localappdata = tmp_path / "localappdata"
    legacy_exe = legacy_localappdata / "SiligenSuite" / "control-apps-build" / "bin" / "Debug" / fake_file_name

    workspace_exe.parent.mkdir(parents=True, exist_ok=True)
    workspace_exe.write_text("", encoding="utf-8")
    workspace_cache = _write_matching_cmake_cache(workspace_build_root, ROOT)
    legacy_exe.parent.mkdir(parents=True, exist_ok=True)
    legacy_exe.write_text("", encoding="utf-8")
    legacy_cache = _write_matching_cmake_cache(legacy_localappdata / "SiligenSuite" / "control-apps-build", ROOT)

    probe_code = "\n".join(
        (
            "import sys",
            "from pathlib import Path",
            f"root = Path(r'{ROOT.as_posix()}')",
            "sys.path.insert(0, str(root / 'shared' / 'testing' / 'test-kit' / 'src'))",
            "sys.path.insert(0, str(root / 'tests' / 'e2e' / 'hardware-in-loop'))",
            "from runtime_gateway_harness import resolve_default_exe",
            f"print(resolve_default_exe('{fake_file_name}'))",
        )
    )

    env = {**os.environ, "LOCALAPPDATA": str(legacy_localappdata)}
    env.pop("SILIGEN_CONTROL_APPS_BUILD_ROOT", None)

    try:
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
        assert completed.stdout.strip() == str(workspace_exe)
    finally:
        if workspace_exe.exists():
            workspace_exe.unlink()
        for cache_path in (workspace_cache, legacy_cache):
            if cache_path.exists():
                cache_path.unlink()


def test_resolve_default_exe_prefers_workspace_build_and_ignores_localappdata(tmp_path: Path) -> None:
    fake_file_name = "codex_runtime_gateway_resolution_probe.exe"
    workspace_build_exe = ROOT / "build" / "bin" / "Debug" / fake_file_name
    legacy_localappdata = tmp_path / "localappdata"
    legacy_exe = legacy_localappdata / "SiligenSuite" / "control-apps-build" / "bin" / "Debug" / fake_file_name

    workspace_build_exe.parent.mkdir(parents=True, exist_ok=True)
    workspace_build_exe.write_text("", encoding="utf-8")
    legacy_exe.parent.mkdir(parents=True, exist_ok=True)
    legacy_exe.write_text("", encoding="utf-8")
    workspace_build_cache = _write_matching_cmake_cache(ROOT / "build", ROOT)
    legacy_cache = _write_matching_cmake_cache(legacy_localappdata / "SiligenSuite" / "control-apps-build", ROOT)

    probe_code = "\n".join(
        (
            "import sys",
            "from pathlib import Path",
            f"root = Path(r'{ROOT.as_posix()}')",
            "sys.path.insert(0, str(root / 'shared' / 'testing' / 'test-kit' / 'src'))",
            "sys.path.insert(0, str(root / 'tests' / 'e2e' / 'hardware-in-loop'))",
            "from runtime_gateway_harness import resolve_default_exe",
            f"print(resolve_default_exe('{fake_file_name}'))",
        )
    )

    env = {**os.environ, "LOCALAPPDATA": str(legacy_localappdata)}
    env.pop("SILIGEN_CONTROL_APPS_BUILD_ROOT", None)

    try:
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
        assert completed.stdout.strip() == str(workspace_build_exe)
    finally:
        if workspace_build_exe.exists():
            workspace_build_exe.unlink()
        for cache_path in (workspace_build_cache, legacy_cache):
            if cache_path.exists():
                cache_path.unlink()
