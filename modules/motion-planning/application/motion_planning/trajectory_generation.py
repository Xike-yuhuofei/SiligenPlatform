from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Sequence

from motion_planning.contracts import PlanningReport, Point2D, TrajectoryArtifact, TrajectorySample


class ScalarTrajectory:
    def __init__(self, total_length: float, vmax: float, amax: float) -> None:
        if total_length <= 0.0:
            raise ValueError("Total length is zero")
        if vmax <= 0.0:
            raise ValueError("vmax must be positive")
        if amax <= 0.0:
            raise ValueError("amax must be positive")

        accel_time = vmax / amax
        accel_distance = 0.5 * amax * accel_time * accel_time

        if 2.0 * accel_distance >= total_length:
            self.peak_velocity = (total_length * amax) ** 0.5
            self.accel_time = self.peak_velocity / amax
            self.accel_distance = 0.5 * amax * self.accel_time * self.accel_time
            self.cruise_distance = 0.0
            self.cruise_time = 0.0
        else:
            self.peak_velocity = vmax
            self.accel_time = accel_time
            self.accel_distance = accel_distance
            self.cruise_distance = total_length - 2.0 * accel_distance
            self.cruise_time = self.cruise_distance / vmax

        self.amax = amax
        self.total_length = total_length
        self.duration = 2.0 * self.accel_time + self.cruise_time

    def at_time(self, t: float):
        clamped_t = min(max(t, 0.0), self.duration)
        if clamped_t <= self.accel_time:
            position = 0.5 * self.amax * clamped_t * clamped_t
            velocity = self.amax * clamped_t
            acceleration = self.amax
        elif clamped_t <= self.accel_time + self.cruise_time:
            cruise_t = clamped_t - self.accel_time
            position = self.accel_distance + self.peak_velocity * cruise_t
            velocity = self.peak_velocity
            acceleration = 0.0
        else:
            decel_t = clamped_t - self.accel_time - self.cruise_time
            position = (
                self.accel_distance
                + self.cruise_distance
                + self.peak_velocity * decel_t
                - 0.5 * self.amax * decel_t * decel_t
            )
            velocity = max(0.0, self.peak_velocity - self.amax * decel_t)
            acceleration = -self.amax

        return [min(position, self.total_length)], [velocity], [acceleration]

    def get_first_time_at_position(self, _axis: int, s: float):
        clamped_s = min(max(s, 0.0), self.total_length)
        if clamped_s <= self.accel_distance:
            return (2.0 * clamped_s / self.amax) ** 0.5
        if clamped_s <= self.accel_distance + self.cruise_distance:
            return self.accel_time + (clamped_s - self.accel_distance) / self.peak_velocity

        remaining = max(0.0, self.total_length - clamped_s)
        return self.duration - (2.0 * remaining / self.amax) ** 0.5


def load_points(path: Path) -> list[Point2D]:
    data = json.loads(path.read_text(encoding="utf-8"))
    points: list[Point2D] = []
    for item in data.get("points", []):
        points.append(Point2D(float(item.get("x", 0.0)), float(item.get("y", 0.0))))
    return points


def cumulative_lengths(points: list[Point2D]) -> list[float]:
    lengths = [0.0]
    total = 0.0
    for index in range(1, len(points)):
        total += points[index - 1].distance_to(points[index])
        lengths.append(total)
    return lengths


def find_segment(cum_lengths: list[float], s: float) -> int:
    for index in range(1, len(cum_lengths)):
        if s <= cum_lengths[index]:
            return index - 1
    return max(0, len(cum_lengths) - 2)


def sample_trajectory(
    points: list[Point2D],
    cum_lengths: list[float],
    traj,
    sample_dt: float,
    sample_ds: float,
) -> list[TrajectorySample]:
    samples: list[TrajectorySample] = []
    duration = float(traj.duration)
    times: list[float] = []
    if sample_ds > 0.0:
        current_s = 0.0
        while current_s <= cum_lengths[-1] + 1e-9:
            current_t = traj.get_first_time_at_position(0, current_s)
            if current_t is None:
                break
            times.append(float(current_t))
            current_s += sample_ds
    else:
        dt = 0.01 if sample_dt <= 0.0 else sample_dt
        current_t = 0.0
        while current_t <= duration + 1e-9:
            times.append(current_t)
            current_t += dt
        if times[-1] < duration - 1e-6:
            times.append(duration)

    for current_t in times:
        pos, vel, acc = traj.at_time(current_t)
        current_s = float(pos[0])
        seg_idx = find_segment(cum_lengths, current_s)
        seg_start = cum_lengths[seg_idx]
        seg_len = max(1e-9, cum_lengths[seg_idx + 1] - seg_start)
        ratio = (current_s - seg_start) / seg_len
        start_point = points[seg_idx]
        end_point = points[seg_idx + 1]
        x = start_point.x + (end_point.x - start_point.x) * ratio
        y = start_point.y + (end_point.y - start_point.y) * ratio

        dx = (end_point.x - start_point.x) / seg_len
        dy = (end_point.y - start_point.y) / seg_len
        vx = dx * float(vel[0])
        vy = dy * float(vel[0])
        samples.append(
            TrajectorySample(
                t=float(current_t),
                position=Point2D(x, y),
                velocity=Point2D(vx, vy),
                scalar_acceleration=float(acc[0]),
                scalar_velocity=float(vel[0]),
            )
        )
    return samples


