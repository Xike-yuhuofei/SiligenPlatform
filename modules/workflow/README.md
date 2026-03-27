# workflow

`modules/workflow/` 是 `M0 workflow` 的 canonical owner 入口。

## Owner 范围

- 工作流编排与阶段推进语义
- 规划链（`M4-M8`）触发与编排边界
- 流程状态与调度决策的事实归属

## 边界约束

- `process-runtime-core/` 与 `src/` 在迁移期仅作为 shell-only bridge，不再承担 `M0` 终态 owner 或真实实现承载面。
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
- `src/` 与 `process-runtime-core/` 已完成 shell-only bridge 收敛，不再承载真实 `.h/.cpp` payload。

## S1 完成态

- `workflow/contracts` 已落地 `WorkflowStageState`、`WorkflowCommand`、`WorkflowPlanningTriggerRequest`、`WorkflowPlanningTriggerResponse`、`WorkflowFailureCategory`、`WorkflowRecoveryDirective`。
- `MotionRuntimeAssemblyFactory` 已改为依赖 runtime services provider，不再直接实例化 motion concrete。
- `PlanningUseCase` 已改为通过 `IPlanningArtifactExportPort` 发出导出意图；CSV/JSON 工件落盘由 `runtime-execution` concrete 承担。
- `assert-module-boundary-bridges.ps1` 已接入 bridge-only 收口标记，并在 `rg` 不可用时回退到 PowerShell 原生搜索。

## S2-A 完成态

- `workflow/application` 已不再直接依赖 `M7/M8` application facade；motion 规划调用改为直接消费 `M7` domain，dispensing planning 改为直接消费 `M8` domain + contracts。
- `workflow/domain/domain/CMakeLists.txt` 中的 `siligen_motion_execution_services` 已改为从 `modules/motion-planning/domain/motion/` 编译 owner source，`workflow/domain/domain/dispensing/CMakeLists.txt` 已降格为转发到 `siligen_dispense_packaging_domain_dispensing` 的兼容 target。
- `motion-planning/application` 与 `dispense-packaging/application` 不再反向修改 `workflow` target，`workflow` 自身负责声明 owner 依赖。
- bridge 报告已恢复绿色，见 `tests/reports/module-boundary-bridges-s2a/`。

## S2 准入清单

- machine owner 事实迁出前，不再允许新增 `workflow/domain/domain/machine/**` 规则实现。
- motion / interpolation owner 事实迁出前，不再允许恢复 `MotionRuntimeAssemblyFactory` 对 `*Impl` 的直接依赖。
- recipes / dispensing planning owner 迁出前，不再允许把文件系统落盘逻辑回流到 `workflow/application`。
