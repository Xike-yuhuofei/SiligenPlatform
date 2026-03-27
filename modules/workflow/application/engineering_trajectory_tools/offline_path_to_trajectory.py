import argparse
import json
from pathlib import Path
from typing import List, Sequence

from engineering_data.models.geometry import Point2D
from engineering_trajectory_tools.models import PlanningReport, TrajectoryArtifact, TrajectorySample


def load_points(path: Path) -> List[Point2D]:
    data = json.loads(path.read_text(encoding="utf-8"))
    points = []
    for item in data.get("points", []):
        points.append(Point2D(float(item.get("x", 0.0)), float(item.get("y", 0.0))))
    return points


def cumulative_lengths(points: List[Point2D]) -> List[float]:
    lengths = [0.0]
    total = 0.0
    for i in range(1, len(points)):
        total += points[i - 1].distance_to(points[i])
        lengths.append(total)
    return lengths


def find_segment(cum_lengths: List[float], s: float) -> int:
    for i in range(1, len(cum_lengths)):
        if s <= cum_lengths[i]:
            return i - 1
    return max(0, len(cum_lengths) - 2)


def sample_trajectory(points: List[Point2D],
                      cum_lengths: List[float],
                      traj,
                      sample_dt: float,
                      sample_ds: float) -> List[TrajectorySample]:
    samples = []
    duration = float(traj.duration)
    times = []
    if sample_ds > 0.0:
        s = 0.0
        while s <= cum_lengths[-1] + 1e-9:
            t = traj.get_first_time_at_position(0, s)
            if t is None:
                break
            times.append(float(t))
            s += sample_ds
    else:
        dt = 0.01 if sample_dt <= 0.0 else sample_dt
        t = 0.0
        while t <= duration + 1e-9:
            times.append(t)
            t += dt
        if times[-1] < duration - 1e-6:
            times.append(duration)

    for t in times:
        pos, vel, acc = traj.at_time(t)
        s = float(pos[0])
        seg_idx = find_segment(cum_lengths, s)
        seg_start = cum_lengths[seg_idx]
        seg_len = max(1e-9, cum_lengths[seg_idx + 1] - seg_start)
        ratio = (s - seg_start) / seg_len
        start_point = points[seg_idx]
        end_point = points[seg_idx + 1]
        x = start_point.x + (end_point.x - start_point.x) * ratio
        y = start_point.y + (end_point.y - start_point.y) * ratio

        dx = (end_point.x - start_point.x) / seg_len
        dy = (end_point.y - start_point.y) / seg_len
        vx = dx * float(vel[0])
        vy = dy * float(vel[0])
        samples.append(TrajectorySample(
            t=float(t),
            position=Point2D(x, y),
            velocity=Point2D(vx, vy),
            scalar_acceleration=float(acc[0]),
            scalar_velocity=float(vel[0]),
        ))
    return samples


def build_report(samples: List[TrajectorySample],
                 vmax: float,
                 amax: float,
                 jmax: float,
                 jerk_enforced: bool) -> PlanningReport:
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


def build_trajectory_artifact(points: List[Point2D],
                              vmax: float,
                              amax: float,
                              jmax: float,
                              sample_dt: float,
                              sample_ds: float) -> TrajectoryArtifact:
    if len(points) < 2:
        raise ValueError("Not enough points to generate trajectory")

    try:
        from ruckig import InputParameter, Result, Ruckig, Trajectory
    except Exception as exc:
        raise RuntimeError(f"Ruckig import failed: {exc}") from exc

    cum = cumulative_lengths(points)
    total_length = cum[-1]
    if total_length <= 0.0:
        raise ValueError("Total length is zero")

    jerk_enforced = jmax > 0.0
    if jmax <= 0.0:
        jmax = 1e6

    dt = 0.01 if sample_dt <= 0.0 else float(sample_dt)

    otg = Ruckig(1, dt)
    inp = InputParameter(1)
    inp.current_position = [0.0]
    inp.current_velocity = [0.0]
    inp.current_acceleration = [0.0]
    inp.target_position = [total_length]
    inp.target_velocity = [0.0]
    inp.target_acceleration = [0.0]
    inp.max_velocity = [vmax]
    inp.max_acceleration = [amax]
    inp.max_jerk = [jmax]

    traj = Trajectory(1)
    result = otg.calculate(inp, traj)
    if result not in (Result.Working, Result.Finished):
        raise RuntimeError(f"Ruckig calculate failed: {result}")

    samples = sample_trajectory(points, cum, traj, sample_dt, sample_ds)
    report = build_report(samples, vmax, amax, jmax, jerk_enforced)
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
    parser = argparse.ArgumentParser(description="Generate trajectory from polyline points using Ruckig")
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
    vmax = float(args.vmax)
    amax = float(args.amax)
    jmax = float(args.jmax)
    try:
        artifact = build_trajectory_artifact(
            points=points,
            vmax=vmax,
            amax=amax,
            jmax=jmax,
            sample_dt=float(args.sample_dt),
            sample_ds=float(args.sample_ds),
        )
    except Exception as exc:
        raise SystemExit(str(exc))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(artifact.to_dict(), ensure_ascii=True), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
