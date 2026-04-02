# workflow

`M0 workflow`

## 模块职责

- 编排 `S0-S16` 阶段推进、状态迁移、幂等重放、回退编排和归档编排。
- 只消费各 owner 模块的对象引用与事件，不直接生成业务阶段产物。

## Owner 对象

- `WorkflowRun`
- `StageTransitionRecord`
- `RollbackDecision`

## 输入契约

- `CreateWorkflow`
- `AdvanceStage`
- `RetryStage`
- `RequestRollback`
- `PerformRollback`
- `AbortWorkflow`
- `CompleteWorkflowArchive`

## 输出契约

- `StageStarted`
- `StageSucceeded`
- `StageFailed`
- `RollbackRequested`
- `RollbackPerformed`
- `WorkflowCompleted`
- `WorkflowAborted`
- `WorkflowArchiveRequested`

## 当前 Public Boundary

- `workflow/contracts/WorkflowContracts.h`
- `workflow/contracts/WorkflowExecutionPort.h`
- `siligen_workflow_contracts_public`
- `siligen_workflow_domain_public`
- `siligen_workflow_application_public`

`runtime-execution` 继续通过 `WorkflowExecutionPort` 承接执行编排契约；`workflow` 不再定义 `domain/machine/*`、`domain/motion/*` 的 compatibility shim 或 compatibility target。`InitializeSystemUseCase` 的 auto-home 依赖固定为 `runtime_execution/contracts/motion/IHomeAxesExecutionPort.h`，不得回退到 `HomeAxesUseCase` concrete。`workflow` 的 live 配置依赖固定为 `process_planning/contracts/configuration/IConfigurationPort.h`，不得再经由 `workflow/domain/include/domain/configuration/**` forwarder。`IMotionRuntimePort` / `IIOControlPort` 的 live consumer 也必须直接包含 `runtime_execution/contracts/motion/*`，不得再经由 `domain/motion/ports/*` alias。涉及 motion concrete 的 live 依赖仅允许落到 canonical owner public target，不得再回流为 workflow public header 或 workflow-owned tests。

## 已移除 Compat Surface

- `siligen_workflow_adapters_public`
- `siligen_workflow_runtime_consumer_public`
- `siligen_workflow_dispensing_planning_compat`
- `siligen_process_runtime_core_*`

## 禁止越权边界

- 不得生成 `ProcessPlan`、`MotionPlan`、`ExecutionPackage`。
- 不得伪造 `WorkflowArchived`。
- 不得在编排层直接修补工艺、路径、轨迹或运行参数。
- 不得把 `workflow` 当作 app / host 的 adapter 聚合出口。
- 不得恢复 machine/motion compatibility target、compatibility header 或 workflow-owned execution source。

## 测试入口

- `S07` 状态机迁移
- `S08` success/block/rollback/recovery/archive 时序
- `S09` 幂等、回退、归档回归
