# Solution C parallel handoffs

## Stable inputs available to parallel tasks

- Runtime bridge work:
  depend on `RuntimeBridgeBindings`, virtual `IMotionRuntimePort` / `IInterpolationPort`
  adapters from `runtime-bridge/process_runtime_ports.h`, the core-owned
  `MotionRuntimeAssemblyFactory`, and the frozen `RuntimeBridgeControl` command surface for
  `home` / `move` / `execute path` / `stop`
- Virtual device work:
  preserve `VirtualAxisGroup` and `VirtualIo` read/write semantics
- Application work:
  use `CanonicalSimulationInput` plus `runMinimalClosedLoop/File` for session assembly,
  populate `SimulationSessionConfig.bridge_bindings` explicitly when defaults are not enough,
  rely on the normalized deterministic replay plan for canonical `io_delays` / `triggers` /
  `valve`, and schedule deterministic stop injection only after `SimulationSession::start()`
- Recording work:
  consume `SimulationSessionResult.recording` without making control decisions; use
  `motion_profile`, `timeline`, and `summary` as the stable regression surface while legacy
  `snapshots` / `events` remain readable for debugging
- Validation work:
  use root `simulation` suite plus `integration/simulated-line/run_simulated_line.py`; do not
  introduce validation entry points that depend on `runtime-host` or `apps/*`

## Handoff expectations

- Axis bindings use `logical_axis_index` values compatible with `X=0`, `Y=1`, `Z=2`, `U=3`.
- IO bindings use numeric channels on the runtime side and named signals on the virtual side.
- The supported virtual-device handoff is now adapter-only:
  `packages/simulation-engine` exposes `IMotionRuntimePort` plus `IInterpolationPort`, and
  `packages/process-runtime-core` owns the formal motion runtime assembly on top of those ports.
- Path execution now delegates from bridge to the core-owned
  `DeterministicPathExecutionUseCase`; deterministic tick ownership stays in
  `SimulationSession`, and interpolation segment dispatch is owned by
  `MotionCoordinationUseCase::DispatchCoordinateSystemSegment` rather than direct bridge-side
  `IInterpolationPort::AddInterpolationData` calls.
- Buffered interpolation is now a stable handoff expectation:
  `AddInterpolationData` stages segments, `FlushInterpolationData` makes them executable,
  `StartCoordinateSystemMotion` advances buffered work on deterministic ticks, and
  `GetInterpolationBufferSpace` / `GetLookAheadBufferSpace` / `GetCoordinateSystemStatus`
  expose real shim-backed buffer state for tests and core refill logic.
- Session pause/resume is expected to freeze and restore the same buffered coordinate-system state;
  neither bridge nor virtual devices may source their own ticks to “help” interpolation continue.
- `ExecuteTrajectoryUseCase` remains blocking for its existing callers and is not the scheme C
  deterministic path execution entry.
- Compare output trigger control is now a stable handoff expectation:
  scheme C bridge injects an internal trigger controller adapter into
  `MotionCoordinationUseCase`, so compare output arming stays on the formal core path without
  widening the frozen public contract.
- Canonical `io_delays` / `triggers` / `valve` fields now normalize into a deterministic replay
  plan owned by runtime bridge:
  `triggers` fire on path-progress positions, `io_delays` apply per-channel actuation latency, and
  `valve` adds extra open/close delay only for deterministic IO timing semantics.
- Replay work that reaches virtual outputs must be observable through recorder `events` /
  `timeline` / IO state trace rather than runner notices or ad-hoc logs.
- The verified scheme C test surface is the `ctest` subset
  `simulation_engine_scheme_c_(baseline|runtime_bridge_shim|runtime_bridge_core_integration|recording_export|application_runner|virtual_devices|virtual_time)_test`.

## Still non-blocking

- Recorder export is now canonically written through `ResultExporter`; future changes may extend
  `RecordingResult`, but existing readable fields and `scheme_c.recording.v1` semantics must remain
  backward-readable until a new schema version is introduced.
- `SC-B-002` is now closed; reducing the former shim to adapter-only virtual port implementation
  still does not widen package boundaries into `runtime-host`, `transport-gateway`,
  `device-adapters`, or `apps/*`.
