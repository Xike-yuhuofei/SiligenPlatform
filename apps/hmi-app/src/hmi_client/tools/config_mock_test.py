#!/usr/bin/env python3
"""Config snapshot test for HMI mock/sim server."""
from __future__ import annotations

import argparse
import configparser
import hashlib
import subprocess
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional, Tuple

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from client import TcpClient


def _normalize_config(parser: configparser.ConfigParser) -> Dict[str, Dict[str, str]]:
    config: Dict[str, Dict[str, str]] = {}
    for section in parser.sections():
        section_key = section.strip().lower()
        config[section_key] = {}
        for key, value in parser.items(section):
            config[section_key][key.strip().lower()] = value.strip()
    return config


def _hash_config(config: Dict[str, Dict[str, str]]) -> str:
    lines: List[str] = []
    for section in sorted(config.keys()):
        for key in sorted(config[section].keys()):
            lines.append(f"{section}.{key}={config[section][key]}")
    payload = "\n".join(lines).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def load_ini_config(path: str) -> Tuple[Dict[str, Dict[str, str]], Optional[str]]:
    if not path:
        return {}, "config path not set"

    parser = configparser.ConfigParser(interpolation=None, strict=False)
    parser.optionxform = str.lower
    try:
        with open(path, "r", encoding="utf-8-sig") as handle:
            parser.read_file(handle)
    except FileNotFoundError:
        return {}, f"config file not found: {path}"
    except (OSError, configparser.Error) as exc:
        return {}, f"failed to load config: {exc}"

    return _normalize_config(parser), None


def start_mock_process(host: str, port: int, config_path: str, verbose: bool) -> subprocess.Popen:
    mock_server_path = Path(__file__).resolve().parent / "mock_server.py"
    cmd = [sys.executable, str(mock_server_path), "--host", host, "--port", str(port)]
    if config_path:
        cmd += ["--config", config_path]
    if verbose:
        cmd.append("--verbose")
    return subprocess.Popen(cmd, cwd=str(mock_server_path.parent))


def wait_for_server(host: str, port: int, timeout_s: float) -> bool:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        client = TcpClient(host=host, port=port)
        if client.connect():
            client.disconnect()
            return True
        time.sleep(0.2)
    return False


def fetch_snapshot(client: TcpClient, timeout: float) -> Dict:
    resp = client.send_request("config.snapshot", timeout=timeout)
    if "error" in resp:
        raise RuntimeError(resp["error"].get("message", "config.snapshot failed"))
    result = resp.get("result", {})
    if not result.get("loaded", False):
        raise RuntimeError(result.get("error", "config not loaded"))
    return result


def compare_configs(expected: Dict[str, Dict[str, str]],
                    actual: Dict[str, Dict[str, str]]) -> Tuple[List[str], List[str]]:
    missing: List[str] = []
    mismatched: List[str] = []
    for section, keys in expected.items():
        if section not in actual:
            missing.append(f"{section}.*")
            continue
        for key, value in keys.items():
            if key not in actual[section]:
                missing.append(f"{section}.{key}")
                continue
            actual_value = str(actual[section][key]).strip()
            if actual_value != value:
                mismatched.append(f"{section}.{key} expected={value} actual={actual_value}")
    return missing, mismatched


def main() -> int:
    parser = argparse.ArgumentParser(description="Config snapshot verification against mock/sim server")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9527)
    parser.add_argument("--config", default=str(Path("config/machine/machine_config.ini")))
    parser.add_argument("--timeout", type=float, default=3.0)
    parser.add_argument("--start-mock", action="store_true")
    parser.add_argument("--mock-verbose", action="store_true")
    parser.add_argument("--wait-timeout", type=float, default=5.0)
    args = parser.parse_args()

    config_path = str(Path(args.config).resolve())
    expected, error = load_ini_config(config_path)
    if error:
        print(error)
        return 2
    expected_hash = _hash_config(expected)

    proc: Optional[subprocess.Popen] = None
    client: Optional[TcpClient] = None
    try:
        if args.start_mock:
            proc = start_mock_process(args.host, args.port, config_path, args.mock_verbose)
            if not wait_for_server(args.host, args.port, args.wait_timeout):
                print("mock server did not start in time")
                return 2

        client = TcpClient(host=args.host, port=args.port)
        if not client.connect():
            print("failed to connect to server")
            return 2

        snapshot = fetch_snapshot(client, args.timeout)
        actual = snapshot.get("config", {})
        remote_hash = snapshot.get("hash", "")
        missing, mismatched = compare_configs(expected, actual)

        total_expected = sum(len(keys) for keys in expected.values())
        total_actual = sum(len(keys) for keys in actual.values())
        matched = total_expected - len(missing) - len(mismatched)

        print(f"expected keys={total_expected} actual keys={total_actual} matched={matched}")
        if remote_hash and remote_hash != expected_hash:
            print("hash mismatch between local config and server snapshot")
            mismatched.append("config.hash mismatch")
        if missing:
            print("missing keys:")
            for item in missing:
                print(f"  {item}")
        if mismatched:
            print("mismatched values:")
            for item in mismatched:
                print(f"  {item}")

        return 0 if not missing and not mismatched else 2
    finally:
        if client:
            client.disconnect()
        if proc:
            proc.terminate()
            try:
                proc.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                proc.kill()


if __name__ == "__main__":
    raise SystemExit(main())
