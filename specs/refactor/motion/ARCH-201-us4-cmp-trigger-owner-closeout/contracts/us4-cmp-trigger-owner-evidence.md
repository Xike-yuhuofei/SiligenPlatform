# US4 CMP/Trigger Owner Evidence

## Expected Owner Cutover

- `modules/dispense-packaging/domain/dispensing/CMakeLists.txt`
- `modules/workflow/domain/domain/CMakeLists.txt`
- `modules/workflow/domain/include/domain/dispensing/domain-services/CMPTriggerService.h`
- `modules/workflow/domain/include/domain/dispensing/planning/domain-services/DispensingPlannerService.h`
- `modules/workflow/domain/domain/dispensing/domain-services/TriggerPlanner.h`
- `modules/motion-planning/tests/unit/domain/motion/MotionPlanningOwnerBoundaryTest.cpp`
- `scripts/validation/assert-module-boundary-bridges.ps1`

## Authority Baseline

- US2 closed `runtime/control` ownership to `runtime-execution`.
- US3 closed canonical public consumer surfaces for planning/runtime contracts.
- `ADR-005` keeps CMP runtime authority on `[ValveDispenser]` and excludes `[CMP]` from canonical runtime ownership.

## This Round

- Live CMP/Trigger owner implementations are moved into the M8 owner target.
- Workflow Trigger/CMP compatibility headers are reduced to forwarding shims.
- Workflow domain no longer compiles M8 Trigger/CMP owner implementations.
- Scoped US4 bridge checks and owner-boundary assertions are added.

## Command Evidence

- Boundary bridge gate:
  `powershell -NoProfile -ExecutionPolicy Bypass -File D:/Projects/wt-spike153/scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot D:/Projects/wt-spike153 -ReportDir tests/reports/module-boundary-bridges-us4`
- Targeted build:
  `cmake --build D:/Projects/wt-spike153/build-phase2 --config Debug --target siligen_motion_planning_unit_tests siligen_unit_tests --parallel`
- `D:/Projects/wt-spike153/build-phase2/bin/Debug/siligen_motion_planning_unit_tests.exe`
- `D:/Projects/wt-spike153/build-phase2/bin/Debug/siligen_unit_tests.exe`

## Validation Result

- Targeted build passed on 2026-04-04:
  `cmake --build D:/Projects/wt-spike153/build-phase2 --config Debug --target siligen_motion_planning_unit_tests siligen_unit_tests --parallel`
- `D:/Projects/wt-spike153/build-phase2/bin/Debug/siligen_motion_planning_unit_tests.exe` passed:
  `28 tests from 9 test suites`.
- `D:/Projects/wt-spike153/build-phase2/bin/Debug/siligen_unit_tests.exe` passed:
  `163 tests from 33 test suites`.
- Boundary bridge gate passed:
  `powershell -NoProfile -ExecutionPolicy Bypass -File D:/Projects/wt-spike153/scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot D:/Projects/wt-spike153 -ReportDir tests/reports/module-boundary-bridges-us4`
- Boundary report artifacts:
  `tests/reports/module-boundary-bridges-us4/module-boundary-bridges.json`
  `tests/reports/module-boundary-bridges-us4/module-boundary-bridges.md`

## Repair Note

- During validation, `modules/workflow/tests/process-runtime-core/unit/dispensing/DispensingWorkflowUseCaseTest.cpp`
  surfaced that the M8 canonical `DispensingPlan` contract had not yet absorbed the full compat-era preview fields.
- The canonical header
  `modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.h`
  was aligned with the workflow-exposed surface by restoring:
  `preview_authority_ready`,
  `preview_authority_shared_with_execution`,
  `preview_spacing_valid`,
  `preview_has_short_segment_exceptions`,
  `preview_failure_reason`,
  `authority_trigger_points`,
  `spacing_validation_groups`,
  plus legacy offline trajectory request fields that should be removed from the compat surface.
- Validation passed after that canonical header repair, with workflow remaining shim-only for Trigger/CMP ownership.
