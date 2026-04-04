# ARCH-201 US3 Public Surface Closeout Tasks

- [x] Create a dedicated US3 spec / plan / evidence bundle.
- [x] Switch planning public consumers to canonical `motion_planning/contracts` report types.
- [x] Switch workflow/runtime public headers to canonical `runtime_execution/contracts/motion` includes.
- [x] Switch runtime-service bootstrap/test consumers to canonical runtime motion contracts.
- [x] Add scoped US3 bridge checks to `scripts/validation/assert-module-boundary-bridges.ps1`.
- [x] Run targeted build/test commands and capture results.

## Validation Result

- `siligen_motion_planning_unit_tests.exe`: 28/28 passing
- `siligen_unit_tests.exe`: 163/163 passing
- `siligen_runtime_host_unit_tests.exe`: 55/56 passing; single unrelated failure at `MockMultiCardCharacterizationTest.CrdClearDropsBufferedTrajectoryBeforeNextRun`
- `runtime_service_integration_host_bootstrap_smoke.exe`: 4/4 passing
- `process_runtime_core_motion_runtime_assembly_test.exe`: exit 0
- `workflow_integration_motion_runtime_assembly_smoke.exe`: exit 0

## Rescue Notes

- `siligen_unit_tests.exe` initially had one US3-blocking regression:
  `DispensingWorkflowUseCaseTest.GetPreviewSnapshotClampsLongExecutionTrajectoryMotionPreviewPreservingCorners`
- The blocker was closed by restoring context-aware corner-anchor detection in:
  - `modules/workflow/application/services/dispensing/WorkflowPreviewSnapshotService.cpp`
  - `modules/dispense-packaging/application/services/dispensing/PreviewSnapshotService.cpp`
- No extra residue was pulled into the batch while closing this regression.

## Closeout Disposition

- `MockMultiCardCharacterizationTest.CrdClearDropsBufferedTrajectoryBeforeNextRun` is adjudicated out of the US3 batch scope.
- Rationale: the failing test file under `modules/runtime-execution/runtime/host/tests/unit/runtime/motion` is unchanged in the current worktree, the `MockMultiCard*` driver/mock sources are unchanged, and the US3 batch only redirects public-surface includes/types plus scoped boundary evidence.
- Result: US3 has the targeted evidence needed for batch-only closeout; any follow-up on the mock buffering failure should be handled as a separate incident or later batch.
