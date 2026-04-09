from __future__ import annotations

from .domain.preview_session_types import (
    MotionPreviewMeta,
    PreflightBlockReason,
    PreflightDecision,
    PreviewConfirmResult,
    PreviewDiagnosticNotice,
    PreviewPayloadResult,
    PreviewSessionState,
    PreviewStatusLike,
)
from .preview_gate import DispensePreviewGate
from .services.preview_playback import (
    PreviewPlaybackController,
    PreviewPlaybackModel,
    PreviewPlaybackState,
    PreviewPlaybackStatus,
)
from .services.preview_payload_authority import PreviewPayloadAuthorityService
from .services.preview_preflight import PreviewPreflightService
from .services.preview_status_projection import PreviewStatusProjectionService


class PreviewSessionOwner:
    RESYNC_RETRY_INTERVAL_S = PreviewPayloadAuthorityService.RESYNC_RETRY_INTERVAL_S
    DEFAULT_LOCAL_PLAYBACK_SPEED_MM_S = PreviewPlaybackController.DEFAULT_LOCAL_PLAYBACK_SPEED_MM_S

    def __init__(self, gate: DispensePreviewGate | None = None) -> None:
        self._state = PreviewSessionState(gate=gate or DispensePreviewGate())
        self._playback = PreviewPlaybackController(self._state)
        self._preflight = PreviewPreflightService(self._state)
        self._projection = PreviewStatusProjectionService(self._state)
        self._authority = PreviewPayloadAuthorityService(self._state, self._playback, self._preflight)

    @property
    def gate(self) -> DispensePreviewGate:
        return self._state.gate

    @property
    def state(self) -> PreviewSessionState:
        return self._state

    def set_refresh_inflight(self, inflight: bool) -> None:
        self._authority.set_refresh_inflight(inflight)

    def set_resync_pending(self, pending: bool) -> None:
        self._authority.set_resync_pending(pending)

    def mark_resync_pending(self) -> None:
        self._authority.mark_resync_pending()

    def clear_resync_pending(self) -> None:
        self._authority.clear_resync_pending()

    def preview_state_text(self) -> str:
        return self._projection.preview_state_text()

    def preview_source_text(self, preview_source: str | None = None) -> str:
        return self._projection.preview_source_text(preview_source)

    def preview_source_warning(self, preview_source: str | None = None) -> str:
        return self._projection.preview_source_warning(preview_source)

    def motion_preview_source_text(self, motion_preview_source: str | None = None) -> str:
        return self._projection.motion_preview_source_text(motion_preview_source)

    @staticmethod
    def normalize_preview_gate_message(message: str) -> str:
        return PreviewPayloadAuthorityService.normalize_preview_gate_message(message)

    def preview_block_message(self, reason) -> str:
        return self._preflight.preview_block_message(reason)

    def load_local_playback(self, points: tuple[tuple[float, float], ...], estimated_time_s: float) -> None:
        self._playback.load(points, estimated_time_s)

    def local_playback_status(self) -> PreviewPlaybackStatus:
        return self._playback.status()

    def local_playback_state_text(self) -> str:
        return self._playback.state_text()

    def play_local_playback(self, now_monotonic: float) -> PreviewPlaybackStatus:
        return self._playback.play(now_monotonic)

    def pause_local_playback(self, now_monotonic: float) -> PreviewPlaybackStatus:
        return self._playback.pause(now_monotonic)

    def replay_local_playback(self, now_monotonic: float) -> PreviewPlaybackStatus:
        return self._playback.replay(now_monotonic)

    def tick_local_playback(self, now_monotonic: float) -> PreviewPlaybackStatus:
        return self._playback.tick(now_monotonic)

    def set_local_playback_speed_ratio(self, ratio: float, now_monotonic: float | None = None) -> PreviewPlaybackStatus:
        return self._playback.set_speed_ratio(ratio, now_monotonic)

    def info_label_text(self) -> str:
        return self._projection.info_label_text()

    def motion_preview_summary_text(self) -> str:
        return self._projection.motion_preview_summary_text()

    def should_enable_refresh(self, *, offline_mode: bool, connected: bool, dxf_loaded: bool) -> bool:
        return self._projection.should_enable_refresh(
            offline_mode=offline_mode,
            connected=connected,
            dxf_loaded=dxf_loaded,
        )

    def update_dxf_info(self, *, total_length_mm: float, total_segments: int, speed_mm_s: float) -> None:
        self._projection.update_dxf_info(
            total_length_mm=total_length_mm,
            total_segments=total_segments,
            speed_mm_s=speed_mm_s,
        )

    def update_estimated_time(self, *, speed_mm_s: float) -> None:
        self._projection.update_estimated_time(speed_mm_s=speed_mm_s)

    def reset_for_loaded_dxf(self, *, segment_count: int = 0) -> None:
        self._authority.reset_for_loaded_dxf(segment_count=segment_count)

    def invalidate_plan(self) -> None:
        self._authority.invalidate_plan()

    def begin_preview_generation(self) -> None:
        self._authority.begin_preview_generation()

    def build_confirmation_summary(self) -> str:
        return self._preflight.build_confirmation_summary()

    def current_preview_diagnostic_notice(self) -> PreviewDiagnosticNotice | None:
        return self._preflight.current_preview_diagnostic_notice()

    def validate_before_confirmation(self) -> PreviewConfirmResult:
        return self._preflight.validate_before_confirmation()

    def apply_confirmation_payload(self, payload: dict) -> PreviewConfirmResult:
        return self._preflight.apply_confirmation_payload(payload)

    def handle_worker_error(self, error_message: str) -> PreviewPayloadResult:
        return self._authority.handle_worker_error(error_message)

    def handle_local_failure(
        self,
        *,
        gate_error_message: str,
        title: str,
        detail: str,
        clear_source: bool = False,
    ) -> PreviewPayloadResult:
        return self._authority.handle_local_failure(
            gate_error_message=gate_error_message,
            title=title,
            detail=detail,
            clear_source=clear_source,
        )

    def process_snapshot_payload(
        self,
        payload: dict,
        *,
        current_dry_run: bool,
        source: str = "worker",
    ) -> PreviewPayloadResult:
        return self._authority.process_snapshot_payload(
            payload,
            current_dry_run=current_dry_run,
            source=source,
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
        decision = self._preflight.build_preflight_decision(
            online_ready=online_ready,
            connected=connected,
            hardware_connected=hardware_connected,
            runtime_status_fault=runtime_status_fault,
            status=status,
            dry_run=dry_run,
        )
        if decision.reason == PreflightBlockReason.PREVIEW_MODE_MISMATCH:
            self._authority.invalidate_plan()
        return decision

    def should_request_runtime_resync(
        self,
        *,
        offline_mode: bool,
        connected: bool,
        production_running: bool,
        current_job_id: str,
        now_monotonic: float,
    ) -> bool:
        return self._authority.should_request_runtime_resync(
            offline_mode=offline_mode,
            connected=connected,
            production_running=production_running,
            current_job_id=current_job_id,
            now_monotonic=now_monotonic,
        )

    def prepare_resync_payload(self, payload: dict, *, current_dry_run: bool) -> dict:
        return self._authority.prepare_resync_payload(payload, current_dry_run=current_dry_run)

    def handle_invalid_resync_payload(self) -> PreviewPayloadResult:
        return self._authority.handle_invalid_resync_payload()

    def handle_resync_error(self, plan_id: str, error_message: str, error_code=None) -> PreviewPayloadResult | None:
        return self._authority.handle_resync_error(plan_id, error_message, error_code)

    @staticmethod
    def extract_points(payload: dict[str, object], field_name: str) -> list[tuple[float, float]]:
        return PreviewPayloadAuthorityService.extract_points(payload, field_name)

    @staticmethod
    def extract_float_list(payload: dict[str, object], field_name: str) -> list[float]:
        return PreviewPayloadAuthorityService.extract_float_list(payload, field_name)

    @staticmethod
    def sampling_warning(glue_point_count: int, execution_source_point_count: int) -> str:
        return PreviewPayloadAuthorityService.sampling_warning(glue_point_count, execution_source_point_count)

    @staticmethod
    def is_unrecoverable_resync_error(error_message: str, error_code=None) -> bool:
        return PreviewPayloadAuthorityService.is_unrecoverable_resync_error(error_message, error_code)


__all__ = [
    "MotionPreviewMeta",
    "PreflightBlockReason",
    "PreflightDecision",
    "PreviewConfirmResult",
    "PreviewDiagnosticNotice",
    "PreviewPayloadResult",
    "PreviewPlaybackModel",
    "PreviewPlaybackState",
    "PreviewPlaybackStatus",
    "PreviewSessionOwner",
    "PreviewSessionState",
    "PreviewStatusLike",
]
