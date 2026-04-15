from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_resolve_default_exe_prefers_legacy_localappdata_over_noncanonical_workspace_build_child(tmp_path: Path) -> None:
    fake_file_name = "codex_runtime_gateway_resolution_probe.exe"
    workspace_build_root = ROOT / "build" / "zz-codex-harness-resolution"
    workspace_exe = workspace_build_root / "bin" / "Debug" / fake_file_name
    legacy_localappdata = tmp_path / "localappdata"
    legacy_exe = legacy_localappdata / "SiligenSuite" / "control-apps-build" / "bin" / "Debug" / fake_file_name

    workspace_exe.parent.mkdir(parents=True, exist_ok=True)
    workspace_exe.write_text("", encoding="utf-8")
    legacy_exe.parent.mkdir(parents=True, exist_ok=True)
    legacy_exe.write_text("", encoding="utf-8")

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
        assert completed.stdout.strip() == str(legacy_exe)
    finally:
        shutil.rmtree(workspace_build_root, ignore_errors=True)


def test_resolve_default_exe_prefers_workspace_build_ca_over_workspace_build_and_legacy_localappdata(tmp_path: Path) -> None:
    fake_file_name = "codex_runtime_gateway_resolution_probe_ca.exe"
    workspace_ca_exe = ROOT / "build" / "ca" / "bin" / "Debug" / fake_file_name
    workspace_build_exe = ROOT / "build" / "bin" / "Debug" / fake_file_name
    legacy_localappdata = tmp_path / "localappdata"
    legacy_exe = legacy_localappdata / "SiligenSuite" / "control-apps-build" / "bin" / "Debug" / fake_file_name

    workspace_ca_exe.parent.mkdir(parents=True, exist_ok=True)
    workspace_ca_exe.write_text("", encoding="utf-8")
    workspace_build_exe.parent.mkdir(parents=True, exist_ok=True)
    workspace_build_exe.write_text("", encoding="utf-8")
    legacy_exe.parent.mkdir(parents=True, exist_ok=True)
    legacy_exe.write_text("", encoding="utf-8")

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
        assert completed.stdout.strip() == str(workspace_ca_exe)
    finally:
        if workspace_ca_exe.exists():
            workspace_ca_exe.unlink()
        if workspace_build_exe.exists():
            workspace_build_exe.unlink()
