# Solution C interface freeze

This document supersedes the task-1 baseline with the blocking seam closures that are now
required for parallel development.

## Canonical targets

- `simulation_engine`:
  compatibility engine retained only for historical baseline / smoke / compare coverage;
  no new simulation capability may be added here
- `simulation_engine_scheme_c_contracts`:
  frozen scheme C contracts, including runtime bridge bindings and the adapter-only
  process-runtime virtual port layer
- `simulation_engine_scheme_c`:
  in-memory implementation of the frozen contracts, including deterministic buffered
  multi-segment interpolation semantics behind the adapter-only virtual port layer

## Frozen public inputs

- `VirtualClock`:
  deterministic fixed-step time source with explicit `start` / `advance` / `pause` / `resume` /
  `stop` lifecycle, optional logical timeout, and structured virtual-time errors
- `TickScheduler`:
  deterministic tick dispatcher owned by `virtual-time/`, ordered by
  `(target tick, phase, registration sequence)` and driven only by explicit application/session
  tick advancement
- `VirtualAxisGroup`:
  axis state read/write plus command application
- `VirtualIo`:
  named signal read/write plus snapshot access
- `Recorder`:
  immutable recording boundary with canonical `RecordingResult` facts:
  raw `snapshots` / `events`, derived `motion_profile`, merged `timeline`, key state-change `trace`,
  and deterministic `summary`
- `RuntimeBridge`:
  bridge metadata plus resolved binding ownership
- `RuntimeBridgeControl`:
  deterministic motion command submission surface for `home`, `move`, `execute path`, and
  `stop`, plus command status queries for session-driven orchestration
- `SimulationSession`:
  orchestration entry with explicit `bridge_bindings`, optional logical timeout input, and
  explicit `start` / `advanceOneTick` / `pause` / `resume` / `stop` control surface while
  keeping `run()` as the convenience wrapper
- `CanonicalSimulationInput`:
  canonical application-owned input normalization for `axis_names`, `io_channels`,
  `bridge_bindings`, optional `motion_realism`, `tick`, `timeout`, `max_ticks`,
  `home_before_run`, optional `initial_move`, `path`,
  optional deterministic replay plan derived from canonical `io_delays` / `triggers` /
  `valve`, and optional deterministic `stop`
- `ApplicationRunner`:
  `loadCanonicalSimulationInputJson/File` plus `runMinimalClosedLoop/File` as the frozen
  minimal closed-loop entry; application may assemble a session, submit bridge commands, and
  coordinate `SimulationSessionResult`, but may not become a runtime-host substitute

## Frozen scheduling contract

- Default logical tick step is `1 ms`.
- Logical time is advanced only from `application/SimulationSession`; runtime bridge and virtual
  devices may observe ticks but may not source them.
- Tick execution order is deterministic:
  `RuntimeBridge -> VirtualAxisGroup -> VirtualIo -> Recorder -> user/default scheduled tasks`.
- Tasks scheduled during a tick dispatch do not preempt the current dispatch batch; they become
  eligible on the next explicit dispatch cycle.
- Timeout is evaluated against logical time after a tick completes; the timed-out tick is still
  recorded before the session transitions to `TimedOut`.
- `SimulationSession::advanceOneTick()` returns `false` without consuming a tick when the session
  is `Paused`; callers must `resume()` before continuing deterministic advancement.
- Application-injected stop work must be scheduled only after `SimulationSession::start()` via
  `TickScheduler`; application does not write virtual device state directly.

## Blocking seam state

- `SC-B-001`:
  `CLOSED`; axis and IO binding contract is frozen through `RuntimeBridgeBindings`
- `SC-B-002`:
  `CLOSED`; formal motion runtime assembly is now owned by
  `process-runtime-core` through `MotionRuntimeAssemblyFactory`
- `SC-B-003`:
  `CLOSED`; `SimulationSessionConfig.bridge_bindings` freezes the handoff input for session assembly
- Remaining non-blocking seam note:
  `SC-NB-001` and `SC-NB-005` are now closed behind the same frozen session/runtime bridge
  contracts; no approved shim boundary remains in this slice.
- Compat note:
  `simulation_engine` compat path remains buildable only to preserve historical baselines and
  smoke comparisons; new path execution, replay, export, and session work belongs to scheme C only.

## Adapter-only virtual port shim

