# device-hal seed

This document defines the first migration slice for `modules/device-hal`.

## Primary ownership

`device-hal` owns:
- controller and runtime connection adapters
- valve / trigger / IO / supply adapters
- configuration and local storage adapters
- vendor SDK wrappers and error mapping
- mock and simulator hardware backends

## First migration slice

Move or wrap first:
- `src/infrastructure/adapters/motion/controller/control/MultiCardMotionAdapter.*`
- `src/infrastructure/adapters/motion/controller/connection/HardwareConnectionAdapter.*`
- `src/infrastructure/adapters/motion/controller/homing/HomingPortAdapter.*`
- `src/infrastructure/adapters/dispensing/dispenser/ValveAdapter.*`
- `src/infrastructure/adapters/dispensing/dispenser/triggering/TriggerControllerAdapter.*`
- `src/infrastructure/adapters/system/configuration/ConfigFileAdapter.*`
- `modules/device-hal/src/drivers/multicard/IMultiCardWrapper.*`
- `modules/device-hal/src/drivers/multicard/MultiCardErrorCodes.*`
- `modules/device-hal/src/drivers/multicard/MockMultiCard*`
- `modules/device-hal/src/drivers/multicard/RealMultiCardWrapper.*`

## Keep out of device-hal

Do not migrate directly:
- recipe validation or activation logic
- dispense process orchestration logic
- motion state-machine semantics
- TCP/HMI request parsing or JSON serialization
- `apps/control-runtime/runtime/events/*` and `apps/control-runtime/runtime/scheduling/*`

If an adapter decides business flow based on recipe or protocol concerns, that logic should be
moved up into `process-core`, `motion-core`, or `control-gateway`.

## Error mapping principle

`device-hal` may know:
- vendor SDK error codes
- transport and IO failures
- timeout categories
- resource acquisition failures

`device-hal` should expose upward only:
- stable shared/process/motion result types
- adapter-neutral failure messages
- translated hardware/runtime categories

## New seed abstractions

The first seed headers added under `modules/device-hal/include` intentionally define:
- a minimal runtime session model
- a motion controller adapter surface
- a dispenser adapter surface
- a machine health surface

These headers are not yet wired into every production path. They define where future adapter wrappers and
error translators should land.

## First real migrated slice

The following asset now has a real implementation under `modules/device-hal`:
- `drivers/multicard/error_mapper.h/.cpp`

This gives device-hal a concrete, low-risk vendor-facing capability without moving the larger
controller and runtime adapters yet.
