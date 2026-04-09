# US3 Public Surface Evidence

## Expected Consumer Cutover

- `apps/planner-cli/CommandHandlers.Dxf.cpp`
- `apps/runtime-service/bootstrap/*`
- `apps/runtime-service/container/*`
- `apps/runtime-service/tests/integration/HostBootstrapSmokeTest.cpp`
- `modules/workflow/application/include/application/planning-trigger/PlanningUseCase.h`
- `modules/workflow/application/include/application/usecases/motion/*`
- `modules/workflow/application/usecases/motion/runtime/MotionRuntimeAssemblyFactory.h`
- `modules/workflow/application/usecases/motion/trajectory/DeterministicPathExecutionUseCase.*`

## Validation Baseline Carried Forward

- `siligen_motion_planning_unit_tests`: Stage B baseline only; superseded by current US3 rescue evidence below
- `siligen_unit_tests` Stage B filter: 33/33 passing
- `siligen_runtime_host_unit_tests`: 5/5 passing
- `process_runtime_core_motion_runtime_assembly_test`: exit 0
- `workflow_integration_motion_runtime_assembly_smoke`: exit 0

## Known Unrelated Gate Failure

- Root local gate still has a pre-existing failure in
  `apps/hmi-app/tests/unit/test_offline_preview_builder.py`
  (`no-loose-mock`)

## This Round

- Planning report consumers are moved to canonical planning contracts.
- Runtime/control public headers are moved to canonical runtime motion contracts.
- Scoped boundary checks are added for the US3 surface.
- Targeted validation results are recorded below.
- The initial US3 branch state was not directly closeout-ready; one workflow preview regression had to be rescued before evidence was green.

## Command Evidence

- Boundary bridge gate:
  `powershell -NoProfile -ExecutionPolicy Bypass -File D:/Projects/wt-arch201-2/scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot D:/Projects/wt-arch201-2 -ReportDir tests/reports/module-boundary-bridges-us3-current`
  Result: `passed`
- Targeted build:
  `cmake --build D:/Projects/wt-arch201-2/build-us3 --config Debug --target siligen_unit_tests --parallel`
  Result: `exit 0`
- Prior targeted builds:
  `cmake --build D:/Projects/wt-arch201-2/build-us3 --config Debug --target siligen_unit_tests siligen_motion_planning_unit_tests siligen_planner_cli_unit_tests siligen_runtime_host_unit_tests runtime_service_integration_host_bootstrap_smoke process_runtime_core_motion_runtime_assembly_test workflow_integration_motion_runtime_assembly_smoke --parallel`
  Result: `exit 0`
- `D:/Projects/wt-arch201-2/build-us3/bin/Debug/siligen_motion_planning_unit_tests.exe`
  Result: `28/28 passing`
- `D:/Projects/wt-arch201-2/build-us3/bin/Debug/siligen_unit_tests.exe`
  Result: `163/163 passing`
- `D:/Projects/wt-arch201-2/build-us3/bin/Debug/siligen_runtime_host_unit_tests.exe`
  Result: `55/56 passing`
- `D:/Projects/wt-arch201-2/build-us3/bin/Debug/runtime_service_integration_host_bootstrap_smoke.exe`
  Result: `4/4 passing`
- `D:/Projects/wt-arch201-2/build-us3/bin/Debug/process_runtime_core_motion_runtime_assembly_test.exe`
  Result: `exit 0`
- `D:/Projects/wt-arch201-2/build-us3/bin/Debug/workflow_integration_motion_runtime_assembly_smoke.exe`
  Result: `exit 0`

## Rescued US3 Blocker

- Initial blocker:
  `D:/Projects/wt-arch201-2/build-us3/bin/Debug/siligen_unit_tests.exe --gtest_filter=DispensingWorkflowUseCaseTest.GetPreviewSnapshotClampsLongExecutionTrajectoryMotionPreviewPreservingCorners`
  Result before rescue: failed because the sampled motion preview dropped the mandatory corner points `(100,0)` and `(100,100)` and introduced an unexpected segment angle.
- Rescue fix:
  restored context-aware corner-anchor detection in the workflow preview clamp and the owner preview clamp so long execution trajectories preserve semantic corners during max-point clamping.
- Re-run:
  `D:/Projects/wt-arch201-2/build-us3/bin/Debug/siligen_unit_tests.exe --gtest_filter=DispensingWorkflowUseCaseTest.GetPreviewSnapshotClampsLongExecutionTrajectoryMotionPreviewPreservingCorners`
  Result after rescue: `1/1 passing`

## Observed Non-Blocking Failure

- Failing test:
  `MockMultiCardCharacterizationTest.CrdClearDropsBufferedTrajectoryBeforeNextRun`
- Repro command:
  `D:/Projects/wt-arch201-2/build-us3/bin/Debug/siligen_runtime_host_unit_tests.exe --gtest_filter=MockMultiCardCharacterizationTest.CrdClearDropsBufferedTrajectoryBeforeNextRun`
- Repro result:
  stable failure at `modules/runtime-execution/runtime/host/tests/unit/runtime/motion/MultiCardMotionAdapterTest.cpp:287`
- Scope assessment:
  this failure is in mock runtime buffering behavior under `runtime-execution/runtime/host/tests/unit/runtime/motion`, outside the US3 public-surface include cutover scope.

## Scope Adjudication For Remaining Failure

- Scoped diff confirmation:
  `git diff --stat -- apps/planner-cli apps/runtime-service modules/workflow/application modules/dispense-packaging/application modules/motion-planning/contracts modules/runtime-execution/contracts/runtime scripts/validation specs/refactor/motion/ARCH-201-us3-public-surface-closeout`
  shows the US3 batch is limited to consumer/public-surface include cutover, canonical contract headers, scoped bridge checks, and evidence docs.
- Unchanged failure location:
  `git diff -- modules/runtime-execution/runtime/host/tests/unit/runtime/motion/MultiCardMotionAdapterTest.cpp`
  returns no diff in the failing test file.
- Unchanged mock implementation:
  `git diff -- ':(glob)**/MockMultiCard.h' ':(glob)**/MockMultiCard.cpp' ':(glob)**/MockMultiCardWrapper.h' ':(glob)**/MockMultiCardWrapper.cpp'`
  returns no diff; the mock driver/runtime buffering implementation was not modified in this batch.
- Adjacent runtime-execution edits in this batch are include/type cutovers only:
  `modules/runtime-execution/adapters/device/src/adapters/motion/controller/control/MultiCardMotionAdapter.h`
  `modules/runtime-execution/adapters/device/src/adapters/motion/controller/interpolation/InterpolationAdapter.*`
  `modules/runtime-execution/adapters/device/src/adapters/motion/controller/runtime/MotionRuntimeFacade.h`
  `modules/runtime-execution/runtime/host/services/motion/HardLimitMonitorService.h`
  and do not change buffered trajectory behavior.

## Closeout Conclusion

- Decision:
  `MockMultiCardCharacterizationTest.CrdClearDropsBufferedTrajectoryBeforeNextRun` does not block `ARCH-201 US3 public surface closeout`.
- Batch status:
  US3 targeted build, scoped boundary checks, consumer cutover, and evidence are sufficient for batch-only closeout.
- Follow-up handling:
  if the mock buffering failure still needs action, it should be opened as a separate incident / follow-up batch under `runtime-execution/runtime/host`, not merged into the US3 public-surface batch.
