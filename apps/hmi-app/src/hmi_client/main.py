#!/usr/bin/env python3
"""Siligen HMI Application Entry Point."""
import argparse
import os
import sys
from pathlib import Path

# Add hmi directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from qt_env import configure_qt_environment
from client import normalize_launch_mode
from client.gateway_launch import load_gateway_launch_spec


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Siligen HMI launcher")
    parser.add_argument(
        "--mode",
        choices=("online", "offline"),
        default=normalize_launch_mode(os.getenv("SILIGEN_HMI_MODE", "online")),
        help="Launch mode. Defaults to online.",
    )
    return parser


def _gateway_autostart_enabled() -> bool:
    raw_value = os.getenv("SILIGEN_GATEWAY_AUTOSTART", "1").strip().lower()
    return raw_value not in ("0", "false", "no", "off")


def _validate_online_launch_contract(mode: str) -> None:
    if mode != "online":
        return
    if not _gateway_autostart_enabled():
        return
    if os.getenv("SILIGEN_HMI_OFFICIAL_ENTRYPOINT", "").strip():
        return
    if load_gateway_launch_spec() is not None:
        return

    raise SystemExit(
        "Direct online launch is blocked. Use apps/hmi-app/run.ps1 or set "
        "SILIGEN_GATEWAY_LAUNCH_SPEC."
    )


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    _validate_online_launch_contract(args.mode)
    configure_qt_environment(headless=False)
    from ui.main_window import main as run_ui

    return run_ui(launch_mode=args.mode)

if __name__ == "__main__":
    raise SystemExit(main())
