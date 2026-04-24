from __future__ import annotations

from dataclasses import dataclass, replace
from typing import Literal, Sequence

from .contracts.launch_supervision_contract import (
    FailureCode,
    FailureStage,
    HardwareState,
    RecoveryAction,
    SessionSnapshot,
    SessionStageEvent,
    TcpState,
    is_online_ready,
    snapshot_timestamp,
)
from .domain.launch_result_types import LaunchResult
from .launch_supervision_session import SupervisorSession
from .services.launch_result_projection import launch_result_from_snapshot

LedState = Literal["off", "green", "red"]
RUNTIME_REQUALIFIABLE_FAILURE_CODES = {"SUP_RUNTIME_HARDWARE_STATE_FAILED"}


@dataclass(frozen=True)
class RecoveryControlsState:
    retry_enabled: bool
    restart_enabled: bool
    stop_enabled: bool


@dataclass(frozen=True)
class RecoveryActionDecision:
    allowed: bool
    message: str = ""


@dataclass(frozen=True)
class LaunchUiState:
    effective_mode: str
    connected: bool
    hardware_connected: bool
    preview_resync_pending: bool
    launch_mode_label: str
    operation_status_text: str
    state_label_text: str
    tcp_state_label: str
    hardware_state_label: str
    tcp_led_state: LedState
    hardware_led_state: LedState
    allow_online_actions: bool
    system_panel_enabled: bool
    stop_enabled: bool
    hw_connect_enabled: bool
    global_estop_enabled: bool
    recovery_controls: RecoveryControlsState


@dataclass(frozen=True)
class RuntimeDegradationResult:
    degraded_snapshot: SessionSnapshot
    launch_result: LaunchResult
    stage_event: SessionStageEvent
    status_message: str


@dataclass(frozen=True)
class RuntimeRequalificationResult:
    recovered_snapshot: SessionSnapshot
    launch_result: LaunchResult
    stage_event: SessionStageEvent
    status_message: str


def current_effective_mode(
    requested_mode: str,
    launch_result: LaunchResult | None,
    session_snapshot: SessionSnapshot | None,
) -> str:
    if session_snapshot is not None:
        return session_snapshot.mode
    if launch_result is not None:
        return launch_result.effective_mode
    return requested_mode


def build_recovery_controls_state(
    snapshot: SessionSnapshot | None,
    *,
    effective_mode: str,
    session_operation_running: bool,
) -> RecoveryControlsState:
    if effective_mode == "offline" or session_operation_running:
        return RecoveryControlsState(False, False, False)

    failed = snapshot is not None and snapshot.session_state == "failed"
    recoverable = bool(snapshot.recoverable) if snapshot is not None and failed else False
    ready = snapshot is not None and is_online_ready(snapshot)
    return RecoveryControlsState(
        retry_enabled=failed and recoverable,
        restart_enabled=failed and recoverable,
        stop_enabled=failed or ready,
    )


