# Solution C parallel handoffs

## Stable inputs available to parallel tasks

- Runtime bridge work:
  depend on `RuntimeBridgeBindings`, `ProcessRuntimeCoreShimBundle`, and the frozen
  `RuntimeBridgeControl` command surface for `home` / `move` / `execute path` / `stop`
- Virtual device work:
  preserve `VirtualAxisGroup` and `VirtualIo` read/write semantics
- Application work:
  populate `SimulationSessionConfig.bridge_bindings` explicitly when defaults are not enough
- Recording work:
  consume `SimulationSessionResult.recording` without making control decisions; use
  `motion_profile`, `timeline`, and `summary` as the stable regression surface while legacy
  `snapshots` / `events` remain readable for debugging

## Handoff expectations

- Axis bindings use `logical_axis_index` values compatible with `X=0`, `Y=1`, `Z=2`, `U=3`.
- IO bindings use numeric channels on the runtime side and named signals on the virtual side.
- The approved shim is the only supported way to expose `IHomingPort`, `IInterpolationPort`,
  `IAxisControlPort`, `IMotionStatePort`, and `IIOControlPort` from virtual devices during this phase.
- Path execution currently reuses core planning plus direct `IInterpolationPort::AddInterpolationData`
  loading inside the bridge; do not assume `ExecuteTrajectoryUseCase` is deterministic-tick-safe yet.

## Still non-blocking

- Trigger controller integration is still separate and should not block motion, session, or recorder work.
- More realistic interpolation buffering may extend behavior later, but must not change the approved shim surface.
- Recorder export is now canonically written through `ResultExporter`; future changes may extend
  `RecordingResult`, but existing readable fields and `scheme_c.recording.v1` semantics must remain
  backward-readable until a new schema version is introduced.
