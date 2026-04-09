# ARCH-201 US4 CMP/Trigger Owner Closeout Plan

## Authority Facts

- US2 fixed `runtime/control` owner on `runtime-execution`.
- US3 fixed canonical public consumer surfaces and is already adjudicated ready for batch-only closeout.
- `ADR-005` remains the authority for CMP runtime configuration: `[ValveDispenser]` is canonical and `[CMP]` is legacy/compat only.
- Current worktree already contains partial US4 residue that moves planning/runtime types to canonical contracts, but workflow still retains Trigger/CMP compatibility surfaces and residual build targets.

## Owner Boundary For US4

### M8 `dispense-packaging`

- Owns `AuthorityTriggerLayoutPlanner`
- Owns `TriggerPlanner`
- Owns `CMPService`
- Owns `DispensingPlannerService` and `UnifiedTrajectoryPlannerService` as Trigger/CMP semantic assembly

### M7 `motion-planning`

- Owns `MotionTrajectory`, `TimePlanningConfig`, `MotionPlan`, `MotionPlanningReport`
- Owns interpolation and time-parameterization math, including `CMPCoordinatedInterpolator` as a math helper that consumes explicit trigger inputs
- Does not own Trigger/CMP business semantics

### `workflow`

- May keep compatibility shims
- May consume canonical M8/M7 owner surfaces
- Must not compile or declare duplicate Trigger/CMP owner implementations

## Execution Order

1. Freeze the US4 stage bundle and traceability.
2. Move live Trigger/CMP owner implementations into the M8 owner target.
3. Turn workflow Trigger/CMP headers into forwarding shims and remove workflow-side live compilation of those owner implementations.
4. Extend owner-boundary tests and scoped bridge checks for US4.
5. Run targeted validation and record closeout evidence.

## Stop Conditions

- A required fix changes CMP hardware behavior or runtime protocol semantics.
- A required fix needs HMI/CLI changes to make US4 pass.
- A required fix requires moving `CMPCoordinatedInterpolator` ownership out of `motion-planning` in this batch.
