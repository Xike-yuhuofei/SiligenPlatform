from __future__ import annotations

import argparse
import json
import subprocess
import time
from dataclasses import asdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

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
    extract_effective_interlocks,
    extract_io,
    extract_supply_open,
    extract_supervision,
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
    json_path = report_dir / "tcp-door-interlock.json"
    md_path = report_dir / "tcp-door-interlock.md"
    json_path.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

    lines = [
        "# TCP Door Interlock",
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


def _door_snapshot(status_response: dict[str, Any]) -> tuple[bool, str]:
    result = status_response.get("result", {})
    io_payload = extract_io(status_response)
    effective_interlocks = extract_effective_interlocks(status_response)
    supervision = extract_supervision(status_response)
    door_flag = bool(effective_interlocks.get("door_open_active", io_payload.get("door", False)))
    door_known = bool(effective_interlocks.get("door_open_known", io_payload.get("door_known", False)))
    machine_state = str(supervision.get("current_state", result.get("machine_state", "")))
    machine_state_reason = str(supervision.get("state_reason", result.get("machine_state_reason", "")))
    requested_state = str(supervision.get("requested_state", machine_state))
    state_change_in_process = bool(supervision.get("state_change_in_process", False))
    desc = (
        f"effective_interlocks.door_open_active={door_flag}; "
        f"door_open_known={door_known}; "
        f"supervision.current_state={machine_state}; "
        f"supervision.requested_state={requested_state}; "
        f"supervision.state_change_in_process={state_change_in_process}; "
        f"reason={machine_state_reason}; "
        f"io.door={bool(io_payload.get('door', False))}"
    )
    return door_flag and door_known, desc


def _wait_for_status(
    client: TcpJsonClient,
    steps: list[StepRecord],
    step_prefix: str,
    predicate,
    timeout_seconds: float = 6.0,
    poll_interval_seconds: float = 0.2,
) -> tuple[bool, dict[str, Any]]:
    deadline = time.time() + timeout_seconds
    last_status: dict[str, Any] = {}
    attempt = 0
    while time.time() < deadline:
        attempt += 1
        last_status = client.send_request("status", {}, timeout_seconds=8.0)
        steps.append(StepRecord(f"{step_prefix}-{attempt}", "status", truncate_json(last_status)))
        if predicate(last_status):
            return True, last_status
        time.sleep(poll_interval_seconds)
    return False, last_status


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run TCP safety-door interlock probe and write a report.")
    parser.add_argument("--report-dir", default=str(ROOT / "tests" / "reports" / "first-layer" / "s5-door-interlock"))
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9527)
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
    results: list[CheckResult] = []
    steps: list[StepRecord] = []

    if not gateway_exe.exists():
        status = "skipped" if args.allow_skip_on_missing_gateway else "known_failure"
        results.append(
            CheckResult("D0-bootstrap", status, "gateway executable exists", str(gateway_exe), "gateway executable missing")
        )
        report = {
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "workspace_root": str(ROOT),
            "gateway_exe": str(gateway_exe),
            "host": args.host,
            "port": args.port,
            "overall_status": compute_overall_status(results),
            "results": [asdict(item) for item in results],
            "steps": [asdict(item) for item in steps],
        }
        json_path, md_path = _write_report(report_dir, report)
        print(f"tcp door interlock complete: status={report['overall_status']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        return SKIPPED_EXIT_CODE if status == "skipped" else KNOWN_FAILURE_EXIT_CODE

    temp_config_dir, temp_config = prepare_mock_config("tcp-door-interlock-", door_input=6)
    process = subprocess.Popen(
        [str(gateway_exe), "--config", str(temp_config), "--port", str(args.port)],
        cwd=str(ROOT),
        env=build_process_env(gateway_exe),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    client = TcpJsonClient(args.host, args.port)

    try:
        ready_status, ready_note = wait_gateway_ready(process, args.host, args.port, timeout_seconds=8.0)
        if ready_status != "passed":
            results.append(CheckResult("D0-bootstrap", ready_status, "gateway ready", ready_note, "gateway readiness check failed"))
            raise RuntimeError("gateway not ready")

        client.connect(timeout_seconds=3.0)

        def call(step_id: str, method: str, params: dict[str, Any] | None = None, timeout_seconds: float = 8.0) -> dict[str, Any]:
            response = client.send_request(method, params, timeout_seconds=timeout_seconds)
            steps.append(StepRecord(step_id, method, truncate_json(response)))
            return response

        connect_response = call("step-connect", "connect", {}, timeout_seconds=10.0)
        if "error" in connect_response:
            results.append(CheckResult("D1-connect", "failed", "connect succeeds", truncate_json(connect_response), response=truncate_json(connect_response)))
            raise RuntimeError("connect failed")
        results.append(CheckResult("D1-connect", "passed", "connect succeeds", "connected=true", response=truncate_json(connect_response)))

        baseline_status = call("step-status-baseline", "status")
        baseline_door, baseline_desc = _door_snapshot(baseline_status)
        results.append(CheckResult("D2-baseline-door-clear", "passed" if not baseline_door else "failed", "baseline status shows door clear", baseline_desc, response=truncate_json(baseline_status)))

        door_set = call("step-mock-door-on", "mock.io.set", {"door": True})
        results.append(CheckResult("D3-door-set", "passed" if "error" not in door_set else "failed", "mock.io.set enables door signal", truncate_json(door_set), response=truncate_json(door_set)))

        door_visible_ok, status_after_door = _wait_for_status(
            client,
            steps,
            "step-status-door-on",
            lambda response: (
                _door_snapshot(response)[0]
                and str(extract_supervision(response).get("current_state", response.get("result", {}).get("machine_state", ""))) == "Fault"
                and str(extract_supervision(response).get("state_reason", response.get("result", {}).get("machine_state_reason", ""))) == "interlock_door_open"
            ),
        )
        door_active, door_desc = _door_snapshot(status_after_door)
        supervision_after_door = extract_supervision(status_after_door)
        machine_state = str(supervision_after_door.get("current_state", status_after_door.get("result", {}).get("machine_state", "")))
        machine_reason = str(supervision_after_door.get("state_reason", status_after_door.get("result", {}).get("machine_state_reason", "")))
        door_visible_ok = door_visible_ok and door_active and machine_state == "Fault" and machine_reason == "interlock_door_open"
        results.append(CheckResult("D4-door-visible", "passed" if door_visible_ok else "failed", "status exposes door fault through effective_interlocks/supervision", door_desc, response=truncate_json(status_after_door)))

        dispenser_start_with_door = call(
            "step-dispenser-start-with-door",
            "dispenser.start",
            {"count": 3, "interval_ms": 50, "duration_ms": 30},
        )
        status_after_dispenser_with_door = call("step-status-after-dispenser-start-with-door", "status")
        dispenser_blocked = "error" in dispenser_start_with_door and not bool(
            status_after_dispenser_with_door.get("result", {}).get("dispenser", {}).get("valve_open", False)
        )
        results.append(
            CheckResult(
                "D5-dispenser-start-blocked-by-door",
                "passed" if dispenser_blocked else "failed",
                "dispenser.start is blocked while door interlock is active",
                truncate_json(dispenser_start_with_door),
                note=f"status_after={truncate_json(status_after_dispenser_with_door)}",
                response=truncate_json(dispenser_start_with_door),
            )
        )

        supply_open_with_door = call("step-supply-open-with-door", "supply.open")
        status_after_supply_with_door = call("step-status-after-supply-open-with-door", "status")
        supply_blocked = "error" in supply_open_with_door and not extract_supply_open(status_after_supply_with_door)
        results.append(
            CheckResult(
                "D6-supply-open-blocked-by-door",
                "passed" if supply_blocked else "failed",
                "supply.open is blocked while door interlock is active",
                truncate_json(supply_open_with_door),
                note=f"status_after={truncate_json(status_after_supply_with_door)}",
                response=truncate_json(supply_open_with_door),
            )
        )

        door_clear = call("step-mock-door-off", "mock.io.set", {"door": False})
        results.append(CheckResult("D7-door-clear", "passed" if "error" not in door_clear else "failed", "mock.io.set clears door signal", truncate_json(door_clear), response=truncate_json(door_clear)))

        door_clear_visible_ok, status_after_clear = _wait_for_status(
            client,
            steps,
            "step-status-door-off",
            lambda response: (
                not _door_snapshot(response)[0]
                and str(extract_supervision(response).get("state_reason", response.get("result", {}).get("machine_state_reason", ""))) != "interlock_door_open"
            ),
        )
        door_cleared, clear_desc = _door_snapshot(status_after_clear)
        results.append(CheckResult("D8-door-clear-visible", "passed" if door_clear_visible_ok and not door_cleared else "failed", "status clears door gate through effective_interlocks/supervision", clear_desc, response=truncate_json(status_after_clear)))

        home_ready = False
        home_attempt_details: list[str] = []
        latest_home_status = status_after_clear
        for attempt in range(1, 4):
            home_response = call(f"step-home-after-clear-{attempt}", "home", {}, timeout_seconds=45.0)
            latest_home_status = call(f"step-status-after-home-after-clear-{attempt}", "status")
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
                "D9-home-recovers-after-door-clear",
                "passed" if home_ready else "failed",
                "home succeeds after door is cleared",
                "all axes homed consistently" if home_ready else "home did not reach fully homed state",
                note="; ".join(home_attempt_details),
                response=truncate_json(latest_home_status),
            )
        )

        supply_open_after_clear = call("step-supply-open-after-clear", "supply.open")
        status_after_supply_open = call("step-status-after-supply-open", "status")
        supply_close_after_clear = call("step-supply-close-after-clear", "supply.close")
        status_after_supply_close = call("step-status-after-supply-close", "status")
        supply_roundtrip_ok = (
            "error" not in supply_open_after_clear
            and extract_supply_open(status_after_supply_open)
            and "error" not in supply_close_after_clear
            and not extract_supply_open(status_after_supply_close)
        )
        results.append(
            CheckResult(
                "D10-supply-recovers-after-door-clear",
                "passed" if supply_roundtrip_ok else "failed",
                "supply.open / supply.close recover after door clear",
                f"open={truncate_json(supply_open_after_clear)} close={truncate_json(supply_close_after_clear)}",
                note=f"status_after_open={truncate_json(status_after_supply_open)} status_after_close={truncate_json(status_after_supply_close)}",
            )
        )

        disconnect_response = call("step-disconnect", "disconnect")
        disconnected, disconnect_desc = disconnect_ack(disconnect_response)
        results.append(CheckResult("D11-disconnect", "passed" if disconnected else "failed", "disconnect acknowledges disconnected=true", disconnect_desc, response=truncate_json(disconnect_response)))

    except Exception as exc:
        text = str(exc)
        if not (results and results[-1].check_id == "D0-bootstrap" and results[-1].status in {"failed", "known_failure"}):
            results.append(CheckResult("D99-unhandled-exception", "known_failure" if matches_known_failure(text) else "failed", "door interlock flow completes", text, "unhandled exception during door interlock flow"))
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
        "port": args.port,
        "overall_status": compute_overall_status(results),
        "results": [asdict(item) for item in results],
        "steps": [asdict(item) for item in steps],
    }
    json_path, md_path = _write_report(report_dir, report)
    print(f"tcp door interlock complete: status={report['overall_status']}")
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
