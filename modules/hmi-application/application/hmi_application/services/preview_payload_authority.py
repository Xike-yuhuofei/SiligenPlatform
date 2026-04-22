from __future__ import annotations

import math

from ..domain.preview_session_types import MotionPreviewMeta, PreviewPayloadResult, PreviewSessionState
from ..preview_gate import PreviewGateState, PreviewSnapshotMeta, StartBlockReason
from .preview_playback import PreviewPlaybackController
from .preview_preflight import PreviewPreflightService


class PreviewPayloadAuthorityService:
    RESYNC_RETRY_INTERVAL_S = 2.0

    def __init__(
        self,
        state: PreviewSessionState,
        playback: PreviewPlaybackController,
        preflight: PreviewPreflightService,
    ) -> None:
        self._state = state
        self._playback = playback
        self._preflight = preflight

    @property
    def gate(self):
        return self._state.gate

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
        self._state.preview_diagnostic_code = ""
        self._playback.clear()

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
        self._playback.clear()

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

    def handle_worker_error(self, error_message: str) -> PreviewPayloadResult:
        normalized_error = self.normalize_preview_gate_message(error_message)
        self._clear_preview_contract_state()
        self.gate.preview_failed(normalized_error)
        return PreviewPayloadResult(
            ok=False,
            title="胶点预览生成失败",
            detail=normalized_error,
            status_message=self._preflight.preview_block_message(StartBlockReason.PREVIEW_FAILED),
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
            self._playback.clear()
        self.gate.preview_failed(gate_error_message)
        return PreviewPayloadResult(
            ok=False,
            title=title,
            detail=detail,
            status_message=self._preflight.preview_block_message(StartBlockReason.PREVIEW_FAILED),
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
        glue_reveal_lengths_mm = self.extract_float_list(payload, "glue_reveal_lengths_mm")
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
        preview_diagnostic_code = str(
            payload.get("preview_diagnostic_code", self._state.preview_diagnostic_code or "")
        ).strip().lower()
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
        current_plan_id = str(payload.get("plan_id", snapshot_id)).strip() or snapshot_id
        self._state.current_plan_id = current_plan_id
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
        self._state.preview_diagnostic_code = preview_diagnostic_code
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
                    f"plan_id={current_plan_id}。"
                    "当前 HMI 连接的 runtime-gateway 很可能还是旧构建。"
                ),
            )

        source_decision = self.gate.validate_preview_source(preview_source)
        if not source_decision.allowed:
            return self.handle_local_failure(
                gate_error_message=self._preflight.preview_block_message(source_decision.reason),
                title="胶点预览生成失败",
                detail=f"返回结果的 preview_source={preview_source}，不属于 planned_glue_snapshot 权威预览契约。",
            )

        kind_decision = self.gate.validate_preview_kind(preview_kind)
        if not kind_decision.allowed:
            return self.handle_local_failure(
                gate_error_message=self._preflight.preview_block_message(kind_decision.reason),
                title="胶点预览生成失败",
                detail=f"返回结果的 preview_kind={preview_kind}，不属于 glue_points 主预览语义。",
            )

        glue_point_decision = self.gate.validate_glue_points(len(glue_points))
        if not glue_point_decision.allowed:
            return self.handle_local_failure(
                gate_error_message=self._preflight.preview_block_message(glue_point_decision.reason),
                title="胶点预览生成失败",
                detail="返回结果缺少非空 glue_points。请核对 runtime-gateway 是否已升级到 planned_glue_snapshot 契约。",
            )
        if not motion_preview:
            return self.handle_local_failure(
                gate_error_message="运行时快照缺少 motion_preview",
                title="胶点预览生成失败",
                detail="返回结果缺少 motion_preview，无法保证点胶头运动轨迹与真实执行轨迹一致。",
            )
        if motion_preview_source != "execution_trajectory_snapshot":
            return self.handle_local_failure(
                gate_error_message=f"运行时快照返回了非执行真值运动轨迹来源: {motion_preview_source or 'missing'}",
                title="胶点预览生成失败",
                detail=(
                    "返回结果的 motion_preview.source="
                    f"{motion_preview_source or 'missing'}，不是 execution_trajectory_snapshot。"
                ),
            )
        if motion_preview_kind != "polyline":
            return self.handle_local_failure(
                gate_error_message=f"运行时快照返回了未知 motion_preview.kind: {motion_preview_kind or 'missing'}",
                title="胶点预览生成失败",
                detail=(
                    "返回结果的 motion_preview.kind="
                    f"{motion_preview_kind or 'missing'}，不属于支持的 polyline 轨迹语义。"
                ),
            )
        if motion_preview_sampling_strategy not in {
            "execution_trajectory_geometry_preserving",
            "execution_trajectory_geometry_preserving_clamp",
        }:
            return self.handle_local_failure(
                gate_error_message=(
                    "运行时快照返回了未知 motion_preview.sampling_strategy: "
                    f"{motion_preview_sampling_strategy or 'missing'}"
                ),
                title="胶点预览生成失败",
                detail=(
                    "返回结果的 motion_preview.sampling_strategy="
                    f"{motion_preview_sampling_strategy or 'missing'}，"
                    "不是 execution_trajectory_geometry_preserving / "
                    "execution_trajectory_geometry_preserving_clamp。"
                ),
            )
        glue_reveal_lengths_reason = self.validate_glue_reveal_lengths(
            glue_points=glue_points,
            motion_preview=motion_preview,
            glue_reveal_lengths_mm=glue_reveal_lengths_mm,
        )
        if glue_reveal_lengths_reason:
            return self.handle_local_failure(
                gate_error_message=(
                    "运行时快照返回了非法 glue_reveal_lengths_mm: "
                    f"{glue_reveal_lengths_reason}"
                ),
                title="胶点预览生成失败",
                detail=self.glue_reveal_lengths_error_detail(glue_reveal_lengths_reason),
            )

        self._state.glue_point_count = len(glue_points)
        self._state.motion_preview_source = motion_preview_meta.source
        self._state.motion_preview_kind = motion_preview_meta.kind
        self._state.motion_preview_point_count = motion_preview_meta.point_count
        self._state.motion_preview_source_point_count = motion_preview_meta.source_point_count
        self._state.motion_preview_sampling_strategy = motion_preview_meta.sampling_strategy
        self._state.motion_preview_is_sampled = motion_preview_meta.is_sampled

        snapshot = PreviewSnapshotMeta(
            snapshot_id=snapshot_id,
            snapshot_hash=snapshot_hash,
            segment_count=int(payload.get("segment_count", 0) or 0),
            point_count=int(payload.get("glue_point_count", payload.get("point_count", len(glue_points))) or len(glue_points)),
            total_length_mm=float(payload.get("total_length_mm", 0.0) or 0.0),
            estimated_time_s=float(payload.get("estimated_time_s", 0.0) or 0.0),
            generated_at=str(payload.get("generated_at", "")),
        )
        self._playback.load(tuple(motion_preview), snapshot.estimated_time_s)
        preview_warning = self.sampling_warning(
            glue_point_count=snapshot.point_count,
            execution_source_point_count=motion_preview_meta.source_point_count,
        )
        preview_diagnostic_notice = self._preflight.current_preview_diagnostic_notice()
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
            glue_reveal_lengths_mm=tuple(glue_reveal_lengths_mm),
            motion_preview=tuple(motion_preview),
            motion_preview_meta=motion_preview_meta,
            preview_source=preview_source,
            preview_kind=preview_kind,
            dry_run=preview_dry_run,
            preview_warning=preview_warning,
            preview_diagnostic_notice=preview_diagnostic_notice,
            motion_preview_warning=motion_preview_warning,
        )

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
        payload.setdefault("preview_validation_classification", self._state.preview_validation_classification)
        payload.setdefault("preview_exception_reason", self._state.preview_exception_reason)
        payload.setdefault("preview_failure_reason", self._state.preview_failure_reason)
        payload.setdefault("preview_diagnostic_code", self._state.preview_diagnostic_code)
        return payload

    def handle_invalid_resync_payload(self) -> PreviewPayloadResult:
        self.clear_resync_pending()
        self._clear_preview_contract_state()
        self.gate.preview_failed("运行时预览同步返回异常，请手动刷新预览")
        return PreviewPayloadResult(
            ok=False,
            title="胶点预览同步失败",
            detail="返回结果格式异常，请手动刷新预览。",
            status_message=self._preflight.preview_block_message(StartBlockReason.PREVIEW_FAILED),
            is_error=True,
        )

    def handle_resync_error(self, plan_id: str, error_message: str, error_code=None) -> PreviewPayloadResult | None:
        del plan_id
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
    def extract_float_list(payload: dict[str, object], field_name: str) -> list[float]:
        values: list[float] = []
        raw_values = payload.get(field_name, [])
        if not isinstance(raw_values, list):
            return values
        for raw in raw_values:
            try:
                value = float(raw)
            except (TypeError, ValueError):
                return []
            if not math.isfinite(value):
                return []
            values.append(value)
        return values

    @staticmethod
    def validate_glue_reveal_lengths(
        *,
        glue_points: list[tuple[float, float]],
        motion_preview: list[tuple[float, float]],
        glue_reveal_lengths_mm: list[float],
    ) -> str:
        if not glue_reveal_lengths_mm:
            return "missing"
        if len(glue_reveal_lengths_mm) != len(glue_points):
            return "length_mismatch"

        previous_length = glue_reveal_lengths_mm[0]
        for current_length in glue_reveal_lengths_mm[1:]:
            if current_length + 1e-6 < previous_length:
                return "non_monotonic"
            previous_length = current_length

        segments: list[tuple[float, float, float, float, float]] = []
        total_length = 0.0
        for index in range(1, len(motion_preview)):
            start_x, start_y = motion_preview[index - 1]
            end_x, end_y = motion_preview[index]
            segment_length = ((end_x - start_x) ** 2 + (end_y - start_y) ** 2) ** 0.5
            if segment_length <= 1e-9:
                continue
            segments.append((start_x, start_y, end_x, end_y, total_length))
            total_length += segment_length
        if not segments:
            return "motion_preview_too_short"

        distance_tolerance_mm = 2.0
        distance_tolerance_sq = distance_tolerance_mm ** 2
        for (point_x, point_y), reveal_length in zip(glue_points, glue_reveal_lengths_mm):
            if reveal_length < -1e-6 or reveal_length > total_length + distance_tolerance_mm:
                return "beyond_motion_length"
            clamped_reveal_length = max(0.0, min(total_length, reveal_length))
            projected_x, projected_y = motion_preview[-1]
            for start_x, start_y, end_x, end_y, start_length in segments:
                segment_length = ((end_x - start_x) ** 2 + (end_y - start_y) ** 2) ** 0.5
                if segment_length <= 1e-9:
                    continue
                end_length = start_length + segment_length
                if clamped_reveal_length > end_length + 1e-6:
                    continue
                ratio = max(0.0, min(1.0, (clamped_reveal_length - start_length) / segment_length))
                projected_x = start_x + ((end_x - start_x) * ratio)
                projected_y = start_y + ((end_y - start_y) * ratio)
                break
            distance_sq = ((point_x - projected_x) ** 2) + ((point_y - projected_y) ** 2)
            if distance_sq > distance_tolerance_sq:
                return "geometry_mismatch"
        return ""

    @staticmethod
    def glue_reveal_lengths_error_detail(reason: str) -> str:
        normalized = str(reason or "").strip().lower()
        detail_map = {
            "missing": (
                "返回结果缺少 glue_reveal_lengths_mm。当前 HMI 已禁用基于 motion_preview 的兼容投影显隐算法，"
                "无法保证胶点显隐与真实执行轨迹一致。"
            ),
            "length_mismatch": (
                "返回结果的 glue_reveal_lengths_mm 数量与 glue_points 不一致。当前 HMI 已禁用兼容投影显隐算法。"
            ),
            "non_monotonic": (
                "返回结果的 glue_reveal_lengths_mm 不是单调递增序列。当前 HMI 已禁用兼容投影显隐算法。"
            ),
            "motion_preview_too_short": (
                "返回结果的 motion_preview 点数不足，无法校验 glue_reveal_lengths_mm。当前 HMI 已禁用兼容投影显隐算法。"
            ),
            "beyond_motion_length": (
                "返回结果的 glue_reveal_lengths_mm 超出 motion_preview 总长度。当前 HMI 已禁用兼容投影显隐算法。"
            ),
            "geometry_mismatch": (
                "返回结果的 glue_reveal_lengths_mm 与 glue_points / motion_preview 几何不一致。"
                "当前 HMI 已禁用兼容投影显隐算法。"
            ),
        }
        return detail_map.get(
            normalized,
            "返回结果的 glue_reveal_lengths_mm 不合法。当前 HMI 已禁用兼容投影显隐算法。",
        )

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


__all__ = ["PreviewPayloadAuthorityService"]