`packages/simulation-engine/runtime-bridge/process_runtime_shim.cpp` still owns the virtual-device
adapter implementation, but it is no longer the formal runtime assembly seam.

The adapter layer is owned by
`packages/simulation-engine/runtime-bridge/process_runtime_shim.cpp`.

It exposes the following `process-runtime-core` compatible ports without copying control logic:

- `IMotionRuntimePort`
- `IInterpolationPort`

The adapter delegates to `VirtualAxisGroup` and `VirtualIo`; formal use-case/service composition
now happens in `packages/process-runtime-core/src/application/usecases/motion/runtime/`.

Closed seam note:

- `runtime_bridge` now resolves virtual `IMotionRuntimePort` / `IInterpolationPort` adapters and
  hands them to the core-owned `MotionRuntimeAssemblyFactory`
- `ProcessRuntimeCoreShimBundle` may still exist for adapter-contract regression coverage, but the
  frozen runtime bridge main path no longer depends on it

Adapter non-impact scope:

- does not change deterministic tick ownership
- does not change the public command categories `home` / `move` / `execute path` / `stop`
- does not change `scheme_c.recording.v1`

## Runtime bridge command freeze

- `RuntimeBridgeControl` is the only frozen public way to submit scheme C motion work into the
  bridge.
- Commands are queued explicitly and consumed only from `RuntimeBridge::advance` during the
  `RuntimeBridge` phase of the deterministic tick loop.
- The frozen command categories for this phase are:
  `home`, `move`, `execute path`, and `stop`.
- A command may call into `process-runtime-core` use cases synchronously to submit work, but
  command completion must be observed and resolved only through later deterministic ticks.
- `execute path` submission is frozen to a core-owned non-blocking application entry:
  `DeterministicPathExecutionUseCase::Start/Advance/Cancel`; runtime bridge may not own direct
  interpolation segment injection as the formal scheme C path execution implementation.
- The adapter layer still provides `IInterpolationPort`, but segment dispatch responsibility now
  sits in `packages/process-runtime-core/application` through
  `MotionCoordinationUseCase::DispatchCoordinateSystemSegment`.
- Compare output trigger control is now wired behind the same frozen bridge surface:
  `MotionCoordinationUseCase::ConfigureCompareOutput` is satisfied by an internal scheme C trigger
  adapter, so no new public bridge command category or shim-bundle field was introduced.

## Frozen buffered interpolation semantics

- No new public simulation-only interpolation API is introduced; buffered interpolation remains
  expressed only through the existing `IInterpolationPort` methods:
  `ConfigureCoordinateSystem`, `AddInterpolationData`, `FlushInterpolationData`,
  `StartCoordinateSystemMotion`, `StopCoordinateSystemMotion`, and the existing status/space
  queries.
- A coordinate system owns three internal deterministic queues behind the adapter layer:
  `staged` segments added by `AddInterpolationData`, `queued` segments made executable by
  `FlushInterpolationData`, and one optional in-flight segment already dispatched to
  `VirtualAxisGroup`.
- `AddInterpolationData` consumes interpolation-buffer capacity immediately but does not make a
  segment executable until `FlushInterpolationData` succeeds.
- `FlushInterpolationData` promotes the staged batch into the executable queue only when both the
  interpolation buffer and the configured look-ahead window have space; otherwise it fails
  deterministically without widening the frozen public contract.
- `StartCoordinateSystemMotion` is idempotent and acts as the coordinate-system service point on
  each deterministic tick: it arms the coordinate system, observes whether the in-flight segment
  has completed, and dispatches the next flushed segment when available.
- `StopCoordinateSystemMotion` clears staged and queued buffered segments and issues axis stop
  commands for the active coordinate system; session stop and path cancel reuse this same stop path.
- `GetInterpolationBufferSpace` now reports real remaining raw buffer slots against the shim-owned
  in-memory buffer capacity.
- `GetLookAheadBufferSpace` now reports remaining executable prefetch slots derived from
  `CoordinateSystemConfig.look_ahead_segments` within the same shim-owned buffer model.
- Coordinate-system status is frozen onto the existing `CoordinateSystemStatus` fields:
  `MOVING` means an in-flight segment exists, `IDLE + remaining_segments == 0` means idle, and
  `IDLE + remaining_segments > 0` means buffered-but-not-currently-moving. The buffered state is
  additionally surfaced through `raw_status_word`.
