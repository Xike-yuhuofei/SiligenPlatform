# motion-core seed

This document defines the first migration slice for `modules/motion-core`.

## Primary ownership

`motion-core` owns:
- axis and coordinate semantics
- motion command semantics
- trajectory and interpolation semantics
- motion state transitions
- safety, interlock, and soft-limit rules

## First migration slice

Move or wrap first:
- `src/domain/motion/value-objects/MotionTypes.h`
- `src/domain/motion/value-objects/MotionTrajectory.h`
- `src/domain/motion/domain-services/JogController.*`
- `src/domain/motion/domain-services/HomingProcess.*`
- `src/domain/motion/domain-services/TrajectoryPlanner.*`
- `src/domain/motion/domain-services/interpolation/*`
- `src/domain/safety/domain-services/InterlockPolicy.*`
- `src/domain/safety/domain-services/SoftLimitValidator.*`
- `src/application/usecases/motion/manual/*`
- `src/application/usecases/motion/homing/*`
- `src/application/usecases/motion/ptp/*`
- `src/application/usecases/motion/interpolation/*`
- `src/application/usecases/motion/safety/*`

## Keep out of motion-core

Do not migrate directly:
- `src/domain/motion/ports/IMotionConnectionPort.h`
- `src/domain/motion/ports/IMotionRuntimePort.h`
- `src/application/usecases/motion/initialization/*`
- any class that embeds controller reset, SDK download, runtime polling, or DLL concerns

These areas lean toward runtime or device implementation and should remain available to
`device-hal` or `apps/control-runtime`.

## New seed abstractions

The seed headers added under `modules/motion-core/include` intentionally define:
- a small motion command model
- a minimal motion state model
- controller and feedback ports
- interlock and emergency ports
- a motion service entry point

These headers are not yet wired into every production path. They provide a stable landing zone for
future extraction.

## First real migrated slice

The following assets now have real implementations under `modules/motion-core`:
- `safety/interlock_types.h`
- `safety/ports/interlock_signal_port.h`
- `safety/services/interlock_policy.h/.cpp`

This slice is intentionally focused on safety semantics and avoids controller runtime,
reset flows, or SDK-bound state polling.
