from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
import math
import threading
import time
from typing import TYPE_CHECKING, Protocol

from PyQt5.QtCore import QThread, pyqtSignal

from .preview_gate import DispensePreviewGate, PreviewGateState, PreviewSnapshotMeta, StartBlockReason

# DXF 打开后的自动预览允许更长等待窗口，但该预算不向 resync/confirm/job.start 扩散。
# 真实复杂 DXF 的 plan.prepare 在 mock 链路下可稳定超过 100s，因此自动预览预算提升到 300s 并与 TCP
# 复测窗口对齐，避免 HMI 先超时而后端仍在正常规划。
DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S = 300.0

if TYPE_CHECKING:
    try:
        from hmi_client.client.protocol import CommandProtocol
        from hmi_client.client.tcp_client import TcpClient
    except ImportError:  # pragma: no cover - script-mode fallback
        from client.protocol import CommandProtocol  # type: ignore
        from client.tcp_client import TcpClient  # type: ignore


class PreviewStatusLike(Protocol):
    connected: bool

    def gate_estop_known(self) -> bool: ...

    def gate_door_known(self) -> bool: ...

    def gate_estop_active(self) -> bool: ...

    def gate_door_active(self) -> bool: ...


@dataclass
class PreviewSessionState:
    gate: DispensePreviewGate
    preview_source: str = ""
    preview_kind: str = ""
    glue_point_count: int = 0
    motion_preview_source: str = ""
    motion_preview_kind: str = ""
    motion_preview_point_count: int = 0
    motion_preview_source_point_count: int = 0
    motion_preview_sampling_strategy: str = ""
    motion_preview_is_sampled: bool = False
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
    local_playback_state: str = "idle"
    local_playback_speed_ratio: float = 1.0
    local_playback_progress: float = 0.0
    local_playback_elapsed_s: float = 0.0
    local_playback_duration_s: float = 0.0
    local_playback_point_count: int = 0


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
class MotionPreviewMeta:
    source: str = ""
    kind: str = ""
    point_count: int = 0
    source_point_count: int = 0
    is_sampled: bool = False
    sampling_strategy: str = ""


class PreviewPlaybackState(str, Enum):
    IDLE = "idle"
    PLAYING = "playing"
    PAUSED = "paused"
    FINISHED = "finished"


@dataclass(frozen=True)
class PreviewPlaybackModel:
    points: tuple[tuple[float, float], ...] = ()
    cumulative_lengths: tuple[float, ...] = ()
    total_length_mm: float = 0.0
    duration_s: float = 0.0


