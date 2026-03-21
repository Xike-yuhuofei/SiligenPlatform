"""Command Protocol - High-level API for motion controller commands."""
import base64
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional, Dict
from .tcp_client import TcpClient


@dataclass
class AxisStatus:
    position: float = 0.0
    velocity: float = 0.0
    enabled: bool = False
    homed: bool = False


@dataclass
class IOStatus:
    """IO status for limit switches, e-stop, and door."""
    limit_x_pos: bool = False
    limit_x_neg: bool = False
    limit_y_pos: bool = False
    limit_y_neg: bool = False
    estop: bool = False
    door: bool = False


@dataclass
class MachineStatus:
    connected: bool = False
    machine_state: str = "Unknown"
    axes: Dict[str, AxisStatus] = field(default_factory=dict)
    io: IOStatus = field(default_factory=IOStatus)
    dispenser_valve_open: bool = False
    supply_valve_open: bool = False


class CommandProtocol:
    """High-level command interface for motion controller."""

    def __init__(self, client: TcpClient):
        self._client = client

    def _build_dxf_execute_params(
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
            door=io_data.get("door", False),
        )

        status = MachineStatus(
            connected=result.get("connected", False),
            machine_state=result.get("machine_state", "Unknown"),
            io=io_status,
            dispenser_valve_open=result.get("dispenser", {}).get("valve_open", False),
            supply_valve_open=result.get("dispenser", {}).get("supply_open", False),
        )
        for name, data in result.get("axes", {}).items():
            status.axes[name] = AxisStatus(
                position=data.get("position", 0.0),
                velocity=data.get("velocity", 0.0),
                enabled=data.get("enabled", False),
                homed=data.get("homed", False),
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
        return result.get("completed", False), result.get("message", "")

    def home_go(self, axes: list = None, speed: float = 0.0) -> tuple:
        params = {"axes": axes} if axes else {}
        if speed and speed > 0.0:
            params["speed"] = speed
        resp = self._client.send_request("home.go", params, timeout=30.0)
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        result = resp.get("result", {})
        return result.get("moving", False), result.get("message", "")

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

    def dxf_execute(
        self,
        dispensing_speed_mm_s: float,
        dry_run: bool = False,
        dry_run_speed_mm_s: float = 0.0,
        snapshot_hash: str = "",
        velocity_trace_enabled: bool = True,
        velocity_trace_interval_ms: int = 0,
        velocity_trace_path: str = ""
    ) -> bool:
        params = self._build_dxf_execute_params(
            dispensing_speed_mm_s=dispensing_speed_mm_s,
            dry_run=dry_run,
            dry_run_speed_mm_s=dry_run_speed_mm_s,
            snapshot_hash=snapshot_hash,
            velocity_trace_enabled=velocity_trace_enabled,
            velocity_trace_interval_ms=velocity_trace_interval_ms,
            velocity_trace_path=velocity_trace_path,
        )
        resp = self._client.send_request("dxf.execute", params)
        return "result" in resp

    def dxf_preview_snapshot(
        self,
        speed_mm_s: float,
        dry_run: bool = False,
        dry_run_speed_mm_s: float = 0.0,
    ) -> tuple:
        params = self._build_dxf_execute_params(
            dispensing_speed_mm_s=speed_mm_s,
            dry_run=dry_run,
            dry_run_speed_mm_s=dry_run_speed_mm_s,
            velocity_trace_enabled=False,
        )
        params.pop("snapshot_hash", None)
        resp = self._client.send_request("dxf.preview.snapshot", params, timeout=15.0)
        if "error" in resp:
            return False, {}, resp["error"].get("message", "Unknown error")
        return True, resp.get("result", {}), ""

    def dxf_prepare_plan(
        self,
        artifact_id: str,
        speed_mm_s: float,
        dry_run: bool = False,
        dry_run_speed_mm_s: float = 0.0,
    ) -> tuple:
        params = self._build_dxf_execute_params(
            dispensing_speed_mm_s=speed_mm_s,
            dry_run=dry_run,
            dry_run_speed_mm_s=dry_run_speed_mm_s,
            velocity_trace_enabled=False,
        )
        params["artifact_id"] = artifact_id
        resp = self._client.send_request("dxf.plan.prepare", params, timeout=15.0)
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

    def dxf_stop(self) -> bool:
        resp = self._client.send_request("dxf.stop")
        return "result" in resp

    def dxf_pause(self) -> bool:
        resp = self._client.send_request("dxf.pause")
        return "result" in resp

    def dxf_resume(self) -> bool:
        resp = self._client.send_request("dxf.resume")
        return "result" in resp

    def dxf_get_progress(self) -> dict:
        resp = self._client.send_request("dxf.progress")
        if "error" in resp:
            return {"running": False, "progress": 0, "current_segment": 0, "total_segments": 0}
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
