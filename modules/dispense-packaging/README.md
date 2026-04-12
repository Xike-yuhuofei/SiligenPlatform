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
- `application/services/dispensing/PlanningAssemblyServices.cpp` 与
  `WorkflowPlanningAssemblyOperationsProvider.cpp` 是当前 planning owner seam。
- `application/services/dispensing/AuthorityPreviewAssemblyService`、
  `ExecutionAssemblyService`、`PlanningArtifactExportAssemblyService`、
  `WorkflowPlanningAssemblyOperationsProvider`
  是当前稳定 public service。
- execution concrete / process port factory 已迁入 `M9 runtime-execution`；`M8` 不再导出 execution provider。
- `WorkflowPlanningAssemblyOperationsProvider` 的跨模块稳定输入/输出已收敛到
  workflow DTO：`AuthorityPreviewAssemblyService` 公开接收
  `WorkflowAuthorityPreviewRequest`，`ExecutionAssemblyService` 公开接收
  `WorkflowExecutionAssemblyRequest`。`application/include/application/services/dispensing/WorkflowPlanningAssemblyTypes.h`
  当前已恢复为 canonical DTO owner；workflow/application 中同名头仅保留 compat
  forwarder。内部 stage 类型当前只作为
  `application/services/dispensing/PlanningAssemblyServices.cpp` 的本地实现细节存在，
  不再保留独立 public 头，也不再作为 workflow-facing seam 的公开类型面。
- `DispensingExecutionPlan` 保留为 `ExecutionPackage` 的 M8 owner backing type；
  `DispensingRuntimeOverrides`、`DispensingRuntimeParams`、
  `DispensingExecutionOptions`、`DispensingExecutionReport` 与
  `GuardDecision`/`MachineMode`/`JobExecutionMode`/`ProcessOutputPolicy`
  的 canonical owner 当前是 `runtime-execution/contracts/runtime`。
- `domain/dispensing/` 当前仍承载部分 planning/process-control residual，需继续分阶段清理，不应再由模块根 target 静默降级到 domain target。
- `domain/dispensing/CMakeLists.txt` 当前 owner/core 口径已收敛为
  `siligen_dispense_packaging_domain_dispensing` header/contract surface；
  concrete planning link 仅保留在 `siligen_dispense_packaging_planning_residual`，
  execution/process-control/valve/CMP concrete 仅保留在
  `siligen_dispense_packaging_execution_residual`。不得恢复 workflow domain target、
  raw workflow include root
  或 raw runtime contract include root 暴露。
- 所有 live 实现与构建入口均已收敛到 canonical roots。

## Phase 4 Closeout 口径

- `dispense-packaging` 阶段 4 的 closeout 证据以独立测试目标为准：
  `siligen_dispense_packaging_boundary_tests` 与
  `siligen_dispense_packaging_workflow_seam_tests`。
- `workflow` / `runtime-service` 的 planning 主线回归仍是阶段 4 的外层消费证据，但不替代 `M8` owner 自身证据。
- `siligen_dispense_packaging_unit_tests` 当前作为相邻回归应继续执行，但不替代
  boundary/workflow seam gate。
- 本阶段额外下游消费证据包括 `siligen_planner_cli` 与 `siligen_unit_tests`
  的成功构建；若后续再次出现编译回归，应优先检查 workflow DTO 头是否重新发生重复定义或 raw include root 泄漏。
