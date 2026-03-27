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

## 禁止越权边界

- 不得生成 `ProcessPlan`、`MotionPlan`、`ExecutionPackage`。
- 不得伪造 `WorkflowArchived`。
- 不得在编排层直接修补工艺、路径、轨迹或运行参数。

## 测试入口

- `S07` 状态机迁移
- `S08` success/block/rollback/recovery/archive 时序
- `S09` 幂等、回退、归档回归
