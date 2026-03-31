"""Command Protocol - High-level API for motion controller commands."""
import base64
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional, Dict

from .gateway_launch import load_gateway_connection_config
from .tcp_client import TcpClient


@dataclass
class AxisStatus:
    position: float = 0.0
    velocity: float = 0.0
    enabled: bool = False
    homed: bool = False
    homing_state: str = "unknown"


@dataclass
class IOStatus:
    """IO status for limit switches, e-stop, and door."""
    limit_x_pos: bool = False
    limit_x_neg: bool = False
    limit_y_pos: bool = False
    limit_y_neg: bool = False
    estop: bool = False
    estop_known: bool = False
    door: bool = False
    door_known: bool = False


@dataclass
class EffectiveInterlocks:
    estop_active: bool = False
    estop_known: bool = False
    door_open_active: bool = False
    door_open_known: bool = False
    home_boundary_x_active: bool = False
    home_boundary_y_active: bool = False
    positive_escape_only_axes: list[str] = field(default_factory=list)
    sources: Dict[str, str] = field(default_factory=dict)


@dataclass
class SupervisionStatus:
    current_state: str = "Unknown"
    requested_state: str = ""
    state_change_in_process: bool = False
    state_reason: str = "unknown"
    failure_code: str = ""
    failure_stage: str = ""
    recoverable: bool = True
    updated_at: str = ""


@dataclass
class MachineStatus:
    connected: bool = False
    connection_state: str = "disconnected"
    machine_state: str = "Unknown"
    machine_state_reason: str = "unknown"
    interlock_latched: bool = False
    active_job_id: str = ""
    active_job_state: str = ""
    axes: Dict[str, AxisStatus] = field(default_factory=dict)
    io: IOStatus = field(default_factory=IOStatus)
    effective_interlocks: EffectiveInterlocks = field(default_factory=EffectiveInterlocks)
    supervision: SupervisionStatus = field(default_factory=SupervisionStatus)
    dispenser_valve_open: bool = False
    supply_valve_open: bool = False

    def gate_estop_known(self) -> bool:
        return bool(self.effective_interlocks.estop_known or self.io.estop_known)

    def gate_estop_active(self) -> bool:
        if self.effective_interlocks.estop_known:
            return bool(self.effective_interlocks.estop_active)
        return bool(self.io.estop)

    def gate_door_known(self) -> bool:
        return bool(self.effective_interlocks.door_open_known or self.io.door_known)

    def gate_door_active(self) -> bool:
        if self.effective_interlocks.door_open_known:
            return bool(self.effective_interlocks.door_open_active)
        return bool(self.io.door)

    def home_boundary_active(self, axis: str) -> bool:
        axis_name = str(axis).strip().upper()
        if axis_name == "X":
            return bool(self.effective_interlocks.home_boundary_x_active)
        if axis_name == "Y":
            return bool(self.effective_interlocks.home_boundary_y_active)
        return False


