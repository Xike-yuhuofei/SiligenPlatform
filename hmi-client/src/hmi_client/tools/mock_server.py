#!/usr/bin/env python3
"""Siligen HMI TCP Mock Server."""
from __future__ import annotations

import argparse
import configparser
import hashlib
import json
import socketserver
import threading
import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple


@dataclass
class AxisState:
    position: float = 0.0
    velocity: float = 0.0
    enabled: bool = False
    homed: bool = False


@dataclass
class IOState:
    limit_x_pos: bool = False
    limit_x_neg: bool = False
    limit_y_pos: bool = False
    limit_y_neg: bool = False
    estop: bool = False
    door: bool = False


@dataclass
class DxfState:
    loaded: bool = False
    filepath: str = ""
    segment_count: int = 0
    total_length: float = 0.0
    bounds: Dict[str, float] = field(default_factory=lambda: {
        "x_min": 0.0, "x_max": 100.0, "y_min": 0.0, "y_max": 100.0
    })
    running: bool = False
    progress: float = 0.0
    current_segment: int = 0


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


def _load_ini_config(path: Optional[str]) -> Tuple[Dict[str, Dict[str, str]], Optional[str], str]:
    if not path:
        return {}, "config path not set", ""

    parser = configparser.ConfigParser(interpolation=None, strict=False)
    parser.optionxform = str.lower

    try:
        with open(path, "r", encoding="utf-8-sig") as handle:
            parser.read_file(handle)
    except FileNotFoundError:
        return {}, f"config file not found: {path}", ""
    except (OSError, configparser.Error) as exc:
        return {}, f"failed to load config: {exc}", ""

    config = _normalize_config(parser)
    return config, None, _hash_config(config)