def build_launch_ui_state(
    requested_mode: str,
    launch_result: LaunchResult | None,
    session_snapshot: SessionSnapshot | None,
    *,
    previous_connected: bool,
    has_current_plan: bool,
    preview_resync_pending: bool,
    session_operation_running: bool,
) -> LaunchUiState:
    effective_mode = current_effective_mode(requested_mode, launch_result, session_snapshot)
    recovery_controls = build_recovery_controls_state(
        session_snapshot,
        effective_mode=effective_mode,
        session_operation_running=session_operation_running,
    )

    if effective_mode == "offline":
        return LaunchUiState(
            effective_mode=effective_mode,
            connected=False,
            hardware_connected=False,
            preview_resync_pending=preview_resync_pending or (previous_connected and has_current_plan),
            launch_mode_label="Offline",
            operation_status_text="离线模式",
            state_label_text="Offline",
            tcp_state_label="未连接",
            hardware_state_label="不可用",
            tcp_led_state="off",
            hardware_led_state="off",
            allow_online_actions=False,
            system_panel_enabled=False,
            stop_enabled=False,
            hw_connect_enabled=False,
            global_estop_enabled=False,
            recovery_controls=recovery_controls,
        )

    if session_snapshot is None:
        return LaunchUiState(
            effective_mode=effective_mode,
            connected=False,
            hardware_connected=False,
            preview_resync_pending=preview_resync_pending or (previous_connected and has_current_plan),
            launch_mode_label="Online",
            operation_status_text="启动中",
            state_label_text="Starting",
            tcp_state_label="未连接",
            hardware_state_label="未初始化",
            tcp_led_state="off",
            hardware_led_state="off",
            allow_online_actions=False,
            system_panel_enabled=True,
            stop_enabled=False,
            hw_connect_enabled=False,
            global_estop_enabled=False,
            recovery_controls=recovery_controls,
        )

    connected = session_snapshot.tcp_state == "ready"
    hardware_connected = session_snapshot.hardware_state == "ready"
    allow_online_actions = is_online_ready(session_snapshot)
    command_channel_ready = effective_mode == "online" and connected

    if allow_online_actions:
        operation_status = "空闲"
        state_label = "Ready"
    elif session_snapshot.session_state == "starting":
        operation_status = "启动中"
        state_label = "Starting"
    elif session_snapshot.session_state == "failed":
        operation_status = "启动失败"
        state_label = "Launch Failed"
    elif session_snapshot.session_state == "stopping":
        operation_status = "停止中"
        state_label = "Stopping"
    else:
        operation_status = "未就绪"
        state_label = "Not Ready"

    return LaunchUiState(
        effective_mode=effective_mode,
        connected=connected,
        hardware_connected=hardware_connected,
        preview_resync_pending=preview_resync_pending or (has_current_plan and connected != previous_connected),
        launch_mode_label="Online",
        operation_status_text=operation_status,
        state_label_text=state_label,
        tcp_state_label=_label_for_tcp_state(session_snapshot.tcp_state),
        hardware_state_label=_label_for_hardware_state(session_snapshot.hardware_state),
        tcp_led_state=_led_state_for_tcp(session_snapshot.tcp_state),
        hardware_led_state=_led_state_for_hardware(session_snapshot.hardware_state),
        allow_online_actions=allow_online_actions,
        system_panel_enabled=True,
        stop_enabled=command_channel_ready,
        hw_connect_enabled=allow_online_actions and not session_operation_running,
        global_estop_enabled=command_channel_ready,
        recovery_controls=recovery_controls,
    )


def build_runtime_degradation_result(
    launch_result: LaunchResult | None,
    session_snapshot: SessionSnapshot | None,
    stage_events: Sequence[SessionStageEvent],
    *,
    failure_code: FailureCode,
    failure_stage: FailureStage,
    message: str,
    tcp_state: TcpState | None = None,
    hardware_state: HardwareState | None = None,
) -> RuntimeDegradationResult | None:
    if launch_result is None or session_snapshot is None:
        return None
    degraded_snapshot = SupervisorSession.build_runtime_failure_snapshot(
        session_snapshot,
        failure_code=failure_code,
        failure_stage=failure_stage,
        message=message,
        tcp_state=tcp_state,
        hardware_state=hardware_state,
        recoverable=True,
    )
    session_id = _current_supervisor_session_id(stage_events)
    stage_event = SessionStageEvent(
        event_type="stage_failed",
        session_id=session_id,
        stage=degraded_snapshot.failure_stage or failure_stage,
        timestamp=degraded_snapshot.updated_at,
        failure_code=degraded_snapshot.failure_code,
        recoverable=degraded_snapshot.recoverable,
        message=degraded_snapshot.last_error_message,
    )
    return RuntimeDegradationResult(
        degraded_snapshot=degraded_snapshot,
        launch_result=launch_result_from_snapshot(
            requested_mode=launch_result.requested_mode,
            snapshot=degraded_snapshot,
            user_message=degraded_snapshot.last_error_message,
        ),
        stage_event=stage_event,
        status_message=degraded_snapshot.last_error_message or "运行态退化，在线能力已收敛。",
    )


