# Trajectory Planning Unification

## Overview

This document describes the unified trajectory planning pipeline that converts
geometry (line, arc, spline) into a single time-parameterized trajectory with
consistent process constraints and motion limits. The unified flow performs
geometry normalization, process annotation, shaping, and motion planning once,
then samples a final trajectory that downstream components must not rewrite.

## Pipeline

1. Normalize geometry (unit scale, continuity tolerance, optional spline approximation)
2. Annotate process semantics (dispense on/off, tag, constraints)
3. Shape path (corner handling, smoothing rules)
4. Plan motion (curvature-based limits and forward/backward pass)
5. Sample once to produce the final MotionTrajectory

## Output Types

### MotionTrajectoryPoint

- t (seconds)
- position (x, y, z)
- velocity (vx, vy, vz)
- process_tag (enum as int)
- dispense_on (bool)
- flow_rate (relative to base flow)

### MotionTrajectory

- points
- total_time
- total_length
- planning_report

### PlanningReport

- total_length_mm
- total_time_s
- max_velocity_observed
- max_acceleration_observed
- max_jerk_observed
- constraint_violations
- time_integration_error_s (sum of |dt_analytic - dt_legacy|)
- time_integration_fallbacks (low-speed fallback count)
- jerk_limit_enforced
- jerk_plan_failed

## Spline Handling

Spline input can be approximated into line segments using bounded error.
Configuration includes:

- approximate_splines (bool)
- spline_max_step_mm
- spline_max_error_mm

These parameters flow through normalization so spline segments follow the same
speed and process rules as line and arc geometry.

## Integration Notes

The unified planner is used by DXF planning and dispensing flows. The web
planning response exposes:

- preview trajectory points
- process tag list aligned with the preview points
- planning report summary
- optional spacing verification warnings (configured via `spacing_tol_ratio/spacing_min_mm/spacing_max_mm`)

CLI export:

- Use `dxf-plan --planning-report` to export planning metrics CSV.
  Optional: `--planning-report-path` to override the output path.

## Motion Planning Notes

- Current planning uses speed limits with forward/backward passes (trapezoidal by construction).
- Enable strict jerk enforcement via `MotionConfig.enforce_jerk_limit` (Ruckig-based time parameterization).
- DXF planning sets `enforce_jerk_limit` when `max_jerk > 0`.
- When `jerk_plan_failed` is true, the planner returns an empty trajectory.
