from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import TYPE_CHECKING

from PyQt5.QtCore import QThread, pyqtSignal

from .preview_gate import DispensePreviewGate, PreviewGateState, PreviewSnapshotMeta, StartBlockReason

# DXF 打开后的自动预览允许更长等待窗口，但该预算不向 resync/confirm/job.start 扩散。
DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S = 100.0

if TYPE_CHECKING:
    try:
        from hmi_client.client.protocol import MachineStatus, CommandProtocol
        from hmi_client.client.tcp_client import TcpClient
    except ImportError:  # pragma: no cover - script-mode fallback
        from client.protocol import MachineStatus, CommandProtocol  # type: ignore
        from client.tcp_client import TcpClient  # type: ignore


@dataclass
class PreviewSessionState:
    gate: DispensePreviewGate
    preview_source: str = ""
    preview_kind: str = ""
    glue_point_count: int = 0
    preview_validation_classification: str = ""
    preview_exception_reason: str = ""
    preview_failure_reason: str = ""
    current_plan_id: str = ""
    current_plan_fingerprint: str = ""
    preview_plan_dry_run: bool | None = None
    preview_refresh_inflight: bool = False
    preview_state_resync_pending: bool = False
    last_preview_resync_attempt_ts: float = 0.0
    dxf_segment_count: int = 0
    dxf_total_length_mm: float = 0.0
    dxf_estimated_time_text: str = "-"


class PreflightBlockReason(str, Enum):
    NONE = ""
    NOT_CONNECTED = "not_connected"
    HARDWARE_NOT_READY = "hardware_not_ready"
    RUNTIME_STATUS_FAULT = "runtime_status_fault"
    PREVIEW_GENERATING = "preview_generating"
    STATUS_UNAVAILABLE = "status_unavailable"
    INTERLOCK_UNKNOWN = "interlock_unknown"
    ESTOP_ACTIVE = "estop_active"
    DOOR_ACTIVE = "door_active"
    PREVIEW_PLAN_MISSING = "preview_plan_missing"
    PREVIEW_MODE_UNKNOWN = "preview_mode_unknown"
    PREVIEW_MODE_MISMATCH = "preview_mode_mismatch"
    INVALID_SOURCE = "invalid_source"
    INVALID_KIND = "invalid_kind"
    EMPTY_GLUE_POINTS = "empty_glue_points"
    PREVIEW_CONFIRM_REQUIRED = "preview_confirm_required"
    PREVIEW_FAILED = "preview_failed"
    PREVIEW_STALE = "preview_stale"
    HASH_MISMATCH = "hash_mismatch"
    NOT_READY = "not_ready"


@dataclass(frozen=True)
class PreflightDecision:
    allowed: bool
    reason: PreflightBlockReason = PreflightBlockReason.NONE
    message: str = ""
    requires_confirmation: bool = False


@dataclass(frozen=True)
class PreviewConfirmResult:
    ok: bool
    message: str = ""


@dataclass(frozen=True)
class PreviewPayloadResult:
    ok: bool
    title: str
    detail: str
    status_message: str
    is_error: bool
    should_render: bool = False
    snapshot: PreviewSnapshotMeta | None = None
    glue_points: tuple[tuple[float, float], ...] = ()
    execution_polyline: tuple[tuple[float, float], ...] = ()
    preview_source: str = ""
    preview_kind: str = "glue_points"
    dry_run: bool = False
    preview_warning: str = ""
    execution_polyline_source_point_count: int = 0


