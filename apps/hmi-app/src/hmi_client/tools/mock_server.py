#!/usr/bin/env python3
"""Siligen HMI TCP Mock Server."""
from __future__ import annotations

import argparse
import configparser
import hashlib
import json
import math
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
    artifact_id: str = ""
    current_plan_id: str = ""
    filepath: str = ""
    segment_count: int = 0
    total_length: float = 0.0
    bounds: Dict[str, float] = field(default_factory=lambda: {
        "x_min": 0.0, "x_max": 100.0, "y_min": 0.0, "y_max": 100.0
    })
    running: bool = False
    paused: bool = False
    awaiting_continue: bool = False
    auto_continue: bool = False
    progress: float = 0.0
    current_segment: int = 0
    preview_snapshot_hash: str = ""
    preview_snapshot_id: str = ""
    preview_generated_at: str = ""
    preview_confirmed_at: str = ""
    preview_state: str = "prepared"
    preview_request_signature: str = ""
    plan_dry_run: bool = False
    plan_speed_mm_s: float = 0.0
    current_job_id: str = ""
    target_count: int = 0
    completed_count: int = 0
    job_dry_run: bool = False


def _build_preview_signature(filepath: str, params: Dict) -> str:
    payload = {
        "filepath": filepath,
        "artifact_id": str(params.get("artifact_id", "")),
        "dry_run": bool(params.get("dry_run", False)),
        "dispensing_speed_mm_s": float(params.get("dispensing_speed_mm_s", 0.0)),
        "dry_run_speed_mm_s": float(params.get("dry_run_speed_mm_s", 0.0)),
        "rapid_speed_mm_s": float(params.get("rapid_speed_mm_s", 0.0)),
        "optimize_path": bool(params.get("optimize_path", False)),
        "start_x": float(params.get("start_x", 0.0)),
        "start_y": float(params.get("start_y", 0.0)),
        "approximate_splines": bool(params.get("approximate_splines", False)),
        "two_opt_iterations": int(params.get("two_opt_iterations", 0)),
        "spline_max_step_mm": float(params.get("spline_max_step_mm", 0.0)),
        "spline_max_error_mm": float(params.get("spline_max_error_mm", 0.0)),
        "arc_tolerance_mm": float(params.get("arc_tolerance_mm", params.get("arc_tolerance", 0.0))),
        "continuity_tolerance_mm": float(
            params.get("continuity_tolerance_mm", params.get("continuity_tolerance", 0.0))
        ),
        "curve_chain_angle_deg": float(params.get("curve_chain_angle_deg", 0.0)),
        "curve_chain_max_segment_mm": float(params.get("curve_chain_max_segment_mm", 0.0)),
        "max_jerk": float(params.get("max_jerk", 0.0)),
        "use_interpolation_planner": bool(params.get("use_interpolation_planner", False)),
        "interpolation_algorithm": int(params.get("interpolation_algorithm", 0)),
    }
    return json.dumps(payload, sort_keys=True, ensure_ascii=True)


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


def _resolve_execution_nominal_time_s(dxf: DxfState) -> float:
    if dxf.plan_speed_mm_s <= 0.0:
        return 0.0
    return dxf.total_length / max(0.1, dxf.plan_speed_mm_s)


def _resolve_execution_owner_span_count(dxf: DxfState) -> int:
    if dxf.segment_count <= 0:
        return 0
    return max(1, math.ceil(dxf.segment_count / 40.0))


def _build_execution_plan_summary(dxf: DxfState) -> Dict[str, object]:
    owner_span_count = _resolve_execution_owner_span_count(dxf)
    point_count = max(dxf.segment_count * 2, 0)
    return {
        "geometry_kind": "path",
        "execution_strategy": "flying_shot",
        "production_trigger_mode": "profile_compare",
        "interpolation_segment_count": dxf.segment_count,
        "interpolation_point_count": point_count,
        "motion_point_count": point_count,
        "trigger_count": dxf.segment_count,
        "owner_span_count": owner_span_count,
        "immediate_only_span_count": 0,
        "future_compare_span_count": owner_span_count,
    }


def _build_execution_budget_breakdown(dxf: DxfState, target_count: int) -> Dict[str, object]:
    normalized_target_count = max(0, int(target_count))
    if normalized_target_count <= 0:
        return {
            "execution_nominal_time_s": 0.0,
            "motion_completion_grace_s": 0.0,
            "owner_span_count": 0,
            "owner_span_overhead_s": 0.0,
            "cycle_budget_s": 0.0,
            "target_count": 0,
            "total_budget_s": 0.0,
        }

    execution_nominal_time_s = _resolve_execution_nominal_time_s(dxf)
    if execution_nominal_time_s <= 0.0:
        motion_completion_grace_s = 5.0
    else:
        motion_completion_grace_s = max(
            5.0,
            min(30.0, math.ceil(execution_nominal_time_s * 1000.0 * 0.15) / 1000.0),
        )
    owner_span_count = _resolve_execution_owner_span_count(dxf)
    owner_span_overhead_s = float(owner_span_count)
    cycle_budget_s = execution_nominal_time_s + motion_completion_grace_s + owner_span_overhead_s
    total_budget_s = cycle_budget_s * normalized_target_count
    return {
        "execution_nominal_time_s": execution_nominal_time_s,
        "motion_completion_grace_s": motion_completion_grace_s,
        "owner_span_count": owner_span_count,
        "owner_span_overhead_s": owner_span_overhead_s,
        "cycle_budget_s": cycle_budget_s,
        "target_count": normalized_target_count,
        "total_budget_s": total_budget_s,
    }


