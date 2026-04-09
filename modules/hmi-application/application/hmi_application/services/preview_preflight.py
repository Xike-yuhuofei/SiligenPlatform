from __future__ import annotations

from ..domain.preview_session_types import (
    PreflightBlockReason,
    PreflightDecision,
    PreviewConfirmResult,
    PreviewDiagnosticNotice,
    PreviewSessionState,
    PreviewStatusLike,
)
from ..preview_gate import StartBlockReason


def _normalize_preview_exception_reason(reason: str) -> str:
    text = str(reason).strip()
    if not text:
        return ""
    if text == "span spacing outside configured window but accepted as explicit exception":
        return "点位间距未落入当前配置窗口，但已按例外规则放行，可继续预览与执行。"
    return text


class PreviewPreflightService:
    def __init__(self, state: PreviewSessionState) -> None:
        self._state = state

    @property
    def gate(self):
        return self._state.gate

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

    @staticmethod
    def map_preview_contract_reason(reason: StartBlockReason) -> PreflightBlockReason:
        if reason == StartBlockReason.INVALID_SOURCE:
            return PreflightBlockReason.INVALID_SOURCE
        if reason == StartBlockReason.INVALID_KIND:
            return PreflightBlockReason.INVALID_KIND
        if reason == StartBlockReason.EMPTY_GLUE_POINTS:
            return PreflightBlockReason.EMPTY_GLUE_POINTS
        return PreflightBlockReason.PREVIEW_FAILED

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
        diagnostic_notice = self.current_preview_diagnostic_notice()
        if diagnostic_notice is not None:
            summary = f"{summary}\n\n{diagnostic_notice.title}: {diagnostic_notice.detail}"
        return summary

    def current_preview_diagnostic_notice(self) -> PreviewDiagnosticNotice | None:
        if self._state.preview_validation_classification == "fail":
            return None
        normalized_exception_reason = _normalize_preview_exception_reason(self._state.preview_exception_reason)
        if self._state.preview_diagnostic_code == "process_path_fragmentation":
            detail = (
                "路径较碎，当前结果已按例外规则放行，可继续预览与执行。"
                "这通常意味着 DXF 几何连通性较差，或导入顺序导致路径被拆成多段。"
            )
            if normalized_exception_reason:
                detail = f"{detail} {normalized_exception_reason}"
            return PreviewDiagnosticNotice("路径碎片化提示", detail)
        if self._state.preview_validation_classification == "pass_with_exception" and normalized_exception_reason:
            return PreviewDiagnosticNotice("可继续提示", normalized_exception_reason)
        return None

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
                self.map_preview_contract_reason(contract_decision.reason),
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


__all__ = ["PreviewPreflightService"]