@dataclass(frozen=True)
class PreviewPlaybackStatus:
    available: bool
    state: str
    progress: float = 0.0
    elapsed_s: float = 0.0
    duration_s: float = 0.0
    speed_ratio: float = 1.0
    point_count: int = 0


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
    motion_preview: tuple[tuple[float, float], ...] = ()
    motion_preview_meta: MotionPreviewMeta | None = None
    preview_source: str = ""
    preview_kind: str = "glue_points"
    dry_run: bool = False
    preview_warning: str = ""
    motion_preview_warning: str = ""
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
        self._cancel_lock = threading.Lock()
        self._cancelled = False
        self._client_ref = None

    def cancel(self) -> None:
        with self._cancel_lock:
            self._cancelled = True
            client = self._client_ref
        if client is not None:
            try:
                client.disconnect()
            except Exception:
                pass

    def _is_cancelled(self) -> bool:
        with self._cancel_lock:
            return self._cancelled

    def run(self):
        client = None
        ok = False
        payload = {}
        error = ""
        worker_started_at = time.perf_counter()
        plan_prepare_elapsed_ms = 0
        snapshot_elapsed_ms = 0
        try:
            try:
                from hmi_client.client.tcp_client import TcpClient  # type: ignore
                from hmi_client.client.protocol import CommandProtocol  # type: ignore
            except ImportError:  # pragma: no cover - script-mode fallback
                from client.tcp_client import TcpClient  # type: ignore
                from client.protocol import CommandProtocol  # type: ignore

            if self._is_cancelled():
                return
            client = TcpClient(host=self._host, port=self._port)
            with self._cancel_lock:
                self._client_ref = client
            if not client.connect():
                error = "无法连接后端，请检查TCP链路"
            else:
                protocol = CommandProtocol(client)
                plan_prepare_started_at = time.perf_counter()
                plan_ok, plan_payload, plan_error = protocol.dxf_prepare_plan(
                    artifact_id=self._artifact_id,
                    speed_mm_s=self._speed_mm_s,
                    dry_run=self._dry_run,
                    dry_run_speed_mm_s=self._dry_run_speed_mm_s,
                    timeout=DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S,
                )
                plan_prepare_elapsed_ms = int(round((time.perf_counter() - plan_prepare_started_at) * 1000.0))
                if self._is_cancelled():
                    return
                if not plan_ok:
                    error = plan_error
                else:
                    plan_id = str(plan_payload.get("plan_id", "")).strip()
                    if not plan_id:
                        error = "plan.prepare 返回缺少 plan_id"
                    else:
                        snapshot_started_at = time.perf_counter()
                        ok, payload, error = protocol.dxf_preview_snapshot(
                            plan_id=plan_id,
                            max_polyline_points=4000,
                            timeout=DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S,
                        )
                        snapshot_elapsed_ms = int(round((time.perf_counter() - snapshot_started_at) * 1000.0))
                        if self._is_cancelled():
                            return
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
                            performance_profile = plan_payload.get("performance_profile")
                            if isinstance(performance_profile, dict):
                                payload.setdefault("performance_profile", dict(performance_profile))
                            payload["worker_profile"] = {
                                "plan_prepare_rpc_ms": plan_prepare_elapsed_ms,
                                "snapshot_rpc_ms": snapshot_elapsed_ms,
                                "worker_total_ms": int(round((time.perf_counter() - worker_started_at) * 1000.0)),
                            }
        except Exception as exc:  # pragma: no cover - defensive path
            error = str(exc) or "预览快照生成异常"
        finally:
            with self._cancel_lock:
                self._client_ref = None
            if client is not None:
                client.disconnect()
        if self._is_cancelled():
            return
        self.completed.emit(ok, payload if isinstance(payload, dict) else {}, error)


