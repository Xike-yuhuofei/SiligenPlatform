# Solution C seam ledger

| Seam ID | Status | Type | Owner | Closure artifact |
|---|---|---|---|---|
| `SC-B-001` | `CLOSED` | blocking | `packages/simulation-engine/runtime-bridge` | `RuntimeBridgeBindings` in `include/sim/scheme_c/runtime_bridge.h` and shim contract test |
| `SC-B-002` | `APPROVED_SHIM` | blocking | `packages/simulation-engine/runtime-bridge` | `ProcessRuntimeCoreShimBundle` in `include/sim/scheme_c/process_runtime_shim.h` + `runtime-bridge/process_runtime_shim.cpp` + contract test |
| `SC-B-003` | `CLOSED` | blocking | `packages/simulation-engine/application` | `SimulationSessionConfig.bridge_bindings` and session baseline test |
| `SC-NB-001` | `OPEN` | non-blocking | `packages/simulation-engine/runtime-bridge` | Trigger controller port shim for compare output is still deferred |
| `SC-NB-002` | `OPEN` | non-blocking | `packages/simulation-engine/virtual-devices` | Deterministic virtual axis/home/limit/io feedback is now implemented; buffered multi-segment interpolation semantics remain minimal and in-memory only |
| `SC-NB-003` | `CLOSED` | non-blocking | `packages/simulation-engine/recording` | `RecordingResult.motion_profile/trace/timeline/summary` + canonical `ResultExporter` session JSON writer + recorder/export tests |
| `SC-NB-004` | `OPEN` | non-blocking | `packages/process-runtime-core/application` | `ExecuteTrajectoryUseCase` is still synchronous/blocking; runtime-bridge currently reuses `MotionPlanner` + `InterpolationProgramPlanner` + `MotionCoordinationUseCase::Configure/Start/Stop` and injects `IInterpolationPort::AddInterpolationData` directly until a non-blocking path execution entry exists |

## Notes

- Blocking seams may not return to `OPEN` without changing this ledger and the associated contract tests.
- `SC-B-002` is intentionally marked `APPROVED_SHIM` rather than `CLOSED` because it is a deliberate adapter seam, not the final direct runtime-core integration.
- `SC-NB-004` shim owner task:
  extract a deterministic submit/poll trajectory execution entry from `process-runtime-core` so
  bridge path execution no longer bypasses `ExecuteTrajectoryUseCase` for segment loading.
- `SC-NB-004` acceptance:
  bridge path commands use a core-owned non-blocking execution API end to end while preserving
  deterministic tick ownership in `SimulationSession`.
