from __future__ import annotations

from ..domain.preview_session_types import PreviewSessionState
from ..preview_gate import PreviewGateState


class PreviewStatusProjectionService:
    def __init__(self, state: PreviewSessionState) -> None:
        self._state = state

    @property
    def gate(self):
        return self._state.gate

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
        if normalized == "offline_local_preview":
            return "离线本地轨迹"
        if normalized:
            return normalized
        return "-"

    def motion_preview_summary_text(self) -> str:
        source_text = self.motion_preview_source_text()
        if source_text == "-" or self._state.motion_preview_point_count <= 0:
            return "-"

        if (
            self._state.motion_preview_is_sampled
            and self._state.motion_preview_source_point_count > self._state.motion_preview_point_count
        ):
            return (
                f"{source_text}({self._state.motion_preview_point_count}/"
                f"{self._state.motion_preview_source_point_count})"
            )

        return f"{source_text}({self._state.motion_preview_point_count}点)"

    def info_label_text(self) -> str:
        return (
            f"段数: {self._state.dxf_segment_count} | 长度: {self._state.dxf_total_length_mm:.1f}mm | "
            f"预估: {self._state.dxf_estimated_time_text} | 预览: {self.preview_state_text()} | "
            f"来源: {self.preview_source_text()} | 轨迹: {self.motion_preview_summary_text()}"
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


__all__ = ["PreviewStatusProjectionService"]
