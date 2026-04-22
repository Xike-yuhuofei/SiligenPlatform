from __future__ import annotations

import argparse
import json
import math
import os
import platform
import socket
import statistics
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
REPORT_ROOT = ROOT / "tests" / "reports" / "performance"
FIXTURE_ROOT = ROOT / "shared" / "contracts" / "engineering" / "fixtures" / "cases" / "rect_diag"
DXF_INPUT = FIXTURE_ROOT / "rect_diag.dxf"
SIM_INPUT = FIXTURE_ROOT / "simulation-input.json"
CONFIG_PATH = ROOT / "config" / "machine" / "machine_config.ini"
MOCK_SERVER = ROOT / "apps" / "hmi-app" / "src" / "hmi_client" / "tools" / "mock_server.py"
HARDWARE_SMOKE = ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_hardware_smoke.py"
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
ENGINEERING_REGRESSION = ROOT / "tests" / "integration" / "scenarios" / "run_engineering_regression.py"
SIMULATED_LINE = ROOT / "tests" / "e2e" / "simulated-line" / "run_simulated_line.py"
DXF_TO_PB = ROOT / "scripts" / "engineering-data" / "dxf_to_pb.py"
EXPORT_SIM_INPUT = ROOT / "scripts" / "engineering-data" / "export_simulation_input.py"
HMI_SRC = ROOT / "apps" / "hmi-app" / "src"

if str(HMI_SRC) not in sys.path:
    sys.path.insert(0, str(HMI_SRC))
if str(HIL_DIR) not in sys.path:
    sys.path.insert(0, str(HIL_DIR))

from hmi_client.client.backend_manager import BackendManager  # noqa: E402
from hmi_client.client.gateway_launch import GatewayLaunchSpec  # noqa: E402
from hmi_client.client.protocol import CommandProtocol  # noqa: E402
from hmi_client.client.tcp_client import TcpClient  # noqa: E402
from runtime_gateway_harness import production_baseline_metadata  # noqa: E402


@dataclass
class CommandSample:
    return_code: int
    duration_seconds: float
    stdout: str
    stderr: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Collect repeatable performance and reliability baselines.")
    parser.add_argument("--preprocess-iterations", type=int, default=10)
    parser.add_argument("--simulation-iterations", type=int, default=10)
    parser.add_argument("--startup-iterations", type=int, default=10)
    parser.add_argument("--tcp-iterations", type=int, default=100)
    parser.add_argument("--mock-flow-iterations", type=int, default=20)
    parser.add_argument("--unsupported-iterations", type=int, default=5)
    parser.add_argument("--hil-iterations", type=int, default=3)
    parser.add_argument("--report-dir", default=str(REPORT_ROOT))
    return parser.parse_args()


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def local_day() -> str:
    return datetime.now().date().isoformat()


def run_command(
    command: list[str],
    *,
    cwd: Path = ROOT,
    timeout: float | None = None,
    env: dict[str, str] | None = None,
) -> CommandSample:
    started = time.perf_counter()
    completed = subprocess.run(
        command,
        cwd=str(cwd),
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        env=env or os.environ.copy(),
        timeout=timeout,
    )
    return CommandSample(
        return_code=completed.returncode,
        duration_seconds=time.perf_counter() - started,
        stdout=(completed.stdout or "").strip(),
        stderr=(completed.stderr or "").strip(),
    )


