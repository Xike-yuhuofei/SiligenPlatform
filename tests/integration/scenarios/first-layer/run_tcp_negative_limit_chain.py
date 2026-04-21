from __future__ import annotations

import argparse
import json
import socket
import subprocess
import sys
import time
from dataclasses import asdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

THIS_DIR = Path(__file__).resolve().parent
ROOT = THIS_DIR.parents[3]
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
if str(HIL_DIR) not in sys.path:
    sys.path.insert(0, str(HIL_DIR))

from runtime_gateway_harness import (
    KNOWN_FAILURE_EXIT_CODE,
    SKIPPED_EXIT_CODE,
    CheckResult,
    ROOT,
    StepRecord,
    TcpJsonClient,
    all_axes_homed_consistently,
    build_process_env,
    compute_overall_status,
    disconnect_ack,
    extract_axes,
    extract_effective_interlocks,
    extract_io,
    load_connection_params,
    extract_position,
    home_succeeded,
    matches_known_failure,
    prepare_mock_config,
    read_process_output,
    resolve_default_exe,
    truncate_json,
    wait_gateway_ready,
)


def _write_report(report_dir: Path, report: dict[str, Any]) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "tcp-negative-limit-chain.json"
    md_path = report_dir / "tcp-negative-limit-chain.md"
    json_path.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

    lines = [
        "# TCP Negative Limit Chain",
        "",
        f"- generated_at: `{report['generated_at']}`",
        f"- overall_status: `{report['overall_status']}`",
        f"- gateway_exe: `{report['gateway_exe']}`",
        f"- host: `{report['host']}`",
        f"- port: `{report['port']}`",
        "",
        "## Check Results",
        "",
    ]
    for item in report["results"]:
        lines.append(
            f"- `{item['status']}` `{item['check_id']}` expected=`{item['expected']}` actual=`{item['actual']}`"
        )
        if item.get("note"):
            lines.append(f"  note: {item['note']}")
        if item.get("response"):
            lines.append("  response:")
            lines.append("  ```json")
            lines.append(f"  {item['response']}")
            lines.append("  ```")
    lines.extend(("", "## Step Transcript", ""))
    for step in report["steps"]:
        lines.append(f"- `{step['step_id']}` method=`{step['method']}`")
        lines.append("  ```json")
        lines.append(f"  {step['response']}")
        lines.append("  ```")
    lines.append("")
    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def _wait_for_axis_position_change(
    client: TcpJsonClient,
    steps: list[StepRecord],
    step_prefix: str,
    axis_name: str,
    baseline_position: float,
    timeout_seconds: float = 4.0,
) -> tuple[bool, dict[str, Any]]:
    deadline = time.time() + timeout_seconds
    last_status: dict[str, Any] = {}
    while time.time() < deadline:
        last_status = client.send_request("status", {}, timeout_seconds=8.0)
        steps.append(StepRecord(f"{step_prefix}-status", "status", truncate_json(last_status)))
        current = float(extract_axes(last_status).get(axis_name, {}).get("position", 0.0))
        if abs(current - baseline_position) > 0.05:
            return True, last_status
        time.sleep(0.2)
    return False, last_status


def _wait_for_axes_idle(
    client: TcpJsonClient,
    steps: list[StepRecord],
    step_prefix: str,
    timeout_seconds: float = 6.0,
    velocity_tolerance: float = 0.05,
) -> tuple[bool, dict[str, Any]]:
    deadline = time.time() + timeout_seconds
    last_status: dict[str, Any] = {}
    while time.time() < deadline:
        last_status = client.send_request("status", {}, timeout_seconds=8.0)
        steps.append(StepRecord(f"{step_prefix}-status", "status", truncate_json(last_status)))
        axes = extract_axes(last_status)
        if axes and all(abs(float(axis_payload.get("velocity", 0.0))) <= velocity_tolerance for axis_payload in axes.values()):
            return True, last_status
        time.sleep(0.2)
    return False, last_status


def _home_boundary_snapshot(status_response: dict[str, Any], axis_name: str, io_key: str) -> tuple[bool, list[str], str]:
    effective_interlocks = extract_effective_interlocks(status_response)
    io_payload = extract_io(status_response)
    boundary_key = "home_boundary_x_active" if axis_name == "X" else "home_boundary_y_active"
    boundary_active = bool(effective_interlocks.get(boundary_key, False))
    positive_escape_only_axes = [str(item) for item in effective_interlocks.get("positive_escape_only_axes", [])]
    desc = (
        f"effective_interlocks.{boundary_key}={boundary_active}; "
        f"positive_escape_only_axes={positive_escape_only_axes}; "
        f"io.{io_key}={bool(io_payload.get(io_key, False))}"
    )
    return boundary_active, positive_escape_only_axes, desc


