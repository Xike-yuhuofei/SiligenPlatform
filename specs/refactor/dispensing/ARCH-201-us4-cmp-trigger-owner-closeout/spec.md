# ARCH-201 US4 CMP Trigger Owner Closeout

## Goal

Complete the final `cmp/trigger owner` closeout on top of US3.

This stage freezes `M8 dispense-packaging` as the only business owner for:

- authority trigger layout
- `TriggerPlan`
- CMP trigger packaging / execution-preparation semantics

`M7 motion-planning` remains responsible only for motion geometry, time-parameterized trajectory output, and reusable math helpers. It must not continue to own CMP/trigger business semantics.

## Authority Facts

- `US3` has already closed public-surface consumer leaks and is the stacked base for this branch.
- ADR-005 has already frozen runtime CMP config authority to `[ValveDispenser]`; US4 must not reopen config-entry ownership.
- `modules/dispense-packaging/README.md` already declares `TriggerPlan` and `ExecutionPackage` as `M8` owner artifacts.
- `modules/dispense-packaging/domain/dispensing/planning/domain-services/AuthorityTriggerLayoutPlanner.*` already provides the strongest current evidence for authority trigger layout ownership.
- `modules/motion-planning/domain/motion/TriggerCalculator.*` and `CMPCoordinatedInterpolator.*` still hold CMP/trigger planning semantics inside `M7`.
- `modules/workflow/domain/domain/dispensing/` still contains duplicated live trigger/CMP planning sources and legacy concrete planning flow.

## Scope

- `modules/dispense-packaging`
- `modules/motion-planning`
- `modules/workflow`
- `apps/planner-cli`
- `apps/runtime-service`
- `scripts/validation`

## In Scope

- Freeze the owner boundary so `M8` is the only business owner for trigger/CMP semantics.
- Remove or demote duplicated trigger/CMP owner logic from `M7` and workflow legacy roots.
- Keep `M7` only as trajectory/math producer or compatibility seam where needed.
- Route preview and execution-preparation chains to consume the same `M8` authority artifacts.
- Add scoped validation that prevents trigger/CMP owner regressions.
- Record dedicated US4 spec/plan/tasks and evidence.

## Out Of Scope

- Reopening runtime CMP config authority already settled by ADR-005
- HMI `no-loose-mock` repair
- `MockMultiCardCharacterizationTest.CrdClearDropsBufferedTrajectoryBeforeNextRun`
- `ARCH-201-m7` residue or other non-US4 cleanup
- New machine config fields, TCP JSON fields, or CLI behavior expansion
- Full hardware bring-up or online validation

## Semantic Contract Freeze

### Semantic Objects

- Authority trigger layout
- `TriggerPlan`
- execution-preparation CMP payload
- preview truth for glue/trigger points

### Inputs

- `ProcessPath` from `M6`
- `MotionTrajectory` / interpolation-capable motion facts from `M7`
- dispenser runtime config from `[ValveDispenser]`

### Outputs

- authority trigger points for preview and diagnostics
- `TriggerPlan`
- execution-ready packaging artifacts consumed by workflow/runtime

### Authority Artifact

- `M8` authority planning artifacts produced by `AuthorityTriggerLayoutPlanner` and `DispensePlanningFacade`

### Owner / Consumer Split

- owner: `modules/dispense-packaging`
- consumers: `modules/workflow`, `apps/planner-cli`, `apps/runtime-service`, `modules/runtime-execution`
- forbidden re-owners: `modules/motion-planning`, `modules/workflow/domain/domain/dispensing`

## Success Criteria

1. `M8 dispense-packaging` is the only module that owns trigger/CMP business semantics and authority trigger layout.
2. `M7 motion-planning` no longer exposes live trigger/CMP business owner logic beyond math/helper seams that do not define business truth.
3. Workflow preview and execution-preparation chains consume the same `M8` authority artifacts instead of rebuilding trigger truth locally.
4. Scoped bridge validation detects forbidden trigger/CMP owner references or live source regressions.
5. Targeted tests pass, or any remaining failure is proven pre-existing and documented out of US4 scope.