def detect_runtime_degradation_result(
    launch_result: LaunchResult | None,
    session_snapshot: SessionSnapshot | None,
    stage_events: Sequence[SessionStageEvent],
    *,
    tcp_connected: bool | None = None,
    hardware_ready: bool | None = None,
    error_message: str | None = None,
) -> RuntimeDegradationResult | None:
    degraded = SupervisorSession.detect_runtime_degradation_with_event(
        session_snapshot,
        session_id=_current_supervisor_session_id(stage_events),
        tcp_connected=tcp_connected,
        hardware_ready=hardware_ready,
        error_message=error_message,
    )
    if degraded is None or launch_result is None:
        return None
    degraded_snapshot, stage_event = degraded
    return RuntimeDegradationResult(
        degraded_snapshot=degraded_snapshot,
        launch_result=launch_result_from_snapshot(
            requested_mode=launch_result.requested_mode,
            snapshot=degraded_snapshot,
            user_message=degraded_snapshot.last_error_message,
        ),
        stage_event=stage_event,
        status_message=degraded_snapshot.last_error_message or "运行态退化，在线能力已收敛。",
    )


def detect_runtime_requalification_result(
    launch_result: LaunchResult | None,
    session_snapshot: SessionSnapshot | None,
    stage_events: Sequence[SessionStageEvent],
    *,
    tcp_connected: bool | None = None,
    hardware_ready: bool | None = None,
    success_message: str | None = None,
) -> RuntimeRequalificationResult | None:
    if launch_result is None or session_snapshot is None:
        return None
    if session_snapshot.mode != "online":
        return None
    if session_snapshot.session_state != "failed" or not session_snapshot.recoverable:
        return None
    if session_snapshot.failure_code not in RUNTIME_REQUALIFIABLE_FAILURE_CODES:
        return None
    if session_snapshot.failure_stage not in ("hardware_probing", "hardware_ready", "online_ready"):
        return None
    if session_snapshot.backend_state != "ready" or session_snapshot.tcp_state != "ready":
        return None
    if not session_snapshot.runtime_contract_verified or session_snapshot.runtime_identity is None:
        return None
    if tcp_connected is False or hardware_ready is not True:
        return None

    message = success_message or "运行态已恢复，系统已就绪。"
    recovered_snapshot = replace(
        session_snapshot,
        session_state="ready",
        hardware_state="ready",
        failure_code=None,
        failure_stage=None,
        recoverable=True,
        last_error_message=message,
        updated_at=snapshot_timestamp(),
        runtime_contract_verified=session_snapshot.runtime_contract_verified,
        runtime_identity=session_snapshot.runtime_identity,
    )
    stage_event = SessionStageEvent(
        event_type="stage_succeeded",
        session_id=_current_supervisor_session_id(stage_events),
        stage="online_ready",
        timestamp=recovered_snapshot.updated_at,
        failure_code=None,
        recoverable=recovered_snapshot.recoverable,
        message=message,
    )
    return RuntimeRequalificationResult(
        recovered_snapshot=recovered_snapshot,
        launch_result=launch_result_from_snapshot(
            requested_mode=launch_result.requested_mode,
            snapshot=recovered_snapshot,
            user_message=message,
        ),
        stage_event=stage_event,
        status_message=message,
    )


def build_online_capability_message(
    capability: str,
    *,
    requested_mode: str,
    launch_result: LaunchResult | None,
    session_snapshot: SessionSnapshot | None,
) -> str:
    effective_mode = current_effective_mode(requested_mode, launch_result, session_snapshot)
    if effective_mode == "offline":
        return f"Offline 模式下不可用: {capability}"

    if session_snapshot is not None and is_online_ready(session_snapshot):
        return ""

    if launch_result is not None and launch_result.failure_code:
        return f"系统未就绪({launch_result.failure_code}): {capability} 不可用"
    return f"系统启动中: {capability} 暂不可用"


