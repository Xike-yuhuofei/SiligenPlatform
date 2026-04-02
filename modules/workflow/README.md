# workflow

`modules/workflow/` 是 `M0 workflow` 的 canonical owner 入口。

## Owner 范围

- 工作流编排与阶段推进语义
- 规划链（`M4-M8`）触发与编排边界
- 流程状态与调度决策的事实归属

## 边界约束

- 历史 `process-runtime-core/` 与 `src/` bridge root 已退役；`workflow` 不再保留这两类 live owner 承载面。
- `workflow` 仅承载 `M0` 编排职责，不直接承载 `M4-M8` 的模块内事实实现。
- 跨模块稳定公共契约应维护在 `shared/contracts/`，`M0` 专属契约放在 `modules/workflow/contracts/`。
- `domain/CMakeLists.txt` 与 `application/CMakeLists.txt` 当前仅允许保留已登记的 bridge-only 聚合，禁止继续新增 sibling source bridge。
- runtime service 装配 concrete 与 planning 工件落盘 concrete 固定由 `modules/runtime-execution/` 承接，`workflow` 只保留契约、编排和端口。

## 迁移来源（当前事实）

- `legacy-archive/process-runtime-core/src/application`
- `legacy-archive/process-runtime-core/src/domain`
- `legacy-archive/process-runtime-core/modules/process-core`
- `legacy-archive/process-runtime-core/modules/motion-core`
- `legacy-archive/workflow/src/application`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- 历史目录当前按 bridge 管理；新增终态实现不得继续进入 bridge 目录。
- `application/include/`、`domain/include/`、`adapters/include/` 现为对外 canonical public surface。
- `contracts/include/workflow/contracts/WorkflowContracts.h` 已成为 `M0 -> M4` 触发边界的 owner 契约入口。
- `domain/`、`application/`、`adapters/`、`tests/` 已成为 `workflow` 的唯一真实实现与验证承载面。
- 历史 `src/` 与 `process-runtime-core/` bridge root 已退出 live graph，不再承载真实 `.h/.cpp` payload。

## S1 完成态

- `workflow/contracts` 已落地 `WorkflowStageState`、`WorkflowCommand`、`WorkflowPlanningTriggerRequest`、`WorkflowPlanningTriggerResponse`、`WorkflowFailureCategory`、`WorkflowRecoveryDirective`。
- `PlanningUseCase` 已改为编排 `IPathSourcePort + ProcessPathFacade + MotionPlanningFacade + DispensePlanningFacade`；CSV/JSON 工件落盘继续通过 `IPlanningArtifactExportPort` 交给 `runtime-execution` concrete 承担。
- `assert-module-boundary-bridges.ps1` 已接入 bridge-only 收口标记，并在 `rg` 不可用时回退到 PowerShell 原生搜索。

## S2-A 完成态

- `workflow/application` 的 live planning 链已改为显式编排 `M6 process-path`、`M7 motion-planning`、`M8 dispense-packaging` 的 public facade，不再注入 `DispensingPlannerService` concrete。
- `workflow/application` 的旧 motion usecase 头文件与 `.cpp` 已删除；live motion owner 统一切到 `modules/runtime-execution/application/` canonical surface。
- `workflow/domain/domain/CMakeLists.txt` 不再定义 `siligen_motion_execution_services`；`workflow/domain/**` 下的 execution-owner motion shim header 已删除。
- `workflow/domain/**` 下的 `IMotionRuntimePort` / `IIOControlPort` legacy port alias header 已删除；live consumer 必须直连 `runtime_execution/contracts/motion/*`。
- `workflow/domain/domain/machine/CMakeLists.txt` 不再定义 `domain_machine`；workflow machine/calibration compatibility header 已删除，owner surface 固定到 `runtime-execution` / `coordinate-alignment` canonical target。
- `workflow/adapters/include/recipes/serialization/RecipeJsonSerializer.h` 已删除；canonical public serializer header 固定为 `modules/workflow/adapters/include/workflow/adapters/recipes/serialization/RecipeJsonSerializer.h`。
- `motion-planning/application` 与 `dispense-packaging/application` 不再反向修改 `workflow` target，`workflow` 自身负责声明 owner 依赖。
- bridge 报告已恢复绿色，见 `tests/reports/module-boundary-bridges-s2a/`。

## S3 当前口径

- `workflow/contracts/WorkflowExecutionPort.h` 是 `workflow -> runtime-execution` 的唯一执行编排契约；`workflow` 只依赖该 port 与 runtime contracts，不再直连 `runtime-execution/application` public surface。
- `runtime_execution/contracts/motion/IHomeAxesExecutionPort.h` 是 `InitializeSystemUseCase` 的唯一 auto-home 依赖；system init 不再引用 `HomeAxesUseCase` concrete。
- workflow live 配置面统一从 `process_planning/contracts/configuration/IConfigurationPort.h` 引入；不得再经由 `workflow/domain/include/domain/configuration/**` forwarder 或 `domain/configuration/**` legacy include 进入 live graph。
- workflow live configuration value objects 统一从 `process_planning/contracts/configuration/ConfigTypes.h` 引入；`workflow/domain/domain/configuration/value-objects/ConfigTypes.h` 已删除，禁止恢复 residual private wrapper。
- `MotionRuntimeAssemblyFactory` 与对应 workflow smoke/unit test 已删除；runtime concrete assembly 只允许留在 `runtime-execution` / `apps/runtime-service` owner 路径。
- `workflow/tests/unit` 不再编译 runtime-owned `JogController` / `MotionBufferController` / `HomeAxesUseCase` / `EnsureAxesReadyZeroUseCase` / `MotionMonitoringUseCase` / `HomingSupport`；这些测试已迁到 `modules/runtime-execution/runtime/host/tests/`。
- `siligen_workflow_adapters_public` 与 `siligen_workflow_runtime_consumer_public` 已删除；app / host 侧不得再把 `workflow` 当作 broad adapter/export surface。
- `siligen_workflow_dispensing_planning_compat` 已删除；`workflow/tests/unit` 不再通过该 compat target 拉起旧规划实现。
- `siligen_process_runtime_core_*` 聚合 target 已删除，不再作为 live public surface 或 owner 证据。

## S2 准入清单

- machine compatibility shell 下不再允许新增 `workflow/domain/domain/machine/**` live 规则实现。
- motion / interpolation owner 迁出后，不再允许恢复 `workflow/domain/domain/motion/**` 对 execution `.cpp` 的重新持有。
- recipes / dispensing planning owner 迁出后，不再允许把文件系统落盘逻辑回流到 `workflow/application`，也不再允许恢复 `adapters/include/recipes/**` legacy public wrapper。

## 当前兼容残留

- `workflow` 不再保留 motion/machine 的 compatibility target 或 public shim header。
- `motion-planning/domain/motion/domain-services/*.h` 仍保留对 runtime-execution 的 thin compatibility include，用于承接历史 motion include。
- `workflow` 已不再保留 configuration public forwarder 或 private wrapper；live consumer 必须直连 canonical owner contracts。
