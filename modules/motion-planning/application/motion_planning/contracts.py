from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Point2D:
    x: float
    y: float

    def distance_to(self, other: "Point2D") -> float:
        dx = other.x - self.x
        dy = other.y - self.y
        return (dx * dx + dy * dy) ** 0.5


@dataclass(frozen=True)
class TrajectorySample:
    t: float
    position: Point2D
    velocity: Point2D
    process_tag: int = 0
    dispense_on: bool = False
    flow_rate: float = 0.0
    scalar_acceleration: float = 0.0
    scalar_velocity: float = 0.0

    def to_dict(self) -> dict:
        return {
            "t": float(self.t),
            "position": {"x": float(self.position.x), "y": float(self.position.y), "z": 0.0},
            "velocity": {"x": float(self.velocity.x), "y": float(self.velocity.y), "z": 0.0},
            "processTag": int(self.process_tag),
            "dispenseOn": bool(self.dispense_on),
            "flowRate": float(self.flow_rate),
        }


@dataclass(frozen=True)
class PlanningReport:
    total_length_mm: float = 0.0
    total_time_s: float = 0.0
    max_velocity_observed: float = 0.0
    max_acceleration_observed: float = 0.0
    max_jerk_observed: float = 0.0
    constraint_violations: int = 0
    time_integration_error_s: float = 0.0
    time_integration_fallbacks: int = 0
    jerk_limit_enforced: bool = False
    jerk_plan_failed: bool = False
    segment_count: int = 0
    rapid_segment_count: int = 0
    rapid_length_mm: float = 0.0
    corner_segment_count: int = 0
    discontinuity_count: int = 0

    def to_dict(self) -> dict:
        return {
            "totalLengthMm": float(self.total_length_mm),
            "totalTimeS": float(self.total_time_s),
            "maxVelocityObserved": float(self.max_velocity_observed),
            "maxAccelerationObserved": float(self.max_acceleration_observed),
            "maxJerkObserved": float(self.max_jerk_observed),
            "constraintViolations": int(self.constraint_violations),
            "timeIntegrationErrorS": float(self.time_integration_error_s),
            "timeIntegrationFallbacks": int(self.time_integration_fallbacks),
            "jerkLimitEnforced": bool(self.jerk_limit_enforced),
            "jerkPlanFailed": bool(self.jerk_plan_failed),
            "segmentCount": int(self.segment_count),
            "rapidSegmentCount": int(self.rapid_segment_count),
            "rapidLengthMm": float(self.rapid_length_mm),
            "cornerSegmentCount": int(self.corner_segment_count),
            "discontinuityCount": int(self.discontinuity_count),
        }


@dataclass(frozen=True)
class TrajectoryArtifact:
    samples: list[TrajectorySample]
    total_time: float
    total_length: float
    planning_report: PlanningReport

    def to_dict(self) -> dict:
        return {
            "points": [sample.to_dict() for sample in self.samples],
            "totalTime": float(self.total_time),
            "totalLength": float(self.total_length),
            "planningReport": self.planning_report.to_dict(),
        }
