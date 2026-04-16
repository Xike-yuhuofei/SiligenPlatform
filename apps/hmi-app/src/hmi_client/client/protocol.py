"""Command Protocol - High-level API for motion controller commands."""
# pyright: reportUnknownVariableType=false, reportUnknownMemberType=false, reportUnknownArgumentType=false, reportUnknownParameterType=false, reportMissingTypeArgument=false
import base64
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, Mapping, Optional, Protocol, cast

from .gateway_launch import load_gateway_connection_config

DEFAULT_HOMING_TIMEOUT_MS = 80000
HOMING_RPC_GRACE_S = 10.0
JsonDict = dict[str, Any]


class RpcClientLike(Protocol):
    def send_request(
        self,
        method: str,
        params: Mapping[str, object] | None = None,
        timeout: float = 5.0,
    ) -> JsonDict: ...


def _as_dict(value: object) -> JsonDict:
    return cast(JsonDict, value) if isinstance(value, dict) else {}


def _as_list(value: object) -> list[object]:
    return cast(list[object], value) if isinstance(value, list) else []


def _resolve_homing_rpc_timeout_s(timeout_ms: int = 0) -> float:
    effective_timeout_ms = int(timeout_ms) if timeout_ms and timeout_ms > 0 else DEFAULT_HOMING_TIMEOUT_MS
    return max(30.0, effective_timeout_ms / 1000.0 + HOMING_RPC_GRACE_S)


def _coord_state_label(value: int) -> str:
    if value == 0:
        return "idle"
    if value == 1:
        return "moving"
    if value == 2:
        return "paused"
    if value == 3:
        return "error"
    return "unknown"


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

    @property
    def runtime_state(self) -> str:
        current = str(self.supervision.current_state or "").strip()
        return current or self.machine_state

    @property
    def runtime_state_reason(self) -> str:
        reason = str(self.supervision.state_reason or "").strip()
        return reason or self.machine_state_reason

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


@dataclass
class JobTransitionResult:
    accepted: bool = False
    transition_state: str = ""
    job_id: str = ""
    message: str = ""
    error_code: int | None = None


@dataclass
class MotionCoordAxisStatus:
    position: float = 0.0
    velocity: float = 0.0
    state: int | None = None
    enabled: bool = False
    homed: bool = False
    in_position: bool = False
    has_error: bool = False
    error_code: int | None = None
    servo_alarm: bool = False
    following_error: bool = False
    home_failed: bool = False


@dataclass
class MotionCoordStatus:
    coord_sys: int = 1
    state: int = 0
    state_label: str = "idle"
    is_moving: bool = False
    remaining_segments: int = 0
    current_velocity: float = 0.0
    raw_status_word: int = 0
    raw_segment: int = 0
    mc_status_ret: int = 0
    available: bool = False
    error_message: str = ""
    error_code: int | None = None
    axes: Dict[str, MotionCoordAxisStatus] = field(default_factory=dict)