class PreviewSnapshotWorker(QThread):
    completed = pyqtSignal(bool, object, str)

    def __init__(
        self,
        host: str,
        port: int,
        artifact_id: str,
        speed_mm_s: float,
        dry_run: bool,
        dry_run_speed_mm_s: float,
    ):
        super().__init__()
        self._host = host
        self._port = port
        self._artifact_id = artifact_id
        self._speed_mm_s = speed_mm_s
        self._dry_run = dry_run
        self._dry_run_speed_mm_s = dry_run_speed_mm_s

    def run(self):
        client = None
        ok = False
        payload = {}
        error = ""
        try:
            try:
                from hmi_client.client.tcp_client import TcpClient  # type: ignore
                from hmi_client.client.protocol import CommandProtocol  # type: ignore
            except ImportError:  # pragma: no cover - script-mode fallback
                from client.tcp_client import TcpClient  # type: ignore
                from client.protocol import CommandProtocol  # type: ignore

            client = TcpClient(host=self._host, port=self._port)
            if not client.connect():
                error = "无法连接后端，请检查TCP链路"
            else:
                protocol = CommandProtocol(client)
                plan_ok, plan_payload, plan_error = protocol.dxf_prepare_plan(
                    artifact_id=self._artifact_id,
                    speed_mm_s=self._speed_mm_s,
                    dry_run=self._dry_run,
                    dry_run_speed_mm_s=self._dry_run_speed_mm_s,
                    timeout=DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S,
                )
                if not plan_ok:
                    error = plan_error
                else:
                    plan_id = str(plan_payload.get("plan_id", "")).strip()
                    if not plan_id:
                        error = "plan.prepare 返回缺少 plan_id"
                    else:
                        ok, payload, error = protocol.dxf_preview_snapshot(
                            plan_id=plan_id,
                            max_polyline_points=4000,
                            timeout=DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S,
                        )
                        if ok and isinstance(payload, dict):
                            payload.setdefault("plan_id", plan_id)
                            plan_fingerprint = str(plan_payload.get("plan_fingerprint", ""))
                            payload.setdefault("plan_fingerprint", plan_fingerprint)
                            payload.setdefault("snapshot_hash", plan_fingerprint)
                            payload.setdefault("dry_run", bool(self._dry_run))
                            payload.setdefault(
                                "preview_validation_classification",
                                str(plan_payload.get("preview_validation_classification", "")),
                            )
                            payload.setdefault(
                                "preview_exception_reason",
                                str(plan_payload.get("preview_exception_reason", "")),
                            )
                            payload.setdefault(
                                "preview_failure_reason",
                                str(plan_payload.get("preview_failure_reason", "")),
                            )
        except Exception as exc:  # pragma: no cover - defensive path
            error = str(exc) or "预览快照生成异常"
        finally:
            if client is not None:
                client.disconnect()
        self.completed.emit(ok, payload if isinstance(payload, dict) else {}, error)


