# dispense-packaging Live Owner Boundary Inventory

本文件只记录 `ARCH-205` 第一阶段可见事实，不宣称 owner 迁移已经完成。

## 1. 当前 live target

### 1.1 `siligen_dispense_packaging_domain_dispensing`

由 `domain/dispensing/CMakeLists.txt` 当前编译的 `.cpp`：

- `domain-services/ArcTriggerPointCalculator.cpp`
- `domain-services/DispenseCompensationService.cpp`
- `domain-services/PathOptimizationStrategy.cpp`
- `domain-services/TriggerPlanner.cpp`
- `domain-services/DispensingController.cpp`
- `domain-services/DispensingProcessService.cpp`
- `planning/domain-services/AuthorityTriggerLayoutPlanner.cpp`
- `planning/domain-services/ClosedLoopCornerAnchorResolver.cpp`
- `planning/domain-services/ClosedLoopAnchorConstraintSolver.cpp`
- `planning/domain-services/PathArcLengthLocator.cpp`
- `planning/domain-services/CurveFlatteningService.cpp`
- `planning/domain-services/TopologySpanSplitter.cpp`
- `planning/domain-services/TopologyComponentClassifier.cpp`

判断：

- 这些文件都属于 live build 面。
- 其中一部分名称明显属于 planning / process-control residual，而不是已经收口的 packaging-only owner。

### 1.2 `siligen_dispense_packaging_application`

由 `application/CMakeLists.txt` 当前编译的 `.cpp`：

- `services/dispensing/PlanningAssemblyServices.cpp`
- `services/dispensing/PreviewSnapshotService.cpp`

判断：

- `PreviewSnapshotService.cpp` 更接近 preview snapshot payload 组装。
- `PlanningAssemblyServices.cpp` 当前以单实现文件承载两类 M8 application assembly service：`AuthorityPreviewAssemblyService` 与 `ExecutionAssemblyService`。
- planning artifact export request contract 已迁回 `workflow` owner，不再由 M8 application public surface 暴露。

## 2. 当前 parked / not-in-target 源文件

### 2.1 `domain/dispensing/domain-services/`

目录存在但当前未进入 `siligen_dispense_packaging_domain_dispensing` 的 `.cpp`：

- `CMPTriggerService.cpp`
- `PositionTriggerController.cpp`
- `PurgeDispenserProcess.cpp`
- `SupplyStabilizationPolicy.cpp`
- `ValveCoordinationService.cpp`

判断：

- 这些文件当前不能被视为 live owner 面。
- 继续和 live 文件混放会误导后续维护者对 M8 真实职责的判断。

### 2.2 `domain/dispensing/planning/domain-services/`

目录存在但当前未进入 `siligen_dispense_packaging_domain_dispensing` 的 `.cpp`：

- `ContourOptimizationService.cpp`
- `DispensingPlannerService.cpp`
- `UnifiedTrajectoryPlannerService.cpp`

补充：

- `ClosedLoopPlanningUtils.h` 当前是 header-only 辅助文件，不单独形成编译事实。

判断：

- 这些文件属于 parked / legacy-candidate 候选，不应被文档写成当前已收口的 canonical live owner 面。

## 3. 当前直接 consumer

以下文件直接消费 M8 application / contract surface：

- `modules/workflow/application/usecases/dispensing/PlanningUseCase.cpp`
- `apps/runtime-service/container/ApplicationContainer.Dispensing.cpp`

判断：

- 这些 consumer 证明 M8 application public surface 仍被跨模块直接依赖。
- 当前 live cutover 已把 façade 硬切为窄 service；export port owner 已迁回 `workflow`，不再由 M8 public surface 暴露。

## 4. 第一阶段结论

1. `dispense-packaging` 目前不是“pure packaging-only” 模块。
2. `domain/dispensing` 当前同时包含 live residual 与 parked residual。
3. 第一阶段先把 current-fact 文档和 inventory 对齐；owner 迁移留到后续阶段。
