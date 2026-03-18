#!/usr/bin/env python3
"""Smoke test for Mock Server and protocol wiring."""
import sys
import time
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
    print("dxf_load:", proto.dxf_load("C:/mock/test.dxf"))
    print("dxf_info:", proto.dxf_get_info())
    print("dxf_execute:", proto.dxf_execute(10.0, dry_run=True, dry_run_speed_mm_s=10.0))

    time.sleep(0.5)
    print("dxf_progress:", proto.dxf_get_progress())
    print("alarms:", proto.get_alarms())
    print("clear_alarms:", proto.clear_alarms())

    client.disconnect()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
