from __future__ import annotations

from dataclasses import asdict, dataclass
from pathlib import Path


@dataclass(frozen=True)
class PreviewRequest:
    input_path: Path
    output_dir: Path
    title: str
    speed_mm_s: float = 10.0
    curve_tolerance_mm: float = 0.25
    max_points: int = 12000


@dataclass(frozen=True)
class PreviewArtifact:
    preview_path: str
    entity_count: int
    segment_count: int
    point_count: int
    total_length_mm: float
    estimated_time_s: float
    width_mm: float
    height_mm: float

    def to_dict(self) -> dict:
        return asdict(self)