- Session `pause()` / `resume()` still belong only to `SimulationSession`; pausing consumes no
  deterministic tick, preserves buffered and in-flight interpolation state, and resumes from the
  same queues without bridge-side path ownership changes.

## Frozen minimal closed-loop runner contract

- `X` and `Y` axes are required for canonical application runs; `Z` remains optional.
- Canonical input is loaded from the root `examples/simulation` JSON shape and normalized before
  session assembly; application may derive `io_channels` from `io_delays` / `triggers` / `valve`.
- Canonical input may additionally provide an optional `motion_realism` block; it remains
  virtual-device-owned, default-off, and does not introduce a new bridge command category.
- Canonical `triggers` normalize into deterministic replay compare outputs expressed in
  path-progress millimeters for the submitted path and are armed through the formal
  `MotionCoordinationUseCase::ConfigureCompareOutput` path.
- Canonical `io_delays` normalize into per-channel actuation delays and apply on the first
  deterministic tick whose logical time is greater than or equal to the configured target.
- Canonical `valve` normalizes into extra deterministic open/close delay semantics for the
  configured valve IO channel after delayed IO application; it does not model material, pressure,
  temperature, or other dispensing physics.
- Replay execution is owned by scheme C runtime bridge and `VirtualIo`; application may supply the
  canonical input, but it does not become a second IO/trigger controller.

## Frozen recording export contract

- `SimulationSessionResult.recording` keeps the legacy readable fields
  `snapshots`, `events`, `final_axes`, `final_io`, `ticks_executed`, and `simulated_time`.
- `RecordingResult.motion_profile` is append-only per `(snapshot order, axis order)`.
- `RecordingResult.trace` contains only fact transitions for key axis/io state fields and does not
  drive control flow.
- `RecordingResult.timeline` is the canonical regression stream ordered by
  `(tick_index, kind_priority, insertion_sequence)`, where `event` entries sort before
  `state_change` entries within the same tick.
- Deterministic replay work records its observable effects through the same recorder surface:
  compare output arming/firing and delayed IO/valve application appear in `events` / `timeline`,
  while actual virtual IO state transitions remain visible in `trace` / `final_io`.
- `RecordingResult.summary` normalizes terminal semantics:
  session/runtime integration keeps `bridge_completed`, `max_ticks_reached`, and
  `session_dependencies_missing`, while minimal closed-loop application results may additionally
  surface `invalid_input`, `missing_required_axes`, `bridge_control_unavailable`, and
  `logical_timeout`.
- Additive observability fields are permitted while keeping `scheme_c.recording.v1`, provided the
  existing contract remains backward-compatible. Current additive fields are
  `command_position_mm`, `command_velocity_mm_per_s`, `following_error_mm`,
  `following_error_active`, `encoder_quantization_mm`, and summary-level following-error stats.
- Canonical scheme C exports now keep `runtime_bridge.follow_up_seams` empty; stale seam lists in
  samples/baselines are non-contractual legacy artifacts and must not be reintroduced.
- A deterministic stop submitted through `RuntimeBridgeControl` may still drain to
  `completed/bridge_completed`; consumers distinguish that case through the canonical timeline and
  truncated final motion result rather than a second terminal state.
- Canonical JSON export is owned by `sim::ResultExporter::toJson(const SimulationSessionResult&)`
  with schema version `scheme_c.recording.v1`.

## Verified validation entry points

- root build:
  `.\build.ps1 -Profile Local -Suite simulation`
- root simulation suite:
  `.\test.ps1 -Profile Local -Suite simulation`
- package scheme C ctest surface:
  `ctest --test-dir D:\Projects\SiligenSuite\build\simulation-engine -C Debug --output-on-failure -R "simulation_engine_scheme_c_.*_test"`
- package fresh configure + full build + full ctest:
  `cmake --fresh -S D:\Projects\SiligenSuite\packages\simulation-engine -B D:\Projects\SiligenSuite\build\simulation-engine -DSIM_ENGINE_BUILD_EXAMPLES=ON -DSIM_ENGINE_BUILD_TESTS=ON`
  `cmake --build D:\Projects\SiligenSuite\build\simulation-engine --config Debug`
  `ctest --test-dir D:\Projects\SiligenSuite\build\simulation-engine -C Debug --output-on-failure`
- minimal closed-loop integration regression:
  `python D:\Projects\SiligenSuite\integration\simulated-line\run_simulated_line.py`
