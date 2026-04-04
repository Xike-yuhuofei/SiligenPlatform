# ARCH-201 US3 Public Surface Closeout Plan

## Authority Facts

- Stage B / US2 evidence is already frozen and remains the validation baseline.
- The current next stage selected by the user is US3 public surface closeout.
- The known unrelated gate failure remains `apps/hmi-app/tests/unit/test_offline_preview_builder.py` `no-loose-mock`.

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

## Stop Conditions

- A required fix would change planning/runtime behavior instead of include direction.
- A required fix would pull HMI gate repair or trigger/CMP owner work into this batch.
- A required fix would require a broad owner refactor outside the declared US3 scope.
