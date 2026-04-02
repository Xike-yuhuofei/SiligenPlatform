# dispense-packaging

`modules/dispense-packaging/` 是 `M8 dispense-packaging` 的 canonical 模块入口。

## Owner 范围

- 当前稳定 owner artifact 以 `ExecutionPackage` 为主；`DispenseTimingPlan`、`TriggerPlan` 仍处于待进一步冻结的过渡面。
- 负责 preview payload 组装、执行包 `built -> validated` 转换与离线校验边界。
- 向 `M9 runtime-execution` 输出可消费但不可回写的执行准备事实。

## Owner 入口

- 构建入口：`CMakeLists.txt`（target：`siligen_module_dispense_packaging`）。
- 模块契约入口：`contracts/README.md`。

## 边界约束

- `M6`、`M7` 应只提供上游路径与运动结果；但当前 `M8 domain/dispensing` 仍残留部分 `planning` / `process-control` 实现，尚未完成 owner 迁出。
- `M9 runtime-execution` 只能消费 `ExecutionPackage`，不得把 `built` 结果伪装成 `validated`。
- 跨模块稳定工程契约应维护在 `shared/contracts/engineering/`，本目录仅承载 `M8` 专属契约。

## 当前事实来源

- `modules/dispense-packaging/application/services/dispensing/`
- `modules/dispense-packaging/domain/dispensing/`
- `modules/workflow/application/usecases/dispensing/PlanningUseCase.cpp` 作为当前直接 consumer

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `application/services/dispensing/PlanningAssemblyServices.cpp` 当前承载两类 M8 application assembly service：`AuthorityPreviewAssemblyService` 负责 authority preview 事实组装，`ExecutionAssemblyService` 负责 execution package 与 execution binding 组装。
- planning artifact export request 已迁出到 `workflow/application/services/dispensing/PlanningArtifactExportPort.h`，`dispense-packaging` 不再 owner 该导出契约。
- `domain/dispensing/` 当前同时包含 packaging owner 实现与尚未迁出的 `planning` / `process-control` residual。
- 当前 live / parked 清单见 `LIVE_OWNER_BOUNDARY_INVENTORY.md`。
