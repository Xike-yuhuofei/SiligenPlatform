from __future__ import annotations

import argparse
import json
import os
import subprocess
import time
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from run_hil_closed_loop import (
    DEFAULT_CONFIG_PATH,
    DEFAULT_DXF_FILE,
    KNOWN_FAILURE_EXIT_CODE,
    SKIPPED_EXIT_CODE,
    TcpJsonClient,
    _load_connection_params,
    _resolve_default_exe,
    _run_tcp_step,
    _wait_for_dispenser_state,
    _wait_gateway_ready,
    build_process_env,
)


ROOT = Path(__file__).resolve().parents[3]


@dataclass
class MatrixRoundReport:
    round_index: int
    status: str = "pending"
    home_command_completed: bool | None = None
    home_message: str = ""
    home_result_succeeded_axes: list[str] = field(default_factory=list)
    cases_executed: list[str] = field(default_factory=list)
    steps: list[dict[str, Any]] = field(default_factory=list)
    state_transition_checks: list[dict[str, Any]] = field(default_factory=list)
    snapshots: list[dict[str, Any]] = field(default_factory=list)
    home_failed_axes: list[str] = field(default_factory=list)
    home_result_failed_axes: list[str] = field(default_factory=list)
    home_result_error_messages: list[str] = field(default_factory=list)
    non_homed_axes: list[str] = field(default_factory=list)
    limit_blockers: list[str] = field(default_factory=list)
    inferred_collision_axes: list[str] = field(default_factory=list)
    estop_active: bool = False
    error: str = ""
    gateway_stdout: str = ""
    gateway_stderr: str = ""


def _truncate(text: str, limit: int = 4000) -> str:
    text = text.strip()
    if len(text) <= limit:
        return text
    return text[:limit] + "\n...[truncated]..."


def _load_result(response: dict[str, Any], method: str) -> dict[str, Any]:
    if "error" in response:
        message = str(response.get("error", {}).get("message", "unknown tcp error"))
        raise RuntimeError(f"{method} returned error: {message}")
    result = response.get("result")
    if not isinstance(result, dict):
        raise RuntimeError(f"{method} returned invalid result payload")
    return result


def _normalize_axis_snapshot(axis_name: str, payload: dict[str, Any]) -> dict[str, Any]:
    return {
        "axis": axis_name,
        "homed": bool(payload.get("homed", False)),
        "state": int(payload.get("state", 0)),
        "position": float(payload.get("position", 0.0)),
        "velocity": float(payload.get("velocity", 0.0)),
        "has_error": bool(payload.get("has_error", False)),
        "error_code": int(payload.get("error_code", 0)),
        "servo_alarm": bool(payload.get("servo_alarm", False)),
        "following_error": bool(payload.get("following_error", False)),
        "home_failed": bool(payload.get("home_failed", False)),
        "hard_limit_positive": bool(payload.get("hard_limit_positive", False)),
        "hard_limit_negative": bool(payload.get("hard_limit_negative", False)),
        "soft_limit_positive": bool(payload.get("soft_limit_positive", False)),
        "soft_limit_negative": bool(payload.get("soft_limit_negative", False)),
        "home_signal": bool(payload.get("home_signal", False)),
        "index_signal": bool(payload.get("index_signal", False)),
    }


