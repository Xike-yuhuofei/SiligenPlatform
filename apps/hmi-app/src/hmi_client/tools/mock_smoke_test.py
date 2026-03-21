#!/usr/bin/env python3
"""Smoke test for Mock Server and protocol wiring."""
import sys
import time
import tempfile
from pathlib import Path

# Add hmi root to path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from client import TcpClient, CommandProtocol


def main(host: str = "127.0.0.1", port: int = 9527) -> int:
    client = TcpClient(host=host, port=port)
    if not client.connect():
        print("连接失败")
        return 1

    proto = CommandProtocol(client)
    print("ping:", proto.ping())
    print("connect_hardware:", proto.connect_hardware())
    print("status:", proto.get_status())
    print("home:", proto.home())
    jog_ok, jog_msg = proto.jog("X", 1, 5.0)
    print("jog:", jog_ok, jog_msg)
    print("move:", proto.move_to(10.5, -3.2, 20.0))
    print("dispenser_start:", proto.dispenser_start(2, 200, 100))
    print("supply_open:", proto.supply_open())
    with tempfile.NamedTemporaryFile("w", suffix=".dxf", delete=False, encoding="utf-8") as handle:
        handle.write("0\nSECTION\n2\nENTITIES\n0\nENDSEC\n0\nEOF\n")
        sample_dxf_path = handle.name

    print("dxf_load:", proto.dxf_load(sample_dxf_path))
    print("dxf_info:", proto.dxf_get_info())
    artifact_ok, artifact_payload, artifact_error = proto.dxf_create_artifact(sample_dxf_path)
    print("dxf_create_artifact:", artifact_ok, artifact_payload if artifact_ok else artifact_error)
    artifact_id = artifact_payload.get("artifact_id", "") if artifact_ok else ""
    plan_ok, plan_payload, plan_error = proto.dxf_prepare_plan(
        artifact_id,
        10.0,
        dry_run=True,
        dry_run_speed_mm_s=10.0,
    )
    print("dxf_prepare_plan:", plan_ok, plan_payload if plan_ok else plan_error)
    plan_id = plan_payload.get("plan_id", "") if plan_ok else ""
    preview_ok, preview_payload, preview_error = proto.dxf_preview_snapshot(
        plan_id=plan_id,
        max_polyline_points=4000,
    )
    print("dxf_preview_snapshot:", preview_ok, preview_payload if preview_ok else preview_error)
    snapshot_hash = preview_payload.get("snapshot_hash", "") if preview_ok else ""
    confirm_ok, confirm_payload, confirm_error = proto.dxf_preview_confirm(plan_id, snapshot_hash)
    print("dxf_preview_confirm:", confirm_ok, confirm_payload if confirm_ok else confirm_error)
    if not confirm_ok:
        return 1
    print("mock_io_set_door:", client.send_request("mock.io.set", {"door": True}))
    door_block_ok, door_block_payload, door_block_error = proto.dxf_start_job(
        plan_id,
        target_count=1,
        plan_fingerprint=plan_payload.get("plan_fingerprint", "") if plan_ok else "",
    )
    print("dxf_start_job_blocked_by_door:", door_block_ok, door_block_payload if door_block_ok else door_block_error)
    if door_block_ok:
        print("期望安全门打开时启动被后端拒绝")
        return 1
    print("mock_io_clear_door:", client.send_request("mock.io.set", {"door": False}))
    job_ok, job_payload, job_error = proto.dxf_start_job(
        plan_id,
        target_count=2,
        plan_fingerprint=plan_payload.get("plan_fingerprint", "") if plan_ok else "",
    )
    print("dxf_start_job:", job_ok, job_payload if job_ok else job_error)
    if not job_ok:
        return 1
    print(
        "dxf_execute:",
        proto.dxf_execute(10.0, dry_run=True, dry_run_speed_mm_s=10.0, snapshot_hash=snapshot_hash),
    )

    time.sleep(0.5)
    print("dxf_job_status:", proto.dxf_get_job_status(job_payload.get("job_id", "") if job_ok else ""))
    print("dxf_progress:", proto.dxf_get_progress())
    print("alarms:", proto.get_alarms())
    print("clear_alarms:", proto.clear_alarms())

    client.disconnect()
    try:
        Path(sample_dxf_path).unlink(missing_ok=True)
    except OSError:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