class PreviewSessionOwner:
    RESYNC_RETRY_INTERVAL_S = 2.0

    def __init__(self, gate: DispensePreviewGate | None = None) -> None:
        self._state = PreviewSessionState(gate=gate or DispensePreviewGate())

    @property
    def gate(self) -> DispensePreviewGate:
        return self._state.gate

    @property
    def state(self) -> PreviewSessionState:
        return self._state

    def set_refresh_inflight(self, inflight: bool) -> None:
        self._state.preview_refresh_inflight = bool(inflight)

    def set_resync_pending(self, pending: bool) -> None:
        if pending:
            self._state.preview_state_resync_pending = True
            return
        self.clear_resync_pending()

    def mark_resync_pending(self) -> None:
        self._state.preview_state_resync_pending = True

    def clear_resync_pending(self) -> None:
        self._state.preview_state_resync_pending = False
        self._state.last_preview_resync_attempt_ts = 0.0

    def preview_state_text(self) -> str:
        state_map = {
            PreviewGateState.DIRTY.value: "待生成",
            PreviewGateState.GENERATING.value: "生成中",
            PreviewGateState.READY_UNSIGNED.value: "待确认",
            PreviewGateState.READY_SIGNED.value: "已确认",
            PreviewGateState.STALE.value: "已过期",
            PreviewGateState.FAILED.value: "失败",
        }
        return state_map.get(self.gate.state.value, self.gate.state.value)

    def preview_source_text(self, preview_source: str | None = None) -> str:
        normalized = str(self._state.preview_source if preview_source is None else preview_source).strip().lower()
        if normalized == "planned_glue_snapshot":
            return "规划胶点快照"
        if normalized == "runtime_snapshot":
            return "旧版轨迹快照"
        if normalized == "mock_synthetic":
            return "Mock模拟"
        if normalized:
            return normalized
        return "-"

    def preview_source_warning(self, preview_source: str | None = None) -> str:
        normalized = str(self._state.preview_source if preview_source is None else preview_source).strip().lower()
        if normalized == "mock_synthetic":
            return "模拟轨迹，非真实几何"
        if normalized == "planned_glue_snapshot":
            return "胶点主预览，执行轨迹仅作辅助叠加"
        if normalized == "runtime_snapshot":
            return "旧版轨迹预览，不能直接视为胶点触发预览"
        return "预览来源未知"

    def preview_block_message(self, reason: StartBlockReason) -> str:
        if reason == StartBlockReason.NOT_READY:
            return "当前未达到 online_ready，禁止将预览记为真实在线结果"
        if reason == StartBlockReason.PREVIEW_MISSING:
            return "请先生成胶点预览"
        if reason == StartBlockReason.PREVIEW_GENERATING:
            return "胶点预览仍在生成中，请稍后"
        if reason == StartBlockReason.PREVIEW_FAILED:
            if self.gate.last_error_message:
                return f"胶点预览失败: {self.gate.last_error_message}"
            return "胶点预览失败，请重新生成"
        if reason == StartBlockReason.PREVIEW_STALE:
            return "轨迹参数已变更，需重新生成并确认预览"
        if reason == StartBlockReason.CONFIRM_MISSING:
            return "请先确认胶点预览"
        if reason == StartBlockReason.INVALID_SOURCE:
            if self._state.preview_source == "mock_synthetic":
                return "当前预览来源为 mock_synthetic，不能作为真实在线预览通过依据"
            return "当前预览来源不是 planned_glue_snapshot，不能作为真实在线预览通过依据"
        if reason == StartBlockReason.INVALID_KIND:
            if self._state.preview_kind:
                return f"当前预览语义为 {self._state.preview_kind}，不是 glue_points，不能作为真实在线预览通过依据"
            return "当前预览语义不是 glue_points，不能作为真实在线预览通过依据"
        if reason == StartBlockReason.EMPTY_GLUE_POINTS:
            return "当前预览缺少非空 glue_points，不能作为真实在线预览通过依据"
        if reason == StartBlockReason.HASH_MISMATCH:
            return "预览快照与执行快照不一致，请重新生成并确认"
        return "预检失败"

    def _clear_preview_contract_state(self) -> None:
        self._state.preview_source = ""
        self._state.preview_kind = ""
        self._state.glue_point_count = 0
        self._state.preview_validation_classification = ""
        self._state.preview_exception_reason = ""
        self._state.preview_failure_reason = ""

    @staticmethod
    def _map_preview_contract_reason(reason: StartBlockReason) -> PreflightBlockReason:
        if reason == StartBlockReason.INVALID_SOURCE:
            return PreflightBlockReason.INVALID_SOURCE
        if reason == StartBlockReason.INVALID_KIND:
            return PreflightBlockReason.INVALID_KIND
        if reason == StartBlockReason.EMPTY_GLUE_POINTS:
            return PreflightBlockReason.EMPTY_GLUE_POINTS
        return PreflightBlockReason.PREVIEW_FAILED

    def info_label_text(self) -> str:
        return (
            f"段数: {self._state.dxf_segment_count} | 长度: {self._state.dxf_total_length_mm:.1f}mm | "
            f"预估: {self._state.dxf_estimated_time_text} | 预览: {self.preview_state_text()} | "
            f"来源: {self.preview_source_text()}"
        )

    def should_enable_refresh(self, *, offline_mode: bool, connected: bool, dxf_loaded: bool) -> bool:
        return (not offline_mode) and connected and dxf_loaded and (not self._state.preview_refresh_inflight)

    def update_dxf_info(self, *, total_length_mm: float, total_segments: int, speed_mm_s: float) -> None:
        self._state.dxf_total_length_mm = float(total_length_mm or 0.0)
        self._state.dxf_segment_count = int(total_segments or 0)
        self.update_estimated_time(speed_mm_s=speed_mm_s)

    def update_estimated_time(self, *, speed_mm_s: float) -> None:
        if speed_mm_s <= 0:
            self._state.dxf_estimated_time_text = "Inf"
            return
        self._state.dxf_estimated_time_text = f"{self._state.dxf_total_length_mm / speed_mm_s:.1f}s"

    def reset_for_loaded_dxf(self, *, segment_count: int = 0) -> None:
        self.gate.reset_for_loaded_dxf()
        self._clear_preview_contract_state()
        self._state.current_plan_id = ""
        self._state.current_plan_fingerprint = ""
        self._state.preview_plan_dry_run = None
        self._state.preview_state_resync_pending = False
        self._state.last_preview_resync_attempt_ts = 0.0
        self._state.dxf_segment_count = int(segment_count or 0)
        self._state.dxf_total_length_mm = 0.0
        self._state.dxf_estimated_time_text = "-"

    def invalidate_plan(self) -> None:
        self.gate.mark_input_changed()
        self._state.current_plan_id = ""
        self._state.current_plan_fingerprint = ""
        self._state.preview_plan_dry_run = None
        self._clear_preview_contract_state()
        self._state.preview_state_resync_pending = False
        self._state.last_preview_resync_attempt_ts = 0.0

    def begin_preview_generation(self) -> None:
        self.gate.begin_preview()

    def build_confirmation_summary(self) -> str:
        snapshot = self.gate.snapshot
        if snapshot is None:
            return ""
        summary = (
            f"段数: {snapshot.segment_count}\n"
            f"点数: {snapshot.point_count}\n"
            f"长度: {snapshot.total_length_mm:.1f}mm\n"
            f"预估: {snapshot.estimated_time_s:.1f}s\n\n"
            "确认以上胶点预览后继续执行？"
        )
        if self._state.preview_validation_classification == "pass_with_exception" and self._state.preview_exception_reason:
            summary = (
                f"{summary}\n\n"
                f"非阻断提示: {self._state.preview_exception_reason}"
            )
        return summary

    def validate_before_confirmation(self) -> PreviewConfirmResult:
        snapshot = self.gate.snapshot
        if snapshot is None:
            return PreviewConfirmResult(False, "请先生成胶点预览")
        if self._state.preview_validation_classification == "fail":
            return PreviewConfirmResult(
                False,
                self._state.preview_failure_reason
                or self._state.preview_exception_reason
                or "当前预览已被阻断，请先修复 authority 问题",
            )
        contract_decision = self.gate.validate_preview_contract(
            preview_source=self._state.preview_source,
            preview_kind=self._state.preview_kind,
            glue_point_count=self._state.glue_point_count,
        )
        if not contract_decision.allowed:
            return PreviewConfirmResult(False, self.preview_block_message(contract_decision.reason))
        return PreviewConfirmResult(True)

    def apply_confirmation_payload(self, payload: dict) -> PreviewConfirmResult:
        snapshot = self.gate.snapshot
        if snapshot is None:
            return PreviewConfirmResult(False, "请先生成胶点预览")

        confirmed = bool(payload.get("confirmed", False))
        if not confirmed:
            return PreviewConfirmResult(False, "预览确认未生效，请重试")

        confirmed_hash = str(payload.get("snapshot_hash", snapshot.snapshot_hash)).strip()
        if confirmed_hash and confirmed_hash != snapshot.snapshot_hash:
            self.gate.preview_failed("确认返回的快照哈希与当前预览不一致")
            return PreviewConfirmResult(False, "预览确认哈希不一致，请重新生成预览")

        if not self.gate.confirm_current_snapshot():
            return PreviewConfirmResult(False, "本地预览确认状态更新失败")
        return PreviewConfirmResult(True)

    def handle_worker_error(self, error_message: str) -> PreviewPayloadResult:
        self._clear_preview_contract_state()
        self.gate.preview_failed(error_message)
        return PreviewPayloadResult(
            ok=False,
            title="胶点预览生成失败",
            detail=error_message,
            status_message=self.preview_block_message(StartBlockReason.PREVIEW_FAILED),
            is_error=True,
        )

    def handle_local_failure(
        self,
        *,
        gate_error_message: str,
        title: str,
        detail: str,
        clear_source: bool = False,
    ) -> PreviewPayloadResult:
        if clear_source:
            self._clear_preview_contract_state()
        self.gate.preview_failed(gate_error_message)
        return PreviewPayloadResult(
            ok=False,
            title=title,
            detail=detail,
            status_message=self.preview_block_message(StartBlockReason.PREVIEW_FAILED),
            is_error=True,
        )

    def process_snapshot_payload(
        self,
        payload: dict,
        *,
        current_dry_run: bool,
        source: str = "worker",
    ) -> PreviewPayloadResult:
        snapshot_id = str(payload.get("snapshot_id", payload.get("plan_id", ""))).strip()
        snapshot_hash = str(payload.get("snapshot_hash", payload.get("plan_fingerprint", ""))).strip()
        if not snapshot_id or not snapshot_hash:
            return self.handle_local_failure(
                gate_error_message="运行时快照缺少 snapshot_id/snapshot_hash",
                title="胶点预览生成失败",
                detail="返回结果缺少快照标识。",
            )

        glue_points = self.extract_points(payload, "glue_points")
        execution_polyline = self.extract_points(payload, "execution_polyline")
        preview_source = str(payload.get("preview_source", "")).strip().lower() or "unknown"
        preview_kind = str(payload.get("preview_kind", "glue_points")).strip().lower() or "glue_points"
        preview_dry_run = bool(payload.get("dry_run", False))
        backend_preview_state = str(payload.get("preview_state", "snapshot_ready")).strip().lower()
        preview_validation_classification = str(
            payload.get(
                "preview_validation_classification",
                self._state.preview_validation_classification or "pass",
            )
            or "pass"
        ).strip().lower()
        preview_exception_reason = str(
            payload.get("preview_exception_reason", self._state.preview_exception_reason or "")
        ).strip()
        preview_failure_reason = str(
            payload.get("preview_failure_reason", self._state.preview_failure_reason or "")
        ).strip()
        self._state.current_plan_id = str(payload.get("plan_id", snapshot_id)).strip() or snapshot_id
        self._state.current_plan_fingerprint = snapshot_hash
        self._state.preview_plan_dry_run = preview_dry_run
        self._state.preview_source = preview_source
        self._state.preview_kind = preview_kind
        self._state.glue_point_count = len(glue_points)
        self._state.preview_validation_classification = preview_validation_classification
        self._state.preview_exception_reason = preview_exception_reason
        self._state.preview_failure_reason = preview_failure_reason
        self._state.dxf_segment_count = int(payload.get("segment_count", 0) or 0)
        self._state.dxf_total_length_mm = float(payload.get("total_length_mm", 0.0) or 0.0)
        estimated_time = float(payload.get("estimated_time_s", 0.0) or 0.0)
        self._state.dxf_estimated_time_text = f"{estimated_time:.1f}s" if estimated_time > 0 else "-"

        legacy_runtime_snapshot = preview_source == "runtime_snapshot"
        legacy_polyline_present = "trajectory_polyline" in payload
        if not glue_points and (legacy_runtime_snapshot or legacy_polyline_present):
            return self.handle_local_failure(
                gate_error_message="运行时仍返回旧版轨迹预览契约",
                title="胶点预览生成失败",
                detail=(
                    "返回结果缺少 glue_points，并检测到旧版 trajectory_polyline/runtime_snapshot；"
                    "当前 HMI 连接的 runtime-gateway 很可能还是旧构建。"
                ),
            )

        source_decision = self.gate.validate_preview_source(preview_source)
        if not source_decision.allowed:
            return self.handle_local_failure(
                gate_error_message=self.preview_block_message(source_decision.reason),
                title="胶点预览生成失败",
                detail=f"返回结果的 preview_source={preview_source}，不属于 planned_glue_snapshot 权威预览契约。",
            )

        kind_decision = self.gate.validate_preview_kind(preview_kind)
        if not kind_decision.allowed:
            return self.handle_local_failure(
                gate_error_message=self.preview_block_message(kind_decision.reason),
                title="胶点预览生成失败",
                detail=f"返回结果的 preview_kind={preview_kind}，不属于 glue_points 主预览语义。",
            )

        glue_point_decision = self.gate.validate_glue_points(len(glue_points))
        if not glue_point_decision.allowed:
            return self.handle_local_failure(
                gate_error_message=self.preview_block_message(glue_point_decision.reason),
                title="胶点预览生成失败",
                detail="返回结果缺少非空 glue_points。请核对 runtime-gateway 是否已升级到 planned_glue_snapshot 契约。",
            )

        snapshot = PreviewSnapshotMeta(
            snapshot_id=snapshot_id,
            snapshot_hash=snapshot_hash,
            segment_count=int(payload.get("segment_count", 0) or 0),
            point_count=int(payload.get("glue_point_count", payload.get("point_count", len(glue_points))) or len(glue_points)),
            total_length_mm=float(payload.get("total_length_mm", 0.0) or 0.0),
            estimated_time_s=float(payload.get("estimated_time_s", 0.0) or 0.0),
            generated_at=str(payload.get("generated_at", "")),
        )
        execution_source_point_count = int(payload.get("execution_polyline_source_point_count", 0) or 0)
        preview_warning = self.sampling_warning(
            glue_point_count=snapshot.point_count,
            execution_source_point_count=execution_source_point_count,
        )
        self.gate.preview_ready(snapshot)
        if backend_preview_state == "confirmed":
            if not self.gate.confirm_current_snapshot():
                return self.handle_local_failure(
                    gate_error_message="运行时状态同步失败：confirmed 快照无效",
                    title="胶点预览同步失败",
                    detail="运行时确认态与本地快照不一致。",
                )

        if current_dry_run != preview_dry_run:
            self.invalidate_plan()
            status_message = "运行模式已变更，请刷新预览并重新确认"
        elif source == "resync":
            if self.gate.state == PreviewGateState.READY_SIGNED:
                status_message = "已从运行时同步预览状态（已确认）"
            else:
                status_message = "已从运行时同步预览状态"
        elif self.gate.state == PreviewGateState.READY_SIGNED:
            if preview_source == "mock_synthetic":
                status_message = "模拟胶点预览已更新（仅供联调，非真实几何）"
            elif preview_validation_classification == "fail":
                status_message = preview_failure_reason or "胶点预览已更新，但当前结果存在阻断原因"
            else:
                status_message = "胶点预览已更新（运行时已确认）"
        else:
            if preview_source == "mock_synthetic":
                status_message = "模拟胶点预览已更新，非真实几何"
            elif preview_validation_classification == "fail":
                status_message = preview_failure_reason or "胶点预览已更新，但当前结果存在阻断原因"
            else:
                status_message = "胶点预览已更新，启动前需确认"

        return PreviewPayloadResult(
            ok=True,
            title="",
            detail="",
            status_message=status_message,
            is_error=False,
            should_render=True,
            snapshot=snapshot,
            glue_points=tuple(glue_points),
            execution_polyline=tuple(execution_polyline),
            preview_source=preview_source,
            preview_kind=preview_kind,
            dry_run=preview_dry_run,
            preview_warning=preview_warning,
            execution_polyline_source_point_count=execution_source_point_count,
        )

    def build_preflight_decision(
        self,
        *,
        online_ready: bool,
        connected: bool,
        hardware_connected: bool,
        runtime_status_fault: bool,
        status: MachineStatus,
        dry_run: bool,
    ) -> PreflightDecision:
        if not connected:
            return PreflightDecision(False, PreflightBlockReason.NOT_CONNECTED, "未连接到后端，请先连接")
        if not hardware_connected:
            return PreflightDecision(False, PreflightBlockReason.HARDWARE_NOT_READY, "硬件未初始化，请先初始化硬件")
        if runtime_status_fault:
            return PreflightDecision(False, PreflightBlockReason.RUNTIME_STATUS_FAULT, "运行状态采集异常，请先恢复状态链路后再启动")
        if self._state.preview_refresh_inflight:
            return PreflightDecision(False, PreflightBlockReason.PREVIEW_GENERATING, "胶点预览仍在生成中，请稍后再启动")

        ready_decision = self.gate.validate_online_ready(online_ready)
        if not ready_decision.allowed:
            return PreflightDecision(False, PreflightBlockReason.NOT_READY, self.preview_block_message(ready_decision.reason))

        if not status.connected:
            return PreflightDecision(False, PreflightBlockReason.STATUS_UNAVAILABLE, "后端状态不可用，请检查连接")
        if not status.gate_estop_known() or not status.gate_door_known():
            return PreflightDecision(False, PreflightBlockReason.INTERLOCK_UNKNOWN, "互锁信号状态未知，禁止启动")
        if status.gate_estop_active():
            return PreflightDecision(False, PreflightBlockReason.ESTOP_ACTIVE, "急停未解除，禁止启动")
        if status.gate_door_active():
            return PreflightDecision(False, PreflightBlockReason.DOOR_ACTIVE, "安全门打开，禁止启动")
        if not self._state.current_plan_id or not self._state.current_plan_fingerprint:
            return PreflightDecision(False, PreflightBlockReason.PREVIEW_PLAN_MISSING, "预览计划未生成，请重新生成预览")
        if self._state.preview_plan_dry_run is None:
            return PreflightDecision(False, PreflightBlockReason.PREVIEW_MODE_UNKNOWN, "预览模式未知，请刷新预览并重新确认")
        if bool(dry_run) != bool(self._state.preview_plan_dry_run):
            self.invalidate_plan()
            return PreflightDecision(False, PreflightBlockReason.PREVIEW_MODE_MISMATCH, "运行模式已变更，请刷新预览并重新确认")

        if self._state.preview_validation_classification == "fail":
            return PreflightDecision(
                False,
                PreflightBlockReason.PREVIEW_FAILED,
                self._state.preview_failure_reason
                or self._state.preview_exception_reason
                or "当前预览 authority 校验失败，禁止启动",
            )

        contract_decision = self.gate.validate_preview_contract(
            preview_source=self._state.preview_source,
            preview_kind=self._state.preview_kind,
            glue_point_count=self._state.glue_point_count,
        )
        if not contract_decision.allowed:
            return PreflightDecision(
                False,
                self._map_preview_contract_reason(contract_decision.reason),
                self.preview_block_message(contract_decision.reason),
            )

        gate_decision = self.gate.decision_for_start()
        if gate_decision.reason == StartBlockReason.CONFIRM_MISSING:
            return PreflightDecision(
                False,
                PreflightBlockReason.PREVIEW_CONFIRM_REQUIRED,
                self.preview_block_message(StartBlockReason.CONFIRM_MISSING),
                requires_confirmation=True,
            )
        if not gate_decision.allowed:
            mapped_reason = (
                PreflightBlockReason.PREVIEW_FAILED
                if gate_decision.reason == StartBlockReason.PREVIEW_FAILED
                else PreflightBlockReason.PREVIEW_STALE
            )
            return PreflightDecision(False, mapped_reason, self.preview_block_message(gate_decision.reason))

        hash_decision = self.gate.validate_execution_snapshot_hash(self._state.current_plan_fingerprint)
        if not hash_decision.allowed:
            return PreflightDecision(False, PreflightBlockReason.HASH_MISMATCH, self.preview_block_message(hash_decision.reason))

        return PreflightDecision(True)

    def should_request_runtime_resync(
        self,
        *,
        offline_mode: bool,
        connected: bool,
        production_running: bool,
        current_job_id: str,
        now_monotonic: float,
    ) -> bool:
        if not self._state.preview_state_resync_pending:
            return False
        if offline_mode or not connected or not self._state.current_plan_id:
            return False
        if self._state.preview_refresh_inflight or production_running or current_job_id:
            return False
        if now_monotonic - self._state.last_preview_resync_attempt_ts < self.RESYNC_RETRY_INTERVAL_S:
            return False
        self._state.last_preview_resync_attempt_ts = now_monotonic
        return True

    def prepare_resync_payload(self, payload: dict, *, current_dry_run: bool) -> dict:
        payload.setdefault("plan_id", self._state.current_plan_id)
        if self._state.preview_plan_dry_run is not None:
            payload.setdefault("dry_run", bool(self._state.preview_plan_dry_run))
        else:
            payload.setdefault("dry_run", bool(current_dry_run))
        payload.setdefault(
            "preview_validation_classification",
            self._state.preview_validation_classification,
        )
        payload.setdefault("preview_exception_reason", self._state.preview_exception_reason)
        payload.setdefault("preview_failure_reason", self._state.preview_failure_reason)
        return payload

    def handle_invalid_resync_payload(self) -> PreviewPayloadResult:
        self.clear_resync_pending()
        self._clear_preview_contract_state()
        self.gate.preview_failed("运行时预览同步返回异常，请手动刷新预览")
        return PreviewPayloadResult(
            ok=False,
            title="胶点预览同步失败",
            detail="返回结果格式异常，请手动刷新预览。",
            status_message=self.preview_block_message(StartBlockReason.PREVIEW_FAILED),
            is_error=True,
        )

    def handle_resync_error(self, plan_id: str, error_message: str, error_code=None) -> PreviewPayloadResult | None:
        if not self.is_unrecoverable_resync_error(error_message, error_code):
            return None
        self.clear_resync_pending()
        self._state.current_plan_id = ""
        self._state.current_plan_fingerprint = ""
        self._state.preview_plan_dry_run = None
        self._clear_preview_contract_state()
        self.gate.preview_failed("运行时预览已失效，请重新生成并确认")
        return PreviewPayloadResult(
            ok=False,
            title="胶点预览已失效",
            detail="运行时计划状态已变化，请重新生成并确认预览。",
            status_message="运行时预览已失效，请重新生成并确认",
            is_error=True,
        )

    @staticmethod
    def extract_points(payload: dict, field_name: str) -> list[tuple[float, float]]:
        points: list[tuple[float, float]] = []
        for raw in payload.get(field_name, []) or []:
            if not isinstance(raw, dict):
                continue
            try:
                x_value = float(raw.get("x"))
                y_value = float(raw.get("y"))
            except (TypeError, ValueError):
                continue
            points.append((x_value, y_value))
        return points

    @staticmethod
    def sampling_warning(glue_point_count: int, execution_source_point_count: int) -> str:
        if execution_source_point_count < 1000:
            return ""
        if glue_point_count * 10 < execution_source_point_count * 9:
            return ""
        return (
            "胶点预览疑似退化为轨迹采样点"
            f"（胶点数 {glue_point_count}，执行轨迹源点 {execution_source_point_count}）。"
            "请核对 runtime-gateway 是否错误返回了轨迹采样点。"
        )

    @staticmethod
    def is_unrecoverable_resync_error(error_message: str, error_code=None) -> bool:
        normalized = str(error_message or "").strip().lower()
        if not normalized:
            return False
        unrecoverable_tokens = (
            "plan not found",
            "not found",
            "plan is not latest",
            "not latest",
            "invalid state",
            "plan_id is required",
        )
        if any(token in normalized for token in unrecoverable_tokens):
            return True
        if error_code == 3012 and "plan" in normalized and ("not" in normalized or "invalid" in normalized):
            return True
        return False