def _collect_snapshot(client: TcpJsonClient, name: str) -> dict[str, Any]:
    status_response = client.send_request("status", {}, timeout_seconds=8.0)
    status_result = _load_result(status_response, "status")
    coord_response = client.send_request("motion.coord.status", {"coord_sys": 1}, timeout_seconds=8.0)
    coord_result = _load_result(coord_response, "motion.coord.status")

    axes_payload = coord_result.get("axes", {})
    axes: dict[str, Any] = {}
    if isinstance(axes_payload, dict):
        for axis_name in ("X", "Y"):
            axis_payload = axes_payload.get(axis_name)
            if isinstance(axis_payload, dict):
                axes[axis_name] = _normalize_axis_snapshot(axis_name, axis_payload)

    io_payload = status_result.get("io", {})
    snapshot = {
        "name": name,
        "machine_state": str(status_result.get("machine_state", "")),
        "machine_state_reason": str(status_result.get("machine_state_reason", "")),
        "connected": bool(status_result.get("connected", False)),
        "io": {
            "estop": bool(io_payload.get("estop", False)),
            "estop_known": bool(io_payload.get("estop_known", False)),
            "limit_x_pos": bool(io_payload.get("limit_x_pos", False)),
            "limit_x_neg": bool(io_payload.get("limit_x_neg", False)),
            "limit_y_pos": bool(io_payload.get("limit_y_pos", False)),
            "limit_y_neg": bool(io_payload.get("limit_y_neg", False)),
        },
        "axes": axes,
    }
    return snapshot


def _merge_snapshot_findings(
    snapshot: dict[str, Any],
    *,
    home_failed_axes: set[str],
    non_homed_axes: set[str],
    limit_blockers: set[str],
    inferred_collision_axes: set[str],
) -> bool:
    estop_active = bool(snapshot.get("io", {}).get("estop", False))
    io_payload = snapshot.get("io", {})
    if io_payload.get("limit_x_pos", False):
        limit_blockers.add("io.limit_x_pos")
    if io_payload.get("limit_x_neg", False):
        limit_blockers.add("io.limit_x_neg")
    if io_payload.get("limit_y_pos", False):
        limit_blockers.add("io.limit_y_pos")
    if io_payload.get("limit_y_neg", False):
        limit_blockers.add("io.limit_y_neg")

    axes = snapshot.get("axes", {})
    if isinstance(axes, dict):
        for axis_name, axis_payload in axes.items():
            if not isinstance(axis_payload, dict):
                continue
            if axis_payload.get("home_failed", False):
                home_failed_axes.add(axis_name)
            if not axis_payload.get("homed", False):
                non_homed_axes.add(axis_name)
            if axis_payload.get("servo_alarm", False) or axis_payload.get("following_error", False):
                inferred_collision_axes.add(axis_name)
            for field_name in (
                "hard_limit_positive",
                "hard_limit_negative",
                "soft_limit_positive",
                "soft_limit_negative",
            ):
                if axis_payload.get(field_name, False):
                    limit_blockers.add(f"{axis_name}.{field_name}")

    return estop_active


def _step_to_dict(name: str, status: str, note: str, command: list[str], return_code: int | None = None) -> dict[str, Any]:
    return {
        "name": name,
        "status": status,
        "note": note,
        "command": command,
        "return_code": return_code,
    }