class PreviewSessionOwner:
    RESYNC_RETRY_INTERVAL_S = 2.0
    DEFAULT_LOCAL_PLAYBACK_SPEED_MM_S = 20.0

    def __init__(self, gate: DispensePreviewGate | None = None) -> None:
        self._state = PreviewSessionState(gate=gate or DispensePreviewGate())
        self._playback_model = PreviewPlaybackModel()
        self._playback_started_at_monotonic: float | None = None
        self._playback_elapsed_anchor_s = 0.0

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
            return "旧版 runtime_snapshot"
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
            return "胶点主预览仍是启动 gate 真值；运动轨迹预览用于离线观察路径"
        if normalized == "runtime_snapshot":
            return "旧版 runtime_snapshot，不能直接视为胶点触发预览"
        return "预览来源未知"

    def motion_preview_source_text(self, motion_preview_source: str | None = None) -> str:
        normalized = str(
            self._state.motion_preview_source if motion_preview_source is None else motion_preview_source
        ).strip().lower()
        if normalized == "execution_trajectory_snapshot":
            return "执行轨迹快照"
        if normalized == "process_path_snapshot":
            return "工艺路径快照"
        if normalized == "execution_polyline":
            return "执行轨迹快照"
        if normalized == "offline_local_preview":
            return "离线本地轨迹"
        if normalized == "legacy_execution_polyline":
            return "execution_polyline 兼容层"
        if normalized:
            return normalized
        return "-"

    @staticmethod
    def normalize_preview_gate_message(message: str) -> str:
        normalized = str(message or "").strip()
        lowered = normalized.lower()
        if not normalized:
            return ""
        if "layout" in lowered and "unavailable" in lowered:
            canonical = "preview authority layout id unavailable"
        elif "binding" in lowered and "unavailable" in lowered:
            canonical = "preview binding unavailable"
        elif "authority" in lowered and "unavailable" in lowered:
            canonical = "preview authority unavailable"
        elif "not shared" in lowered and "execution" in lowered:
            canonical = "preview authority is not shared with execution"
        elif "runtime_snapshot" in lowered:
            canonical = "旧版 runtime_snapshot"
        else:
            canonical = ""

        if not canonical or normalized.lower() == canonical:
            return normalized
        return f"{canonical} ({normalized})"

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
        self._state.motion_preview_source = ""
        self._state.motion_preview_kind = ""
        self._state.motion_preview_point_count = 0
        self._state.motion_preview_source_point_count = 0
        self._state.motion_preview_sampling_strategy = ""
        self._state.motion_preview_is_sampled = False
        self._state.preview_validation_classification = ""
        self._state.preview_exception_reason = ""
        self._state.preview_failure_reason = ""
        self._clear_local_playback_state()

    def _clear_local_playback_state(self) -> None:
        self._playback_model = PreviewPlaybackModel()
        self._playback_started_at_monotonic = None
        self._playback_elapsed_anchor_s = 0.0
        self._state.local_playback_state = PreviewPlaybackState.IDLE.value
        self._state.local_playback_speed_ratio = 1.0
        self._state.local_playback_progress = 0.0
        self._state.local_playback_elapsed_s = 0.0
        self._state.local_playback_duration_s = 0.0
        self._state.local_playback_point_count = 0

    @staticmethod
    def _build_local_playback_model(
        points: tuple[tuple[float, float], ...],
        estimated_time_s: float,
    ) -> PreviewPlaybackModel:
        if len(points) < 2:
            return PreviewPlaybackModel()
        cumulative_lengths = [0.0]
        total_length_mm = 0.0
        for index in range(1, len(points)):
            prev_x, prev_y = points[index - 1]
            curr_x, curr_y = points[index]
            total_length_mm += math.hypot(curr_x - prev_x, curr_y - prev_y)
            cumulative_lengths.append(total_length_mm)
        duration_s = float(estimated_time_s or 0.0)
        if duration_s <= 0.0:
            duration_s = max(total_length_mm / PreviewSessionOwner.DEFAULT_LOCAL_PLAYBACK_SPEED_MM_S, 0.1)
        return PreviewPlaybackModel(
            points=tuple(points),
            cumulative_lengths=tuple(cumulative_lengths),
            total_length_mm=total_length_mm,
            duration_s=duration_s,
        )

    def load_local_playback(self, points: tuple[tuple[float, float], ...], estimated_time_s: float) -> None:
        self._clear_local_playback_state()
        model = self._build_local_playback_model(points, estimated_time_s)
        self._playback_model = model
        self._state.local_playback_duration_s = model.duration_s
        self._state.local_playback_point_count = len(model.points)

    def local_playback_status(self) -> PreviewPlaybackStatus:
        return PreviewPlaybackStatus(
            available=len(self._playback_model.points) >= 2,
            state=self._state.local_playback_state,
            progress=self._state.local_playback_progress,
            elapsed_s=self._state.local_playback_elapsed_s,
            duration_s=self._state.local_playback_duration_s,
            speed_ratio=self._state.local_playback_speed_ratio,
            point_count=self._state.local_playback_point_count,
        )

    def local_playback_state_text(self) -> str:
        state_map = {
            PreviewPlaybackState.IDLE.value: "未播放",
            PreviewPlaybackState.PLAYING.value: "播放中",
            PreviewPlaybackState.PAUSED.value: "已暂停",
            PreviewPlaybackState.FINISHED.value: "已完成",
        }
        return state_map.get(self._state.local_playback_state, self._state.local_playback_state or "未播放")

    def _sync_local_playback_timing(self, now_monotonic: float) -> None:
        if self._state.local_playback_state != PreviewPlaybackState.PLAYING.value:
            return
        if self._playback_started_at_monotonic is None or self._playback_model.duration_s <= 0.0:
            return
        elapsed_s = self._playback_elapsed_anchor_s + (
            max(0.0, now_monotonic - self._playback_started_at_monotonic) * self._state.local_playback_speed_ratio
        )
        duration_s = self._playback_model.duration_s
        if elapsed_s >= duration_s:
            elapsed_s = duration_s
            self._state.local_playback_state = PreviewPlaybackState.FINISHED.value
            self._playback_started_at_monotonic = None
            self._playback_elapsed_anchor_s = elapsed_s
        self._state.local_playback_elapsed_s = elapsed_s
        self._state.local_playback_progress = min(1.0, max(0.0, elapsed_s / duration_s))

    def play_local_playback(self, now_monotonic: float) -> PreviewPlaybackStatus:
        if len(self._playback_model.points) < 2:
            return self.local_playback_status()
        if self._state.local_playback_state == PreviewPlaybackState.PLAYING.value:
            return self.local_playback_status()
        if self._state.local_playback_state == PreviewPlaybackState.FINISHED.value:
            self.replay_local_playback(now_monotonic)
            return self.local_playback_status()
        self._playback_elapsed_anchor_s = self._state.local_playback_elapsed_s
        self._playback_started_at_monotonic = now_monotonic
        self._state.local_playback_state = PreviewPlaybackState.PLAYING.value
        return self.local_playback_status()

    def pause_local_playback(self, now_monotonic: float) -> PreviewPlaybackStatus:
        self._sync_local_playback_timing(now_monotonic)
        if self._state.local_playback_state == PreviewPlaybackState.PLAYING.value:
            self._playback_elapsed_anchor_s = self._state.local_playback_elapsed_s
            self._playback_started_at_monotonic = None
            self._state.local_playback_state = PreviewPlaybackState.PAUSED.value
        return self.local_playback_status()

    def replay_local_playback(self, now_monotonic: float) -> PreviewPlaybackStatus:
        if len(self._playback_model.points) < 2:
            return self.local_playback_status()
        self._playback_elapsed_anchor_s = 0.0
        self._state.local_playback_elapsed_s = 0.0
        self._state.local_playback_progress = 0.0
        self._playback_started_at_monotonic = now_monotonic
        self._state.local_playback_state = PreviewPlaybackState.PLAYING.value
        return self.local_playback_status()

    def tick_local_playback(self, now_monotonic: float) -> PreviewPlaybackStatus:
        self._sync_local_playback_timing(now_monotonic)
        return self.local_playback_status()

    def set_local_playback_speed_ratio(self, ratio: float, now_monotonic: float | None = None) -> PreviewPlaybackStatus:
        clamped = min(4.0, max(0.25, float(ratio or 1.0)))
        if now_monotonic is not None:
            self._sync_local_playback_timing(now_monotonic)
        self._playback_elapsed_anchor_s = self._state.local_playback_elapsed_s
        if self._state.local_playback_state == PreviewPlaybackState.PLAYING.value and now_monotonic is not None:
            self._playback_started_at_monotonic = now_monotonic
        self._state.local_playback_speed_ratio = clamped
        return self.local_playback_status()

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
            f"来源: {self.preview_source_text()} | 轨迹: {self.motion_preview_summary_text()}"
        )

    def motion_preview_summary_text(self) -> str:
        source_text = self.motion_preview_source_text()
        if source_text == "-" or self._state.motion_preview_point_count <= 0:
            return "-"

        if (
            self._state.motion_preview_is_sampled and
            self._state.motion_preview_source_point_count > self._state.motion_preview_point_count
        ):
            return (
                f"{source_text}({self._state.motion_preview_point_count}/"
                f"{self._state.motion_preview_source_point_count})"
            )

        return f"{source_text}({self._state.motion_preview_point_count}点)"

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
        self._clear_local_playback_state()

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
        normalized_error = self.normalize_preview_gate_message(error_message)
        self._clear_preview_contract_state()
        self.gate.preview_failed(normalized_error)
        return PreviewPayloadResult(
            ok=False,
            title="胶点预览生成失败",
            detail=normalized_error,
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
        else:
            self._clear_local_playback_state()
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
        motion_preview_payload = payload.get("motion_preview")
        motion_preview_block = motion_preview_payload if isinstance(motion_preview_payload, dict) else {}
        motion_preview = self.extract_points(motion_preview_block, "polyline")
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
        preview_failure_reason = self.normalize_preview_gate_message(
            str(payload.get("preview_failure_reason", self._state.preview_failure_reason or "")).strip()
        )
        motion_preview_source = str(motion_preview_block.get("source", "")).strip().lower()
        motion_preview_kind = str(motion_preview_block.get("kind", "")).strip().lower()
        motion_preview_source_point_count = int(motion_preview_block.get("source_point_count", 0) or 0)
        motion_preview_point_count = int(motion_preview_block.get("point_count", len(motion_preview)) or len(motion_preview))
        motion_preview_is_sampled = bool(
            motion_preview_block.get(
                "is_sampled",
                motion_preview_point_count < motion_preview_source_point_count if motion_preview_source_point_count > 0 else False,
            )
        )
        motion_preview_sampling_strategy = str(motion_preview_block.get("sampling_strategy", "")).strip().lower()
        motion_preview_warning = ""
        if not motion_preview:
            motion_preview = list(execution_polyline)
            if motion_preview:
                motion_preview_source = "legacy_execution_polyline"
                motion_preview_kind = "polyline"
                motion_preview_source_point_count = int(
                    payload.get("execution_polyline_source_point_count", len(motion_preview)) or len(motion_preview)
                )
                motion_preview_point_count = int(
                    payload.get("execution_polyline_point_count", len(motion_preview)) or len(motion_preview)
                )
                motion_preview_is_sampled = motion_preview_point_count < motion_preview_source_point_count
                motion_preview_sampling_strategy = "legacy_execution_polyline_compat"
                motion_preview_warning = "当前结果未显式返回 motion_preview，HMI 已回退到 execution_polyline 兼容字段。"
        if motion_preview and not motion_preview_kind:
            motion_preview_kind = "polyline"
        if motion_preview and motion_preview_point_count <= 0:
            motion_preview_point_count = len(motion_preview)
        if motion_preview and motion_preview_source_point_count <= 0:
            motion_preview_source_point_count = motion_preview_point_count
        motion_preview_meta = MotionPreviewMeta(
            source=motion_preview_source,
            kind=motion_preview_kind,
            point_count=motion_preview_point_count,
            source_point_count=motion_preview_source_point_count,
            is_sampled=motion_preview_is_sampled,
            sampling_strategy=motion_preview_sampling_strategy,
        )
        self._state.current_plan_id = str(payload.get("plan_id", snapshot_id)).strip() or snapshot_id
        self._state.current_plan_fingerprint = snapshot_hash
        self._state.preview_plan_dry_run = preview_dry_run
        self._state.preview_source = preview_source
        self._state.preview_kind = preview_kind
        self._state.glue_point_count = len(glue_points)
        self._state.motion_preview_source = motion_preview_meta.source
        self._state.motion_preview_kind = motion_preview_meta.kind
        self._state.motion_preview_point_count = motion_preview_meta.point_count
        self._state.motion_preview_source_point_count = motion_preview_meta.source_point_count
        self._state.motion_preview_sampling_strategy = motion_preview_meta.sampling_strategy
        self._state.motion_preview_is_sampled = motion_preview_meta.is_sampled
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
                gate_error_message="旧版 runtime_snapshot 预览契约仍在返回",
                title="胶点预览生成失败",
                detail=(
                    "返回结果缺少 glue_points，并检测到旧版 trajectory_polyline/runtime_snapshot；"
                    f"plan_id={self._state.current_plan_id or snapshot_id}。"
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
        self.load_local_playback(tuple(motion_preview), snapshot.estimated_time_s)
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
            motion_preview=tuple(motion_preview),
            motion_preview_meta=motion_preview_meta,
            preview_source=preview_source,
            preview_kind=preview_kind,
            dry_run=preview_dry_run,
            preview_warning=preview_warning,
            motion_preview_warning=motion_preview_warning,
            execution_polyline_source_point_count=execution_source_point_count,
        )

    def build_preflight_decision(
        self,
        *,
        online_ready: bool,
        connected: bool,
        hardware_connected: bool,
        runtime_status_fault: bool,
        status: PreviewStatusLike,
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
    def extract_points(payload: dict[str, object], field_name: str) -> list[tuple[float, float]]:
        points: list[tuple[float, float]] = []
        raw_points = payload.get(field_name, [])
        if not isinstance(raw_points, list):
            return points
        for raw in raw_points:
            if not isinstance(raw, dict):
                continue
            x_raw = raw.get("x")
            y_raw = raw.get("y")
            if x_raw is None or y_raw is None:
                continue
            try:
                x_value = float(x_raw)
                y_value = float(y_raw)
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