def build_report(
    samples: list[TrajectorySample],
    vmax: float,
    amax: float,
    jmax: float,
    jerk_enforced: bool,
) -> PlanningReport:
    max_v = 0.0
    max_a = 0.0
    max_j = 0.0
    violations = 0
    last_acc = None
    last_t = None
    for sample in samples:
        velocity = abs(sample.scalar_velocity)
        acceleration = abs(sample.scalar_acceleration)
        max_v = max(max_v, velocity)
        max_a = max(max_a, acceleration)
        if last_acc is not None and last_t is not None:
            dt = max(1e-9, sample.t - last_t)
            jerk = abs(sample.scalar_acceleration - last_acc) / dt
            max_j = max(max_j, jerk)
            if jerk_enforced and jerk > jmax + 1e-6:
                violations += 1
        if velocity > vmax + 1e-6 or acceleration > amax + 1e-6:
            violations += 1
        last_acc = sample.scalar_acceleration
        last_t = sample.t

    return PlanningReport(
        total_time_s=samples[-1].t if samples else 0.0,
        max_velocity_observed=max_v,
        max_acceleration_observed=max_a,
        max_jerk_observed=max_j,
        constraint_violations=violations,
        jerk_limit_enforced=jerk_enforced,
    )


def build_trajectory_artifact(
    points: list[Point2D],
    vmax: float,
    amax: float,
    jmax: float,
    sample_dt: float,
    sample_ds: float,
) -> TrajectoryArtifact:
    if len(points) < 2:
        raise ValueError("Not enough points to generate trajectory")

    cum = cumulative_lengths(points)
    total_length = cum[-1]
    traj = ScalarTrajectory(total_length=total_length, vmax=vmax, amax=amax)

    samples = sample_trajectory(points, cum, traj, sample_dt, sample_ds)
    report = build_report(samples, vmax, amax, jmax, False)
    report = PlanningReport(
        total_length_mm=total_length,
        total_time_s=float(traj.duration),
        max_velocity_observed=report.max_velocity_observed,
        max_acceleration_observed=report.max_acceleration_observed,
        max_jerk_observed=report.max_jerk_observed,
        constraint_violations=report.constraint_violations,
        time_integration_error_s=report.time_integration_error_s,
        time_integration_fallbacks=report.time_integration_fallbacks,
        jerk_limit_enforced=report.jerk_limit_enforced,
        jerk_plan_failed=report.jerk_plan_failed,
        segment_count=report.segment_count,
        rapid_segment_count=report.rapid_segment_count,
        rapid_length_mm=report.rapid_length_mm,
        corner_segment_count=report.corner_segment_count,
        discontinuity_count=report.discontinuity_count,
    )
    return TrajectoryArtifact(
        samples=samples,
        total_time=float(traj.duration),
        total_length=total_length,
        planning_report=report,
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate trajectory from polyline points")
    parser.add_argument("--input", required=True, help="Input path points JSON")
    parser.add_argument("--output", required=True, help="Output trajectory JSON")
    parser.add_argument("--vmax", type=float, required=True)
    parser.add_argument("--amax", type=float, required=True)
    parser.add_argument("--jmax", type=float, required=True)
    parser.add_argument("--sample-dt", type=float, default=0.01)
    parser.add_argument("--sample-ds", type=float, default=0.0)
    args = parser.parse_args(argv)

    input_path = Path(args.input)
    output_path = Path(args.output)
    points = load_points(input_path)
    try:
        artifact = build_trajectory_artifact(
            points=points,
            vmax=float(args.vmax),
            amax=float(args.amax),
            jmax=float(args.jmax),
            sample_dt=float(args.sample_dt),
            sample_ds=float(args.sample_ds),
        )
    except Exception as exc:
        raise SystemExit(str(exc))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(artifact.to_dict(), ensure_ascii=True), encoding="utf-8")
    return 0


if __name__ == "__main__":
    import sys

    raise SystemExit(main(sys.argv[1:]))