def _run_closed_loop_case(
    *,
    args: argparse.Namespace,
    client: TcpJsonClient,
    round_report: MatrixRoundReport,
) -> None:
    dxf_step, _ = _run_tcp_step(
        name="tcp-dxf-load",
        client=client,
        method="dxf.load",
        params={"filepath": str(args.dxf_file)},
        timeout_seconds=60.0,
    )
    round_report.steps.append(asdict(dxf_step))
    if dxf_step.status != "passed":
        raise RuntimeError(dxf_step.note or "dxf.load failed")

    status_step, _ = _run_tcp_step(
        name="tcp-status",
        client=client,
        method="status",
        params={},
        timeout_seconds=8.0,
    )
    round_report.steps.append(asdict(status_step))
    if status_step.status != "passed":
        raise RuntimeError(status_step.note or "status failed before closed loop")

    sequence: list[tuple[str, str, dict[str, Any], float]] = [
        ("tcp-supply-open", "supply.open", {}, 15.0),
        (
            "tcp-dispenser-start",
            "dispenser.start",
            {
                "count": args.dispenser_count,
                "interval_ms": args.dispenser_interval_ms,
                "duration_ms": args.dispenser_duration_ms,
            },
            15.0,
        ),
    ]
    for idx in range(max(1, args.pause_resume_cycles)):
        sequence.append((f"tcp-dispenser-pause-{idx + 1}", "dispenser.pause", {}, 15.0))
        sequence.append((f"tcp-dispenser-resume-{idx + 1}", "dispenser.resume", {}, 15.0))
    sequence.extend(
        [
            ("tcp-dispenser-stop", "dispenser.stop", {}, 15.0),
            ("tcp-supply-close", "supply.close", {}, 15.0),
            ("tcp-stop", "stop", {}, 15.0),
            ("tcp-disconnect", "disconnect", {}, 8.0),
        ]
    )

    for name, method, params, timeout_seconds in sequence:
        step, _ = _run_tcp_step(
            name=name,
            client=client,
            method=method,
            params=params,
            timeout_seconds=timeout_seconds,
        )
        round_report.steps.append(asdict(step))
        if step.status != "passed":
            raise RuntimeError(step.note or f"{method} failed")

        if name == "tcp-dispenser-start":
            wait_step, transition = _wait_for_dispenser_state(
                name="wait-dispenser-running",
                client=client,
                expected_state="running",
                timeout_seconds=args.state_wait_timeout_seconds,
            )
            round_report.steps.append(asdict(wait_step))
            round_report.state_transition_checks.append(transition)
            if wait_step.status != "passed":
                raise RuntimeError(wait_step.note or "dispenser did not enter running state")
        elif name.startswith("tcp-dispenser-pause-"):
            wait_step, transition = _wait_for_dispenser_state(
                name=f"wait-{name}",
                client=client,
                expected_state="paused",
                timeout_seconds=args.state_wait_timeout_seconds,
            )
            round_report.steps.append(asdict(wait_step))
            round_report.state_transition_checks.append(transition)
            if wait_step.status != "passed":
                raise RuntimeError(wait_step.note or "dispenser did not enter paused state")
        elif name.startswith("tcp-dispenser-resume-"):
            wait_step, transition = _wait_for_dispenser_state(
                name=f"wait-{name}",
                client=client,
                expected_state="running",
                timeout_seconds=args.state_wait_timeout_seconds,
            )
            round_report.steps.append(asdict(wait_step))
            round_report.state_transition_checks.append(transition)
            if wait_step.status != "passed":
                raise RuntimeError(wait_step.note or "dispenser did not return to running state")


