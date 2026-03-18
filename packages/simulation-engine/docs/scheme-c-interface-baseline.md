# Scheme C interface baseline

This document freezes the minimal public surface that allows `packages/simulation-engine`
scheme C work to proceed in parallel without expanding the legacy compatibility engine
into a second long-term controller.

## Targets

- `simulation_engine`:
  legacy compatibility timing engine and existing regression targets
- `simulation_engine_scheme_c_contracts`:
  frozen scheme C headers only
- `simulation_engine_scheme_c`:
  minimal placeholder implementation for the frozen scheme C interfaces

## Frozen interfaces

- `sim/scheme_c/virtual_clock.h`:
  deterministic fixed-step clock for all scheme C ticks
- `sim/scheme_c/virtual_axis_group.h`:
  virtual axis capability seam owned by `virtual-devices/`
- `sim/scheme_c/virtual_io.h`:
  virtual digital IO seam owned by `virtual-devices/`
- `sim/scheme_c/recorder.h`:
  non-decision recording seam owned by `recording/`
- `sim/scheme_c/runtime_bridge.h`:
  thin adapter seam owned by `runtime-bridge/`
- `sim/scheme_c/simulation_session.h`:
  application orchestration entry owned by `application/`

## Runtime bridge seam

`process-runtime-core` does not currently expose a standalone package header surface that
`simulation-engine` can bind to directly in its standalone build. The scheme C baseline
therefore keeps `RuntimeBridge` as the thinnest possible seam and freezes the follow-up
integration points:

- `IHomingPort` -> `VirtualAxisGroup`
- `IInterpolationPort` and `IAxisControlPort` -> `VirtualAxisGroup`
- `IMotionStatePort` and `IIOControlPort` -> `VirtualAxisGroup` and `VirtualIo`

Owner:
`packages/simulation-engine/runtime-bridge`

Future attachment point:
`packages/process-runtime-core` motion use cases (`HomeAxesUseCase`,
`MotionCoordinationUseCase`, `MotionMonitoringUseCase`)

## Tick order

The frozen session order is:

1. `RuntimeBridge::advance`
2. `VirtualAxisGroup::advance`
3. `VirtualIo::advance`
4. `Recorder::recordSnapshot`
5. `VirtualClock::advance`

Later work may deepen behavior behind each seam, but should preserve this order unless the
baseline is revised explicitly.
