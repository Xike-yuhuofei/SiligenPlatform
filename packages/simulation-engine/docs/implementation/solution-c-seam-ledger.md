# Solution C seam ledger

| Seam ID | Status | Type | Owner | Closure artifact |
|---|---|---|---|---|
| `SC-B-001` | `CLOSED` | blocking | `packages/simulation-engine/runtime-bridge` | `RuntimeBridgeBindings` in `include/sim/scheme_c/runtime_bridge.h` and shim contract test |
| `SC-B-002` | `CLOSED` | blocking | `packages/process-runtime-core/application` | `MotionRuntimeAssemblyFactory` in `packages/process-runtime-core/src/application/usecases/motion/runtime/` + `runtime-bridge/process_runtime_ports.h` / `runtime-bridge/process_runtime_shim.cpp` adapter path + scheme C baseline/core integration tests + core factory test |
| `SC-B-003` | `CLOSED` | blocking | `packages/simulation-engine/application` | `SimulationSessionConfig.bridge_bindings` and session baseline test |
| `SC-NB-001` | `CLOSED` | non-blocking | `packages/simulation-engine/runtime-bridge` | Internal trigger controller adapter in `runtime-bridge/runtime_bridge.cpp` injected into `MotionCoordinationUseCase` without widening `ProcessRuntimeCoreShimBundle`, plus scheme C runtime-bridge core integration coverage |
| `SC-NB-002` | `CLOSED` | non-blocking | `packages/simulation-engine/virtual-devices` | `runtime-bridge/process_runtime_shim.cpp` buffered coordinate-system runtime + core `DeterministicPathExecutionUseCase` buffered refill + scheme C shim/core/runner regression tests |
| `SC-NB-003` | `CLOSED` | non-blocking | `packages/simulation-engine/recording` | `RecordingResult.motion_profile/trace/timeline/summary` + canonical `ResultExporter` session JSON writer + recorder/export tests |
| `SC-NB-004` | `CLOSED` | non-blocking | `packages/process-runtime-core/application` | `DeterministicPathExecutionUseCase` in `packages/process-runtime-core/src/application/usecases/motion/trajectory/` + `MotionCoordinationUseCase::DispatchCoordinateSystemSegment` + `runtime-bridge/runtime_bridge.cpp` path delegation + core/runtime-bridge integration tests |
| `SC-NB-005` | `CLOSED` | non-blocking | `packages/simulation-engine/application` | Canonical replay plan normalization in `include/sim/scheme_c/runtime_bridge.h` + `application/application_runner.cpp`, deterministic IO/valve replay in `runtime-bridge/runtime_bridge.cpp`, and application/runtime-bridge regression coverage |

## Notes

- Blocking seams may not return to `OPEN` without changing this ledger and the associated contract tests.
- Blocking seam audit summary:
  `OPEN=0`, `CLOSED=3`, `APPROVED_SHIM=0`.
- Closure status note:
  scheme C canonical motion/runtime/replay/export seams are closed; compat engine remains only for
  historical baseline and smoke comparison coverage.
- `SC-B-002` owner:
  `packages/process-runtime-core/application`.
- `SC-B-002` closure:
  `runtime_bridge` no longer assembles the formal motion runtime chain from
  `ProcessRuntimeCoreShimBundle`; it now resolves virtual `IMotionRuntimePort` /
  `IInterpolationPort` adapters from `packages/simulation-engine` and delegates formal
  use-case/service composition to the core-owned `MotionRuntimeAssemblyFactory`.
- `SC-B-002` non-impact scope:
  it does not widen package boundaries into `runtime-host`, `transport-gateway`, `apps/*`, or
  change deterministic tick ownership and recording/export contracts.
- `SC-NB-004` closure:
  `execute path` now delegates from `packages/simulation-engine/runtime-bridge` into the
  core-owned `DeterministicPathExecutionUseCase::Start/Advance/Cancel` flow, while interpolation
  segment dispatch is owned by `MotionCoordinationUseCase::DispatchCoordinateSystemSegment`.
- `SC-NB-004` non-regression note:
  `ExecuteTrajectoryUseCase` remains available for its existing blocking callers, but it is no
  longer the scheme C deterministic-tick path execution entry and bridge no longer calls
  `IInterpolationPort::AddInterpolationData` directly.
- `SC-NB-002` closure:
  the adapter-only virtual port layer now keeps deterministic in-memory coordinate-system buffers with explicit
  staged/flushed/in-flight segments, real `buffer space` / `lookahead space` / buffered status,
  and stop-driven buffer clearing, while the core-owned deterministic path execution use case
  refills those buffers on ticks without widening `ProcessRuntimeCoreShimBundle`.
- `SC-NB-002` non-regression note:
  `SC-B-002` is now `CLOSED`; the buffered interpolation/runtime refinement remains valid because
  the former shim was reduced to adapter-only virtual port implementation while the formal motion
  runtime assembly moved into `process-runtime-core`.
- `SC-NB-001` closure:
  scheme C runtime bridge now constructs an internal trigger controller adapter, injects it into
  `MotionCoordinationUseCase`, and arms compare output from the formal core use case path while the
  resulting trigger firings flow through deterministic bridge ticks, `VirtualIo`, and recorder
  timeline/events.
- `SC-NB-001` non-regression note:
  no new public `RuntimeBridgeControl` command category was introduced, and
  `ProcessRuntimeCoreShimBundle` kept the same frozen public fields.
- `SC-NB-005` closure:
  canonical `io_delays`, `triggers`, and `valve` fields are now normalized into a deterministic
  replay plan, handed through `CanonicalSimulationInput` / `SimulationSessionConfig`, executed by
  runtime-bridge-owned replay state on deterministic ticks, and surfaced through recorder
  `events` / `timeline` / IO state trace.
- `SC-NB-005` deterministic scope:
  `triggers` are replayed as path-progress compare outputs, `io_delays` remain channel actuation
  delays, and `valve` adds only deterministic open/close delay semantics for the configured IO
  channel; no dispensing physics or runtime-host/UI/TCP expansion was introduced.
- Metadata note:
  canonical scheme C runtime bridge metadata now keeps `follow_up_seams=[]`; old sample exports
  that still list seam IDs must be treated as stale artifacts and updated or removed.
