from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class IoDelay:
    channel: str
    delay_seconds: float


@dataclass(frozen=True)
class ValveConfig:
    open_delay_seconds: float
    close_delay_seconds: float


@dataclass(frozen=True)
class ExportDefaults:
    timestep_seconds: float = 0.001
    max_simulation_time_seconds: float = 600.0
    max_velocity: float = 220.0
    max_acceleration: float = 600.0
    io_delays: tuple[IoDelay, ...] = (
        IoDelay(channel="DO_VALVE", delay_seconds=0.003),
        IoDelay(channel="DO_CAMERA_TRIGGER", delay_seconds=0.001),
    )
    valve: ValveConfig = ValveConfig(
        open_delay_seconds=0.012,
        close_delay_seconds=0.008,
    )


DEFAULT_EXPORTS = ExportDefaults()


@dataclass(frozen=True)
class TriggerPoint:
    x_mm: float
    y_mm: float
    channel: str
    state: bool
