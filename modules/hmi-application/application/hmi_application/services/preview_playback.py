from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
import math

from ..domain.preview_session_types import PreviewSessionState


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


class PreviewPlaybackController:
    DEFAULT_LOCAL_PLAYBACK_SPEED_MM_S = 20.0

    def __init__(self, state: PreviewSessionState) -> None:
        self._state = state
        self._playback_model = PreviewPlaybackModel()
        self._playback_started_at_monotonic: float | None = None
        self._playback_elapsed_anchor_s = 0.0

    def clear(self) -> None:
        self._playback_model = PreviewPlaybackModel()
        self._playback_started_at_monotonic = None
        self._playback_elapsed_anchor_s = 0.0
        self._state.local_playback_state = PreviewPlaybackState.IDLE.value
        self._state.local_playback_speed_ratio = 1.0
        self._state.local_playback_progress = 0.0
        self._state.local_playback_elapsed_s = 0.0
        self._state.local_playback_duration_s = 0.0
        self._state.local_playback_point_count = 0

    @classmethod
    def build_model(
        cls,
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
            duration_s = max(total_length_mm / cls.DEFAULT_LOCAL_PLAYBACK_SPEED_MM_S, 0.1)
        return PreviewPlaybackModel(
            points=tuple(points),
            cumulative_lengths=tuple(cumulative_lengths),
            total_length_mm=total_length_mm,
            duration_s=duration_s,
        )

    def load(self, points: tuple[tuple[float, float], ...], estimated_time_s: float) -> None:
        self.clear()
        model = self.build_model(points, estimated_time_s)
        self._playback_model = model
        self._state.local_playback_duration_s = model.duration_s
        self._state.local_playback_point_count = len(model.points)

    def status(self) -> PreviewPlaybackStatus:
        return PreviewPlaybackStatus(
            available=len(self._playback_model.points) >= 2,
            state=self._state.local_playback_state,
            progress=self._state.local_playback_progress,
            elapsed_s=self._state.local_playback_elapsed_s,
            duration_s=self._state.local_playback_duration_s,
            speed_ratio=self._state.local_playback_speed_ratio,
            point_count=self._state.local_playback_point_count,
        )

    def state_text(self) -> str:
        state_map = {
            PreviewPlaybackState.IDLE.value: "未播放",
            PreviewPlaybackState.PLAYING.value: "播放中",
            PreviewPlaybackState.PAUSED.value: "已暂停",
            PreviewPlaybackState.FINISHED.value: "已完成",
        }
        return state_map.get(self._state.local_playback_state, self._state.local_playback_state or "未播放")

    def sync_timing(self, now_monotonic: float) -> None:
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

    def play(self, now_monotonic: float) -> PreviewPlaybackStatus:
        if len(self._playback_model.points) < 2:
            return self.status()
        if self._state.local_playback_state == PreviewPlaybackState.PLAYING.value:
            return self.status()
        if self._state.local_playback_state == PreviewPlaybackState.FINISHED.value:
            self.replay(now_monotonic)
            return self.status()
        self._playback_elapsed_anchor_s = self._state.local_playback_elapsed_s
        self._playback_started_at_monotonic = now_monotonic
        self._state.local_playback_state = PreviewPlaybackState.PLAYING.value
        return self.status()

    def pause(self, now_monotonic: float) -> PreviewPlaybackStatus:
        self.sync_timing(now_monotonic)
        if self._state.local_playback_state == PreviewPlaybackState.PLAYING.value:
            self._playback_elapsed_anchor_s = self._state.local_playback_elapsed_s
            self._playback_started_at_monotonic = None
            self._state.local_playback_state = PreviewPlaybackState.PAUSED.value
        return self.status()

    def replay(self, now_monotonic: float) -> PreviewPlaybackStatus:
        if len(self._playback_model.points) < 2:
            return self.status()
        self._playback_elapsed_anchor_s = 0.0
        self._state.local_playback_elapsed_s = 0.0
        self._state.local_playback_progress = 0.0
        self._playback_started_at_monotonic = now_monotonic
        self._state.local_playback_state = PreviewPlaybackState.PLAYING.value
        return self.status()

    def tick(self, now_monotonic: float) -> PreviewPlaybackStatus:
        self.sync_timing(now_monotonic)
        return self.status()

    def set_speed_ratio(self, ratio: float, now_monotonic: float | None = None) -> PreviewPlaybackStatus:
        clamped = min(4.0, max(0.25, float(ratio or 1.0)))
        if now_monotonic is not None:
            self.sync_timing(now_monotonic)
        self._playback_elapsed_anchor_s = self._state.local_playback_elapsed_s
        if self._state.local_playback_state == PreviewPlaybackState.PLAYING.value and now_monotonic is not None:
            self._playback_started_at_monotonic = now_monotonic
        self._state.local_playback_speed_ratio = clamped
        return self.status()


__all__ = [
    "PreviewPlaybackController",
    "PreviewPlaybackModel",
    "PreviewPlaybackState",
    "PreviewPlaybackStatus",
]
