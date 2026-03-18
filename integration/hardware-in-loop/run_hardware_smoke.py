from __future__ import annotations

import os
import socket
import subprocess
import sys
import time
from pathlib import Path


KNOWN_FAILURE_EXIT_CODE = 10
SKIPPED_EXIT_CODE = 11
ROOT = Path(__file__).resolve().parents[2]
CONTROL_APPS_BUILD_ROOT = Path(
    os.getenv(
        "SILIGEN_CONTROL_APPS_BUILD_ROOT",
        str(Path(os.getenv("LOCALAPPDATA", str(ROOT))) / "SiligenSuite" / "control-apps-build"),
    )
)


def _resolve_default_exe() -> Path:
    candidates = (
        CONTROL_APPS_BUILD_ROOT / "bin" / "siligen_tcp_server.exe",
        CONTROL_APPS_BUILD_ROOT / "bin" / "Debug" / "siligen_tcp_server.exe",
        CONTROL_APPS_BUILD_ROOT / "bin" / "Release" / "siligen_tcp_server.exe",
        CONTROL_APPS_BUILD_ROOT / "bin" / "RelWithDebInfo" / "siligen_tcp_server.exe",
    )
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


DEFAULT_EXE = _resolve_default_exe()
HOST = os.getenv("SILIGEN_HIL_HOST", "127.0.0.1")
PORT = int(os.getenv("SILIGEN_HIL_PORT", "9527"))


def _read_process_output(process: subprocess.Popen[str]) -> tuple[str, str]:
    try:
        stdout, stderr = process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate(timeout=5)
    return stdout or "", stderr or ""


def main() -> int:
    exe_path = Path(os.getenv("SILIGEN_HIL_GATEWAY_EXE", str(DEFAULT_EXE)))
    if not exe_path.exists():
        print(f"HIL gateway executable missing: {exe_path}", file=sys.stderr)
        return KNOWN_FAILURE_EXIT_CODE

    process = subprocess.Popen(
        [str(exe_path)],
        cwd=str(ROOT),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )

    try:
        deadline = time.time() + 8.0
        while time.time() < deadline:
            if process.poll() is not None:
                stdout, stderr = _read_process_output(process)
                combined = "\n".join(item for item in (stdout, stderr) if item)
                if "IDiagnosticsPort 未注册" in combined:
                    print(combined)
                    return KNOWN_FAILURE_EXIT_CODE
                print(combined or "hardware smoke failed before TCP port became ready", file=sys.stderr)
                return 1

            try:
                with socket.create_connection((HOST, PORT), timeout=1.0):
                    print("hardware smoke passed: TCP endpoint is reachable")
                    return 0
            except OSError:
                time.sleep(0.2)

        print(f"hardware smoke timeout: {HOST}:{PORT} not reachable", file=sys.stderr)
        return 1
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())
