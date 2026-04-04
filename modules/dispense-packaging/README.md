# dispense-packaging

`modules/dispense-packaging/` 是 `M8 dispense-packaging` 的 canonical owner 入口。

## Owner 范围

- 承接 `TriggerPlan`、`DispensingPlan`、`ExecutionPackage`、`PlanningArtifactExportRequest`、
  `AuthorityTriggerLayout` 的正式 owner 语义。
- 负责 preview payload 组装、执行包 built/validated 转换与离线校验边界。
- 向 `M9 runtime-execution` 输出可消费但不可回写的执行准备事实。
- `DispenseTimingPlan` 仅作为历史术语保留在旧文档中，不再对应新的 live 类型或 public API。

## Owner 入口

- 构建入口：`CMakeLists.txt`（target：`siligen_module_dispense_packaging`）。
- 模块契约入口：`contracts/README.md`。

## 边界约束

- `M6`、`M7` 只提供上游路径与运动结果，不在 `M8` 内继续编译 `*Planner*` / `*Optimization*` 规划实现。
- `M9 runtime-execution` 只能消费 `ExecutionPackage`，不得把 `built` 结果伪装成 `validated`。
- `workflow` 只能编排 `M8` 的 public service，不再自建 `PlanningArtifactExportRequest` 组装逻辑。
- 跨模块稳定工程契约应维护在 `shared/contracts/engineering/`，本目录仅承载 `M8` 专属契约。

## 当前事实来源

- `modules/dispense-packaging/application/services/dispensing/`
- `modules/dispense-packaging/domain/dispensing/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `application/services/dispensing/DispensePlanningFacade.cpp` 已成为 preview payload 与 execution package 组装 owner 面。
- `application/services/dispensing/AuthorityPreviewAssemblyService`、
  `ExecutionAssemblyService`、`PlanningArtifactExportAssemblyService`、
  `WorkflowPlanningAssemblyOperationsProvider`、
  `WorkflowDispensingProcessOperationsProvider`
  是当前稳定 public service；`DispensePlanningFacade` 仅保留兼容 wrapper 角色。
- `WorkflowPlanningAssemblyOperationsProvider` 的跨模块稳定输入/输出已收敛到
  `WorkflowPlanningAssemblyTypes.h`；`PlanningAssemblyTypes.h` 仅保留 `M8` 内部 stage
  组装使用，不再作为 workflow-facing seam 的公开类型面。
- `domain/dispensing/` 当前仍承载部分 planning/process-control residual，需继续分阶段清理，不应再由模块根 target 静默降级到 domain target。
- 所有 live 实现与构建入口均已收敛到 canonical roots。

## Phase 4 Closeout 口径

- `dispense-packaging` 阶段 4 的 closeout 证据以独立测试目标为准：
  `siligen_dispense_packaging_boundary_tests` 与
  `siligen_dispense_packaging_workflow_seam_tests`。
- `workflow` / `runtime-service` 的 planning 主线回归仍是阶段 4 的外层消费证据，但不替代 `M8` owner 自身证据。
- `siligen_dispense_packaging_unit_tests` 当前不作为阶段 4 closeout gate，因为其中
  `PreviewSnapshotServiceTest.cpp` 仍依赖旧 `PreviewSnapshotPayload` 字段名；这属于既有测试漂移，不应误记为本轮 seam 迁移回归。
- `PreviewSnapshotServiceTest.cpp` 的字段漂移仍是当前唯一已知任务外阻塞；`PlanningAssemblyTypes.h`
  收回内部后，不改变该阻塞判断。
