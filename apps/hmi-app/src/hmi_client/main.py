#!/usr/bin/env python3
"""Siligen HMI Application Entry Point."""
import argparse
import os
import sys
from pathlib import Path

# Add hmi directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from client import normalize_launch_mode
from ui.main_window import main as run_ui


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Siligen HMI launcher")
    parser.add_argument(
        "--mode",
        choices=("online", "offline"),
        default=normalize_launch_mode(os.getenv("SILIGEN_HMI_MODE", "online")),
        help="Launch mode. Defaults to online.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    return run_ui(launch_mode=args.mode)

if __name__ == "__main__":
    raise SystemExit(main())
