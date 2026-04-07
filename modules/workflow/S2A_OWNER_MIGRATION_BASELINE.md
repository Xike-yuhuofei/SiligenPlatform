# S2-A Owner Migration Baseline

## Motion / Interpolation -> M7

| workflow compatibility target | owner path | note |
|---|---|---|
| `siligen_motion_execution_services` | `modules/workflow/domain/domain/motion/domain-services/MotionBufferController.cpp` | execution owner 已冻结到 workflow |
| `siligen_motion_execution_services` | `modules/workflow/domain/domain/motion/domain-services/JogController.cpp` | execution owner 已冻结到 workflow |
| `siligen_motion_execution_services` | `modules/workflow/domain/domain/motion/domain-services/HomingProcess.cpp` | execution owner 已冻结到 workflow |
| `siligen_motion_execution_services` | `modules/workflow/domain/domain/motion/domain-services/ReadyZeroDecisionService.cpp` | execution owner 已冻结到 workflow |
| `siligen_motion` | `modules/motion-planning/domain/motion/*.cpp` | planning owner 已冻结到 motion-planning，workflow 不再保留 fallback |
| `domain/motion/*` workflow legacy paths | `modules/motion-planning/domain/motion/*.h` | 仅保留 thin shim header，不再持有 live `.cpp` |
| `domain/motion/{BezierCalculator,BSplineCalculator,CircleCalculator,CMPValidator,CMPCompensation}` | `modules/motion-planning/domain/motion/*.h` | workflow legacy root helper 已改为 shim-only，duplicate `.cpp` 已移除 |
| `domain/motion/domain-services/interpolation/*` | `modules/motion-planning/domain/motion/domain-services/interpolation/*.h` | workflow legacy interpolation surface 已改为 shim-only，禁止再保留第二份实现 |
| `domain/motion/domain-services/{MotionControlServiceImpl,MotionStatusServiceImpl}` | `modules/runtime-execution/application/include/runtime_execution/application/services/motion/*.h` | runtime concrete owner 已冻结到 runtime-execution；motion-planning / workflow duplicate `.cpp` 已移除 |

## Dispensing Planning / Execution Package -> M8

| workflow compatibility surface | owner path | note |
|---|---|---|
| `domain_dispensing` | `siligen_dispense_packaging_domain_dispensing` | `workflow/domain/domain/dispensing/CMakeLists.txt` 已改为纯转发 |
| `PlanningUseCase` live planning dependency | `IPathSourcePort + ProcessPathFacade + MotionPlanningFacade + IWorkflowPlanningAssemblyOperations` | workflow live planning seam 已切到 M8 workflow-facing assembly operations |
| `PlanningUseCase` execution package dependency | `domain/dispensing/contracts/ExecutionPackage.h` | 继续通过 M8 contracts 消费执行包契约 |

## HMI Application -> M11

| compatibility surface | owner path | note |
|---|---|---|
| `apps/hmi-app/src/hmi_client/ui/main_window.py` launch/recovery/runtime degradation semantics | `modules/hmi-application/application/hmi_application/launch_state.py` | host 仅保留 Qt 宿主与 UI 呈现，launch owner 语义已下沉到 M11 |
| `apps/hmi-app/src/hmi_client/ui/main_window.py` preview session / preflight / resync semantics | `modules/hmi-application/application/hmi_application/preview_session.py` | `glue_points + motion_preview` 语义治理、旧契约拒绝与 preflight owner 已下沉到 M11 |
| `apps/hmi-app/src/hmi_client/client/*` and `features/dispense_preview_gate/*` | `modules/hmi-application/application/hmi_application/*.py` | 仅保留 compat re-export，不再新增 owner 逻辑 |
| `modules/hmi-application/tests/unit/*` | `modules/hmi-application/application/hmi_application/*` | M11 owner 规则 canonical tests；app 侧单测只保留 thin-host 集成断言 |

## Build Boundary Freeze

- `modules/motion-planning/application/CMakeLists.txt` 不再反向修改 `siligen_application_motion` / `siligen_application_dispensing`
- `modules/dispense-packaging/application/CMakeLists.txt` 不再反向修改 `siligen_application_dispensing`
- `siligen_workflow_dispensing_planning_compat` 已从 build graph 退场；live/test target 不得再依赖该 target 名称
- `modules/workflow/domain/domain/CMakeLists.txt` 不再提供 `siligen_motion` 本地 fallback，缺少 canonical owner target 时显式失败
- `siligen_process_runtime_core_*` 仅保留为 deprecated compatibility target，README 与后续 owner 论证不得再将其视为 live public surface
- `tests/reports/module-boundary-bridges-s2a/module-boundary-bridges.md` 当前状态为 `passed`