def _is_tcp_endpoint_reachable(host: str, port: int, timeout_seconds: float = 0.5) -> bool:
    try:
        with socket.create_connection((host, port), timeout=timeout_seconds):
            return True
    except OSError:
        return False


def _pick_free_port(host: str) -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((host, 0))
        return int(sock.getsockname()[1])


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run TCP negative-limit chain probe and write a report.")
    parser.add_argument(
        "--report-dir",
        default=str(ROOT / "tests" / "reports" / "first-layer" / "s6-negative-limit-chain"),
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument(
        "--gateway-exe",
        default=str(resolve_default_exe("siligen_runtime_gateway.exe", "siligen_tcp_server.exe")),
    )
    parser.add_argument("--allow-skip-on-missing-gateway", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir)
    gateway_exe = Path(args.gateway_exe)
    port = args.port if args.port > 0 else _pick_free_port(args.host)
    results: list[CheckResult] = []
    steps: list[StepRecord] = []

    if not gateway_exe.exists():
        status = "skipped" if args.allow_skip_on_missing_gateway else "known_failure"
        results.append(
            CheckResult("L0-bootstrap", status, "gateway executable exists", str(gateway_exe), "gateway executable missing")
        )
        report = {
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "workspace_root": str(ROOT),
            "gateway_exe": str(gateway_exe),
            "host": args.host,
            "port": port,
            "overall_status": compute_overall_status(results),
            "results": [asdict(item) for item in results],
            "steps": [asdict(item) for item in steps],
        }
        json_path, md_path = _write_report(report_dir, report)
        print(f"tcp negative limit chain complete: status={report['overall_status']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        return SKIPPED_EXIT_CODE if status == "skipped" else KNOWN_FAILURE_EXIT_CODE

    if args.port > 0 and _is_tcp_endpoint_reachable(args.host, port):
        results.append(
            CheckResult(
                "L0-bootstrap",
                "failed",
                "requested TCP port is available before gateway launch",
                f"{args.host}:{port}",
                "requested port already has a reachable listener; refusing to connect to a stale process",
            )
        )
        report = {
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "workspace_root": str(ROOT),
            "gateway_exe": str(gateway_exe),
            "host": args.host,
            "port": port,
            "overall_status": compute_overall_status(results),
            "results": [asdict(item) for item in results],
            "steps": [asdict(item) for item in steps],
        }
        json_path, md_path = _write_report(report_dir, report)
        print(f"tcp negative limit chain complete: status={report['overall_status']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        return 1

    temp_config_dir, temp_config = prepare_mock_config("tcp-negative-limit-")
    process = subprocess.Popen(
        [str(gateway_exe), "--config", str(temp_config), "--port", str(port)],
        cwd=str(ROOT),
        env=build_process_env(gateway_exe),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    client = TcpJsonClient(args.host, port)

    try:
        ready_status, ready_note = wait_gateway_ready(process, args.host, port, timeout_seconds=8.0)
        if ready_status != "passed":
            results.append(CheckResult("L0-bootstrap", ready_status, "gateway ready", ready_note, "gateway readiness check failed"))
            raise RuntimeError("gateway not ready")

        client.connect(timeout_seconds=3.0)

        def call(step_id: str, method: str, params: dict[str, Any] | None = None, timeout_seconds: float = 8.0) -> dict[str, Any]:
            response = client.send_request(method, params, timeout_seconds=timeout_seconds)
            steps.append(StepRecord(step_id, method, truncate_json(response)))
            return response

        connect_response = call(
            "step-connect",
            "connect",
            load_connection_params(temp_config),
            timeout_seconds=10.0,
        )
        if "error" in connect_response:
            results.append(CheckResult("L1-connect", "failed", "connect succeeds", truncate_json(connect_response), response=truncate_json(connect_response)))
            raise RuntimeError("connect failed")
        results.append(CheckResult("L1-connect", "passed", "connect succeeds", "connected=true", response=truncate_json(connect_response)))

        home_ready = False
        home_attempt_details: list[str] = []
        latest_home_status: dict[str, Any] = {}
        for attempt in range(1, 4):
            home_response = call(f"step-home-{attempt}", "home", {}, timeout_seconds=45.0)
            latest_home_status = call(f"step-status-after-home-{attempt}", "status")
            fully_homed, non_homed_axes, inconsistent_axes = all_axes_homed_consistently(latest_home_status)
            home_attempt_details.append(
                f"attempt={attempt} completed={home_succeeded(home_response)} "
                f"non_homed={','.join(non_homed_axes) if non_homed_axes else 'none'} "
                f"inconsistent={','.join(inconsistent_axes) if inconsistent_axes else 'none'}"
            )
            if home_succeeded(home_response) and fully_homed:
                home_ready = True
                break
        results.append(
            CheckResult(
                "L2-home-ready",
                "passed" if home_ready else "failed",
                "home reaches fully homed state before limit probe",
                "all axes homed consistently" if home_ready else "home did not reach fully homed state",
                note="; ".join(home_attempt_details),
                response=truncate_json(latest_home_status),
            )
        )
        if not home_ready:
            raise RuntimeError("home did not reach ready state")

        call("step-stop-after-home", "stop")
        idle_ready, idle_status = _wait_for_axes_idle(client, steps, "step-wait-idle-after-home")
        results.append(
            CheckResult(
                "L2b-home-idle",
                "passed" if idle_ready else "failed",
                "axes settle to idle after home before limit probe",
                truncate_json(idle_status),
                response=truncate_json(idle_status),
            )
        )
        if not idle_ready:
            raise RuntimeError("axes did not settle after home")

        probes = (
            ("X", "limit_x_neg", {"limit_x_neg": True}, {"limit_x_neg": False}, "x"),
            ("Y", "limit_y_neg", {"limit_y_neg": True}, {"limit_y_neg": False}, "y"),
        )

        for axis_name, io_key, set_params, clear_params, move_axis_key in probes:
            case_baseline = call(f"step-status-before-{io_key}", "status")
            current_x, current_y = extract_position(case_baseline)
            baseline_axis_position = float(extract_axes(case_baseline).get(axis_name, {}).get("position", 0.0))

            set_response = call(f"step-set-{io_key}", "mock.io.set", set_params)
            results.append(CheckResult(f"L3-{io_key}-set", "passed" if "error" not in set_response else "failed", f"mock.io.set enables {io_key}", truncate_json(set_response), response=truncate_json(set_response)))
            if "error" in set_response:
                continue

            status_after_set = call(f"step-status-after-set-{io_key}", "status")
            boundary_active, positive_escape_only_axes, boundary_desc = _home_boundary_snapshot(
                status_after_set,
                axis_name,
                io_key,
            )
            results.append(
                CheckResult(
                    f"L4-{io_key}-effective-interlock-visible",
                    "passed" if boundary_active and axis_name in positive_escape_only_axes else "failed",
                    f"status exposes {axis_name} HOME boundary via effective_interlocks",
                    boundary_desc,
                    note=f"raw status.io.{io_key} remains compatibility data and is not used as HOME boundary authority",
                    response=truncate_json(status_after_set),
                )
            )

            negative_move_payload = {
                "x": round(current_x - 1.0, 3) if move_axis_key == "x" else round(current_x, 3),
                "y": round(current_y - 1.0, 3) if move_axis_key == "y" else round(current_y, 3),
                "speed": 5.0,
            }
            negative_move = call(f"step-move-negative-{io_key}", "move", negative_move_payload)
            time.sleep(0.25)
            status_after_negative = call(f"step-status-after-move-negative-{io_key}", "status")
            negative_axis_position = float(extract_axes(status_after_negative).get(axis_name, {}).get("position", 0.0))
            negative_axis_delta = negative_axis_position - baseline_axis_position
            negative_text = json.dumps(negative_move, ensure_ascii=True)
            negative_axis_not_more_negative = negative_axis_delta >= -0.05
            negative_rejection_visible = (
                "error" in negative_move
                and int(negative_move.get("error", {}).get("code", -1)) == 2401
                and "HOME boundary is active" in negative_text
            )
            negative_positive_escape_visible = negative_axis_delta > 0.05
            negative_guard_preserved = negative_axis_not_more_negative and (
                negative_rejection_visible or "error" not in negative_move
            )
            results.append(
                CheckResult(
                    f"L5-{io_key}-blocks-negative-move",
                    "passed" if negative_guard_preserved else "failed",
                    "negative direction request does not drive the guarded axis further negative under HOME boundary",
                    truncate_json(negative_move),
                    note=(
                        f"baseline_axis_position={baseline_axis_position:.3f} "
                        f"after_axis_position={negative_axis_position:.3f} "
                        f"axis_delta={negative_axis_delta:.3f} "
                        f"negative_rejection_visible={negative_rejection_visible} "
                        f"positive_escape_visible={negative_positive_escape_visible} "
                        f"status_after={truncate_json(status_after_negative)}"
                    ),
                    response=truncate_json(negative_move),
                )
            )

            stop_after_negative = call(f"step-stop-after-negative-{io_key}", "stop")
            idle_after_negative, idle_status_after_negative = _wait_for_axes_idle(
                client,
                steps,
                f"step-wait-idle-after-negative-{io_key}",
            )
            if not idle_after_negative:
                raise RuntimeError(f"axes did not settle after negative probe for {io_key}")

            positive_baseline = idle_status_after_negative if "result" in idle_status_after_negative else status_after_negative
            positive_current_x, positive_current_y = extract_position(positive_baseline)
            positive_baseline_axis_position = float(extract_axes(positive_baseline).get(axis_name, {}).get("position", 0.0))
            positive_move_payload = {
                "x": round(positive_current_x + 1.0, 3) if move_axis_key == "x" else round(positive_current_x, 3),
                "y": round(positive_current_y + 1.0, 3) if move_axis_key == "y" else round(positive_current_y, 3),
                "speed": 5.0,
            }
            positive_move = call(f"step-move-positive-{io_key}", "move", positive_move_payload)
            moved, status_after_positive = _wait_for_axis_position_change(
                client,
                steps,
                f"step-wait-positive-{io_key}",
                axis_name,
                positive_baseline_axis_position,
            )
            positive_axis_position = float(extract_axes(status_after_positive).get(axis_name, {}).get("position", positive_baseline_axis_position))
            positive_axis_delta = positive_axis_position - positive_baseline_axis_position
            results.append(
                CheckResult(
                    f"L6-{io_key}-allows-positive-move",
                    "passed" if moved and positive_axis_delta > 0.05 else "failed",
                    "positive direction move still succeeds under HOME boundary latch",
                    truncate_json(positive_move),
                    note=(
                        f"stop_after_negative={truncate_json(stop_after_negative)} "
                        f"idle_after_negative={truncate_json(idle_status_after_negative)} "
                        f"positive_baseline_axis_position={positive_baseline_axis_position:.3f} "
                        f"after_axis_position={positive_axis_position:.3f} "
                        f"axis_delta={positive_axis_delta:.3f} "
                        f"status_after={truncate_json(status_after_positive)}"
                    ),
                    response=truncate_json(positive_move),
                )
            )

            call(f"step-stop-after-positive-{io_key}", "stop")
            idle_after_positive, idle_status_after_positive = _wait_for_axes_idle(
                client,
                steps,
                f"step-wait-idle-after-positive-{io_key}",
            )
            if not idle_after_positive:
                raise RuntimeError(f"axes did not settle after positive move for {io_key}")

            clear_response = call(f"step-clear-{io_key}", "mock.io.set", clear_params)
            results.append(CheckResult(f"L7-{io_key}-clear", "passed" if "error" not in clear_response else "failed", f"mock.io.set clears {io_key}", truncate_json(clear_response), response=truncate_json(clear_response)))

            status_after_clear = call(f"step-status-after-clear-{io_key}", "status")
            boundary_cleared, escape_axes_after_clear, clear_desc = _home_boundary_snapshot(
                status_after_clear,
                axis_name,
                io_key,
            )
            results.append(
                CheckResult(
                    f"L8-{io_key}-clear-visible",
                    "passed" if not boundary_cleared and axis_name not in escape_axes_after_clear else "failed",
                    f"status clears {axis_name} HOME boundary via effective_interlocks",
                    clear_desc,
                    note=(
                        f"idle_after_positive={truncate_json(idle_status_after_positive)}; "
                        f"raw status.io.{io_key} is retained only as compatibility data"
                    ),
                    response=truncate_json(status_after_clear),
                )
            )

        stop_response = call("step-stop", "stop")
        results.append(CheckResult("L9-stop", "passed" if "error" not in stop_response else "failed", "stop succeeds after limit probe", truncate_json(stop_response), response=truncate_json(stop_response)))

        disconnect_response = call("step-disconnect", "disconnect")
        disconnected, disconnect_desc = disconnect_ack(disconnect_response)
        results.append(CheckResult("L10-disconnect", "passed" if disconnected else "failed", "disconnect acknowledges disconnected=true", disconnect_desc, response=truncate_json(disconnect_response)))

    except Exception as exc:
        text = str(exc)
        if not (results and results[-1].check_id == "L0-bootstrap" and results[-1].status in {"failed", "known_failure"}):
            results.append(CheckResult("L99-unhandled-exception", "known_failure" if matches_known_failure(text) else "failed", "negative limit flow completes", text, "unhandled exception during negative limit flow"))
    finally:
        try:
            client.close()
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
            stdout, stderr = read_process_output(process)
            if stdout.strip():
                steps.append(StepRecord("gateway-stdout", "process.stdout", stdout.strip()))
            if stderr.strip():
                steps.append(StepRecord("gateway-stderr", "process.stderr", stderr.strip()))
            temp_config_dir.cleanup()

    report = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "gateway_exe": str(gateway_exe),
        "host": args.host,
        "port": port,
        "overall_status": compute_overall_status(results),
        "results": [asdict(item) for item in results],
        "steps": [asdict(item) for item in steps],
    }
    json_path, md_path = _write_report(report_dir, report)
    print(f"tcp negative limit chain complete: status={report['overall_status']}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")

    if report["overall_status"] == "passed":
        return 0
    if report["overall_status"] == "known_failure":
        return KNOWN_FAILURE_EXIT_CODE
    if report["overall_status"] == "skipped":
        return SKIPPED_EXIT_CODE
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