class MockState:
    def __init__(self, seed_alarms: bool = True, config_path: Optional[str] = None):
        self._lock = threading.Lock()
        self.hardware_connected = False
        self.axes: Dict[str, AxisState] = {"X": AxisState(), "Y": AxisState()}
        self.io = IOState()
        self.dispenser_valve_open = False
        self.supply_valve_open = False
        self.dxf = DxfState()
        self._alarm_seq = 0
        self.alarms: List[Dict] = []
        if seed_alarms:
            self._add_alarm("WARN", "气压偏低")
            self._add_alarm("INFO", "待机时间过长")
        self._dxf_thread: Optional[threading.Thread] = None
        self._disp_thread: Optional[threading.Thread] = None
        self.config_path = config_path
        self.config: Dict[str, Dict[str, str]] = {}
        self.config_error: Optional[str] = None
        self.config_hash: str = ""
        self.reload_config()

    def reload_config(self) -> None:
        config, error, config_hash = _load_ini_config(self.config_path)
        self.config = config
        self.config_error = error
        self.config_hash = config_hash

    def _config_loaded(self) -> bool:
        return self.config_error is None

    def _add_alarm(self, level: str, message: str) -> Dict:
        self._alarm_seq += 1
        alarm = {"id": f"A{self._alarm_seq:03d}", "level": level, "message": message}
        self.alarms.append(alarm)
        return alarm

    def _start_dxf_progress(self):
        def worker():
            while True:
                with self._lock:
                    if not self.dxf.running:
                        break
                    self.dxf.progress = min(100.0, self.dxf.progress + 2.5)
                    if self.dxf.segment_count > 0:
                        self.dxf.current_segment = int(
                            self.dxf.progress / 100.0 * self.dxf.segment_count
                        )
                    if self.dxf.progress >= 100.0:
                        self.dxf.running = False
                        break
                time.sleep(0.2)

        if self._dxf_thread and self._dxf_thread.is_alive():
            return
        self._dxf_thread = threading.Thread(target=worker, daemon=True)
        self._dxf_thread.start()

    def _start_dispenser_cycle(self, duration_ms: int):
        def worker():
            time.sleep(max(0.0, duration_ms / 1000.0))
            with self._lock:
                self.dispenser_valve_open = False

        if self._disp_thread and self._disp_thread.is_alive():
            return
        self._disp_thread = threading.Thread(target=worker, daemon=True)
        self._disp_thread.start()

    def handle_request(self, method: str, params: Optional[Dict]) -> Dict:
        params = params or {}
        with self._lock:
            if method == "config.snapshot":
                return {
                    "result": {
                        "loaded": self._config_loaded(),
                        "error": self.config_error or "",
                        "hash": self.config_hash,
                        "path": self.config_path or "",
                        "config": self.config,
                    }
                }
            if method == "config.get":
                section = str(params.get("section", "")).strip().lower()
                key = str(params.get("key", "")).strip().lower()
                found = section in self.config and key in self.config[section]
                value = self.config.get(section, {}).get(key, "")
                return {
                    "result": {
                        "loaded": self._config_loaded(),
                        "error": self.config_error or "",
                        "hash": self.config_hash,
                        "path": self.config_path or "",
                        "section": section,
                        "key": key,
                        "found": found,
                        "value": value,
                    }
                }
            if method == "config.hash":
                return {
                    "result": {
                        "loaded": self._config_loaded(),
                        "error": self.config_error or "",
                        "hash": self.config_hash,
                        "path": self.config_path or "",
                    }
                }
            if method == "config.reload":
                self.reload_config()
                return {
                    "result": {
                        "loaded": self._config_loaded(),
                        "error": self.config_error or "",
                        "hash": self.config_hash,
                        "path": self.config_path or "",
                    }
                }
            if method == "ping":
                return {"result": {"pong": True}}
            if method == "connect":
                self.hardware_connected = True
                for axis in self.axes.values():
                    axis.enabled = True
                return {"result": {"connected": True, "message": "Mock hardware connected"}}
            if method == "status":
                machine_state = "Running" if self.dxf.running else "Idle"
                return {
                    "result": {
                        "connected": self.hardware_connected,
                        "machine_state": machine_state,
                        "axes": {
                            name: {
                                "position": axis.position,
                                "velocity": axis.velocity,
                                "enabled": axis.enabled,
                                "homed": axis.homed,
                            }
                            for name, axis in self.axes.items()
                        },
                        "io": {
                            "limit_x_pos": self.io.limit_x_pos,
                            "limit_x_neg": self.io.limit_x_neg,
                            "limit_y_pos": self.io.limit_y_pos,
                            "limit_y_neg": self.io.limit_y_neg,
                            "estop": self.io.estop,
                            "door": self.io.door,
                        },
                        "dispenser": {
                            "valve_open": self.dispenser_valve_open,
                            "supply_open": self.supply_valve_open,
                        },
                    }
                }
            if method == "home":
                axes = params.get("axes")
                targets = axes if axes else list(self.axes.keys())
                for name in targets:
                    if name in self.axes:
                        self.axes[name].position = 0.0
                        self.axes[name].homed = True
                        self.axes[name].velocity = 0.0
                return {"result": {"completed": True, "message": "Homed"}}
            if method == "home.go":
                axes = params.get("axes")
                targets = axes if axes else list(self.axes.keys())
                for name in targets:
                    if name in self.axes:
                        self.axes[name].position = 0.0
                        self.axes[name].velocity = 0.0
                return {"result": {"moving": True, "message": "Go home"}}
            if method == "jog":
                axis = params.get("axis")
                direction = params.get("direction", 0)
                speed = float(params.get("speed", 10.0))
                if axis in self.axes:
                    self.axes[axis].position += direction * 0.1
                    self.axes[axis].velocity = speed
                return {"result": {"ok": True}}
            if method == "move":
                x = float(params.get("x", 0.0))
                y = float(params.get("y", 0.0))
                speed = float(params.get("speed", 10.0))
                self.axes["X"].position = x
                self.axes["Y"].position = y
                self.axes["X"].velocity = speed
                self.axes["Y"].velocity = speed
                return {"result": {"ok": True}}
            if method == "stop":
                for axis in self.axes.values():
                    axis.velocity = 0.0
                self.dxf.running = False
                return {"result": {"ok": True}}
            if method == "estop":
                self.io.estop = True
                self.dxf.running = False
                self._add_alarm("CRIT", "急停触发")
                return {"result": {"motion_stopped": True, "message": "E-Stop"}}
            if method == "dispenser.start":
                count = int(params.get("count", 1))
                interval_ms = int(params.get("interval_ms", 1000))
                duration_ms = int(params.get("duration_ms", 100))
                self.dispenser_valve_open = True
                total_ms = max(0, (count - 1) * interval_ms + duration_ms)
                self._start_dispenser_cycle(total_ms)
                return {"result": {"ok": True}}
            if method == "dispenser.stop":
                self.dispenser_valve_open = False
                return {"result": {"ok": True}}
            if method == "dispenser.pause":
                self.dispenser_valve_open = False
                return {"result": {"ok": True}}
            if method == "dispenser.resume":
                self.dispenser_valve_open = True
                return {"result": {"ok": True}}
            if method == "purge":
                timeout_ms = int(params.get("timeout_ms", 5000))
                self.dispenser_valve_open = True
                self._start_dispenser_cycle(timeout_ms)
                return {"result": {"ok": True}}
            if method == "supply.open":
                self.supply_valve_open = True
                return {"result": {"ok": True}}
            if method == "supply.close":
                self.supply_valve_open = False
                return {"result": {"ok": True}}
            if method == "dxf.load":
                self.dxf.loaded = True
                self.dxf.filepath = params.get("filepath", "")
                self.dxf.segment_count = 120
                self.dxf.total_length = 256.0
                self.dxf.progress = 0.0
                self.dxf.current_segment = 0
                return {"result": {"loaded": True, "segment_count": self.dxf.segment_count}}
            if method == "dxf.execute":
                if not self.dxf.loaded:
                    return {"error": {"code": -32000, "message": "DXF not loaded"}}
                self.dxf.running = True
                self.dxf.progress = 0.0
                self.dxf.current_segment = 0
                self._start_dxf_progress()
                return {"result": {"ok": True}}
            if method == "dxf.stop":
                self.dxf.running = False
                self.dxf.progress = 0.0
                self.dxf.current_segment = 0
                return {"result": {"ok": True}}
            if method == "dxf.info":
                return {
                    "result": {
                        "total_length": self.dxf.total_length,
                        "bounds": self.dxf.bounds,
                    }
                }
            if method == "dxf.progress":
                return {
                    "result": {
                        "running": self.dxf.running,
                        "progress": self.dxf.progress,
                        "current_segment": self.dxf.current_segment,
                        "total_segments": self.dxf.segment_count,
                    }
                }
            if method == "alarms.list":
                return {"result": {"alarms": list(self.alarms)}}
            if method == "alarms.clear":
                self.alarms.clear()
                return {"result": {"ok": True}}
            if method == "alarms.acknowledge":
                alarm_id = params.get("id")
                self.alarms = [a for a in self.alarms if a.get("id") != alarm_id]
                return {"result": {"ok": True}}

        return {"error": {"code": -32601, "message": f"Unknown method: {method}"}}


