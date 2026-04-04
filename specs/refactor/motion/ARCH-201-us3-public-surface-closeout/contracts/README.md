# US3 Contract Surface

## Canonical Planning Surface

- `motion_planning/contracts/MotionTrajectory.h`
- `motion_planning/contracts/MotionPlan.h`
- `motion_planning/contracts/MotionPlanningReport.h`
- `motion_planning/contracts/TimePlanningConfig.h`

## Canonical Runtime / Control Surface

- `runtime_execution/contracts/motion/IInterpolationPort.h`
- `runtime_execution/contracts/motion/IIOControlPort.h`
- `runtime_execution/contracts/motion/IMotionRuntimePort.h`

## Consumer Rule

US3 consumers in `apps/*`, `modules/workflow/application`, and
`modules/dispense-packaging/application` must depend on the canonical roots above
instead of legacy bridge headers under `domain/motion/*` or
`domain/trajectory/value-objects/*`.
