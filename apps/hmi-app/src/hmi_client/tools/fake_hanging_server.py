#!/usr/bin/env python3
"""Test helper that stays alive without ever listening on the target port."""
from __future__ import annotations

import argparse
import os
import signal
import sys
import threading
import time


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Keep a mock process alive without opening the requested port."
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    stop_event = threading.Event()

    def handle_signal(signum, _frame):
        if args.verbose:
            print(f"[fake-hanging-server] received signal={signum}", flush=True)
        stop_event.set()

    for sig_name in ("SIGTERM", "SIGINT"):
        sig = getattr(signal, sig_name, None)
        if sig is not None:
            signal.signal(sig, handle_signal)

    print(
        f"[fake-hanging-server] alive pid={os.getpid()} host={args.host} port={args.port} mode=not-listening",
        flush=True,
    )
    while not stop_event.wait(1.0):
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