class MockRequestHandler(socketserver.StreamRequestHandler):
    def handle(self):
        while True:
            try:
                line = self.rfile.readline()
            except ConnectionResetError:
                break
            if not line:
                break
            try:
                req = json.loads(line.decode("utf-8").strip())
            except json.JSONDecodeError:
                continue
            req_id = req.get("id")
            if not req_id:
                continue
            method = req.get("method", "")
            params = req.get("params")
            resp_body = self.server.state.handle_request(method, params)
            resp = {"id": req_id}
            resp.update(resp_body)
            if self.server.verbose:
                print(f"[MockServer] {method} -> {resp_body}")
            try:
                self.wfile.write((json.dumps(resp) + "\n").encode("utf-8"))
            except BrokenPipeError:
                break


class MockTcpServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True

    def __init__(self, server_address, handler_class, state: MockState, verbose: bool):
        super().__init__(server_address, handler_class)
        self.state = state
        self.verbose = verbose


def main():
    parser = argparse.ArgumentParser(description="Siligen HMI TCP Mock Server")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9527)
    parser.add_argument("--config", dest="config_path", default=None)
    parser.add_argument("--no-seed-alarms", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    state = MockState(seed_alarms=not args.no_seed_alarms, config_path=args.config_path)
    server = MockTcpServer((args.host, args.port), MockRequestHandler, state, args.verbose)
    print(f"[MockServer] Listening on {args.host}:{args.port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("[MockServer] Stopped")


if __name__ == "__main__":
    main()