def build_recovery_action_decision(
    action: RecoveryAction,
    snapshot: SessionSnapshot | None,
    *,
    effective_mode: str,
    session_operation_running: bool,
) -> RecoveryActionDecision:
    if effective_mode == "offline":
        return RecoveryActionDecision(False, "Offline 模式下不可用: 会话恢复")
    if session_operation_running:
        return RecoveryActionDecision(False, "会话操作进行中，请稍候")
    if snapshot is None:
        return RecoveryActionDecision(False, "无可用会话快照，无法执行恢复动作")
    if action in ("retry_stage", "restart_session"):
        if snapshot.session_state != "failed":
            return RecoveryActionDecision(False, "当前会话未处于失败态，恢复动作不可用")
        if not snapshot.recoverable:
            return RecoveryActionDecision(False, "当前失败不可恢复，仅允许停止会话")
    if action == "stop_session":
        if snapshot.mode != "online":
            return RecoveryActionDecision(False, "仅在线会话支持停止动作")
        if snapshot.session_state not in ("ready", "failed"):
            return RecoveryActionDecision(False, "当前会话未处于可停止态，停止动作不可用")
    return RecoveryActionDecision(True)


def build_recovery_finished_message(action: str, result: LaunchResult) -> str:
    if result.success and result.online_ready:
        return f"恢复动作完成({action})，系统已就绪"
    if action == "stop_session" and result.session_state == "idle":
        return "会话已停止"
    code = result.failure_code or "SUP_UNKNOWN"
    return f"恢复动作失败({action}): {code}"


def build_startup_error_message(result: LaunchResult) -> str:
    stage_names = {
        "backend": "Backend startup",
        "tcp": "TCP connection",
        "hardware": "Hardware initialization",
        "backend_starting": "Backend starting",
        "backend_ready": "Backend ready",
        "tcp_connecting": "TCP connecting",
        "tcp_ready": "TCP ready",
        "runtime": "Runtime contract",
        "runtime_contract_ready": "Runtime contract verification",
        "hardware_probing": "Hardware probing",
        "hardware_ready": "Hardware ready",
        "online_ready": "Online ready",
    }
    stage_name = stage_names.get(result.phase, result.phase)
    code_text = f"\nFailure Code: {result.failure_code}" if result.failure_code else ""
    stage_text = f"\nFailure Stage: {result.failure_stage}" if result.failure_stage else ""
    if result.phase == "runtime" or result.failure_stage == "runtime_contract_ready":
        checks = (
            "1. Local launch contract points to the expected runtime executable\n"
            "2. Runtime working directory matches launch contract\n"
            "3. Runtime protocol and preview snapshot contract match HMI expectation"
        )
    else:
        checks = (
            "1. Backend executable exists\n"
            "2. Port is not in use\n"
            "3. Gateway launched successfully"
        )
    return (
        f"{stage_name} failed:\n{result.user_message}{code_text}{stage_text}\n\n"
        "Please check:\n"
        f"{checks}"
    )


def _label_for_tcp_state(state: str) -> str:
    if state == "ready":
        return "已连接"
    if state == "connecting":
        return "连接中"
    if state == "failed":
        return "连接失败"
    return "未连接"


def _label_for_hardware_state(state: str) -> str:
    if state == "ready":
        return "就绪"
    if state == "probing":
        return "初始化中"
    if state == "failed":
        return "初始化失败"
    if state == "unavailable":
        return "不可用"
    return "未初始化"


def _led_state_for_tcp(state: str) -> LedState:
    if state == "ready":
        return "green"
    if state == "failed":
        return "red"
    return "off"


def _led_state_for_hardware(state: str) -> LedState:
    if state == "ready":
        return "green"
    if state == "failed":
        return "red"
    return "off"


def _current_supervisor_session_id(stage_events: Sequence[SessionStageEvent]) -> str:
    if stage_events:
        session_id = getattr(stage_events[-1], "session_id", "")
        if isinstance(session_id, str) and session_id:
            return session_id
    return "runtime-session"
