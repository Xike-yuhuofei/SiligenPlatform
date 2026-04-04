# ARCH-201 US4 CMP/Trigger Owner Closeout

## Goal

Finalize CMP/Trigger semantic ownership so that:

- `dispense-packaging` owns trigger planning, authority layout, and CMP service semantics
- `motion-planning` owns motion planning and interpolation math only
- `workflow` consumes canonical owner surfaces through compatibility shims only

This stage changes owner boundaries, build wiring, compatibility headers, and validation rules.
It does not change HMI behavior, CLI contracts, runtime protocols, or hardware-facing semantics.

## Scope

- `modules/dispense-packaging`
- `modules/workflow/domain`
- `modules/workflow/tests/process-runtime-core`
- `modules/motion-planning/tests/unit/domain/motion`
- `scripts/validation`

## In Scope

- Move live `CMPService`, `DispensingPlannerService`, and `UnifiedTrajectoryPlannerService` implementations to the M8 owner target
- Turn workflow Trigger/CMP compatibility headers into forwarding shims
- Stop workflow domain targets from compiling M8 Trigger/CMP owner implementations
- Add scoped US4 bridge checks and owner-boundary assertions
- Record targeted build/test evidence in a dedicated US4 bundle

## Out Of Scope

- HMI `no-loose-mock` repair
- Trigger/CMP hardware behavior rewrites
- CLI or protocol field changes
- Full worktree residue cleanup outside the Trigger/CMP owner slice
- Re-homing `CMPCoordinatedInterpolator` out of `motion-planning`

## Success Criteria

1. `siligen_dispense_packaging_domain_dispensing` becomes the live build owner for `CMPService`, `DispensingPlannerService`, and `UnifiedTrajectoryPlannerService`.
2. Workflow compatibility headers for Trigger/CMP owner types no longer declare local owner classes.
3. Workflow domain CMake no longer compiles local Trigger/CMP owner implementations.
4. Scoped boundary validation catches any regression that reintroduces workflow-side Trigger/CMP owner implementations.
5. Targeted build/test commands pass, or any remaining failure is explicitly documented as outside US4 scope.
