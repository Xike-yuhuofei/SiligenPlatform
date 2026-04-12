# dispense-packaging

`modules/dispense-packaging/` 是 `M8 dispense-packaging` 的 canonical owner 入口。

## Owner 范围

- 承接 `TriggerPlan`、`DispensingPlan`、`ExecutionPackage`、`PlanningArtifactExportRequest`、
  `AuthorityTriggerLayout` 的正式 owner 语义。
- `PlanningArtifactExportRequest` 仅以 `domain/dispensing/contracts` canonical namespace
  对外暴露，不再提供 application namespace alias。
- 负责 preview payload 组装、执行包 built/validated 转换与离线校验边界。
- 向 `M9 runtime-execution` 输出可消费但不可回写的执行准备事实。
- `DispenseTimingPlan` 仅作为历史术语保留在旧文档中，不再对应新的 live 类型或 public API。

## Owner 入口

- 构建入口：`CMakeLists.txt`（target：`siligen_module_dispense_packaging`）。
- 模块契约入口：`contracts/README.md`。

## 边界约束

- `M6`、`M7` 只提供上游路径与运动结果，不在 `M8` 内继续编译 `*Planner*` / `*Optimization*` 规划实现。
- `M9 runtime-execution` 只能消费 `ExecutionPackage`，不得把 `built` 结果伪装成 `validated`。
- planning artifact export 的 port/result contract 由 `M9 runtime-execution` owner；
  `M8` 仅 owner request contract 与 request assembly。
- `workflow` 只能编排 `M8` 的 public service，不再自建 `PlanningArtifactExportRequest` 组装逻辑。
- 跨模块稳定工程契约应维护在 `shared/contracts/engineering/`，本目录仅承载 `M8` 专属契约。

## 当前事实来源

- `modules/dispense-packaging/application/services/dispensing/`
- `modules/dispense-packaging/domain/dispensing/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `application/services/dispensing/PlanningAssemblyServices.cpp` 与
  `WorkflowPlanningAssemblyOperationsProvider.cpp` 是当前 planning owner seam；
  其中 `PlanningAssemblyServices.cpp` 只保留 workflow-facing wrapper。
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
  `application/services/dispensing/PlanningAssemblyResidualFacade.cpp` 与
  `PlanningAssemblyResidual*.cpp` 私有实现的本地细节存在，不再保留独立 public 头，
  也不再作为 workflow-facing seam 的公开类型面。
- `DispensingExecutionPlan` 保留为 `ExecutionPackage` 的 M8 owner backing type；
  `DispensingRuntimeOverrides`、`DispensingRuntimeParams`、
  `DispensingExecutionOptions`、`DispensingExecutionReport` 与
  `GuardDecision`/`MachineMode`/`JobExecutionMode`/`ProcessOutputPolicy`
  的 canonical owner 当前是 `runtime-execution/contracts/runtime`。
- `domain/dispensing/` 当前仍承载部分 planning/process-control residual，需继续分阶段清理，不应再由模块根 target 静默降级到 domain target。
- `domain/dispensing/CMakeLists.txt` 当前 owner/core 口径已收敛为
  `siligen_dispense_packaging_domain_dispensing` header/contract surface；
  runtime/motion fat include 依赖仅允许经 module-local
  `siligen_dispense_packaging_domain_dispensing_planning_residual_headers` 与
  `siligen_dispense_packaging_domain_dispensing_execution_residual_headers`
  提供给 residual/internal target，不得再挂在 owner/core target 上；
  legacy DXF planning concrete 仅保留在
  `siligen_dispense_packaging_planning_legacy_dxf_quarantine_support` 测试支撑 target，
  `UnifiedTrajectoryPlannerService` wrapper 已删除，残余 DXF 主线在
  `DispensingPlannerService.cpp` 内仅通过私有 helper 编排
  `ProcessPathFacade` / `MotionPlanningFacade`；
  `PathOptimizationStrategy` 已删除，轮廓重排 / 最近邻 / 2-Opt 优化残余当前仅收敛在
  `ContourOptimizationService.cpp` 这一条 quarantine seam；
  原 wrapper 行为证据由 `modules/process-path/tests/unit/application/services/process_path/ProcessPathFacadeTest.cpp`
  与 `modules/process-path/tests/integration/ProcessPathToMotionPlanningIntegrationTest.cpp`
  承接；
  execution/process-control/valve/CMP concrete 仅保留在
  `siligen_dispense_packaging_execution_residual`。不得恢复 workflow domain target、
  raw workflow include root
  或 raw runtime contract include root 暴露。
- `tests/` 当前已把 owner boundary / workflow seam / residual acceptance /
  planning residual regression / execution residual regression / shared-adjacent
  regression 分 lane；`unit` 不再混入显式 residual topology 守护。
- owner-facing canonical roots 已收敛，但 `domain/dispensing/` 下仍保留
  planning / execution residual targets，属于迁移中事实，不应误记为“所有 live
  实现都已完成 owner 归位”。

## Phase 4 Closeout 口径

- `dispense-packaging` 阶段 4 的 closeout 证据以独立测试目标为准：
  `siligen_dispense_packaging_boundary_tests` 与
  `siligen_dispense_packaging_workflow_seam_tests`。
- `siligen_dispense_packaging_residual_regression_tests` 当前作为结构治理回归 lane
  单独执行，用于守 `BR-*` / residual topology 事实，但不替代 closeout gate。
- `workflow` / `runtime-service` 的 planning 主线回归仍是阶段 4 的外层消费证据，但不替代 `M8` owner 自身证据。
- `siligen_dispense_packaging_unit_tests` 当前作为相邻回归应继续执行，但不替代
  boundary/workflow seam gate，也不再承载 `DispensePackagingResidualAcceptanceTest.cpp`
  这类结构治理守护。
- planning / execution residual 与 shared-adjacent 回归当前分别通过
  `siligen_dispense_packaging_planning_residual_regression_tests`、
  `siligen_dispense_packaging_execution_residual_regression_tests`、
  `siligen_dispense_packaging_shared_adjacent_regression_tests` 执行；这些目标用于
  邻接稳定性守护，不构成 `M8` owner closeout 证据。
- 本阶段额外下游消费证据包括 `siligen_planner_cli` 与 `siligen_unit_tests`
  的成功构建；若后续再次出现编译回归，应优先检查 workflow DTO 头是否重新发生重复定义或 raw include root 泄漏。