class CommandProtocol:
    """High-level command interface for motion controller."""

    _DEFAULT_PREVIEW_MAX_POLYLINE_POINTS = 4000
    _DEFAULT_PREVIEW_MAX_GLUE_POINTS = 5000

    def __init__(self, client: RpcClientLike):
        self._client = client

    def _resolve_action_result(
        self,
        resp: JsonDict,
        *,
        missing_result_message: str = "响应缺少 result 字段",
    ) -> tuple[bool, str, int | None]:
        if "error" in resp:
            error_payload = _as_dict(resp.get("error"))
            message = str(error_payload.get("message", "") or "Unknown error")
            code_raw = error_payload.get("code")
            if code_raw is None:
                error_code = None
            else:
                try:
                    error_code = int(code_raw)
                except (TypeError, ValueError):
                    error_code = None
            return False, message, error_code

        if "result" not in resp:
            return False, missing_result_message, None

        return True, "", None

    def _resolve_job_transition_result(
        self,
        resp: JsonDict,
        *,
        accepted_key: str,
        default_transition_state: str,
        missing_result_message: str = "响应缺少 result 字段",
    ) -> JobTransitionResult:
        if "error" in resp:
            error_payload = _as_dict(resp.get("error"))
            message = str(error_payload.get("message", "") or "Unknown error")
            code_raw = error_payload.get("code")
            if code_raw is None:
                error_code = None
            else:
                try:
                    error_code = int(code_raw)
                except (TypeError, ValueError):
                    error_code = None
            return JobTransitionResult(message=message, error_code=error_code)

        if "result" not in resp:
            return JobTransitionResult(message=missing_result_message)

        result = _as_dict(resp.get("result"))
        return JobTransitionResult(
            accepted=bool(result.get(accepted_key, False)),
            transition_state=str(result.get("transition_state", default_transition_state) or default_transition_state),
            job_id=str(result.get("job_id", "") or ""),
            message=str(result.get("message", "") or ""),
        )

    def _build_dxf_plan_params(
        self,
        dispensing_speed_mm_s: float,
        dry_run: bool = False,
        dry_run_speed_mm_s: float = 0.0,
        snapshot_hash: str = "",
        velocity_trace_enabled: bool = True,
        velocity_trace_interval_ms: int = 0,
        velocity_trace_path: str = "",
    ) -> dict[str, object]:
        params: dict[str, object] = {
            "dispensing_speed_mm_s": float(dispensing_speed_mm_s),
            "dry_run": bool(dry_run),
        }
        params["optimize_path"] = True
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

    def connect_hardware(self, card_ip: str = "", local_ip: str = "", timeout: float = 15.0) -> tuple[bool, str]:
        params: dict[str, object] = {}
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
        result = _as_dict(resp.get("result"))
        return result.get("connected", False), result.get("message", "")

    def get_status(self) -> MachineStatus:
        resp = self._client.send_request("status")
        if "error" in resp:
            return MachineStatus()
        result = _as_dict(resp.get("result"))

        # Parse IO status
        io_data = _as_dict(result.get("io"))
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

        effective_interlocks_data = _as_dict(result.get("effective_interlocks"))
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

        compat_state = str(result.get("machine_state", "Unknown"))
        compat_reason = str(result.get("machine_state_reason", "unknown"))
        supervision_data: object = result.get("supervision")
        if not isinstance(supervision_data, dict):
            supervision_data = {
                "current_state": compat_state,
                "requested_state": compat_state,
                "state_change_in_process": False,
                "state_reason": compat_reason,
                "failure_code": "",
                "failure_stage": "",
                "recoverable": True,
                "updated_at": "",
            }

        supervision_payload = _as_dict(supervision_data)
        current_state = str(supervision_payload.get("current_state", "Unknown") or "Unknown")
        current_reason = str(supervision_payload.get("state_reason", "unknown") or "unknown")
        supervision = SupervisionStatus(
            current_state=current_state,
            requested_state=str(supervision_payload.get("requested_state", current_state) or current_state),
            state_change_in_process=bool(supervision_payload.get("state_change_in_process", False)),
            state_reason=current_reason,
            failure_code=str(supervision_payload.get("failure_code", "") or ""),
            failure_stage=str(supervision_payload.get("failure_stage", "") or ""),
            recoverable=bool(supervision_payload.get("recoverable", True)),
            updated_at=str(supervision_payload.get("updated_at", "")),
        )

        status = MachineStatus(
            connected=result.get("connected", False),
            connection_state=result.get(
                "connection_state", "connected" if result.get("connected", False) else "disconnected"
            ),
            machine_state=compat_state,
            machine_state_reason=compat_reason,
            interlock_latched=result.get("interlock_latched", False),
            active_job_id=str(result.get("active_job_id", "")),
            active_job_state=str(result.get("active_job_state", "")),
            io=io_status,
            effective_interlocks=effective_interlocks,
            supervision=supervision,
            dispenser_valve_open=_as_dict(result.get("dispenser")).get("valve_open", False),
            supply_valve_open=_as_dict(result.get("dispenser")).get("supply_open", False),
        )
        axes_payload = _as_dict(result.get("axes"))
        for name, data in axes_payload.items():
            axis_data = _as_dict(data)
            status.axes[name] = AxisStatus(
                position=float(axis_data.get("position", 0.0)),
                velocity=float(axis_data.get("velocity", 0.0)),
                enabled=bool(axis_data.get("enabled", False)),
                homed=bool(axis_data.get("homed", False)),
                homing_state=str(axis_data.get("homing_state", "unknown")),
            )
        return status

    def get_motion_coord_status(self, coord_sys: int = 1) -> MotionCoordStatus:
        resp = self._client.send_request("motion.coord.status", {"coord_sys": int(coord_sys)})
        if "error" in resp:
            error_payload = _as_dict(resp.get("error"))
            message = str(error_payload.get("message", "") or "Unknown error")
            code_raw = error_payload.get("code")
            error_code = None
            if code_raw is not None:
                try:
                    error_code = int(code_raw)
                except (TypeError, ValueError):
                    error_code = None
            return MotionCoordStatus(
                coord_sys=int(coord_sys),
                available=False,
                error_message=message,
                error_code=error_code,
            )

        if "result" not in resp:
            return MotionCoordStatus(
                coord_sys=int(coord_sys),
                available=False,
                error_message="响应缺少 result 字段",
            )

        result = _as_dict(resp.get("result"))
        state_value = int(result.get("state", 0) or 0)
        status = MotionCoordStatus(
            coord_sys=int(result.get("coord_sys", coord_sys) or coord_sys),
            state=state_value,
            state_label=_coord_state_label(state_value),
            is_moving=bool(result.get("is_moving", False)),
            remaining_segments=int(result.get("remaining_segments", 0) or 0),
            current_velocity=float(result.get("current_velocity", 0.0) or 0.0),
            raw_status_word=int(result.get("raw_status_word", 0) or 0),
            raw_segment=int(result.get("raw_segment", 0) or 0),
            mc_status_ret=int(result.get("mc_status_ret", 0) or 0),
            available=True,
        )
        axes_payload = _as_dict(result.get("axes"))
        for name, data in axes_payload.items():
            axis_data = _as_dict(data)
            error_code_raw = axis_data.get("error_code")
            axis_error_code = None
            if error_code_raw is not None:
                try:
                    axis_error_code = int(error_code_raw)
                except (TypeError, ValueError):
                    axis_error_code = None
            state_raw = axis_data.get("state")
            axis_state = None
            if state_raw is not None:
                try:
                    axis_state = int(state_raw)
                except (TypeError, ValueError):
                    axis_state = None
            status.axes[name] = MotionCoordAxisStatus(
                position=float(axis_data.get("position", 0.0) or 0.0),
                velocity=float(axis_data.get("velocity", 0.0) or 0.0),
                state=axis_state,
                enabled=bool(axis_data.get("enabled", False)),
                homed=bool(axis_data.get("homed", False)),
                in_position=bool(axis_data.get("in_position", False)),
                has_error=bool(axis_data.get("has_error", False)),
                error_code=axis_error_code,
                servo_alarm=bool(axis_data.get("servo_alarm", False)),
                following_error=bool(axis_data.get("following_error", False)),
                home_failed=bool(axis_data.get("home_failed", False)),
            )
        return status

    def home(self, axes: list[str] | None = None, force: bool = False) -> tuple[bool, str]:
        params: dict[str, object] = {"axes": axes} if axes else {}
        if force:
            params["force"] = True
        resp = self._client.send_request("home", params, timeout=_resolve_homing_rpc_timeout_s())
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        result = _as_dict(resp.get("result"))
        completed = result.get("completed", False)
        message = result.get("message", "")
        if completed:
            return True, message

        axis_results = _as_list(result.get("axis_results"))
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

        error_messages = [str(item) for item in _as_list(result.get("error_messages")) if str(item)]
        if error_messages:
            details = "; ".join(error_messages)
            if message and message != "Homing completed with errors":
                return False, f"{message}: {details}"
            return False, details
        return False, message

    def home_go(self, axes: list[str] | None = None, speed: float = 0.0) -> tuple[bool, str]:
        params: dict[str, object] = {"axes": axes} if axes else {}
        if speed and speed > 0.0:
            params["speed"] = speed
        resp = self._client.send_request("home.go", params, timeout=30.0)
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        result = _as_dict(resp.get("result"))
        return result.get("moving", False), result.get("message", "")

    def home_auto(
        self,
        axes: list[str] | None = None,
        force: bool = False,
        wait_for_completion: bool = True,
        timeout_ms: int = 0,
    ) -> tuple[bool, str]:
        params: dict[str, object] = {"axes": axes} if axes else {}
        if force:
            params["force"] = True
        if not wait_for_completion:
            params["wait_for_completion"] = False
        if timeout_ms > 0:
            params["timeout_ms"] = int(timeout_ms)
        resp = self._client.send_request("home.auto", params, timeout=_resolve_homing_rpc_timeout_s(timeout_ms))
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")

        result = _as_dict(resp.get("result"))
        accepted = bool(result.get("accepted", False))
        summary_state = str(result.get("summary_state", ""))
        message = str(result.get("message", "") or "")
        axis_results = _as_list(result.get("axis_results"))

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
        result = _as_dict(resp.get("result"))
        return result.get("jogging", "result" in resp), ""

    def move_to(self, x: float, y: float, speed: float = 10.0) -> tuple[bool, str, int | None]:
        resp = self._client.send_request("move", {"x": x, "y": y, "speed": speed})
        ok, message, error_code = self._resolve_action_result(resp)
        if not ok:
            return False, message, error_code

        result = _as_dict(resp.get("result"))
        status_message = str(result.get("status_message", "") or result.get("message", "") or "")

        if "motion_completed" in result:
            completed = bool(result.get("motion_completed", False))
            return completed, status_message, None

        if "moving" in result:
            moving = bool(result.get("moving", False))
            return moving, status_message, None

        return True, status_message, None

    def stop(self) -> bool:
        resp = self._client.send_request("stop")
        return "result" in resp

    def emergency_stop(self) -> tuple[bool, str]:
        resp = self._client.send_request("estop")
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        result = _as_dict(resp.get("result"))
        return result.get("motion_stopped", False), result.get("message", "")

    def estop_reset(self) -> tuple[bool, str]:
        resp = self._client.send_request("estop.reset")
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        result = _as_dict(resp.get("result"))
        return result.get("reset", False), result.get("message", "")

    def mock_io_set(
        self,
        *,
        estop: Optional[bool] = None,
        door: Optional[bool] = None,
        limit_x_neg: Optional[bool] = None,
        limit_y_neg: Optional[bool] = None,
    ) -> tuple[bool, object, str]:
        params: dict[str, object] = {}
        if estop is not None:
            params["estop"] = bool(estop)
        if door is not None:
            params["door"] = bool(door)
        if limit_x_neg is not None:
            params["limit_x_neg"] = bool(limit_x_neg)
        if limit_y_neg is not None:
            params["limit_y_neg"] = bool(limit_y_neg)
        return self._call("mock.io.set", params)

    def dispenser_start(
        self,
        count: int = 1,
        interval_ms: int = 1000,
        duration_ms: int = 15,
    ) -> tuple[bool, str, int | None]:
        resp = self._client.send_request(
            "dispenser.start",
            {
                "count": count,
                "interval_ms": interval_ms,
                "duration_ms": duration_ms,
            },
        )
        return self._resolve_action_result(resp)

    def dispenser_stop(self) -> tuple[bool, str, int | None]:
        resp = self._client.send_request("dispenser.stop")
        return self._resolve_action_result(resp)

    def dispenser_pause(self) -> bool:
        resp = self._client.send_request("dispenser.pause")
        return "result" in resp

    def dispenser_resume(self) -> bool:
        resp = self._client.send_request("dispenser.resume")
        return "result" in resp

    def purge(self, timeout_ms: int = 5000) -> bool:
        resp = self._client.send_request("purge", {"timeout_ms": timeout_ms})
        return "result" in resp

    def supply_open(self) -> tuple[bool, str, int | None]:
        resp = self._client.send_request("supply.open")
        return self._resolve_action_result(resp)

    def supply_close(self) -> tuple[bool, str, int | None]:
        resp = self._client.send_request("supply.close")
        return self._resolve_action_result(resp)

    def dxf_load(self, filepath: str, timeout: float = 15.0) -> tuple[bool, object]:
        resp = self._client.send_request("dxf.load", {"filepath": filepath}, timeout=timeout)
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        result = _as_dict(resp.get("result"))
        return result.get("loaded", False), result.get("segment_count", 0)

    def dxf_create_artifact(self, filepath: str, timeout: float = 30.0) -> tuple[bool, JsonDict, str]:
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
        return True, _as_dict(resp.get("result")), ""

    def dxf_preview_snapshot(
        self,
        plan_id: str,
        max_polyline_points: int = _DEFAULT_PREVIEW_MAX_POLYLINE_POINTS,
        max_glue_points: int = _DEFAULT_PREVIEW_MAX_GLUE_POINTS,
        timeout: float = 15.0,
    ) -> tuple[bool, JsonDict, str]:
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
    ) -> tuple[bool, JsonDict, str, int | None]:
        params: dict[str, object] = {"plan_id": plan_id}
        if max_polyline_points > 0 and max_polyline_points != self._DEFAULT_PREVIEW_MAX_POLYLINE_POINTS:
            params["max_polyline_points"] = int(max_polyline_points)
        if max_glue_points > 0 and max_glue_points != self._DEFAULT_PREVIEW_MAX_GLUE_POINTS:
            params["max_glue_points"] = int(max_glue_points)
        resp = self._client.send_request("dxf.preview.snapshot", params, timeout=timeout)
        if "error" in resp:
            error_payload = _as_dict(resp.get("error"))
            error_message = error_payload.get("message", "Unknown error")
            error_code_raw = error_payload.get("code")
            if error_code_raw is None:
                error_code = None
            else:
                try:
                    error_code = int(error_code_raw)
                except (TypeError, ValueError):
                    error_code = None
            return False, {}, error_message, error_code
        return True, _as_dict(resp.get("result")), "", None

    def dxf_preview_confirm(self, plan_id: str, snapshot_hash: str) -> tuple[bool, JsonDict, str]:
        params: dict[str, object] = {"plan_id": plan_id, "snapshot_hash": snapshot_hash}
        resp = self._client.send_request("dxf.preview.confirm", params, timeout=15.0)
        if "error" in resp:
            return False, {}, resp["error"].get("message", "Unknown error")
        return True, _as_dict(resp.get("result")), ""

    def dxf_prepare_plan(
        self,
        artifact_id: str,
        recipe_id: str,
        version_id: str,
        speed_mm_s: float,
        dry_run: bool = False,
        dry_run_speed_mm_s: float = 0.0,
        timeout: float = 15.0,
    ) -> tuple[bool, JsonDict, str]:
        params = self._build_dxf_plan_params(
            dispensing_speed_mm_s=speed_mm_s,
            dry_run=dry_run,
            dry_run_speed_mm_s=dry_run_speed_mm_s,
            velocity_trace_enabled=False,
        )
        params["artifact_id"] = artifact_id
        params["recipe_id"] = recipe_id
        params["version_id"] = version_id
        resp = self._client.send_request("dxf.plan.prepare", params, timeout=timeout)
        if "error" in resp:
            return False, {}, resp["error"].get("message", "Unknown error")
        return True, _as_dict(resp.get("result")), ""

    def dxf_start_job(self, plan_id: str, target_count: int = 1, plan_fingerprint: str = "") -> tuple[bool, JsonDict, str]:
        params: dict[str, object] = {"plan_id": plan_id, "target_count": max(1, int(target_count))}
        if plan_fingerprint:
            params["plan_fingerprint"] = plan_fingerprint
        resp = self._client.send_request("dxf.job.start", params, timeout=15.0)
        if "error" in resp:
            return False, {}, resp["error"].get("message", "Unknown error")
        return True, _as_dict(resp.get("result")), ""

    def dxf_get_job_status(self, job_id: str = "") -> JsonDict:
        params: dict[str, object] = {"job_id": job_id} if job_id else {}
        resp = self._client.send_request("dxf.job.status", params)
        if "error" in resp:
            return {"state": "unknown", "error_message": resp["error"].get("message", "Unknown error")}
        return _as_dict(resp.get("result"))

    def dxf_job_pause(self, job_id: str = "") -> tuple[bool, str]:
        params = {"job_id": job_id} if job_id else {}
        resp = self._client.send_request("dxf.job.pause", params, timeout=15.0)
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        return True, ""

    def dxf_job_resume(self, job_id: str = "") -> tuple[bool, str]:
        params = {"job_id": job_id} if job_id else {}
        resp = self._client.send_request("dxf.job.resume", params, timeout=15.0)
        if "error" in resp:
            return False, resp["error"].get("message", "Unknown error")
        return True, ""

    def dxf_job_stop(self, job_id: str = "") -> JobTransitionResult:
        params = {"job_id": job_id} if job_id else {}
        resp = self._client.send_request("dxf.job.stop", params, timeout=15.0)
        return self._resolve_job_transition_result(
            resp,
            accepted_key="stopped",
            default_transition_state="stopping",
        )

    def dxf_job_cancel(self, job_id: str = "") -> JobTransitionResult:
        params = {"job_id": job_id} if job_id else {}
        resp = self._client.send_request("dxf.job.cancel", params, timeout=15.0)
        return self._resolve_job_transition_result(
            resp,
            accepted_key="cancelled",
            default_transition_state="canceling",
        )

    def dxf_get_info(self) -> JsonDict:
        """Get DXF file info including bounding box and total length."""
        resp = self._client.send_request("dxf.info")
        if "error" in resp:
            return {}
        return _as_dict(resp.get("result"))

    def get_alarms(self) -> list[object]:
        resp = self._client.send_request("alarms.list")
        if "error" in resp:
            return []
        return _as_list(_as_dict(resp.get("result")).get("alarms"))

    def clear_alarms(self) -> bool:
        resp = self._client.send_request("alarms.clear")
        return "result" in resp

    def acknowledge_alarm(self, alarm_id: str) -> bool:
        resp = self._client.send_request("alarms.acknowledge", {"id": alarm_id})
        return "result" in resp

    def _call(
        self,
        method: str,
        params: Mapping[str, object] | None = None,
        result_key: str | None = None,
        default: object = None,
    ) -> tuple[bool, object, str]:
        resp = self._client.send_request(method, params or {})
        if "error" in resp:
            return False, default, resp["error"].get("message", "Unknown error")
        result = _as_dict(resp.get("result"))
        if result_key:
            return True, result.get(result_key, default), ""
        return True, result, ""

    def recipe_list(self, status: str = "", query: str = "", tag: str = "") -> tuple:
        params: dict[str, object] = {}
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

    def recipe_create(
        self,
        name: str,
        description: str = "",
        tags: list[object] | None = None,
        actor: str = "",
    ) -> tuple:
        params: dict[str, object] = {"name": name}
        if description:
            params["description"] = description
        if tags is not None:
            params["tags"] = tags
        if actor:
            params["actor"] = actor
        return self._call("recipe.create", params, "recipe", {})

    def recipe_update(
        self,
        recipe_id: str,
        name: str | None = None,
        description: str | None = None,
        tags: list[object] | None = None,
        actor: str = "",
    ) -> tuple:
        params: dict[str, object] = {"recipeId": recipe_id}
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
        params: dict[str, object] = {"recipeId": recipe_id}
        if actor:
            params["actor"] = actor
        return self._call("recipe.archive", params, "archived", False)

    def recipe_draft_create(self, recipe_id: str, template_id: str, base_version_id: str = "",
                            version_label: str = "", change_note: str = "", actor: str = "") -> tuple:
        params: dict[str, object] = {
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

    def recipe_draft_update(self, recipe_id: str, version_id: str, parameters: list[object],
                            change_note: str = "", actor: str = "") -> tuple:
        params: dict[str, object] = {
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
        params: dict[str, object] = {"recipeId": recipe_id, "versionId": version_id}
        if actor:
            params["actor"] = actor
        return self._call("recipe.publish", params, "version", {})

    def recipe_versions(self, recipe_id: str) -> tuple:
        return self._call("recipe.versions", {"recipeId": recipe_id}, "versions", [])

    def recipe_version_create(self, recipe_id: str, base_version_id: str = "", version_label: str = "",
                              change_note: str = "", actor: str = "") -> tuple:
        params: dict[str, object] = {"recipeId": recipe_id}
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
        params: dict[str, object] = {
            "recipeId": recipe_id,
            "baseVersionId": base_version_id,
            "versionId": version_id
        }
        return self._call("recipe.version.compare", params, "changes", [])

    def recipe_activate(self, recipe_id: str, version_id: str, actor: str = "") -> tuple:
        params: dict[str, object] = {"recipeId": recipe_id, "versionId": version_id}
        if actor:
            params["actor"] = actor
        return self._call("recipe.version.activate", params, "activated", False)

    def recipe_audit(self, recipe_id: str, version_id: str = "") -> tuple:
        params: dict[str, object] = {"recipeId": recipe_id}
        if version_id:
            params["versionId"] = version_id
        return self._call("recipe.audit", params, "records", [])

    def recipe_export(self, recipe_id: str, output_path: str = "", actor: str = "") -> tuple:
        params: dict[str, object] = {"recipeId": recipe_id}
        if output_path:
            params["outputPath"] = output_path
        if actor:
            params["actor"] = actor
        if output_path:
            return self._call("recipe.export", params, "outputPath", "")
        return self._call("recipe.export", params, None, {})

    def recipe_import(self, bundle_json: str = "", bundle_path: str = "", resolution: str = "",
                      dry_run: bool = False, actor: str = "") -> tuple:
        params: dict[str, object] = {}
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