def _run_round(args: argparse.Namespace, round_index: int, selected_modes: tuple[str, ...]) -> MatrixRoundReport:
    round_report = MatrixRoundReport(round_index=round_index, cases_executed=list(selected_modes))
    gateway_exe = Path(args.gateway_exe)
    connect_params = _load_connection_params(args.config_path)
    process = subprocess.Popen(
        [str(gateway_exe), "--config", str(args.config_path), "--port", str(args.port)],
        cwd=str(ROOT),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        env=build_process_env(gateway_exe),
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    client = TcpJsonClient(args.host, args.port)

    home_failed_axes: set[str] = set()
    non_homed_axes: set[str] = set()
    limit_blockers: set[str] = set()
    inferred_collision_axes: set[str] = set()

    try:
        ready_status, ready_note = _wait_gateway_ready(process, args.host, args.port, timeout_seconds=8.0)
        round_report.steps.append(
            _step_to_dict(
                name="gateway-ready",
                status=ready_status,
                note=ready_note,
                command=[str(gateway_exe)],
                return_code=0 if ready_status == "passed" else 1,
            )
        )
        if ready_status != "passed":
            raise RuntimeError(ready_note)

        client.connect(timeout_seconds=3.0)
        round_report.steps.append(
            _step_to_dict(
                name="tcp-session-open",
                status="passed",
                note="tcp session established",
                command=["tcp-session-open", f"{args.host}:{args.port}"],
                return_code=0,
            )
        )

        connect_step, _ = _run_tcp_step(
            name="tcp-connect",
            client=client,
            method="connect",
            params=connect_params,
            timeout_seconds=15.0,
        )
        round_report.steps.append(asdict(connect_step))
        if connect_step.status != "passed":
            raise RuntimeError(connect_step.note or "connect failed")

        baseline = _collect_snapshot(client, "baseline")
        round_report.snapshots.append(baseline)
        round_report.estop_active = _merge_snapshot_findings(
            baseline,
            home_failed_axes=home_failed_axes,
            non_homed_axes=set(),
            limit_blockers=limit_blockers,
            inferred_collision_axes=inferred_collision_axes,
        )

        if "home" in selected_modes:
            home_step, home_response = _run_tcp_step(
                name="tcp-home",
                client=client,
                method="home",
                params={"force": args.force_home},
                timeout_seconds=args.home_timeout_seconds,
            )
            round_report.steps.append(asdict(home_step))
            if home_step.status != "passed":
                raise RuntimeError(home_step.note or "home failed")

            home_result = _load_result(home_response or {}, "home")
            round_report.home_command_completed = bool(home_result.get("completed", False))
            round_report.home_message = str(home_result.get("message", ""))
            if isinstance(home_result.get("succeeded_axes"), list):
                round_report.home_result_succeeded_axes = [str(value) for value in home_result["succeeded_axes"]]
            if isinstance(home_result.get("failed_axes"), list):
                round_report.home_result_failed_axes = [str(value) for value in home_result["failed_axes"]]
            if isinstance(home_result.get("error_messages"), list):
                round_report.home_result_error_messages = [str(value) for value in home_result["error_messages"]]
            if not round_report.home_command_completed:
                detail_parts = [
                    "home completed=false",
                    f"message={round_report.home_message}",
                ]
                if round_report.home_result_failed_axes:
                    detail_parts.append(f"failed_axes={','.join(round_report.home_result_failed_axes)}")
                if round_report.home_result_error_messages:
                    detail_parts.append("error_messages=" + " | ".join(round_report.home_result_error_messages))
                raise RuntimeError(" ".join(detail_parts))

            post_home = _collect_snapshot(client, "post-home")
            round_report.snapshots.append(post_home)
            round_report.estop_active = round_report.estop_active or _merge_snapshot_findings(
                post_home,
                home_failed_axes=home_failed_axes,
                non_homed_axes=non_homed_axes,
                limit_blockers=limit_blockers,
                inferred_collision_axes=inferred_collision_axes,
            )

        if "closed_loop" in selected_modes:
            _run_closed_loop_case(args=args, client=client, round_report=round_report)
            post_closed_loop = _collect_snapshot(client, "post-closed-loop")
            round_report.snapshots.append(post_closed_loop)
            round_report.estop_active = round_report.estop_active or _merge_snapshot_findings(
                post_closed_loop,
                home_failed_axes=home_failed_axes,
                non_homed_axes=set(),
                limit_blockers=limit_blockers,
                inferred_collision_axes=inferred_collision_axes,
            )
    except Exception as exc:
        round_report.status = "failed"
        round_report.error = str(exc)
    else:
        round_report.status = "passed"
    finally:
        round_report.home_failed_axes = sorted(home_failed_axes)
        round_report.non_homed_axes = sorted(non_homed_axes)
        round_report.limit_blockers = sorted(limit_blockers)
        round_report.inferred_collision_axes = sorted(inferred_collision_axes)

        if round_report.status == "passed":
            if round_report.estop_active:
                round_report.status = "failed"
                round_report.error = "estop observed during round"
            elif round_report.home_failed_axes:
                round_report.status = "failed"
                round_report.error = "home_failed observed during round"
            elif round_report.non_homed_axes:
                round_report.status = "failed"
                round_report.error = "axes remained non-homed after home case"
            elif round_report.limit_blockers:
                round_report.status = "failed"
                round_report.error = "limit blocker observed during round"
            elif round_report.inferred_collision_axes:
                round_report.status = "failed"
                round_report.error = "servo_alarm/following_error observed during round"

        client.close()
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
        stdout, stderr = process.communicate(timeout=5)
        round_report.gateway_stdout = _truncate(stdout or "")
        round_report.gateway_stderr = _truncate(stderr or "")

    return round_report


def _write_report(report_dir: Path, report: dict[str, Any]) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "case-matrix-summary.json"
    md_path = report_dir / "case-matrix-summary.md"
    json_path.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

    counts = report["counts"]
    lines = [
        "# Hardware Online Case Matrix Summary",
        "",
        f"- generated_at: `{report['generated_at']}`",
        f"- mode: `{report['mode']}`",
        f"- rounds_requested: `{report['rounds_requested']}`",
        f"- rounds_executed: `{report['rounds_executed']}`",
        f"- overall_status: `{report['overall_status']}`",
        f"- rounds_passed: `{counts['passed_rounds']}`",
        f"- rounds_failed: `{counts['failed_rounds']}`",
        f"- home_failed_occurrences: `{counts['home_failed_occurrences']}`",
        f"- estop_occurrences: `{counts['estop_occurrences']}`",
        f"- limit_occurrences: `{counts['limit_occurrences']}`",
        f"- inferred_collision_occurrences: `{counts['inferred_collision_occurrences']}`",
        "",
        "## Rounds",
        "",
    ]

    for round_report in report["rounds"]:
        lines.append(
            f"- `round-{round_report['round_index']:02d}` status=`{round_report['status']}` "
            f"home_failed={len(round_report['home_failed_axes'])} "
            f"limit={len(round_report['limit_blockers'])} "
            f"collision={len(round_report['inferred_collision_axes'])} "
            f"estop=`{round_report['estop_active']}`"
        )
        if round_report.get("home_message"):
            lines.append(f"  home_message: `{round_report['home_message']}`")
        if round_report.get("home_result_succeeded_axes"):
            lines.append(f"  home_result_succeeded_axes: `{','.join(round_report['home_result_succeeded_axes'])}`")
        if round_report.get("error"):
            lines.append(f"  error: {round_report['error']}")
        if round_report.get("home_failed_axes"):
            lines.append(f"  home_failed_axes: `{','.join(round_report['home_failed_axes'])}`")
        if round_report.get("home_result_failed_axes"):
            lines.append(f"  home_result_failed_axes: `{','.join(round_report['home_result_failed_axes'])}`")
        if round_report.get("home_result_error_messages"):
            lines.append(f"  home_result_error_messages: `{' | '.join(round_report['home_result_error_messages'])}`")
        if round_report.get("non_homed_axes"):
            lines.append(f"  non_homed_axes: `{','.join(round_report['non_homed_axes'])}`")
        if round_report.get("limit_blockers"):
            lines.append(f"  limit_blockers: `{','.join(round_report['limit_blockers'])}`")
        if round_report.get("inferred_collision_axes"):
            lines.append(f"  inferred_collision_axes: `{','.join(round_report['inferred_collision_axes'])}`")
    lines.append("")
    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run hardware online case matrix and write structured reports.")
    parser.add_argument("--mode", choices=("home", "closed_loop", "both"), default="both")
    parser.add_argument("--rounds", type=int, default=20)
    parser.add_argument("--report-dir", default=str(ROOT / "tests" / "reports" / "adhoc" / "hil-case-matrix"))
    parser.add_argument("--host", default=os.getenv("SILIGEN_HIL_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.getenv("SILIGEN_HIL_PORT", "9527")))
    parser.add_argument("--config-path", type=Path, default=DEFAULT_CONFIG_PATH)
    parser.add_argument(
        "--gateway-exe",
        default=str(_resolve_default_exe("siligen_runtime_gateway.exe", "siligen_tcp_server.exe")),
    )
    parser.add_argument("--dxf-file", type=Path, default=DEFAULT_DXF_FILE)
    parser.add_argument("--home-timeout-seconds", type=float, default=90.0)
    parser.add_argument("--pause-resume-cycles", type=int, default=3)
    parser.add_argument("--dispenser-count", type=int, default=int(os.getenv("SILIGEN_HIL_DISPENSER_COUNT", "300")))
    parser.add_argument("--dispenser-interval-ms", type=int, default=int(os.getenv("SILIGEN_HIL_DISPENSER_INTERVAL_MS", "200")))
    parser.add_argument("--dispenser-duration-ms", type=int, default=int(os.getenv("SILIGEN_HIL_DISPENSER_DURATION_MS", "80")))
    parser.add_argument("--state-wait-timeout-seconds", type=float, default=float(os.getenv("SILIGEN_HIL_STATE_WAIT_TIMEOUT_SECONDS", "8")))
    parser.add_argument("--force-home", action=argparse.BooleanOptionalAction, default=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir)
    selected_modes = ("home", "closed_loop") if args.mode == "both" else (args.mode,)

    gateway_exe = Path(args.gateway_exe)
    if not gateway_exe.exists():
        report = {
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "workspace_root": str(ROOT),
            "mode": args.mode,
            "rounds_requested": args.rounds,
            "rounds_executed": 0,
            "overall_status": "known_failure",
            "error": f"gateway executable missing: {gateway_exe}",
            "counts": {
                "passed_rounds": 0,
                "failed_rounds": 0,
                "home_failed_occurrences": 0,
                "estop_occurrences": 0,
                "limit_occurrences": 0,
                "inferred_collision_occurrences": 0,
            },
            "rounds": [],
        }
        json_path, md_path = _write_report(report_dir, report)
        print(f"case matrix complete: status={report['overall_status']}")
        print(f"json report: {json_path}")
        print(f"markdown report: {md_path}")
        return KNOWN_FAILURE_EXIT_CODE

    rounds: list[MatrixRoundReport] = []
    for round_index in range(1, args.rounds + 1):
        round_report = _run_round(args, round_index, selected_modes)
        rounds.append(round_report)
        print(f"case matrix round {round_index}/{args.rounds}: status={round_report.status}")
        if round_report.status != "passed":
            break

    passed_rounds = sum(1 for item in rounds if item.status == "passed")
    failed_rounds = sum(1 for item in rounds if item.status != "passed")
    counts = {
        "passed_rounds": passed_rounds,
        "failed_rounds": failed_rounds,
        "home_failed_occurrences": sum(len(item.home_failed_axes) for item in rounds),
        "estop_occurrences": sum(1 for item in rounds if item.estop_active),
        "limit_occurrences": sum(len(item.limit_blockers) for item in rounds),
        "inferred_collision_occurrences": sum(len(item.inferred_collision_axes) for item in rounds),
    }
    overall_status = (
        "passed"
        if passed_rounds == args.rounds
        and counts["home_failed_occurrences"] == 0
        and counts["estop_occurrences"] == 0
        and counts["limit_occurrences"] == 0
        and counts["inferred_collision_occurrences"] == 0
        else "failed"
    )

    report = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "mode": args.mode,
        "rounds_requested": args.rounds,
        "rounds_executed": len(rounds),
        "gateway_exe": str(gateway_exe),
        "dxf_file": str(args.dxf_file),
        "force_home": args.force_home,
        "pause_resume_cycles": args.pause_resume_cycles,
        "dispenser_start_params": {
            "count": args.dispenser_count,
            "interval_ms": args.dispenser_interval_ms,
            "duration_ms": args.dispenser_duration_ms,
        },
        "state_wait_timeout_seconds": args.state_wait_timeout_seconds,
        "home_timeout_seconds": args.home_timeout_seconds,
        "overall_status": overall_status,
        "counts": counts,
        "rounds": [asdict(item) for item in rounds],
    }
    json_path, md_path = _write_report(report_dir, report)
    print(f"case matrix complete: status={overall_status}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")

    if overall_status == "passed":
        return 0
    if not rounds:
        return SKIPPED_EXIT_CODE
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