def _build_import_contract(dxf: DxfState) -> Dict[str, object]:
    return {
        "prepared_filepath": dxf.filepath.replace(".dxf", ".pb"),
        "import_result_classification": "success",
        "import_preview_ready": True,
        "import_production_ready": True,
        "formal_compare_gate": None,
        "import_summary": "DXF import succeeded and is ready for production.",
        "import_primary_code": "",
        "import_warning_codes": [],
        "import_error_codes": [],
        "import_resolved_units": "mm",
        "import_resolved_unit_scale": 1.0,
    }


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
        self._recipe_seq = 0
        self.config_path = config_path
        self.config: Dict[str, Dict[str, str]] = {}
        self.config_error: Optional[str] = None
        self.config_hash: str = ""
        self.recipe_schema = {
            "schemaId": "default",
            "entries": [
                {
                    "key": "dispense_speed",
                    "displayName": "Dispense Speed",
                    "type": "float",
                    "required": True,
                    "unit": "mm/s",
                    "constraints": {"min": 0.1, "max": 200.0},
                },
                {
                    "key": "dispense_pressure",
                    "displayName": "Dispense Pressure",
                    "type": "float",
                    "required": True,
                    "unit": "kPa",
                    "constraints": {"min": 0.0, "max": 500.0},
                },
                {
                    "key": "trigger_spatial_interval_mm",
                    "displayName": "Trigger Spatial Interval",
                    "type": "float",
                    "required": True,
                    "unit": "mm",
                    "constraints": {"min": 0.1, "max": 1000.0},
                },
                {
                    "key": "point_flying_direction_mode",
                    "displayName": "Point Flying Direction Mode",
                    "type": "enum",
                    "required": True,
                    "constraints": {"allowedValues": ["approach_direction"]},
                },
                {
                    "key": "material_type",
                    "displayName": "Material Type",
                    "type": "enum",
                    "required": True,
                    "constraints": {"allowedValues": ["epoxy", "silicone", "acrylic"]},
                },
            ],
        }
        self.recipe_templates = [
            {
                "id": "template_demo",
                "name": "Demo Template",
                "description": "Default demo template",
                "parameters": [
                    {"key": "dispense_speed", "value": 120.0},
                    {"key": "dispense_pressure", "value": 220.0},
                    {"key": "trigger_spatial_interval_mm", "value": 3.0},
                    {"key": "point_flying_direction_mode", "value": "approach_direction"},
                    {"key": "material_type", "value": "epoxy"},
                ],
                "createdAt": int(time.time() * 1000),
                "updatedAt": int(time.time() * 1000),
            }
        ]
        self.recipes: List[Dict] = []
        self.recipe_versions_by_recipe: Dict[str, List[Dict]] = {}
        self._seed_recipe()
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

    def _seed_recipe(self) -> None:
        now = int(time.time() * 1000)
        self._recipe_seq += 1
        recipe_id = f"recipe-mock-{self._recipe_seq:03d}"
        version_id = f"version-mock-{self._recipe_seq:03d}"
        version = {
            "id": version_id,
            "recipeId": recipe_id,
            "versionLabel": "v1",
            "status": "published",
            "parameters": [
                dict(item)
                for item in self.recipe_templates[0].get("parameters", [])
                if isinstance(item, dict)
            ],
            "createdAt": now,
            "updatedAt": now,
        }
        self.recipes.append(
            {
                "id": recipe_id,
                "name": "Mock Demo Recipe",
                "description": "Mock recipe for UI smoke",
                "status": "active",
                "tags": ["mock", "demo"],
                "createdAt": now,
                "updatedAt": now,
                "activeVersionId": version_id,
                "versionIds": [version_id],
            }
        )
        self.recipe_versions_by_recipe[recipe_id] = [version]

    def _start_dxf_progress(self):
        def worker():
            while True:
                with self._lock:
                    if not self.dxf.running:
                        break
                    if self.dxf.paused:
                        pass
                    else:
                        self.dxf.progress = min(100.0, self.dxf.progress + 2.5)
                        if self.dxf.segment_count > 0:
                            self.dxf.current_segment = int(
                                self.dxf.progress / 100.0 * self.dxf.segment_count
                            )
                        if self.dxf.progress >= 100.0:
                            self.dxf.completed_count += 1
                            if self.dxf.completed_count >= max(1, self.dxf.target_count):
                                self.dxf.running = False
                                self.dxf.paused = False
                                self.dxf.awaiting_continue = False
                                self.dxf.progress = 100.0
                                break
                            self.dxf.progress = 0.0
                            self.dxf.current_segment = 0
                            if self.dxf.auto_continue:
                                pass
                            else:
                                self.dxf.running = False
                                self.dxf.paused = False
                                self.dxf.awaiting_continue = True
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

    def _interlock_error(self) -> Optional[Dict]:
        if self.io.estop:
            return {"error": {"code": -32011, "message": "emergency stop active"}}
        if self.io.door:
            return {"error": {"code": -32012, "message": "safety door open"}}
        return None

    def _coord_axis_state(self, axis: AxisState) -> int:
        if not axis.enabled:
            return 0
        return 1 if abs(axis.velocity) > 1e-6 else 0

    def _current_dxf_job_state(self) -> str:
        if not self.dxf.current_job_id:
            return "idle"
        if self.dxf.paused:
            return "paused"
        if self.dxf.awaiting_continue:
            return "awaiting_continue"
        if self.dxf.running:
            return "running"
        return "completed"

    def _current_dxf_cycle(self) -> int:
        if not self.dxf.current_job_id:
            return 0
        if self.dxf.running or self.dxf.paused:
            return min(self.dxf.target_count, self.dxf.completed_count + 1)
        if self.dxf.awaiting_continue:
            return min(self.dxf.target_count, max(1, self.dxf.completed_count))
        return min(self.dxf.target_count, self.dxf.completed_count)

    def _motion_coord_status(self, coord_sys: int) -> Dict:
        if not self.hardware_connected:
            return {"error": {"code": 2201, "message": "coord status unavailable: hardware disconnected"}}

        current_velocity = max((abs(axis.velocity) for axis in self.axes.values()), default=0.0)
        any_axis_moving = current_velocity > 1e-6
        has_fault = bool(self.io.estop or self.io.door)
        if has_fault:
            coord_state = 3
        elif self.dxf.paused:
            coord_state = 2
        elif self.dxf.running or any_axis_moving:
            coord_state = 1
        else:
            coord_state = 0

        remaining_segments = 1 if coord_state in (1, 2) else 0
        return {
            "result": {
                "coord_sys": int(coord_sys),
                "state": coord_state,
                "is_moving": bool(coord_state == 1),
                "remaining_segments": remaining_segments,
                "current_velocity": current_velocity,
                "raw_status_word": coord_state,
                "raw_segment": remaining_segments,
                "mc_status_ret": 0,
                "axes": {
                    name: {
                        "position": axis.position,
                        "velocity": axis.velocity,
                        "state": self._coord_axis_state(axis),
                        "enabled": axis.enabled,
                        "homed": axis.homed,
                        "in_position": abs(axis.velocity) <= 1e-6,
                        "has_error": has_fault,
                        "error_code": -32011 if self.io.estop else (-32012 if self.io.door else None),
                        "servo_alarm": False,
                        "following_error": False,
                        "home_failed": False,
                    }
                    for name, axis in self.axes.items()
                },
            }
        }

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
                job_execution_state = self._current_dxf_job_state()
                if not self.hardware_connected:
                    supervision_current_state = "Disconnected"
                    supervision_reason = "hardware_disconnected"
                elif self.io.estop:
                    supervision_current_state = "Estop"
                    supervision_reason = "interlock_estop"
                elif self.io.door:
                    supervision_current_state = "Fault"
                    supervision_reason = "interlock_door_open"
                elif self.dxf.awaiting_continue:
                    supervision_current_state = "Idle"
                    supervision_reason = "job_waiting_continue"
                else:
                    supervision_current_state = "Running" if self.dxf.running else ("Paused" if self.dxf.paused else "Idle")
                    supervision_reason = (
                        "job_running" if self.dxf.running else ("job_paused" if self.dxf.paused else "idle")
                    )
                effective_interlocks = {
                    "estop_active": self.io.estop,
                    "estop_known": True,
                    "door_open_active": self.io.door,
                    "door_open_known": True,
                    "home_boundary_x_active": self.io.limit_x_neg,
                    "home_boundary_y_active": self.io.limit_y_neg,
                    "positive_escape_only_axes": [
                        axis
                        for axis, active in (("X", self.io.limit_x_neg), ("Y", self.io.limit_y_neg))
                        if active
                    ],
                    "sources": {
                        "estop": "mock_io",
                        "door_open": "mock_io",
                        "home_boundary_x": "mock_io_home",
                        "home_boundary_y": "mock_io_home",
                    },
                }
                supervision = {
                    "current_state": supervision_current_state,
                    "requested_state": supervision_current_state,
                    "state_change_in_process": False,
                    "state_reason": supervision_reason,
                    "failure_code": supervision_reason if supervision_current_state in ("Fault", "Estop", "Degraded") else "",
                    "failure_stage": "runtime_status" if supervision_current_state in ("Fault", "Estop", "Degraded") else "",
                    "recoverable": True,
                    "updated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                }
                return {
                    "result": {
                        "connected": self.hardware_connected,
                        "connection_state": "connected" if self.hardware_connected else "disconnected",
                        "supervision": supervision,
                        "interlock_latched": bool(self.io.estop or self.io.door),
                        "job_execution": {
                            "job_id": self.dxf.current_job_id,
                            "plan_id": self.dxf.current_plan_id,
                            "plan_fingerprint": self.dxf.preview_snapshot_hash,
                            "state": job_execution_state,
                            "target_count": self.dxf.target_count,
                            "completed_count": self.dxf.completed_count,
                            "current_cycle": self._current_dxf_cycle(),
                            "current_segment": self.dxf.current_segment,
                            "total_segments": self.dxf.segment_count,
                            "cycle_progress_percent": int(self.dxf.progress),
                            "overall_progress_percent": int(
                                ((self.dxf.completed_count * 100.0) + self.dxf.progress)
                                / max(1, self.dxf.target_count)
                            ) if self.dxf.current_job_id else 0,
                            "elapsed_seconds": 0.0,
                            "execution_budget_s": _build_execution_budget_breakdown(
                                self.dxf,
                                self.dxf.target_count if self.dxf.current_job_id else 0,
                            )["total_budget_s"],
                            "execution_budget_breakdown": _build_execution_budget_breakdown(
                                self.dxf,
                                self.dxf.target_count if self.dxf.current_job_id else 0,
                            ),
                            "error_message": "",
                            "dry_run": self.dxf.job_dry_run,
                        },
                        "axes": {
                            name: {
                                "position": axis.position,
                                "velocity": axis.velocity,
                                "enabled": axis.enabled,
                                "homed": axis.homed,
                                "homing_state": "homed" if axis.homed else "not_homed",
                            }
                            for name, axis in self.axes.items()
                        },
                        "effective_interlocks": effective_interlocks,
                        "io": {
                            "limit_x_pos": self.io.limit_x_pos,
                            "limit_x_neg": self.io.limit_x_neg,
                            "limit_y_pos": self.io.limit_y_pos,
                            "limit_y_neg": self.io.limit_y_neg,
                            "estop": self.io.estop,
                            "estop_known": True,
                            "door": self.io.door,
                            "door_known": True,
                        },
                        "dispenser": {
                            "valve_open": self.dispenser_valve_open,
                            "supply_open": self.supply_valve_open,
                        },
                    }
                }
            if method == "motion.coord.status":
                coord_sys = int(params.get("coord_sys", 1) or 1)
                return self._motion_coord_status(coord_sys)
            if method == "mock.io.set":
                if "estop" in params:
                    self.io.estop = bool(params.get("estop"))
                if "door" in params:
                    self.io.door = bool(params.get("door"))
                if "limit_x_pos" in params:
                    self.io.limit_x_pos = bool(params.get("limit_x_pos"))
                if "limit_x_neg" in params:
                    self.io.limit_x_neg = bool(params.get("limit_x_neg"))
                if "limit_y_pos" in params:
                    self.io.limit_y_pos = bool(params.get("limit_y_pos"))
                if "limit_y_neg" in params:
                    self.io.limit_y_neg = bool(params.get("limit_y_neg"))
                if self.io.estop:
                    self.dxf.running = False
                    self.dxf.paused = False
                    for axis in self.axes.values():
                        axis.velocity = 0.0
                    self.dispenser_valve_open = False
                return {
                    "result": {
                        "estop": self.io.estop,
                        "door": self.io.door,
                        "limit_x_pos": self.io.limit_x_pos,
                        "limit_x_neg": self.io.limit_x_neg,
                        "limit_y_pos": self.io.limit_y_pos,
                        "limit_y_neg": self.io.limit_y_neg,
                    }
                }
            if method == "disconnect":
                self.hardware_connected = False
                for axis in self.axes.values():
                    axis.enabled = False
                    axis.velocity = 0.0
                return {"result": {"disconnected": True}}
            if method == "home":
                interlock_error = self._interlock_error()
                if interlock_error:
                    return interlock_error
                axes = params.get("axes")
                targets = axes if axes else list(self.axes.keys())
                for name in targets:
                    if name in self.axes:
                        self.axes[name].position = 0.0
                        self.axes[name].homed = True
                        self.axes[name].velocity = 0.0
                return {"result": {"completed": True, "message": "Homed"}}
            if method == "home.go":
                interlock_error = self._interlock_error()
                if interlock_error:
                    return interlock_error
                if "speed" in params:
                    return {
                        "error": {
                            "code": -32015,
                            "message": "home.go speed override is not supported; configure ready_zero_speed_mm_s",
                        }
                    }
                axes = params.get("axes")
                targets = axes if axes else list(self.axes.keys())
                for name in targets:
                    if name in self.axes and not self.axes[name].homed:
                        return {"error": {"code": -32013, "message": "axis not homed, run homing first"}}
                for name in targets:
                    if name in self.axes:
                        self.axes[name].position = 0.0
                        self.axes[name].velocity = 0.0
                return {"result": {"moving": True, "message": "Go home"}}
            if method == "home.auto":
                interlock_error = self._interlock_error()
                if interlock_error:
                    return interlock_error
                axes = params.get("axes")
                targets = axes if axes else list(self.axes.keys())
                force = bool(params.get("force", False))
                axis_results = []
                any_executed = False
                for name in targets:
                    axis = self.axes.get(name)
                    if axis is None:
                        continue
                    planned_action = "noop"
                    supervisor_state = "homed_at_zero"
                    success = True
                    executed = False
                    reason_code = "already_at_zero"
                    message = "Already homed at zero"
                    if force or not axis.homed:
                        planned_action = "home"
                        supervisor_state = "not_homed"
                        executed = True
                        success = True
                        axis.homed = True
                        axis.position = 0.0
                        axis.velocity = 0.0
                        reason_code = "not_homed" if not force else "force_rehome"
                        message = "Homing completed" if not force else "Rehomed"
                    elif not math.isclose(axis.position, 0.0, abs_tol=1e-9):
                        planned_action = "go_home"
                        supervisor_state = "homed_away_from_zero"
                        executed = True
                        success = True
                        axis.position = 0.0
                        axis.velocity = 0.0
                        reason_code = "homed_away_from_zero"
                        message = "Moved to zero"
                    any_executed = any_executed or executed
                    axis_results.append(
                        {
                            "axis": name,
                            "supervisor_state": supervisor_state,
                            "planned_action": planned_action,
                            "executed": executed,
                            "success": success,
                            "reason_code": reason_code,
                            "message": message,
                        }
                    )
                return {
                    "result": {
                        "accepted": True,
                        "summary_state": "completed" if any_executed else "noop",
                        "message": "Axes ready at zero" if any_executed else "Axes already homed at zero",
                        "axis_results": axis_results,
                        "total_time_ms": 0,
                    }
                }
            if method == "jog":
                interlock_error = self._interlock_error()
                if interlock_error:
                    return interlock_error
                axis = params.get("axis")
                direction = params.get("direction", 0)
                speed = float(params.get("speed", 10.0))
                if axis in self.axes:
                    if not self.axes[axis].homed:
                        return {"error": {"code": -32013, "message": "axis not homed, run homing first"}}
                    self.axes[axis].position += direction * 0.1
                    self.axes[axis].velocity = speed
                return {"result": {"jogging": True}}
            if method == "move":
                interlock_error = self._interlock_error()
                if interlock_error:
                    return interlock_error
                if not self.axes["X"].homed or not self.axes["Y"].homed:
                    return {"error": {"code": -32013, "message": "axis not homed, run homing first"}}
                x = float(params.get("x", 0.0))
                y = float(params.get("y", 0.0))
                speed = float(params.get("speed", 10.0))
                self.axes["X"].position = x
                self.axes["Y"].position = y
                self.axes["X"].velocity = speed
                self.axes["Y"].velocity = speed
                return {"result": {"moving": True}}
            if method == "stop":
                for axis in self.axes.values():
                    axis.velocity = 0.0
                self.dxf.running = False
                return {"result": {"stopped": True}}
            if method == "estop":
                self.io.estop = True
                self.dxf.running = False
                for axis in self.axes.values():
                    axis.velocity = 0.0
                self.dispenser_valve_open = False
                self._add_alarm("CRIT", "急停触发")
                return {"result": {"motion_stopped": True, "message": "E-Stop"}}
            if method == "estop.reset":
                if not self.io.estop:
                    return {"error": {"code": -32014, "message": "emergency stop not active"}}
                self.io.estop = False
                return {"result": {"reset": True, "message": "E-Stop reset"}}
            if method == "dispenser.start":
                interlock_error = self._interlock_error()
                if interlock_error:
                    return interlock_error
                count = int(params.get("count", 1))
                interval_ms = int(params.get("interval_ms", 1000))
                duration_ms = int(params.get("duration_ms", 100))
                self.dispenser_valve_open = True
                total_ms = max(0, (count - 1) * interval_ms + duration_ms)
                self._start_dispenser_cycle(total_ms)
                return {"result": {"dispensing": True}}
            if method == "dispenser.stop":
                self.dispenser_valve_open = False
                return {"result": {"dispensing": False}}
            if method == "dispenser.pause":
                if self.dxf.running or self.dxf.paused:
                    return {"error": {"code": 2811, "message": "manual dispenser pause is forbidden while DXF job is active"}}
                self.dispenser_valve_open = False
                return {"result": {"paused": True}}
            if method == "dispenser.resume":
                if self.dxf.running or self.dxf.paused:
                    return {"error": {"code": 2821, "message": "manual dispenser resume is forbidden while DXF job is active"}}
                self.dispenser_valve_open = True
                return {"result": {"resumed": True}}
            if method == "purge":
                timeout_ms = int(params.get("timeout_ms", 5000))
                self.dispenser_valve_open = True
                self._start_dispenser_cycle(timeout_ms)
                return {"result": {"purged": True, "timeout_ms": timeout_ms}}
            if method == "supply.open":
                interlock_error = self._interlock_error()
                if interlock_error:
                    return interlock_error
                self.supply_valve_open = True
                return {"result": {"supply_open": True}}
            if method == "supply.close":
                self.supply_valve_open = False
                return {"result": {"supply_open": False}}
            if method == "dxf.artifact.create":
                self.dxf.loaded = True
                self.dxf.artifact_id = f"artifact-{int(time.time() * 1000)}"
                self.dxf.filepath = params.get("filename", "mock.dxf")
                self.dxf.segment_count = 120
                self.dxf.total_length = 256.0
                self.dxf.preview_snapshot_hash = ""
                self.dxf.preview_snapshot_id = ""
                self.dxf.current_plan_id = ""
                self.dxf.preview_generated_at = ""
                self.dxf.preview_confirmed_at = ""
                self.dxf.preview_state = "prepared"
                self.dxf.preview_request_signature = ""
                self.dxf.plan_dry_run = False
                self.dxf.plan_speed_mm_s = 0.0
                self.dxf.current_job_id = ""
                self.dxf.target_count = 0
                self.dxf.completed_count = 0
                self.dxf.job_dry_run = False
                return {
                    "result": {
                        "created": True,
                        "artifact_id": self.dxf.artifact_id,
                        "filepath": self.dxf.filepath,
                        "prepared_filepath": self.dxf.filepath.replace(".dxf", ".pb"),
                        "import_result_classification": "success",
                        "import_preview_ready": True,
                        "import_production_ready": True,
                        "import_summary": "DXF import succeeded and is ready for production.",
                        "import_primary_code": "",
                        "import_warning_codes": [],
                        "import_error_codes": [],
                        "import_resolved_units": "mm",
                        "import_resolved_unit_scale": 1.0,
                        "segment_count": self.dxf.segment_count,
                    }
                }
            if method == "dxf.preview.snapshot":
                if not self.dxf.loaded:
                    return {"error": {"code": -32003, "message": "DXF not loaded"}}
                plan_id = str(params.get("plan_id", self.dxf.current_plan_id)).strip()
                if not plan_id or not self.dxf.current_plan_id or plan_id != self.dxf.current_plan_id:
                    return {"error": {"code": -32009, "message": "plan not prepared"}}

                max_polyline_points = int(params.get("max_polyline_points", 4000))
                if max_polyline_points < 2:
                    max_polyline_points = 2
                generated_at = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
                self.dxf.preview_snapshot_id = plan_id
                self.dxf.preview_generated_at = generated_at
                glue_point_count = max(self.dxf.segment_count * 2, 0)
                execution_point_count = max(glue_point_count, 2)
                glue_points = []
                glue_reveal_lengths_mm = []
                cumulative_length = 0.0
                previous_point = None
                for i in range(glue_point_count):
                    t = i / float(max(glue_point_count - 1, 1))
                    x = t * self.dxf.total_length
                    y = 6.0 * math.sin(t * 4.0 * math.pi)
                    glue_points.append({"x": x, "y": y})
                    if previous_point is not None:
                        cumulative_length += math.hypot(x - previous_point[0], y - previous_point[1])
                    glue_reveal_lengths_mm.append(round(cumulative_length, 6))
                    previous_point = (x, y)
                motion_preview_point_count = min(execution_point_count, max_polyline_points)
                motion_preview = []
                for i in range(motion_preview_point_count):
                    t = i / float(max(motion_preview_point_count - 1, 1))
                    x = t * self.dxf.total_length
                    y = 6.0 * math.sin(t * 4.0 * math.pi)
                    motion_preview.append({"x": x, "y": y})
                self.dxf.preview_state = "snapshot_ready"
                self.dxf.preview_confirmed_at = ""
                return {
                    "result": {
                        "snapshot_id": self.dxf.preview_snapshot_id,
                        "snapshot_hash": self.dxf.preview_snapshot_hash,
                        "plan_id": self.dxf.current_plan_id,
                        "preview_state": self.dxf.preview_state,
                        "preview_source": "planned_glue_snapshot",
                        "preview_kind": "glue_points",
                        "confirmed_at": self.dxf.preview_confirmed_at,
                        "segment_count": self.dxf.segment_count,
                        "point_count": glue_point_count,
                        "glue_point_count": glue_point_count,
                        "glue_points": glue_points,
                        "glue_reveal_lengths_mm": glue_reveal_lengths_mm,
                        "execution_point_count": execution_point_count,
                        "motion_preview": {
                            "source": "execution_trajectory_snapshot",
                            "kind": "polyline",
                            "source_point_count": execution_point_count,
                            "point_count": motion_preview_point_count,
                            "is_sampled": motion_preview_point_count < execution_point_count,
                            "sampling_strategy": (
                                "execution_trajectory_geometry_preserving_clamp"
                                if motion_preview_point_count < execution_point_count
                                else "execution_trajectory_geometry_preserving"
                            ),
                            "polyline": motion_preview,
                        },
                        "total_length_mm": self.dxf.total_length,
                        "estimated_time_s": self.dxf.total_length / max(0.1, self.dxf.plan_speed_mm_s),
                        "generated_at": generated_at,
                    }
                }
            if method == "dxf.preview.confirm":
                plan_id = str(params.get("plan_id", self.dxf.current_plan_id)).strip()
                snapshot_hash = str(params.get("snapshot_hash", "")).strip()
                if not plan_id or not self.dxf.current_plan_id or plan_id != self.dxf.current_plan_id:
                    return {"error": {"code": -32009, "message": "plan not prepared"}}
                if not snapshot_hash:
                    return {"error": {"code": -32005, "message": "Missing snapshot_hash"}}
                if self.dxf.preview_state not in ("snapshot_ready", "confirmed"):
                    return {"error": {"code": -32011, "message": "preview snapshot not prepared"}}
                if snapshot_hash != self.dxf.preview_snapshot_hash:
                    return {"error": {"code": -32006, "message": "Preview snapshot hash mismatch"}}
                self.dxf.preview_state = "confirmed"
                self.dxf.preview_confirmed_at = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
                return {
                    "result": {
                        "confirmed": True,
                        "plan_id": self.dxf.current_plan_id,
                        "snapshot_hash": self.dxf.preview_snapshot_hash,
                        "preview_state": self.dxf.preview_state,
                        "confirmed_at": self.dxf.preview_confirmed_at,
                    }
                }
            if method == "dxf.plan.prepare":
                if not self.dxf.loaded:
                    return {"error": {"code": -32003, "message": "DXF not loaded"}}
                artifact_id = str(params.get("artifact_id", "")).strip()
                if not artifact_id:
                    return {"error": {"code": -32005, "message": "Missing artifact_id"}}
                if artifact_id != self.dxf.artifact_id:
                    return {"error": {"code": -32008, "message": "artifact not found"}}
                speed = float(params.get("dispensing_speed_mm_s", 0.0))
                if speed <= 0:
                    return {"error": {"code": -32004, "message": "Invalid speed_mm_s"}}
                signature = _build_preview_signature(self.dxf.filepath, params)
                seed = f"{signature}|{self.dxf.segment_count}|{self.dxf.total_length:.6f}"
                snapshot_hash = hashlib.sha256(seed.encode("utf-8")).hexdigest()[:16]
                snapshot_id = f"plan-{snapshot_hash}"
                generated_at = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
                self.dxf.preview_snapshot_hash = snapshot_hash
                self.dxf.preview_snapshot_id = snapshot_id
                self.dxf.current_plan_id = snapshot_id
                self.dxf.preview_generated_at = generated_at
                self.dxf.preview_confirmed_at = ""
                self.dxf.preview_state = "prepared"
                self.dxf.preview_request_signature = signature
                self.dxf.plan_dry_run = bool(params.get("dry_run", False))
                self.dxf.plan_speed_mm_s = speed
                point_count = max(self.dxf.segment_count * 2, 0)
                execution_nominal_time_s = _resolve_execution_nominal_time_s(self.dxf)
                return {
                    "result": {
                        "artifact_id": self.dxf.artifact_id,
                        "plan_id": snapshot_id,
                        "plan_fingerprint": snapshot_hash,
                        "segment_count": self.dxf.segment_count,
                        "point_count": point_count,
                        "total_length_mm": self.dxf.total_length,
                        "execution_nominal_time_s": execution_nominal_time_s,
                        "execution_plan_summary": _build_execution_plan_summary(self.dxf),
                        **_build_import_contract(self.dxf),
                        "preview_validation_classification": "pass",
                        "preview_exception_reason": "",
                        "preview_failure_reason": "",
                        "preview_diagnostic_code": "",
                        "generated_at": generated_at,
                    }
                }
            if method == "dxf.job.start":
                interlock_error = self._interlock_error()
                if interlock_error:
                    return interlock_error
                plan_id = str(params.get("plan_id", "")).strip()
                plan_fingerprint = str(params.get("plan_fingerprint", "")).strip()
                if not plan_id or plan_id != self.dxf.current_plan_id:
                    return {"error": {"code": -32009, "message": "plan not prepared"}}
                if not plan_fingerprint or plan_fingerprint != self.dxf.preview_snapshot_hash:
                    return {"error": {"code": -32018, "message": "plan fingerprint mismatch"}}
                if self.dxf.preview_state != "confirmed":
                    return {"error": {"code": -32019, "message": "preview not confirmed"}}
                self.dxf.current_job_id = f"job-{int(time.time() * 1000)}"
                self.dxf.running = True
                self.dxf.paused = False
                self.dxf.awaiting_continue = False
                self.dxf.progress = 0.0
                self.dxf.current_segment = 0
                self.dxf.target_count = max(1, int(params.get("target_count", 1)))
                self.dxf.completed_count = 0
                self.dxf.job_dry_run = self.dxf.plan_dry_run
                self.dxf.auto_continue = bool(params.get("auto_continue", True))
                self._start_dxf_progress()
                execution_budget_breakdown = _build_execution_budget_breakdown(self.dxf, self.dxf.target_count)
                return {
                    "result": {
                        "started": True,
                        "job_id": self.dxf.current_job_id,
                        "plan_id": self.dxf.current_plan_id,
                        "plan_fingerprint": self.dxf.preview_snapshot_hash,
                        "target_count": self.dxf.target_count,
                        "execution_budget_s": execution_budget_breakdown["total_budget_s"],
                        "execution_budget_breakdown": execution_budget_breakdown,
                        **_build_import_contract(self.dxf),
                        "performance_profile": {
                            "execution_cache_hit": False,
                            "execution_joined_inflight": False,
                            "execution_wait_ms": 0,
                            "motion_plan_ms": 0,
                            "assembly_ms": 0,
                            "export_ms": 0,
                            "execution_total_ms": 0,
                        },
                    }
                }
            if method == "dxf.job.status":
                if not self.dxf.current_job_id:
                    return {"error": {"code": -32010, "message": "job not found"}}
                state = self._current_dxf_job_state()
                execution_budget_breakdown = _build_execution_budget_breakdown(self.dxf, self.dxf.target_count)
                return {
                    "result": {
                        "job_id": self.dxf.current_job_id,
                        "plan_id": self.dxf.current_plan_id,
                        "plan_fingerprint": self.dxf.preview_snapshot_hash,
                        "state": state,
                        "target_count": self.dxf.target_count,
                        "completed_count": self.dxf.completed_count,
                        "current_cycle": self._current_dxf_cycle(),
                        "current_segment": self.dxf.current_segment,
                        "total_segments": self.dxf.segment_count,
                        "cycle_progress_percent": int(self.dxf.progress),
                        "overall_progress_percent": int(
                            ((self.dxf.completed_count * 100.0) + self.dxf.progress) / max(1, self.dxf.target_count)
                        ),
                        "elapsed_seconds": 0.0,
                        "execution_budget_s": execution_budget_breakdown["total_budget_s"],
                        "execution_budget_breakdown": execution_budget_breakdown,
                        "error_message": "",
                        "dry_run": self.dxf.job_dry_run,
                    }
                }
            if method == "dxf.job.pause":
                if not self.dxf.running:
                    return {"error": {"code": -32001, "message": "DXF not running"}}
                self.dxf.paused = True
                return {"result": {"paused": True, "job_id": self.dxf.current_job_id}}
            if method == "dxf.job.resume":
                if not self.dxf.current_job_id or self.dxf.awaiting_continue:
                    return {"error": {"code": -32002, "message": "DXF not paused"}}
                interlock_error = self._interlock_error()
                if interlock_error:
                    return interlock_error
                self.dxf.paused = False
                self.dxf.running = True
                self._start_dxf_progress()
                return {"result": {"resumed": True, "job_id": self.dxf.current_job_id}}
            if method == "dxf.job.continue":
                if not self.dxf.current_job_id or not self.dxf.awaiting_continue:
                    return {"error": {"code": -32020, "message": "DXF not waiting for continue"}}
                interlock_error = self._interlock_error()
                if interlock_error:
                    return interlock_error
                self.dxf.awaiting_continue = False
                self.dxf.paused = False
                self.dxf.running = True
                self._start_dxf_progress()
                return {"result": {"continued": True, "job_id": self.dxf.current_job_id}}
            if method in ("dxf.job.stop", "dxf.job.cancel"):
                job_id = self.dxf.current_job_id
                self.dxf.running = False
                self.dxf.paused = False
                self.dxf.awaiting_continue = False
                self.dxf.progress = 0.0
                self.dxf.current_segment = 0
                self.dxf.current_job_id = ""
                self.dxf.completed_count = 0
                self.dxf.target_count = 0
                self.dxf.job_dry_run = False
                self.dxf.auto_continue = False
                if method == "dxf.job.cancel":
                    return {"result": {"cancelled": True, "job_id": job_id, "transition_state": "canceling"}}
                return {"result": {"stopped": True, "job_id": job_id, "transition_state": "stopping"}}
            if method == "dxf.info":
                return {
                    "result": {
                        "total_length": self.dxf.total_length,
                        "bounds": self.dxf.bounds,
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
            if method == "recipe.list":
                return {"result": {"recipes": list(self.recipes)}}
            if method == "recipe.get":
                recipe_id = params.get("recipeId") or params.get("recipe_id")
                recipe = next((item for item in self.recipes if item["id"] == recipe_id), None)
                if recipe is None:
                    return {"error": {"code": -33001, "message": "Recipe not found"}}
                return {"result": {"recipe": dict(recipe)}}
            if method == "recipe.versions":
                recipe_id = str(params.get("recipeId") or params.get("recipe_id") or "").strip()
                recipe = next((item for item in self.recipes if item["id"] == recipe_id), None)
                if recipe is None:
                    return {"error": {"code": -33005, "message": "Recipe not found"}}
                versions = self.recipe_versions_by_recipe.get(recipe_id, [])
                return {"result": {"versions": [dict(version) for version in versions]}}
            if method == "recipe.templates":
                return {"result": {"templates": list(self.recipe_templates)}}
            if method == "recipe.schema.default":
                return {"result": {"schema": dict(self.recipe_schema)}}
            if method == "recipe.create":
                name = str(params.get("name", "")).strip()
                if not name:
                    return {"error": {"code": -33002, "message": "Missing 'name' parameter"}}
                self._recipe_seq += 1
                now = int(time.time() * 1000)
                recipe = {
                    "id": f"recipe-mock-{self._recipe_seq:03d}",
                    "name": name,
                    "description": str(params.get("description", "")),
                    "status": "active",
                    "tags": list(params.get("tags", [])),
                    "createdAt": now,
                    "updatedAt": now,
                    "activeVersionId": f"version-mock-{self._recipe_seq:03d}",
                    "versionIds": [f"version-mock-{self._recipe_seq:03d}"],
                }
                version = {
                    "id": recipe["activeVersionId"],
                    "recipeId": recipe["id"],
                    "versionLabel": "v1",
                    "status": "published",
                    "parameters": [
                        dict(item)
                        for item in self.recipe_templates[0].get("parameters", [])
                        if isinstance(item, dict)
                    ],
                    "createdAt": now,
                    "updatedAt": now,
                }
                self.recipes.append(recipe)
                self.recipe_versions_by_recipe[recipe["id"]] = [version]
                return {"result": {"recipe": dict(recipe)}}
            if method == "recipe.update":
                recipe_id = params.get("recipeId") or params.get("recipe_id")
                recipe = next((item for item in self.recipes if item["id"] == recipe_id), None)
                if recipe is None:
                    return {"error": {"code": -33003, "message": "Recipe not found"}}
                if "name" in params:
                    recipe["name"] = str(params.get("name", ""))
                if "description" in params:
                    recipe["description"] = str(params.get("description", ""))
                if "tags" in params:
                    recipe["tags"] = list(params.get("tags", []))
                recipe["updatedAt"] = int(time.time() * 1000)
                return {"result": {"recipe": dict(recipe)}}
            if method == "recipe.archive":
                recipe_id = params.get("recipeId") or params.get("recipe_id")
                recipe = next((item for item in self.recipes if item["id"] == recipe_id), None)
                if recipe is None:
                    return {"error": {"code": -33004, "message": "Recipe not found"}}
                recipe["status"] = "archived"
                recipe["updatedAt"] = int(time.time() * 1000)
                return {"result": {"archived": True}}

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
