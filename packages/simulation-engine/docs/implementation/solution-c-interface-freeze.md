# Solution C interface freeze

This document supersedes the task-1 baseline with the blocking seam closures that are now
required for parallel development.

## Canonical targets

- `simulation_engine`:
  compatibility engine and existing regression coverage
- `simulation_engine_scheme_c_contracts`:
  frozen scheme C contracts, including runtime bridge bindings and approved process-runtime shim
- `simulation_engine_scheme_c`:
  minimal in-memory implementation of the frozen contracts

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

## Closed blocking seams

- `SC-B-001`:
  axis and IO binding contract is frozen through `RuntimeBridgeBindings`
- `SC-B-002`:
  approved shim bundle exists through `ProcessRuntimeCoreShimBundle`
- `SC-B-003`:
  `SimulationSessionConfig.bridge_bindings` freezes the handoff input for session assembly

## Approved shim

The project now approves one thin shim, owned by
`packages/simulation-engine/runtime-bridge/process_runtime_shim.cpp`.

It exposes the following `process-runtime-core` compatible ports without copying control logic:

- `IHomingPort`
- `IInterpolationPort`
- `IAxisControlPort`
- `IMotionStatePort`
- `IIOControlPort`

The shim delegates to `VirtualAxisGroup` and `VirtualIo`, so future runtime work can wire real
use cases on top of the same contract without changing downstream inputs.

## Runtime bridge command freeze

- `RuntimeBridgeControl` is the only frozen public way to submit scheme C motion work into the
  bridge.
- Commands are queued explicitly and consumed only from `RuntimeBridge::advance` during the
  `RuntimeBridge` phase of the deterministic tick loop.
- The frozen command categories for this phase are:
  `home`, `move`, `execute path`, and `stop`.
- A command may call into `process-runtime-core` use cases synchronously to submit work, but
  command completion must be observed and resolved only through later deterministic ticks.

## Frozen recording export contract

- `SimulationSessionResult.recording` keeps the legacy readable fields
  `snapshots`, `events`, `final_axes`, `final_io`, `ticks_executed`, and `simulated_time`.
- `RecordingResult.motion_profile` is append-only per `(snapshot order, axis order)`.
- `RecordingResult.trace` contains only fact transitions for key axis/io state fields and does not
  drive control flow.
- `RecordingResult.timeline` is the canonical regression stream ordered by
  `(tick_index, kind_priority, insertion_sequence)`, where `event` entries sort before
  `state_change` entries within the same tick.
- `RecordingResult.summary` normalizes terminal semantics:
  `completed -> bridge_completed`, `stopped -> max_ticks_reached`, `failed -> session_dependencies_missing`
  for the currently approved application/session integration.
- Canonical JSON export is owned by `sim::ResultExporter::toJson(const SimulationSessionResult&)`
  with schema version `scheme_c.recording.v1`.
