from __future__ import annotations

from dataclasses import dataclass
import math


@dataclass(frozen=True)
class Point2D:
    x: float
    y: float

    def distance_to(self, other: "Point2D") -> float:
        return math.hypot(other.x - self.x, other.y - self.y)

    def to_dict(self) -> dict[str, float]:
        return {"x": float(self.x), "y": float(self.y)}


@dataclass(frozen=True)
class LineSegment:
    start: Point2D
    end: Point2D

    @property
    def length(self) -> float:
        return self.start.distance_to(self.end)

    def to_segment_dict(self) -> dict:
        return {
            "type": "LINE",
            "start": self.start.to_dict(),
            "end": self.end.to_dict(),
        }


@dataclass(frozen=True)
class ArcSegment:
    center: Point2D
    radius: float
    start_angle: float
    end_angle: float

    def to_segment_dict(self) -> dict:
        return {
            "type": "ARC",
            "center": self.center.to_dict(),
            "radius": float(self.radius),
            "start_angle": float(self.start_angle),
            "end_angle": float(self.end_angle),
        }
