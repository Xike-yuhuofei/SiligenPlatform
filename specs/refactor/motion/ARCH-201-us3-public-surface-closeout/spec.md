# ARCH-201 US3 Public Surface Closeout

## Goal

Make the canonical public surface visible at the consumer edge for:

- M7 planning contracts
- M9 runtime/control contracts

This stage only changes consumer include direction and exposed public types.
It does not change behavior, protocol fields, or owner semantics.

## Scope

- `apps/planner-cli`
- `apps/runtime-service`
- `modules/workflow/application`
- `modules/dispense-packaging/application`
- `scripts/validation`

## In Scope

- Replace legacy planning report includes with `motion_planning/contracts/*`
- Replace legacy runtime/control port includes with `runtime_execution/contracts/motion/*`
- Update public headers so exposed types point at canonical contract roots
- Add scoped boundary checks for the above paths
- Record evidence in a dedicated US3 spec bundle

## Out Of Scope

- HMI `no-loose-mock` repair
- Trigger/CMP owner unification
- Runtime/control behavior rewrites
- New protocol fields or DTO shape changes

## Success Criteria

1. Target consumer/public headers no longer directly include legacy planning report headers.
2. Target workflow/runtime public headers no longer directly include legacy runtime/control contract headers.
3. Planner CLI exports planning report data from canonical planning contracts.
4. Boundary validation includes scoped checks for the US3 paths.
5. Targeted build/test commands pass, or any remaining failure is pre-existing and explicitly documented.
