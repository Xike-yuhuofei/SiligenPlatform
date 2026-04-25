from __future__ import annotations

import base64
import socket
import subprocess
import sys
import time
from pathlib import Path
from unittest import SkipTest


THIS_DIR = Path(__file__).resolve().parent
ROOT = THIS_DIR.parents[3]
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
if str(HIL_DIR) not in sys.path:
    sys.path.insert(0, str(HIL_DIR))

from run_real_dxf_machine_dryrun import non_homed_axes, require_safe_for_motion, status_result
from runtime_gateway_harness import (
    TcpJsonClient,
    all_axes_homed_consistently,
    build_process_env,
    ensure_matching_production_baseline,
    home_succeeded,
    prepare_mock_config,
    production_baseline_metadata,
    resolve_default_exe,
    tcp_connect_and_ensure_ready,
    truncate_json,
    wait_gateway_ready,
)


DEFAULT_DXF_FILE = ROOT / "samples" / "dxf" / "rect_diag.dxf"


def _pick_free_port(host: str) -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((host, 0))
        return int(sock.getsockname()[1])


def _wait_until_axes_homed(client: TcpJsonClient, *, timeout_seconds: float) -> dict[str, object]:
    deadline = time.perf_counter() + timeout_seconds
    last_status: dict[str, object] = {}
    while time.perf_counter() < deadline:
        status_response = client.send_request("status", None, timeout_seconds=5.0)
        if "error" in status_response:
            raise AssertionError("status failed after home: " + truncate_json(status_response))

        last_status = status_response
        homed, non_homed, inconsistent = all_axes_homed_consistently(status_response)
        if homed and not inconsistent:
            return status_response
        time.sleep(0.2)

    raise AssertionError(
        "axes not homed before timeout: "
        + truncate_json(
            {
                "non_homed_axes": non_homed_axes(last_status),
                "status": status_result(last_status),
            },
            max_chars=1200,
        )
    )


def test_dxf_prepare_and_start_return_runtime_owned_production_baseline() -> None:
    gateway_exe = resolve_default_exe("siligen_runtime_gateway.exe")
    if not gateway_exe.exists():
        raise SkipTest(f"runtime gateway executable missing: {gateway_exe}")

    host = "127.0.0.1"
    port = _pick_free_port(host)
    temp_config_dir, temp_config = prepare_mock_config(prefix="dxf-production-baseline-")
    process = subprocess.Popen(
        [str(gateway_exe), "--config", str(temp_config), "--port", str(port)],
        cwd=str(ROOT),
        env=build_process_env(gateway_exe),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    client = TcpJsonClient(host, port)

    try:
        ready_status, ready_note = wait_gateway_ready(process, host, port, timeout_seconds=8.0)
        assert ready_status == "passed", ready_note

        client.connect(timeout_seconds=10.0)
        admission = tcp_connect_and_ensure_ready(
            client,
            config_path=temp_config,
            connect_timeout_seconds=10.0,
        )
        assert admission.status == "passed", truncate_json(
            {
                "note": admission.note,
                "connect_params": admission.connect_params,
                "connect_response": admission.connect_response,
                "status_response": admission.status_response,
            }
        )

        status_before = client.send_request("status", None, timeout_seconds=5.0)
        assert "error" not in status_before, truncate_json(status_before)

        if non_homed_axes(status_before):
            home_response = client.send_request("home", None, timeout_seconds=45.0)
            assert home_succeeded(home_response), truncate_json(home_response)
            status_after_home = _wait_until_axes_homed(client, timeout_seconds=45.0)
        else:
            status_after_home = status_before

        require_safe_for_motion(status_after_home, ignore_home_boundaries=True)

        artifact_response = client.send_request(
            "dxf.artifact.create",
            {
                "filename": DEFAULT_DXF_FILE.name,
                "original_filename": DEFAULT_DXF_FILE.name,
                "content_type": "application/dxf",
                "file_content_b64": base64.b64encode(DEFAULT_DXF_FILE.read_bytes()).decode("ascii"),
            },
            timeout_seconds=30.0,
        )
        assert "error" not in artifact_response, truncate_json(artifact_response)
        artifact_result = status_result(artifact_response)
        artifact_id = str(artifact_result.get("artifact_id", "")).strip()
        assert artifact_id, truncate_json(artifact_response)

        plan_response = client.send_request(
            "dxf.plan.prepare",
            {
                "artifact_id": artifact_id,
                "dispensing_speed_mm_s": 10.0,
                "dry_run": False,
                "rapid_speed_mm_s": 10.0,
                "velocity_trace_enabled": False,
            },
            timeout_seconds=30.0,
        )
        assert "error" not in plan_response, truncate_json(plan_response)
        plan_result = status_result(plan_response)
        plan_id = str(plan_result.get("plan_id", "")).strip()
        plan_fingerprint = str(plan_result.get("plan_fingerprint", "")).strip()
        assert plan_id, truncate_json(plan_response)
        assert plan_fingerprint, truncate_json(plan_response)
        assert plan_result["path_quality"]["blocking"] is False, truncate_json(plan_response)
        assert plan_result["path_quality"]["verdict"] == "pass", truncate_json(plan_response)
        plan_baseline = production_baseline_metadata(plan_result, source="dxf.plan.prepare")

        snapshot_response = client.send_request(
            "dxf.preview.snapshot",
            {"plan_id": plan_id},
            timeout_seconds=30.0,
        )
        assert "error" not in snapshot_response, truncate_json(snapshot_response)
        snapshot_result = status_result(snapshot_response)
        snapshot_hash = str(snapshot_result.get("snapshot_hash", "")).strip()
        assert snapshot_hash, truncate_json(snapshot_response)
        assert snapshot_result["path_quality"]["blocking"] is False, truncate_json(snapshot_response)
        assert snapshot_result["path_quality"]["verdict"] == "pass", truncate_json(snapshot_response)

        confirm_response = client.send_request(
            "dxf.preview.confirm",
            {"plan_id": plan_id, "snapshot_hash": snapshot_hash},
            timeout_seconds=15.0,
        )
        assert "error" not in confirm_response, truncate_json(confirm_response)

        job_start_response = client.send_request(
            "dxf.job.start",
            {
                "plan_id": plan_id,
                "plan_fingerprint": plan_fingerprint,
                "target_count": 1,
            },
            timeout_seconds=15.0,
        )
        assert "error" not in job_start_response, truncate_json(job_start_response)
        job_start_result = status_result(job_start_response)
        assert str(job_start_result.get("job_id", "")).strip(), truncate_json(job_start_response)
        assert str(job_start_result.get("plan_id", "")).strip() == plan_id, truncate_json(job_start_response)
        assert (
            str(job_start_result.get("plan_fingerprint", "")).strip() == plan_fingerprint
        ), truncate_json(job_start_response)

        job_start_baseline = production_baseline_metadata(job_start_result, source="dxf.job.start")
        ensure_matching_production_baseline(plan_baseline, job_start_baseline, label="dxf.job.start")
    finally:
        try:
            client.send_request("disconnect", {}, timeout_seconds=5.0)
        except Exception:
            pass
        client.close()
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
        else:
            process.communicate(timeout=5)
        temp_config_dir.cleanup()
