# dispense-packaging

`modules/dispense-packaging/` 是 `M8 dispense-packaging` 的 canonical owner 入口。

## Owner 范围

- 承接 `DispenseTimingPlan`、`TriggerPlan`、`ExecutionPackage` 的正式 owner 语义。
- 负责 preview payload 组装、执行包 built/validated 转换与离线校验边界。
- 向 `M9 runtime-execution` 输出可消费但不可回写的执行准备事实。

## Owner 入口

- 构建入口：`CMakeLists.txt`（target：`siligen_module_dispense_packaging`）。
- 模块契约入口：`contracts/README.md`。

## 边界约束

- `M6`、`M7` 只提供上游路径与运动结果，不在 `M8` 内继续编译 `*Planner*` / `*Optimization*` 规划实现。
- `M9 runtime-execution` 只能消费 `ExecutionPackage`，不得把 `built` 结果伪装成 `validated`。
- 跨模块稳定工程契约应维护在 `shared/contracts/engineering/`，本目录仅承载 `M8` 专属契约。

## 当前事实来源

- `modules/dispense-packaging/application/services/dispensing/`
- `modules/dispense-packaging/domain/dispensing/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `application/services/dispensing/DispensePlanningFacade.cpp` 已成为 preview payload 与 execution package 组装 owner 面。
- `application/services/dispensing/PlanningAssemblyTypes.h`、`AuthorityPreviewAssemblyService`、`ExecutionAssemblyService` 是 authority preview / execution assembly 的 canonical owner public surface。
- `domain/dispensing/` 当前仍承接 `DispensingPlannerService`、`ContourOptimizationService`、`UnifiedTrajectoryPlannerService` 等 planning domain owner 实现；这些实现不再允许回流到 `workflow`。
- 所有 live 实现与构建入口均已收敛到 canonical roots。
