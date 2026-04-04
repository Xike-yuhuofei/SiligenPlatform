# ARCH-201 US3 Public Surface Closeout Plan

## Authority Facts

- Stage B / US2 evidence is already frozen and remains the validation baseline.
- The current next stage selected by the user is US3 public surface closeout.
- US3 closeout is executed in `D:\Projects\wt-arch201-2` on branch `refactor/motion/ARCH-201-us3-public-surface-closeout`.
- The original "ready to closeout" assumption was invalidated by current-branch regressions and had to be rescued against the live worktree.
- The known unrelated gate failure remains `apps/hmi-app/tests/unit/test_offline_preview_builder.py` `no-loose-mock`.
- The known unrelated non-US3 runtime-host failure remains `MockMultiCardCharacterizationTest.CrdClearDropsBufferedTrajectoryBeforeNextRun`.

## Public Surface Targets

### Planning

- Canonical root: `motion_planning/contracts/*`
- Required visible types:
  - `MotionTrajectory`
  - `MotionPlan`
  - `MotionPlanningReport`
  - `TimePlanningConfig`

### Runtime / Control

- Canonical root: `runtime_execution/contracts/motion/*`
- Required visible types:
  - `IInterpolationPort`
  - `IIOControlPort`
  - `IMotionRuntimePort`

## Execution Order

1. Create a dedicated US3 spec bundle.
2. Cut planning consumer leaks in `workflow/application` and `planner-cli`.
3. Cut runtime/control public header leaks in `workflow/application` and `runtime-service`.
4. Add scoped bridge checks to `assert-module-boundary-bridges.ps1`.
5. Run targeted validation and record evidence.
6. Rescue the long execution-trajectory motion preview clamp so the workflow preview preserves mandatory corners.

## Stop Conditions

- A required fix would change planning/runtime behavior instead of include direction.
- A required fix would pull HMI gate repair or trigger/CMP owner work into this batch.
- A required fix would require a broad owner refactor outside the declared US3 scope.
- Any residue from `ARCH-201-m7`, `DXF_full_analysis_compare.md`, HMI `no-loose-mock`, or `MockMultiCardCharacterizationTest.CrdClearDropsBufferedTrajectoryBeforeNextRun` must stay out of this batch.
