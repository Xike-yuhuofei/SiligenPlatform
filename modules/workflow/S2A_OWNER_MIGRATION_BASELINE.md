# S2-A Owner Migration Baseline

## Motion / Interpolation -> M7

| workflow compatibility target | owner path | note |
|---|---|---|
| `siligen_motion_execution_services` | `modules/motion-planning/domain/motion/domain-services/MotionBufferController.cpp` | workflow 不再编译本地 motion execution source |
| `siligen_motion_execution_services` | `modules/motion-planning/domain/motion/domain-services/JogController.cpp` | workflow 不再编译本地 motion execution source |
| `siligen_motion_execution_services` | `modules/motion-planning/domain/motion/domain-services/HomingProcess.cpp` | workflow 不再编译本地 motion execution source |
| `siligen_motion_execution_services` | `modules/motion-planning/domain/motion/domain-services/ReadyZeroDecisionService.cpp` | S2-A 新增 canonical owner 副本 |
| `siligen_motion_execution_services` | `modules/motion-planning/domain/motion/domain-services/MotionControlServiceImpl.cpp` | workflow 不再编译本地 motion concrete |
| `siligen_motion_execution_services` | `modules/motion-planning/domain/motion/domain-services/MotionStatusServiceImpl.cpp` | workflow 不再编译本地 motion concrete |

## Dispensing Planning / Execution Package -> M8

| workflow compatibility surface | owner path | note |
|---|---|---|
| `domain_dispensing` | `siligen_dispense_packaging_domain_dispensing` | `workflow/domain/domain/dispensing/CMakeLists.txt` 已改为纯转发 |
| `PlanningUseCase` planner dependency | `domain/dispensing/planning/domain-services/DispensingPlannerService.h` | workflow 不再依赖 `DispensePlanningFacade` |
| `PlanningUseCase` execution package dependency | `domain/dispensing/contracts/ExecutionPackage.h` | 继续通过 M8 contracts 消费执行包契约 |

## Build Boundary Freeze

- `modules/motion-planning/application/CMakeLists.txt` 不再反向修改 `siligen_application_motion` / `siligen_application_dispensing`
- `modules/dispense-packaging/application/CMakeLists.txt` 不再反向修改 `siligen_application_dispensing`
- `tests/reports/module-boundary-bridges-s2a/module-boundary-bridges.md` 当前状态为 `passed`
