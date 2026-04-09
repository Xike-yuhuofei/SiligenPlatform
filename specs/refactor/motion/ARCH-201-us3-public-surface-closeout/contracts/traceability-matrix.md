# US3 Traceability Matrix

| Requirement | Consumer Surface | Primary Files | Validation |
| --- | --- | --- | --- |
| P1 | Planning report public surface comes from `motion_planning/contracts/*` | `modules/workflow/application/include/application/planning-trigger/PlanningUseCase.h`, `apps/planner-cli/CommandHandlers.Dxf.cpp` | targeted build of `siligen_planner_cli` |
| R1 | Runtime/control public headers come from `runtime_execution/contracts/motion/*` | `modules/workflow/application/include/application/usecases/motion/*`, `modules/workflow/application/usecases/motion/runtime/MotionRuntimeAssemblyFactory.h`, `modules/workflow/application/usecases/motion/trajectory/DeterministicPathExecutionUseCase.*` | targeted workflow/runtime builds |
| A1 | Runtime-service app surface consumes canonical runtime motion contracts | `apps/runtime-service/bootstrap/*`, `apps/runtime-service/container/*`, `apps/runtime-service/tests/integration/HostBootstrapSmokeTest.cpp` | runtime-service unit/integration targets |
| G1 | Boundary gate blocks US3 regressions | `scripts/validation/assert-module-boundary-bridges.ps1` | bridge assertion script |
