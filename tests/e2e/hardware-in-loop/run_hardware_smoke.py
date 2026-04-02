from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


KNOWN_FAILURE_EXIT_CODE = 10
SKIPPED_EXIT_CODE = 11
ROOT = Path(__file__).resolve().parents[3]
CONTROL_APPS_BUILD_ROOT = Path(
    os.getenv(
        "SILIGEN_CONTROL_APPS_BUILD_ROOT",
        str(Path(os.getenv("LOCALAPPDATA", str(ROOT))) / "SiligenSuite" / "control-apps-build"),
    )
)


def _resolve_default_exe() -> Path:
    candidates = (
        CONTROL_APPS_BUILD_ROOT / "bin" / "siligen_runtime_gateway.exe",
        CONTROL_APPS_BUILD_ROOT / "bin" / "Debug" / "siligen_runtime_gateway.exe",
        CONTROL_APPS_BUILD_ROOT / "bin" / "Release" / "siligen_runtime_gateway.exe",
        CONTROL_APPS_BUILD_ROOT / "bin" / "RelWithDebInfo" / "siligen_runtime_gateway.exe",
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


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run hardware smoke and optionally write structured report artifacts.")
    parser.add_argument("--report-dir", default="")
    parser.add_argument("--host", default=os.getenv("SILIGEN_HIL_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.getenv("SILIGEN_HIL_PORT", "9527")))
    parser.add_argument("--gateway-exe", default=os.getenv("SILIGEN_HIL_GATEWAY_EXE", str(DEFAULT_EXE)))
    parser.add_argument("--allow-skip-on-missing-gateway", action="store_true")
    return parser.parse_args()


def _read_process_output(process: subprocess.Popen[str]) -> tuple[str, str]:
    try:
        stdout, stderr = process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate(timeout=5)
    return stdout or "", stderr or ""


def _write_report(report_dir: Path, payload: dict[str, object]) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "hardware-smoke-summary.json"
    md_path = report_dir / "hardware-smoke-summary.md"
    json_path.write_text(json.dumps(payload, ensure_ascii=True, indent=2), encoding="utf-8")

    lines = [
        "# Hardware Smoke Summary",
        "",
        f"- generated_at: `{payload['generated_at']}`",
        f"- overall_status: `{payload['overall_status']}`",
        f"- host: `{payload['host']}`",
        f"- port: `{payload['port']}`",
        f"- gateway_exe: `{payload['gateway_exe']}`",
        f"- note: `{payload.get('note', '')}`",
        "",
    ]
    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def _emit_report(args: argparse.Namespace, *, status: str, note: str, stdout: str = "", stderr: str = "") -> None:
    if not args.report_dir:
        return
    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "overall_status": status,
        "host": args.host,
        "port": args.port,
        "gateway_exe": str(Path(args.gateway_exe)),
        "note": note,
        "stdout": stdout,
        "stderr": stderr,
    }
    json_path, md_path = _write_report(Path(args.report_dir), payload)
    print(f"hardware smoke json report: {json_path}")
    print(f"hardware smoke markdown report: {md_path}")


def main() -> int:
    args = parse_args()
    exe_path = Path(args.gateway_exe)
    if not exe_path.exists():
        status = "skipped" if args.allow_skip_on_missing_gateway else "known_failure"
        note = f"gateway executable missing: {exe_path}"
        print(f"HIL gateway executable missing: {exe_path}", file=sys.stderr)
        _emit_report(args, status=status, note=note)
        return SKIPPED_EXIT_CODE if status == "skipped" else KNOWN_FAILURE_EXIT_CODE

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
                    _emit_report(
                        args,
                        status="known_failure",
                        note="matched known failure pattern: IDiagnosticsPort 未注册",
                        stdout=stdout,
                        stderr=stderr,
                    )
                    return KNOWN_FAILURE_EXIT_CODE
                print(combined or "hardware smoke failed before TCP port became ready", file=sys.stderr)
                _emit_report(
                    args,
                    status="failed",
                    note="hardware smoke failed before TCP port became ready",
                    stdout=stdout,
                    stderr=stderr,
                )
                return 1

            try:
                with socket.create_connection((args.host, args.port), timeout=1.0):
                    print("hardware smoke passed: TCP endpoint is reachable")
                    _emit_report(
                        args,
                        status="passed",
                        note="TCP endpoint is reachable",
                    )
                    return 0
            except OSError:
                time.sleep(0.2)

        timeout_note = f"hardware smoke timeout: {args.host}:{args.port} not reachable"
        print(timeout_note, file=sys.stderr)
        _emit_report(args, status="failed", note=timeout_note)
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