def percentile(values: list[float], q: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * q
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    weight = position - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def summarize_samples(values: list[float]) -> dict[str, Any]:
    if not values:
        return {
            "samples": [],
            "count": 0,
            "mean_ms": 0.0,
            "median_ms": 0.0,
            "p95_ms": 0.0,
            "min_ms": 0.0,
            "max_ms": 0.0,
            "stdev_ms": 0.0,
            "cv": 0.0,
        }
    mean = statistics.fmean(values)
    stdev = statistics.stdev(values) if len(values) > 1 else 0.0
    cv = (stdev / mean) if mean else 0.0
    return {
        "samples": [round(item * 1000.0, 3) for item in values],
        "count": len(values),
        "mean_ms": round(mean * 1000.0, 3),
        "median_ms": round(statistics.median(values) * 1000.0, 3),
        "p95_ms": round(percentile(values, 0.95) * 1000.0, 3),
        "min_ms": round(min(values) * 1000.0, 3),
        "max_ms": round(max(values) * 1000.0, 3),
        "stdev_ms": round(stdev * 1000.0, 3),
        "cv": round(cv, 4),
    }


def confidence_from_stats(values: list[float], *, failures: int = 0, minimum_samples: int = 5) -> str:
    if failures:
        return "low"
    if len(values) < minimum_samples:
        return "low"
    summary = summarize_samples(values)
    cv = summary["cv"]
    if cv <= 0.05:
        return "high"
    if cv <= 0.15:
        return "medium"
    return "low"


def classify_error(text: str) -> str:
    lowered = text.lower()
    if "idiagnosticsport 未注册" in lowered:
        return "startup.diagnostics_port_unregistered"
    if "unknown method:" in lowered:
        return "protocol.unsupported_method"
    if "dxf not paused" in lowered or "dxf job not paused" in lowered:
        return "protocol.invalid_state"
    if "request timed out" in lowered or "timeout" in lowered:
        return "transport.timeout"
    if "tcp未连接" in text:
        return "transport.not_connected"
    if "connection failed" in lowered or "connection refused" in lowered:
        return "transport.connection_refused"
    if "gateway executable not found" in lowered:
        return "startup.gateway_missing"
    if "backend startup timeout" in lowered:
        return "startup.backend_timeout"
    return "other"


def free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def wait_for_port(host: str, port: int, timeout: float = 5.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection((host, port), timeout=0.5):
                return True
        except OSError:
            time.sleep(0.05)
    return False


def resolve_production_baseline_from_result(payload: dict[str, Any], *, source: str) -> tuple[dict[str, str] | None, str]:
    try:
        return production_baseline_metadata(payload, source=source), ""
    except Exception as exc:  # noqa: BLE001
        return None, str(exc)


def start_mock_server(port: int) -> subprocess.Popen[str]:
    return subprocess.Popen(
        [
            sys.executable,
            str(MOCK_SERVER),
            "--host",
            "127.0.0.1",
            "--port",
            str(port),
            "--config",
            str(CONFIG_PATH),
            "--no-seed-alarms",
        ],
        cwd=str(MOCK_SERVER.parent),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )


def stop_process(process: subprocess.Popen[str] | None) -> None:
    if process is None:
        return
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=3)
    else:
        try:
            process.communicate(timeout=0.5)
        except subprocess.TimeoutExpired:
            pass


def measure_preprocessing(iterations: int) -> dict[str, Any]:
    dxf_to_pb_durations: list[float] = []
    export_sim_durations: list[float] = []
    combined_durations: list[float] = []
    regression_durations: list[float] = []

    for _ in range(iterations):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            pb_path = temp_root / "rect_diag.pb"
            sim_path = temp_root / "rect_diag.simulation-input.json"

            pb_result = run_command(
                [sys.executable, str(DXF_TO_PB), "--input", str(DXF_INPUT), "--output", str(pb_path)]
            )
            if pb_result.return_code != 0:
                raise RuntimeError(f"dxf_to_pb failed: {pb_result.stderr or pb_result.stdout}")
            dxf_to_pb_durations.append(pb_result.duration_seconds)

            sim_result = run_command(
                [sys.executable, str(EXPORT_SIM_INPUT), "--input", str(pb_path), "--output", str(sim_path)]
            )
            if sim_result.return_code != 0:
                raise RuntimeError(f"export_simulation_input failed: {sim_result.stderr or sim_result.stdout}")
            export_sim_durations.append(sim_result.duration_seconds)
            combined_durations.append(pb_result.duration_seconds + sim_result.duration_seconds)

        regression_result = run_command([sys.executable, str(ENGINEERING_REGRESSION)])
        if regression_result.return_code != 0:
            raise RuntimeError(f"engineering regression failed: {regression_result.stderr or regression_result.stdout}")
        regression_durations.append(regression_result.duration_seconds)

    return {
        "fixture": "shared/contracts/engineering/fixtures/cases/rect_diag/rect_diag.dxf",
        "iterations": iterations,
        "commands": {
            "dxf_to_pb": [sys.executable, str(DXF_TO_PB), "--input", str(DXF_INPUT), "--output", "<temp>/rect_diag.pb"],
            "export_simulation_input": [
                sys.executable,
                str(EXPORT_SIM_INPUT),
                "--input",
                "<temp>/rect_diag.pb",
                "--output",
                "<temp>/rect_diag.simulation-input.json",
            ],
            "engineering_regression": [sys.executable, str(ENGINEERING_REGRESSION)],
        },
        "metrics": {
            "dxf_to_pb": summarize_samples(dxf_to_pb_durations),
            "export_simulation_input": summarize_samples(export_sim_durations),
            "combined_preprocess": summarize_samples(combined_durations),
            "engineering_regression": summarize_samples(regression_durations),
        },
        "confidence": {
            "dxf_to_pb": confidence_from_stats(dxf_to_pb_durations),
            "export_simulation_input": confidence_from_stats(export_sim_durations),
            "combined_preprocess": confidence_from_stats(combined_durations),
            "engineering_regression": confidence_from_stats(regression_durations),
        },
    }


def measure_simulation(iterations: int) -> dict[str, Any]:
    simulated_line_durations: list[float] = []

    for _ in range(iterations):
        simulated_line = run_command([sys.executable, str(SIMULATED_LINE)], timeout=30)
        if simulated_line.return_code != 0:
            raise RuntimeError(f"simulated-line failed: {simulated_line.stderr or simulated_line.stdout}")
        simulated_line_durations.append(simulated_line.duration_seconds)

    return {
        "fixture": "shared/contracts/engineering/fixtures/cases/rect_diag/simulation-input.json",
        "iterations": iterations,
        "commands": {
            "simulated_line": [sys.executable, str(SIMULATED_LINE)],
        },
        "metrics": {
            "simulated_line_regression": summarize_samples(simulated_line_durations),
        },
        "confidence": {
            "simulated_line_regression": confidence_from_stats(simulated_line_durations),
        },
    }


def measure_hmi_startup(iterations: int) -> dict[str, Any]:
    total_durations: list[float] = []
    backend_start_durations: list[float] = []
    backend_ready_durations: list[float] = []
    tcp_connect_durations: list[float] = []
    hardware_init_durations: list[float] = []

    for _ in range(iterations):
        host = "127.0.0.1"
        port = free_port()
        backend = BackendManager(
            host=host,
            port=port,
            launch_spec=GatewayLaunchSpec(
                executable=Path(sys.executable),
                args=(
                    str(MOCK_SERVER),
                    "--host",
                    host,
                    "--port",
                    str(port),
                    "--config",
                    str(CONFIG_PATH),
                    "--no-seed-alarms",
                ),
                cwd=MOCK_SERVER.parent,
            ),
        )
        client = TcpClient(host=host, port=port)
        protocol = CommandProtocol(client)

        total_started = time.perf_counter()
        stage_started = time.perf_counter()
        ok, message = backend.start()
        backend_start_durations.append(time.perf_counter() - stage_started)
        if not ok:
            backend.stop()
            raise RuntimeError(f"backend start failed: {message}")

        stage_started = time.perf_counter()
        ok, message = backend.wait_ready(timeout=5.0)
        backend_ready_durations.append(time.perf_counter() - stage_started)
        if not ok:
            client.disconnect()
            backend.stop()
            raise RuntimeError(f"backend wait_ready failed: {message}")

        stage_started = time.perf_counter()
        connected = client.connect()
        tcp_connect_durations.append(time.perf_counter() - stage_started)
        if not connected:
            client.disconnect()
            backend.stop()
            raise RuntimeError("tcp connect failed")

        stage_started = time.perf_counter()
        ok, message = protocol.connect_hardware()
        hardware_init_durations.append(time.perf_counter() - stage_started)
        if not ok:
            client.disconnect()
            backend.stop()
            raise RuntimeError(f"connect_hardware failed: {message}")

        total_durations.append(time.perf_counter() - total_started)
        client.disconnect()
        backend.stop()

    return {
        "mode": "mock backend via StartupWorker-equivalent sequence",
        "iterations": iterations,
        "fixture": "apps/hmi-app/src/hmi_client/tools/mock_server.py + config/machine/machine_config.ini",
        "sequence_reference": "apps/hmi-app/src/hmi_client/client/startup_sequence.py",
        "metrics": {
            "startup_total": summarize_samples(total_durations),
            "backend_start": summarize_samples(backend_start_durations),
            "backend_wait_ready": summarize_samples(backend_ready_durations),
            "tcp_connect": summarize_samples(tcp_connect_durations),
            "hardware_init": summarize_samples(hardware_init_durations),
        },
        "confidence": {
            "startup_total": confidence_from_stats(total_durations),
            "backend_start": confidence_from_stats(backend_start_durations),
            "backend_wait_ready": confidence_from_stats(backend_ready_durations),
            "tcp_connect": confidence_from_stats(tcp_connect_durations),
            "hardware_init": confidence_from_stats(hardware_init_durations),
        },
    }


def request_duration(client: TcpClient, method: str, params: dict[str, Any] | None = None) -> tuple[float, dict[str, Any]]:
    started = time.perf_counter()
    response = client.send_request(method, params or {})
    return time.perf_counter() - started, response


def measure_tcp_latency(iterations: int) -> dict[str, Any]:
    port = free_port()
    process = start_mock_server(port)
    if not wait_for_port("127.0.0.1", port):
        stop_process(process)
        raise RuntimeError("mock server did not start")

    client = TcpClient(host="127.0.0.1", port=port)
    if not client.connect():
        stop_process(process)
        raise RuntimeError("failed to connect to mock server")

    methods = {
        "ping": {"method": "ping", "params": {}},
        "status": {"method": "status", "params": {}},
        "connect": {"method": "connect", "params": {}},
    }
    latency_samples: dict[str, list[float]] = {key: [] for key in methods}
    failures: dict[str, int] = {key: 0 for key in methods}

    try:
        client.send_request("connect", {})
        for _ in range(5):
            client.send_request("ping", {})

        for name, definition in methods.items():
            for _ in range(iterations):
                duration, response = request_duration(client, definition["method"], definition["params"])
                if "error" in response:
                    failures[name] += 1
                else:
                    latency_samples[name].append(duration)
    finally:
        client.disconnect()
        stop_process(process)

    metrics: dict[str, Any] = {}
    confidence: dict[str, str] = {}
    for name, values in latency_samples.items():
        metrics[name] = {
            **summarize_samples(values),
            "failures": failures[name],
            "failure_rate": round(failures[name] / iterations, 4) if iterations else 0.0,
        }
        confidence[name] = confidence_from_stats(values, failures=failures[name], minimum_samples=20)

    return {
        "mode": "localhost mock TCP server",
        "iterations": iterations,
        "fixture": "apps/hmi-app/src/hmi_client/tools/mock_server.py",
        "metrics": metrics,
        "confidence": confidence,
    }


def measure_reliability(
    mock_flow_iterations: int,
    unsupported_iterations: int,
    hil_iterations: int,
) -> dict[str, Any]:
    flows: list[dict[str, Any]] = []
    error_categories: dict[str, int] = {}
    production_baseline: dict[str, str] = {}

    def record_error(category: str) -> None:
        error_categories[category] = error_categories.get(category, 0) + 1

    port = free_port()
    process = start_mock_server(port)
    if not wait_for_port("127.0.0.1", port):
        stop_process(process)
        raise RuntimeError("mock server did not start for reliability checks")

    client = TcpClient(host="127.0.0.1", port=port)
    if not client.connect():
        stop_process(process)
        raise RuntimeError("failed to connect to mock server for reliability checks")
    protocol = CommandProtocol(client)

    mock_failures = 0
    try:
        for _ in range(mock_flow_iterations):
            cycle_errors: list[str] = []
            if not protocol.ping():
                cycle_errors.append("ping")
            ok, message = protocol.connect_hardware()
            if not ok:
                cycle_errors.append(message or "connect")
            ok, message = protocol.home()
            if not ok:
                cycle_errors.append(message or "home")
            move_ok, _, _ = protocol.move_to(10.0, 5.0, 20.0)
            if not move_ok:
                cycle_errors.append("move")
            artifact_ok, artifact_payload, artifact_error = protocol.dxf_create_artifact(str(DXF_INPUT))
            if not artifact_ok:
                cycle_errors.append(artifact_error or "dxf.artifact.create")
            else:
                artifact_id = str(artifact_payload.get("artifact_id", "")).strip()
                plan_ok, plan_payload, plan_error = protocol.dxf_prepare_plan(
                    artifact_id,
                    speed_mm_s=10.0,
                    dry_run=True,
                    dry_run_speed_mm_s=10.0,
                )
                if not plan_ok:
                    cycle_errors.append(plan_error or "dxf.plan.prepare")
                else:
                    plan_id = str(plan_payload.get("plan_id", "")).strip()
                    plan_fingerprint = str(plan_payload.get("plan_fingerprint", "")).strip()
                    if not plan_id or not plan_fingerprint:
                        cycle_errors.append("dxf.plan.prepare.missing_payload")
                        continue
                    baseline_payload, baseline_error = resolve_production_baseline_from_result(
                        plan_payload,
                        source="dxf.plan.prepare",
                    )
                    if baseline_payload is None:
                        cycle_errors.append(baseline_error or "dxf.plan.prepare.missing_production_baseline")
                    elif not production_baseline:
                        production_baseline = baseline_payload
                    preview_ok, preview_payload, preview_error = protocol.dxf_preview_snapshot(plan_id=plan_id)
                    if not preview_ok:
                        cycle_errors.append(preview_error or "dxf.preview.snapshot")
                    else:
                        snapshot_hash = str(preview_payload.get("snapshot_hash", "")).strip()
                        if not snapshot_hash:
                            cycle_errors.append("dxf.preview.snapshot.missing_hash")
                        else:
                            confirm_ok, _, confirm_error = protocol.dxf_preview_confirm(plan_id, snapshot_hash)
                            if not confirm_ok:
                                cycle_errors.append(confirm_error or "dxf.preview.confirm")
                            else:
                                job_id = ""
                                job_ok, job_payload, job_error = protocol.dxf_start_job(
                                    plan_id,
                                    target_count=1,
                                    plan_fingerprint=plan_fingerprint,
                                )
                                if not job_ok:
                                    cycle_errors.append(job_error or "dxf.job.start")
                                else:
                                    job_id = str(job_payload.get("job_id", "")).strip()
                                    if not job_id:
                                        cycle_errors.append("dxf.job.start.missing_job_id")
                                    else:
                                        time.sleep(0.3)
                                        job_status = protocol.dxf_get_job_status(job_id)
                                        if str(job_status.get("state", "")).strip().lower() not in {
                                            "running",
                                            "paused",
                                            "completed",
                                        }:
                                            cycle_errors.append(f"dxf.job.status:{job_status}")
                                if job_id and not protocol.dxf_job_stop(job_id):
                                    cycle_errors.append("dxf.job.stop")

            if cycle_errors:
                mock_failures += 1
                for item in cycle_errors:
                    record_error(classify_error(item))
    finally:
        client.disconnect()
        stop_process(process)

    flows.append(
        {
            "flow": "mock_startup_and_dxf_cycle",
            "samples": mock_flow_iterations,
            "failures": mock_failures,
            "failure_rate": round(mock_failures / mock_flow_iterations, 4) if mock_flow_iterations else 0.0,
            "error_categories": {},
            "confidence": "high" if mock_failures == 0 and mock_flow_iterations >= 20 else "medium",
        }
    )

    port = free_port()
    process = start_mock_server(port)
    if not wait_for_port("127.0.0.1", port):
        stop_process(process)
        raise RuntimeError("mock server did not restart for unsupported method checks")
    client = TcpClient(host="127.0.0.1", port=port)
    if not client.connect():
        stop_process(process)
        raise RuntimeError("failed to connect for unsupported method checks")

    probe_specs = (
        {
            "flow": "dxf.job.resume_without_pause",
            "method": "dxf.job.resume",
            "expectation": "reject:protocol.invalid_state",
            "expected_error_category": "protocol.invalid_state",
        },
    )
    try:
        for probe in probe_specs:
            failures = 0
            flow_categories: dict[str, int] = {}
            expected_rejections = 0
            unexpected_successes = 0
            for _ in range(unsupported_iterations):
                response = client.send_request(probe["method"], {})
                if "error" in response:
                    category = classify_error(response["error"].get("message", ""))
                    flow_categories[category] = flow_categories.get(category, 0) + 1
                    expected_category = probe["expected_error_category"]
                    if expected_category and category == expected_category:
                        expected_rejections += 1
                    else:
                        failures += 1
                        record_error(category)
                elif probe["expected_error_category"]:
                    failures += 1
                    unexpected_successes += 1
            flow_payload = {
                "flow": probe["flow"],
                "expectation": probe["expectation"],
                "samples": unsupported_iterations,
                "failures": failures,
                "failure_rate": round(failures / unsupported_iterations, 4) if unsupported_iterations else 0.0,
                "error_categories": flow_categories,
                "confidence": "medium" if unsupported_iterations >= 5 else "low",
            }
            if probe["expected_error_category"]:
                flow_payload["expected_rejections"] = expected_rejections
                flow_payload["unexpected_successes"] = unexpected_successes
            flows.append(flow_payload)
    finally:
        client.disconnect()
        stop_process(process)

    hil_failures = 0
    hil_categories: dict[str, int] = {}
    hil_samples: list[float] = []
    for _ in range(hil_iterations):
        result = run_command([sys.executable, str(HARDWARE_SMOKE)], timeout=15)
        hil_samples.append(result.duration_seconds)
        if result.return_code != 0:
            hil_failures += 1
            category = classify_error("\n".join(item for item in (result.stdout, result.stderr) if item))
            hil_categories[category] = hil_categories.get(category, 0) + 1
            record_error(category)

    flows.append(
        {
            "flow": "hardware_smoke_hil",
            "samples": hil_iterations,
            "failures": hil_failures,
            "failure_rate": round(hil_failures / hil_iterations, 4) if hil_iterations else 0.0,
            "error_categories": hil_categories,
            "confidence": "medium" if hil_iterations >= 3 else "low",
            "duration": summarize_samples(hil_samples),
        }
    )

    return {
        "flows": flows,
        "error_categories": dict(sorted(error_categories.items())),
        "production_baseline": production_baseline,
        "notes": [
            "mock_startup_and_dxf_cycle 使用本地 mock server，目标是建立稳定回归基线，而不是替代真实硬件可靠性数据。",
            "hardware_smoke_hil 直接执行 tests/e2e/hardware-in-loop/run_hardware_smoke.py，当前失败被视为已知环境/装配缺口。",
        ],
    }


def environment_snapshot() -> dict[str, Any]:
    return {
        "generated_at": utc_now(),
        "workspace_root": str(ROOT),
        "platform": platform.platform(),
        "python_version": sys.version.splitlines()[0],
        "processor": platform.processor() or "unknown",
        "cpu_count": os.cpu_count(),
        "hostname": platform.node(),
        "cwd": str(ROOT),
    }


def render_markdown(payload: dict[str, Any]) -> str:
    performance = payload["performance"]
    reliability = payload["reliability"]

    def table_line(name: str, summary: dict[str, Any], confidence: str) -> str:
        return (
            f"| {name} | {summary['median_ms']:.3f} | {summary['p95_ms']:.3f} | "
            f"{summary['mean_ms']:.3f} | {summary['count']} | {confidence} |"
        )

    lines = [
        "# Performance And Reliability Baseline",
        "",
        f"- generated_at: `{payload['environment']['generated_at']}`",
        f"- workspace_root: `{payload['environment']['workspace_root']}`",
        f"- baseline_id: `{payload.get('production_baseline', {}).get('baseline_id', '')}`",
        f"- baseline_fingerprint: `{payload.get('production_baseline', {}).get('baseline_fingerprint', '')}`",
        f"- production_baseline_source: `{payload.get('production_baseline', {}).get('production_baseline_source', '')}`",
        f"- platform: `{payload['environment']['platform']}`",
        f"- python_version: `{payload['environment']['python_version']}`",
        f"- cpu_count: `{payload['environment']['cpu_count']}`",
        "",
        "## Performance",
        "",
        "### DXF / Engineering Preprocess",
        "",
        "| Metric | median_ms | p95_ms | mean_ms | samples | confidence |",
        "|---|---:|---:|---:|---:|---|",
        table_line(
            "dxf_to_pb",
            performance["preprocessing"]["metrics"]["dxf_to_pb"],
            performance["preprocessing"]["confidence"]["dxf_to_pb"],
        ),
        table_line(
            "export_simulation_input",
            performance["preprocessing"]["metrics"]["export_simulation_input"],
            performance["preprocessing"]["confidence"]["export_simulation_input"],
        ),
        table_line(
            "combined_preprocess",
            performance["preprocessing"]["metrics"]["combined_preprocess"],
            performance["preprocessing"]["confidence"]["combined_preprocess"],
        ),
        table_line(
            "engineering_regression",
            performance["preprocessing"]["metrics"]["engineering_regression"],
            performance["preprocessing"]["confidence"]["engineering_regression"],
        ),
        "",
        "### Simulation",
        "",
        "| Metric | median_ms | p95_ms | mean_ms | samples | confidence |",
        "|---|---:|---:|---:|---:|---|",
        table_line(
            "simulated_line_regression",
            performance["simulation"]["metrics"]["simulated_line_regression"],
            performance["simulation"]["confidence"]["simulated_line_regression"],
        ),
        "",
        "### HMI Startup",
        "",
        "| Metric | median_ms | p95_ms | mean_ms | samples | confidence |",
        "|---|---:|---:|---:|---:|---|",
        table_line(
            "startup_total",
            performance["hmi_startup"]["metrics"]["startup_total"],
            performance["hmi_startup"]["confidence"]["startup_total"],
        ),
        table_line(
            "backend_start",
            performance["hmi_startup"]["metrics"]["backend_start"],
            performance["hmi_startup"]["confidence"]["backend_start"],
        ),
        table_line(
            "backend_wait_ready",
            performance["hmi_startup"]["metrics"]["backend_wait_ready"],
            performance["hmi_startup"]["confidence"]["backend_wait_ready"],
        ),
        table_line(
            "tcp_connect",
            performance["hmi_startup"]["metrics"]["tcp_connect"],
            performance["hmi_startup"]["confidence"]["tcp_connect"],
        ),
        table_line(
            "hardware_init",
            performance["hmi_startup"]["metrics"]["hardware_init"],
            performance["hmi_startup"]["confidence"]["hardware_init"],
        ),
        "",
        "### TCP Request Latency",
        "",
        "| Metric | median_ms | p95_ms | mean_ms | samples | confidence |",
        "|---|---:|---:|---:|---:|---|",
        table_line(
            "ping",
            performance["tcp_latency"]["metrics"]["ping"],
            performance["tcp_latency"]["confidence"]["ping"],
        ),
        table_line(
            "status",
            performance["tcp_latency"]["metrics"]["status"],
            performance["tcp_latency"]["confidence"]["status"],
        ),
        table_line(
            "connect",
            performance["tcp_latency"]["metrics"]["connect"],
            performance["tcp_latency"]["confidence"]["connect"],
        ),
        "",
        "## Reliability",
        "",
        "| Flow | expectation | samples | failures | failure_rate | confidence | error_categories |",
        "|---|---|---:|---:|---:|---|---|",
    ]

    for flow in reliability["flows"]:
        categories = ", ".join(f"{key}={value}" for key, value in flow["error_categories"].items()) or "none"
        lines.append(
            f"| {flow['flow']} | {flow.get('expectation', 'success')} | {flow['samples']} | {flow['failures']} | "
            f"{flow['failure_rate']:.4f} | {flow['confidence']} | {categories} |"
        )

    lines.extend(
        [
            "",
            "### Error Categories",
            "",
            "| Category | count |",
            "|---|---:|",
        ]
    )
    for category, count in reliability["error_categories"].items():
        lines.append(f"| {category} | {count} |")

    lines.extend(
        [
            "",
            "## Comparability Gaps",
            "",
        ]
    )
    for item in payload["comparability_gaps"]:
        lines.append(f"- {item}")

    lines.extend(
        [
            "",
            "## Monitoring Suggestions",
            "",
        ]
    )
    for item in payload["monitoring_suggestions"]:
        lines.append(f"- {item}")

    return "\n".join(lines) + "\n"


def write_reports(payload: dict[str, Any], report_dir: Path) -> dict[str, str]:
    report_dir.mkdir(parents=True, exist_ok=True)
    day = local_day()
    json_path = report_dir / f"{day}-baseline.json"
    md_path = report_dir / f"{day}-baseline.md"
    latest_json = report_dir / "latest.json"
    latest_md = report_dir / "latest.md"

    json_text = json.dumps(payload, ensure_ascii=False, indent=2)
    md_text = render_markdown(payload)

    json_path.write_text(json_text, encoding="utf-8")
    md_path.write_text(md_text, encoding="utf-8")
    latest_json.write_text(json_text, encoding="utf-8")
    latest_md.write_text(md_text, encoding="utf-8")
    return {
        "json": str(json_path),
        "markdown": str(md_path),
        "latest_json": str(latest_json),
        "latest_markdown": str(latest_md),
    }


def main() -> int:
    args = parse_args()
    reliability = measure_reliability(
        args.mock_flow_iterations,
        args.unsupported_iterations,
        args.hil_iterations,
    )
    payload = {
        "environment": environment_snapshot(),
        "production_baseline": reliability.get("production_baseline", {}),
        "sampling": {
            "preprocess_iterations": args.preprocess_iterations,
            "simulation_iterations": args.simulation_iterations,
            "startup_iterations": args.startup_iterations,
            "tcp_iterations": args.tcp_iterations,
            "mock_flow_iterations": args.mock_flow_iterations,
            "unsupported_iterations": args.unsupported_iterations,
            "hil_iterations": args.hil_iterations,
        },
        "performance": {
            "preprocessing": measure_preprocessing(args.preprocess_iterations),
            "simulation": measure_simulation(args.simulation_iterations),
            "hmi_startup": measure_hmi_startup(args.startup_iterations),
            "tcp_latency": measure_tcp_latency(args.tcp_iterations),
        },
        "reliability": reliability,
        "comparability_gaps": [
            "DXF/工程数据预处理基线只覆盖 canonical fixture rect_diag.dxf，不可直接外推到大尺寸或高实体数工程。",
            "HMI 启动时间基于 mock backend 的 StartupWorker 等价顺序，不包含 Qt 首帧渲染、窗口管理器调度和真实网关初始化。",
            "TCP 响应时间基于 localhost mock server，不可直接与真实控制卡、跨主机网络或 HIL 环境比较。",
            "HIL 可靠性当前只得到已知失败基线；一旦 IDiagnosticsPort 注册问题被修复，历史失败率将不可横向比较。",
        ],
        "monitoring_suggestions": [
            "把 tests/performance/collect_baselines.py 纳入每次重构里程碑或 nightly 执行，并保留 latest.json 与日期版报告。",
            "对 dxf_to_pb、export_simulation_input、simulated_line_regression、startup_total 设置回归阈值，建议以当前中位数上浮 20% 作为首版告警线。",
            "把 protocol.unsupported_method 和 startup.diagnostics_port_unregistered 作为显式错误标签接入后续 CI/验收汇总，避免只看成功/失败。",
            "后续接入真实 HIL 后，新增真实 TCP RTT、真实 HMI 首帧时间、真实硬件 smoke soak failure rate，和 mock 基线分开存档。",
        ],
    }
    written = write_reports(payload, Path(args.report_dir))
    print(json.dumps(written, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