class CommandProtocol:
    """High-level command interface for motion controller."""

    _DEFAULT_PREVIEW_MAX_POLYLINE_POINTS = 4000
    _DEFAULT_PREVIEW_MAX_GLUE_POINTS = 5000

    def __init__(self, client: TcpClient):
        self._client = client

    def _build_dxf_plan_params(
        self,
        dispensing_speed_mm_s: float,
        dry_run: bool = False,
        dry_run_speed_mm_s: float = 0.0,
        snapshot_hash: str = "",
        velocity_trace_enabled: bool = True,
        velocity_trace_interval_ms: int = 0,
        velocity_trace_path: str = "",
    ) -> dict:
        params = {"dispensing_speed_mm_s": float(dispensing_speed_mm_s), "dry_run": bool(dry_run)}
        params["use_interpolation_planner"] = True
        params["interpolation_algorithm"] = 0
        if dry_run and dry_run_speed_mm_s > 0.0:
            params["dry_run_speed_mm_s"] = float(dry_run_speed_mm_s)
        params["velocity_trace_enabled"] = bool(velocity_trace_enabled)
        if velocity_trace_interval_ms and velocity_trace_interval_ms > 0:
            params["velocity_trace_interval_ms"] = int(velocity_trace_interval_ms)
        if velocity_trace_path:
            params["velocity_trace_path"] = velocity_trace_path
        if snapshot_hash:
            params["snapshot_hash"] = snapshot_hash
        return params

    def ping(self) -> bool:
        resp = self._client.send_request("ping")
        return "result" in resp

    def connect_hardware(self, card_ip: str = "", local_ip: str = "", timeout: float = 15.0) -> tuple:
        params = {}
        if not card_ip or not local_ip:
            connection_config = load_gateway_connection_config()
            if connection_config is not None:
                if not card_ip:
                    card_ip = connection_config.card_ip
                if not local_ip:
                    local_ip = connection_config.local_ip
        if card_ip:
            params["card_ip"] = card_ip
        if local_ip:
            params["local_ip"] = local_ip
        resp = self._client.send_request("connect", params, timeout=timeout)
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        result = resp.get("result", {})
        return result.get("connected", False), result.get("message", "")

    def get_status(self) -> MachineStatus:
        resp = self._client.send_request("status")
        if "error" in resp:
            return MachineStatus()
        result = resp.get("result", {})

        # Parse IO status
        io_data = result.get("io", {})
        io_status = IOStatus(
            limit_x_pos=io_data.get("limit_x_pos", False),
            limit_x_neg=io_data.get("limit_x_neg", False),
            limit_y_pos=io_data.get("limit_y_pos", False),
            limit_y_neg=io_data.get("limit_y_neg", False),
            estop=io_data.get("estop", False),
            estop_known=io_data.get("estop_known", False),
            door=io_data.get("door", False),
            door_known=io_data.get("door_known", False),
        )

        effective_interlocks_data = result.get("effective_interlocks", {})
        effective_interlocks = EffectiveInterlocks(
            estop_active=effective_interlocks_data.get("estop_active", io_status.estop),
            estop_known=effective_interlocks_data.get("estop_known", io_status.estop_known),
            door_open_active=effective_interlocks_data.get("door_open_active", io_status.door),
            door_open_known=effective_interlocks_data.get("door_open_known", io_status.door_known),
            home_boundary_x_active=effective_interlocks_data.get("home_boundary_x_active", False),
            home_boundary_y_active=effective_interlocks_data.get("home_boundary_y_active", False),
            positive_escape_only_axes=list(effective_interlocks_data.get("positive_escape_only_axes", [])),
            sources=dict(effective_interlocks_data.get("sources", {})),
        )

        supervision_data = result.get("supervision", {})
        current_state = str(supervision_data.get("current_state", result.get("machine_state", "Unknown")))
        current_reason = str(supervision_data.get("state_reason", result.get("machine_state_reason", "unknown")))
        supervision = SupervisionStatus(
            current_state=current_state,
            requested_state=str(supervision_data.get("requested_state", current_state)),
            state_change_in_process=bool(supervision_data.get("state_change_in_process", False)),
            state_reason=current_reason,
            failure_code=str(supervision_data.get("failure_code", "") or ""),
            failure_stage=str(supervision_data.get("failure_stage", "") or ""),
            recoverable=bool(supervision_data.get("recoverable", True)),
            updated_at=str(supervision_data.get("updated_at", "")),
        )

        status = MachineStatus(
            connected=result.get("connected", False),
            connection_state=result.get(
                "connection_state", "connected" if result.get("connected", False) else "disconnected"
            ),
            machine_state=current_state,
            machine_state_reason=current_reason,
            interlock_latched=result.get("interlock_latched", False),
            active_job_id=str(result.get("active_job_id", "")),
            active_job_state=str(result.get("active_job_state", "")),
            io=io_status,
            effective_interlocks=effective_interlocks,
            supervision=supervision,
            dispenser_valve_open=result.get("dispenser", {}).get("valve_open", False),
            supply_valve_open=result.get("dispenser", {}).get("supply_open", False),
        )
        for name, data in result.get("axes", {}).items():
            status.axes[name] = AxisStatus(
                position=data.get("position", 0.0),
                velocity=data.get("velocity", 0.0),
                enabled=data.get("enabled", False),
                homed=data.get("homed", False),
                homing_state=str(data.get("homing_state", "unknown")),
            )
        return status

    def home(self, axes: list = None, force: bool = False) -> tuple:
        params = {"axes": axes} if axes else {}
        if force:
            params["force"] = True
        resp = self._client.send_request("home", params, timeout=30.0)
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        result = resp.get("result", {})
        completed = result.get("completed", False)
        message = result.get("message", "")
        if completed:
            return True, message

        axis_results = result.get("axis_results", [])
        failed_axis_messages = []
        for item in axis_results:
            if not isinstance(item, dict) or item.get("success", False):
                continue
            axis = str(item.get("axis", "")).strip()
            axis_message = str(item.get("message", "")).strip() or "Homing failed"
            if axis and not axis_message.startswith(f"{axis}:"):
                failed_axis_messages.append(f"{axis}: {axis_message}")
            else:
                failed_axis_messages.append(axis_message)
        if failed_axis_messages:
            return False, "; ".join(failed_axis_messages)

        error_messages = [str(item) for item in result.get("error_messages", []) if str(item)]
        if error_messages:
            details = "; ".join(error_messages)
            if message and message != "Homing completed with errors":
                return False, f"{message}: {details}"
            return False, details
        return False, message

    def home_go(self, axes: list = None, speed: float = 0.0) -> tuple:
        params = {"axes": axes} if axes else {}
        if speed and speed > 0.0:
            params["speed"] = speed
        resp = self._client.send_request("home.go", params, timeout=30.0)
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        result = resp.get("result", {})
        return result.get("moving", False), result.get("message", "")

    def home_auto(
        self,
        axes: list = None,
        force: bool = False,
        wait_for_completion: bool = True,
        timeout_ms: int = 0,
    ) -> tuple:
        params = {"axes": axes} if axes else {}
        if force:
            params["force"] = True
        if not wait_for_completion:
            params["wait_for_completion"] = False
        if timeout_ms > 0:
            params["timeout_ms"] = int(timeout_ms)
        resp = self._client.send_request("home.auto", params, timeout=30.0)
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")

        result = resp.get("result", {})
        accepted = bool(result.get("accepted", False))
        summary_state = str(result.get("summary_state", ""))
        message = str(result.get("message", "") or "")
        axis_results = result.get("axis_results", [])

        failed_axis_messages = []
        for item in axis_results:
            if not isinstance(item, dict) or item.get("success", False):
                continue
            axis = str(item.get("axis", "")).strip()
            axis_message = str(item.get("message", "")).strip() or "Ready-zero failed"
            if axis and not axis_message.startswith(f"{axis}:"):
                failed_axis_messages.append(f"{axis}: {axis_message}")
            else:
                failed_axis_messages.append(axis_message)

        if accepted and summary_state in ("completed", "noop", "in_progress"):
            return True, message
        if failed_axis_messages:
            return False, "; ".join(failed_axis_messages)
        return False, message or "Ready-zero rejected"

    def jog(self, axis: str, direction: int, speed: float = 10.0) -> tuple[bool, str]:
        resp = self._client.send_request("jog", {"axis": axis, "direction": direction, "speed": speed})
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        result = resp.get("result", {})
        return result.get("jogging", "result" in resp), ""

    def move_to(self, x: float, y: float, speed: float = 10.0) -> bool:
        resp = self._client.send_request("move", {"x": x, "y": y, "speed": speed})
        return "result" in resp

    def stop(self) -> bool:
        resp = self._client.send_request("stop")
        return "result" in resp

    def emergency_stop(self) -> tuple:
        resp = self._client.send_request("estop")
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        result = resp.get("result", {})
        return result.get("motion_stopped", False), result.get("message", "")

    def estop_reset(self) -> tuple:
        resp = self._client.send_request("estop.reset")
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        result = resp.get("result", {})
        return result.get("reset", False), result.get("message", "")

    def mock_io_set(
        self,
        *,
        estop: Optional[bool] = None,
        door: Optional[bool] = None,
        limit_x_neg: Optional[bool] = None,
        limit_y_neg: Optional[bool] = None,
    ) -> tuple:
        params = {}
        if estop is not None:
            params["estop"] = bool(estop)
        if door is not None:
            params["door"] = bool(door)
        if limit_x_neg is not None:
            params["limit_x_neg"] = bool(limit_x_neg)
        if limit_y_neg is not None:
            params["limit_y_neg"] = bool(limit_y_neg)
        return self._call("mock.io.set", params)

    def dispenser_start(self, count: int = 1, interval_ms: int = 1000, duration_ms: int = 15) -> bool:
        resp = self._client.send_request("dispenser.start", {
            "count": count,
            "interval_ms": interval_ms,
            "duration_ms": duration_ms
        })
        return "result" in resp

    def dispenser_stop(self) -> bool:
        resp = self._client.send_request("dispenser.stop")
        return "result" in resp

    def dispenser_pause(self) -> bool:
        resp = self._client.send_request("dispenser.pause")
        return "result" in resp

    def dispenser_resume(self) -> bool:
        resp = self._client.send_request("dispenser.resume")
        return "result" in resp

    def purge(self, timeout_ms: int = 5000) -> bool:
        resp = self._client.send_request("purge", {"timeout_ms": timeout_ms})
        return "result" in resp

    def supply_open(self) -> bool:
        resp = self._client.send_request("supply.open")
        return "result" in resp

    def supply_close(self) -> bool:
        resp = self._client.send_request("supply.close")
        return "result" in resp

    def dxf_load(self, filepath: str, timeout: float = 15.0) -> tuple:
        resp = self._client.send_request("dxf.load", {"filepath": filepath}, timeout=timeout)
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        result = resp.get("result", {})
        return result.get("loaded", False), result.get("segment_count", 0)

    def dxf_create_artifact(self, filepath: str, timeout: float = 30.0) -> tuple:
        try:
            data = Path(filepath).read_bytes()
        except OSError as exc:
            return False, {}, str(exc)
        if not data:
            return False, {}, "DXF文件为空"
        params = {
            "filename": Path(filepath).name,
            "original_filename": Path(filepath).name,
            "content_type": "application/dxf",
            "file_content_b64": base64.b64encode(data).decode("ascii"),
        }
        resp = self._client.send_request("dxf.artifact.create", params, timeout=timeout)
        if "error" in resp:
            return False, {}, resp["error"].get("message", "Unknown error")
        return True, resp.get("result", {}), ""

    def dxf_preview_snapshot(
        self,
        plan_id: str,
        max_polyline_points: int = _DEFAULT_PREVIEW_MAX_POLYLINE_POINTS,
        max_glue_points: int = _DEFAULT_PREVIEW_MAX_GLUE_POINTS,
        timeout: float = 15.0,
    ) -> tuple:
        ok, payload, error, _ = self.dxf_preview_snapshot_with_error_details(
            plan_id=plan_id,
            max_polyline_points=max_polyline_points,
            max_glue_points=max_glue_points,
            timeout=timeout,
        )
        return ok, payload, error

    def dxf_preview_snapshot_with_error_details(
        self,
        plan_id: str,
        max_polyline_points: int = _DEFAULT_PREVIEW_MAX_POLYLINE_POINTS,
        max_glue_points: int = _DEFAULT_PREVIEW_MAX_GLUE_POINTS,
        timeout: float = 15.0,
    ) -> tuple:
        params = {"plan_id": plan_id}
        if max_polyline_points > 0 and max_polyline_points != self._DEFAULT_PREVIEW_MAX_POLYLINE_POINTS:
            params["max_polyline_points"] = int(max_polyline_points)
        if max_glue_points > 0 and max_glue_points != self._DEFAULT_PREVIEW_MAX_GLUE_POINTS:
            params["max_glue_points"] = int(max_glue_points)
        resp = self._client.send_request("dxf.preview.snapshot", params, timeout=timeout)
        if "error" in resp:
            error_payload = resp.get("error", {}) or {}
            error_message = error_payload.get("message", "Unknown error")
            error_code = error_payload.get("code")
            try:
                error_code = int(error_code)
            except (TypeError, ValueError):
                error_code = None
            return False, {}, error_message, error_code
        return True, resp.get("result", {}), "", None

    def dxf_preview_confirm(self, plan_id: str, snapshot_hash: str) -> tuple:
        params = {"plan_id": plan_id, "snapshot_hash": snapshot_hash}
        resp = self._client.send_request("dxf.preview.confirm", params, timeout=15.0)
        if "error" in resp:
            return False, {}, resp["error"].get("message", "Unknown error")
        return True, resp.get("result", {}), ""

    def dxf_prepare_plan(
        self,
        artifact_id: str,
        speed_mm_s: float,
        dry_run: bool = False,
        dry_run_speed_mm_s: float = 0.0,
        timeout: float = 15.0,
    ) -> tuple:
        params = self._build_dxf_plan_params(
            dispensing_speed_mm_s=speed_mm_s,
            dry_run=dry_run,
            dry_run_speed_mm_s=dry_run_speed_mm_s,
            velocity_trace_enabled=False,
        )
        params["artifact_id"] = artifact_id
        resp = self._client.send_request("dxf.plan.prepare", params, timeout=timeout)
        if "error" in resp:
            return False, {}, resp["error"].get("message", "Unknown error")
        return True, resp.get("result", {}), ""

    def dxf_start_job(self, plan_id: str, target_count: int = 1, plan_fingerprint: str = "") -> tuple:
        params = {"plan_id": plan_id, "target_count": max(1, int(target_count))}
        if plan_fingerprint:
            params["plan_fingerprint"] = plan_fingerprint
        resp = self._client.send_request("dxf.job.start", params, timeout=15.0)
        if "error" in resp:
            return False, {}, resp["error"].get("message", "Unknown error")
        return True, resp.get("result", {}), ""

    def dxf_get_job_status(self, job_id: str = "") -> dict:
        params = {"job_id": job_id} if job_id else {}
        resp = self._client.send_request("dxf.job.status", params)
        if "error" in resp:
            return {"state": "unknown", "error_message": resp["error"].get("message", "Unknown error")}
        return resp.get("result", {})

    def dxf_job_pause(self, job_id: str = "") -> bool:
        params = {"job_id": job_id} if job_id else {}
        resp = self._client.send_request("dxf.job.pause", params)
        return "result" in resp

    def dxf_job_resume(self, job_id: str = "") -> bool:
        params = {"job_id": job_id} if job_id else {}
        resp = self._client.send_request("dxf.job.resume", params)
        return "result" in resp

    def dxf_job_stop(self, job_id: str = "") -> bool:
        params = {"job_id": job_id} if job_id else {}
        resp = self._client.send_request("dxf.job.stop", params)
        return "result" in resp

    def dxf_get_info(self) -> dict:
        """Get DXF file info including bounding box and total length."""
        resp = self._client.send_request("dxf.info")
        if "error" in resp:
            return {}
        return resp.get("result", {})

    def get_alarms(self) -> list:
        resp = self._client.send_request("alarms.list")
        if "error" in resp:
            return []
        return resp.get("result", {}).get("alarms", [])

    def clear_alarms(self) -> bool:
        resp = self._client.send_request("alarms.clear")
        return "result" in resp

    def acknowledge_alarm(self, alarm_id: str) -> bool:
        resp = self._client.send_request("alarms.acknowledge", {"id": alarm_id})
        return "result" in resp

    def _call(self, method: str, params: dict = None, result_key: str = None, default=None) -> tuple:
        resp = self._client.send_request(method, params or {})
        if "error" in resp:
            return False, default, resp["error"].get("message", "Unknown error")
        result = resp.get("result", {})
        if result_key:
            return True, result.get(result_key, default), ""
        return True, result, ""

    def recipe_list(self, status: str = "", query: str = "", tag: str = "") -> tuple:
        params = {}
        if status:
            params["status"] = status
        if query:
            params["query"] = query
        if tag:
            params["tag"] = tag
        return self._call("recipe.list", params, "recipes", [])

    def recipe_get(self, recipe_id: str) -> tuple:
        return self._call("recipe.get", {"recipeId": recipe_id}, "recipe", {})

    def recipe_templates(self) -> tuple:
        return self._call("recipe.templates", {}, "templates", [])

    def recipe_schema_default(self) -> tuple:
        return self._call("recipe.schema.default", {}, "schema", {})

    def recipe_create(self, name: str, description: str = "", tags: list = None, actor: str = "") -> tuple:
        params = {"name": name}
        if description:
            params["description"] = description
        if tags is not None:
            params["tags"] = tags
        if actor:
            params["actor"] = actor
        return self._call("recipe.create", params, "recipe", {})

    def recipe_update(self, recipe_id: str, name: str = None, description: str = None,
                      tags: list = None, actor: str = "") -> tuple:
        params = {"recipeId": recipe_id}
        if name is not None:
            params["name"] = name
        if description is not None:
            params["description"] = description
        if tags is not None:
            params["tags"] = tags
        if actor:
            params["actor"] = actor
        return self._call("recipe.update", params, "recipe", {})

    def recipe_archive(self, recipe_id: str, actor: str = "") -> tuple:
        params = {"recipeId": recipe_id}
        if actor:
            params["actor"] = actor
        return self._call("recipe.archive", params, "archived", False)

    def recipe_draft_create(self, recipe_id: str, template_id: str, base_version_id: str = "",
                            version_label: str = "", change_note: str = "", actor: str = "") -> tuple:
        params = {
            "recipeId": recipe_id,
            "templateId": template_id
        }
        if base_version_id:
            params["baseVersionId"] = base_version_id
        if version_label:
            params["versionLabel"] = version_label
        if change_note:
            params["changeNote"] = change_note
        if actor:
            params["actor"] = actor
        return self._call("recipe.draft.create", params, "version", {})

    def recipe_draft_update(self, recipe_id: str, version_id: str, parameters: list,
                            change_note: str = "", actor: str = "") -> tuple:
        params = {
            "recipeId": recipe_id,
            "versionId": version_id,
            "parameters": parameters
        }
        if change_note:
            params["changeNote"] = change_note
        if actor:
            params["actor"] = actor
        return self._call("recipe.draft.update", params, "version", {})

    def recipe_publish(self, recipe_id: str, version_id: str, actor: str = "") -> tuple:
        params = {"recipeId": recipe_id, "versionId": version_id}
        if actor:
            params["actor"] = actor
        return self._call("recipe.publish", params, "version", {})

    def recipe_versions(self, recipe_id: str) -> tuple:
        return self._call("recipe.versions", {"recipeId": recipe_id}, "versions", [])

    def recipe_version_create(self, recipe_id: str, base_version_id: str = "", version_label: str = "",
                              change_note: str = "", actor: str = "") -> tuple:
        params = {"recipeId": recipe_id}
        if base_version_id:
            params["baseVersionId"] = base_version_id
        if version_label:
            params["versionLabel"] = version_label
        if change_note:
            params["changeNote"] = change_note
        if actor:
            params["actor"] = actor
        return self._call("recipe.version.create", params, "version", {})

    def recipe_compare(self, recipe_id: str, base_version_id: str, version_id: str) -> tuple:
        params = {
            "recipeId": recipe_id,
            "baseVersionId": base_version_id,
            "versionId": version_id
        }
        return self._call("recipe.version.compare", params, "changes", [])

    def recipe_activate(self, recipe_id: str, version_id: str, actor: str = "") -> tuple:
        params = {"recipeId": recipe_id, "versionId": version_id}
        if actor:
            params["actor"] = actor
        return self._call("recipe.version.activate", params, "activated", False)

    def recipe_audit(self, recipe_id: str, version_id: str = "") -> tuple:
        params = {"recipeId": recipe_id}
        if version_id:
            params["versionId"] = version_id
        return self._call("recipe.audit", params, "records", [])

    def recipe_export(self, recipe_id: str, output_path: str = "", actor: str = "") -> tuple:
        params = {"recipeId": recipe_id}
        if output_path:
            params["outputPath"] = output_path
        if actor:
            params["actor"] = actor
        if output_path:
            return self._call("recipe.export", params, "outputPath", "")
        return self._call("recipe.export", params, None, {})

    def recipe_import(self, bundle_json: str = "", bundle_path: str = "", resolution: str = "",
                      dry_run: bool = False, actor: str = "") -> tuple:
        params = {}
        if bundle_json:
            params["bundleJson"] = bundle_json
        if bundle_path:
            params["bundlePath"] = bundle_path
        if resolution:
            params["resolution"] = resolution
        if dry_run:
            params["dryRun"] = True
        if actor:
            params["actor"] = actor
        return self._call("recipe.import", params, None, {})
