# S2-A Owner Migration Baseline

## Motion / Interpolation -> M7

| workflow compatibility target | owner path | note |
|---|---|---|
| `runtime-execution motion services leaf target` | `modules/runtime-execution/application/services/motion/MotionBufferController.cpp` | `siligen_runtime_execution_motion_services` 是唯一 execution-owner build target；workflow compat target 已删除 |
| `runtime-execution motion services leaf target` | `modules/runtime-execution/application/services/motion/JogController.cpp` | `siligen_runtime_execution_motion_services` 是唯一 execution-owner build target；workflow compat target 已删除 |
| `runtime-execution motion services leaf target` | `modules/runtime-execution/application/services/motion/HomingProcess.cpp` | `siligen_runtime_execution_motion_services` 是唯一 execution-owner build target；workflow compat target 已删除 |
| `runtime-execution motion services leaf target` | `modules/runtime-execution/application/services/motion/ReadyZeroDecisionService.cpp` | `siligen_runtime_execution_motion_services` 是唯一 execution-owner build target；workflow compat target 已删除 |
| `siligen_motion` | `modules/motion-planning/domain/motion/*.cpp` | planning owner 已冻结到 motion-planning，workflow 不再保留 fallback |
| `domain/motion/*` legacy include | `modules/motion-planning/domain/motion/*.h` | 仅 planning owner header 保留；runtime port alias 与 execution service shim 已删除 |
| `domain/motion/{BezierCalculator,BSplineCalculator,CircleCalculator,CMPValidator,CMPCompensation}` | `modules/motion-planning/domain/motion/*.h` | workflow legacy root helper 已改为 shim-only，duplicate `.cpp` 已移除 |
| `domain/motion/domain-services/interpolation/*` | `modules/motion-planning/domain/motion/domain-services/interpolation/*.h` | workflow legacy interpolation surface 已改为 shim-only，禁止再保留第二份实现 |
| `domain/motion/ports/{IMotionRuntimePort,IIOControlPort}` | `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/*` | motion-planning / workflow legacy port alias header 已删除；live consumer 必须切 canonical runtime contract path |
| `domain/motion/domain-services/{HomingProcess,JogController,MotionBufferController,ReadyZeroDecisionService,MotionControlServiceImpl,MotionStatusServiceImpl}` | `modules/runtime-execution/application/include/runtime_execution/application/services/motion/*.h` | motion-planning / workflow legacy shim header 已删除；仅保留 canonical runtime path |

## Machine / Calibration -> Runtime Execution

| workflow compatibility target / surface | owner path | note |
|---|---|---|
| `runtime-execution machine model leaf target` | `modules/runtime-execution/application/system/LegacyDispenserModel.cpp` | `siligen_runtime_execution_machine_model` 是唯一 machine aggregate build target；workflow `domain_machine` 已删除 |
| `domain/machine/aggregates/DispenserModel.h` | `modules/runtime-execution/application/include/runtime_execution/application/system/LegacyDispenserModel.h` | workflow public header 已删除；live consumer 需切 canonical runtime path |
| `domain/machine/domain-services/CalibrationProcess.h` | `modules/runtime-execution/application/include/runtime_execution/application/services/calibration/CalibrationWorkflowService.h` | workflow calibration alias 已删除；runtime canonical test 已接管验证 |
| `domain/machine/value-objects/CalibrationTypes.h` | `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/system/CalibrationExecutionTypes.h` | workflow calibration types alias 已删除 |
| `domain/machine/ports/{ICalibrationDevicePort,ICalibrationResultPort}.h` | `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/system/*` | workflow calibration port alias 已删除 |

## Recipe Serialization Surface

| compatibility surface | owner path | note |
|---|---|---|
| `workflow/adapters/include/workflow/adapters/recipes/serialization/RecipeJsonSerializer.h` | canonical public serializer header | live consumer 统一包含 canonical workflow/adapters path |
| `workflow/adapters/include/recipes/serialization/RecipeJsonSerializer.h` | removed | legacy public wrapper 已删除，不再保留第二个 public truth |

## Dispensing Planning / Execution Package -> M8

| workflow compatibility surface | owner path | note |
|---|---|---|
| `domain_dispensing` | `siligen_dispense_packaging_domain_dispensing` | `workflow/domain/domain/dispensing/CMakeLists.txt` 已改为纯转发 |
| `PlanningUseCase` live planning dependency | `IPathSourcePort + ProcessPathFacade + MotionPlanningFacade + DispensePlanningFacade` | workflow live 链已不再注入 `DispensingPlannerService` concrete |
| `PlanningUseCase` execution package dependency | `domain/dispensing/contracts/ExecutionPackage.h` | 继续通过 M8 contracts 消费执行包契约 |

## HMI Application -> M11

| compatibility surface | owner path | note |
|---|---|---|
| `apps/hmi-app/src/hmi_client/ui/main_window.py` launch/recovery/runtime degradation semantics | `modules/hmi-application/application/hmi_application/launch_state.py` | host 仅保留 Qt 宿主与 UI 呈现，launch owner 语义已下沉到 M11 |
| `apps/hmi-app/src/hmi_client/ui/main_window.py` preview session / preflight / resync semantics | `modules/hmi-application/application/hmi_application/preview_session.py` | `glue_points + execution_polyline` 语义治理、旧契约拒绝与 preflight owner 已下沉到 M11 |
| `apps/hmi-app/src/hmi_client/client/*` and `features/dispense_preview_gate/*` | `modules/hmi-application/application/hmi_application/*.py` | 仅保留 compat re-export，不再新增 owner 逻辑 |
| `modules/hmi-application/tests/unit/*` | `modules/hmi-application/application/hmi_application/*` | M11 owner 规则 canonical tests；app 侧单测只保留 thin-host 集成断言 |

## Build Boundary Freeze

- `modules/motion-planning/application/CMakeLists.txt` 不再反向修改 `siligen_application_motion` / `siligen_application_dispensing`
- `modules/dispense-packaging/application/CMakeLists.txt` 不再反向修改 `siligen_application_dispensing`
- `siligen_workflow_dispensing_planning_compat` 已删除，live target 禁止新增任何同类 compat target
- `modules/workflow/domain/domain/CMakeLists.txt` 不再提供 `siligen_motion` 本地 fallback，缺少 canonical owner target 时显式失败
- `modules/workflow/application/CMakeLists.txt` 不再链接 `siligen_runtime_execution_application_public`；`MotionRuntimeAssemblyFactory` 已删除
- `modules/workflow/tests/unit` 是 workflow 自有 canonical tests root；runtime-owned motion/device tests 固定到 `modules/runtime-execution/runtime/host/tests`
- workflow test helper / smoke target 已去掉 `process_runtime_core_*` 历史命名，不再把该命名空间当作 live public surface 或 owner 证据
- `tests/reports/module-boundary-bridges-s2a/module-boundary-bridges.md` 当前状态为 `passed`
