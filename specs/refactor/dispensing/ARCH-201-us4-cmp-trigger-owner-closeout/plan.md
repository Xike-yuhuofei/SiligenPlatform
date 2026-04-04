# ARCH-201 US4 CMP Trigger Owner Closeout Plan

## Summary

US4 is the owner-closeout stage after US3 public-surface cleanup.

The core decision is:

- `M8 dispense-packaging` owns trigger/CMP business semantics and authority truth
- `M7 motion-planning` only provides motion/trajectory calculation and math-oriented support
- workflow becomes orchestration and compatibility consumption only

US4 must not reopen runtime config authority already frozen by ADR-005.

## Context Bootstrap

### Related Modules

- `modules/dispense-packaging`
- `modules/motion-planning`
- `modules/workflow`
- `apps/planner-cli`
- `apps/runtime-service`
- `scripts/validation`

### Priority Documents

- `docs/session-handoffs/2026-04-04-1703-arch201-us4-session-handoff.md`
- `docs/decisions/ADR-005-cmp-runtime-config-boundary.md`
- `docs/process-model/reviews/motion-planning-module-architecture-review-20260331-075136-944.md`
- `specs/refactor/motion/ARCH-201-us3-public-surface-closeout/*`

### Key Code Entrances

- `modules/dispense-packaging/domain/dispensing/planning/domain-services/AuthorityTriggerLayoutPlanner.*`
- `modules/dispense-packaging/application/services/dispensing/DispensePlanningFacade.*`
- `modules/dispense-packaging/domain/dispensing/domain-services/TriggerPlanner.*`
- `modules/motion-planning/domain/motion/domain-services/TriggerCalculator.*`
- `modules/motion-planning/domain/motion/CMPCoordinatedInterpolator.*`
- `modules/workflow/domain/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp`

## Contract Freeze

### Authority Artifact

- `AuthorityTriggerLayout` plus `DispensePlanningFacade` assembled planning artifacts are the business truth source for trigger layout and preview truth.

### Stable Inputs

- `ProcessPath`
- `MotionTrajectory`
- `[ValveDispenser]` dispenser runtime config

### Stable Outputs

- authority trigger points
- `TriggerPlan`
- execution-preparation artifacts consumed by workflow/runtime

### Owner Boundary

- owner layer: `modules/dispense-packaging`
- consumer layers: workflow/application, planner CLI, runtime service bootstrap/runtime execution
- prohibited re-interpretation layers:
  - `modules/motion-planning/domain/motion`
  - `modules/workflow/domain/domain/dispensing`

## Design Decisions

1. **Keep motion math in M7, move business truth to M8**
   `TriggerCalculator` and `CMPCoordinatedInterpolator` may survive only as math-oriented helpers if still required by `M8`, but they must stop defining trigger/CMP business truth, authority layout, or packaging semantics.

2. **Authority preview and execution share one truth**
   Preview glue/trigger points and execution-preparation payload must be derived from the same `M8` authority artifacts. Workflow must not rebuild a separate local trigger truth.

3. **Legacy workflow trigger/CMP planner code becomes compatibility residue only**
   `modules/workflow/domain/domain/dispensing` must not keep live duplicate trigger/CMP planning implementations. Remaining headers, if needed, are shim-only.

4. **Config authority stays closed**
   Runtime config continues to come from `[ValveDispenser]`. No US4 change may reintroduce `[CMP]` as a runtime authority source or add misleading CLI overrides.

## Intended Change Shape

### Owner Landing Zone

- `modules/dispense-packaging/domain/dispensing/planning/domain-services/`
- `modules/dispense-packaging/application/services/dispensing/`
- `modules/dispense-packaging/domain/dispensing/value-objects/`

### Expected Demotions

- `modules/motion-planning/domain/motion/domain-services/TriggerCalculator.*`
- `modules/motion-planning/domain/motion/CMPCoordinatedInterpolator.*`
- `modules/workflow/domain/domain/dispensing/domain-services/TriggerPlanner.*`
- `modules/workflow/domain/domain/dispensing/domain-services/CMPTriggerService.*`
- `modules/workflow/domain/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp`

### Bridge / Validation Updates

- `scripts/validation/assert-module-boundary-bridges.ps1`
- targeted module-owner tests for `M8`
- targeted regression around preview authority and execution-package assembly

## Validation Strategy

### Required Checks

- `modules/dispense-packaging/tests/unit/domain/dispensing/AuthorityTriggerLayoutPlannerTest.cpp`
- `modules/dispense-packaging/tests/unit/domain/dispensing/TriggerPlannerTest.cpp`
- `modules/dispense-packaging/tests/unit/application/services/dispensing/DispensePlanningFacadeTest.cpp`
- relevant workflow dispensing use-case tests touching `preview_authority_ready`
- `scripts/validation/assert-module-boundary-bridges.ps1`

### Stop Conditions

- A required change would alter `[ValveDispenser]` runtime config authority or reopen ADR-005.
- A required change would pull HMI, mock multicard, or non-US4 residue into this batch.
- A required change needs broad protocol/schema redesign outside trigger/CMP owner closeout.
- Evidence shows a necessary owner is outside `M8` but lacks authoritative artifact support; in that case the batch must pause and re-freeze contract before implementation.
