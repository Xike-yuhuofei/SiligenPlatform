# 《点胶机端到端流程规格说明 s07：状态机与命令总线模板（修订版）》

本版目标是把“状态、命令、事件、幂等、持久化、回退、归档”彻底闭合。相较原稿，本版重点修复：

- 显式区分 `ExecutionPackageBuilt` 与 `ExecutionPackageValidated`
- 补齐 `PreflightBlocked` 的退出迁移
- 让“无需对位”通过 `AlignmentNotRequired` 进入 `Aligned`
- 统一首件审批命名与条件
- 把 `WorkflowArchived`、`ArtifactSuperseded` 纳入正式状态/事件集合
- 让命令总线公共字段与对象字典、测试矩阵对齐

---

# 1. 总体原则

## 1.1 状态机只表达事实状态
状态必须可由事件确认，不能用含糊描述代替。

## 1.2 一个状态机只服务一个控制对象
至少分开：
- 顶层工作流状态机（`WorkflowRun`）
- 执行状态机（`ExecutionSession`）
- 首件状态机（`FirstArticleRun`）
- 回退状态机（`RollbackDecision`）

## 1.3 状态迁移必须由命令驱动、事件确认
命令表达“意图”，事件表达“已发生”。

## 1.4 状态与产物解耦，但必须绑定产物版本
进入关键状态时，必须能追溯到对应 `artifact_ref`。

## 1.5 失败不等于终止
必须区分：
- 可重试阻断
- 可回退失败
- 需人工决策失败
- 不可恢复终止

## 1.6 设备门禁不得污染规划状态
`PreflightBlocked` 只表示当前不能执行，不表示规划产物无效。

## 1.7 所有重入命令都必须幂等
同一 `idempotency_key` 的重放必须返回同一事实结果或明确拒绝。

## 1.8 归档与失效必须纳入状态协作
`ArtifactSuperseded` 与 `WorkflowArchived` 都是正式事件，不允许只写日志不入状态链。

---

# 2. 顶层工作流状态机

控制对象：`WorkflowRun`

## 2.1 状态定义

| 状态 | 含义 |
|---|---|
| `Created` | 任务已创建，尚未接收有效源图 |
| `SourceAccepted` | 源文件已接收并封存 |
| `GeometryReady` | `CanonicalGeometry` 已生成 |
| `TopologyReady` | `TopologyModel` 已生成 |
| `FeaturesReady` | `FeatureGraph` 已生成 |
| `ProcessPlanned` | `ProcessPlan` 已生成 |
| `CoordinatesResolved` | `CoordinateTransformSet` 已生成 |
| `Aligned` | `AlignmentCompensation` 已生成，或已明确 `not_required` |
| `ProcessPathReady` | `ProcessPath` 已生成 |
| `MotionPlanned` | `MotionPlan` 已生成 |
| `TimingPlanned` | `DispenseTimingPlan` 已生成 |
| `PackageBuilt` | `ExecutionPackage` 已组装完成 |
| `PackageValidated` | `ExecutionPackage` 已离线校验通过 |
| `ReadyForPreflight` | 等待执行会话与设备预检 |
| `PreflightBlocked` | 设备未 ready，停在门禁层 |
| `MachineReady` | 设备 ready 快照已建立 |
| `FirstArticlePending` | 需要首件门控，但尚未放行 |
| `RollbackPending` | 首件失败或人工决策后等待回退 |
| `ReadyForProduction` | 可进入正式执行 |
| `Executing` | 正在正式执行 |
| `Paused` | 顶层视角处于暂停态 |
| `Recovering` | 顶层视角处于恢复态 |
| `Completed` | 成功终止 |
| `Aborted` | 人工或系统中止 |
| `Faulted` | 不可恢复失败终止 |
| `Archived` | 归档完成 |

## 2.2 合法迁移（主干）

| From | 命令 | 成功事件 | To |
|---|---|---|---|
| `Created` | `AttachSourceFileCommand` | `SourceFileAccepted` | `SourceAccepted` |
| `SourceAccepted` | `ParseSourceDrawingCommand` | `CanonicalGeometryBuilt` | `GeometryReady` |
| `GeometryReady` | `BuildTopologyCommand` | `TopologyBuilt` | `TopologyReady` |
| `TopologyReady` | `ExtractFeaturesCommand` | `FeatureGraphBuilt` | `FeaturesReady` |
| `FeaturesReady` | `PlanProcessCommand` | `ProcessPlanBuilt` | `ProcessPlanned` |
| `ProcessPlanned` | `ResolveCoordinateTransformsCommand` | `CoordinateTransformResolved` | `CoordinatesResolved` |
| `CoordinatesResolved` | `RunAlignmentCommand / MarkAlignmentNotRequiredCommand` | `AlignmentSucceeded / AlignmentNotRequired` | `Aligned` |
| `Aligned` | `BuildProcessPathCommand` | `ProcessPathBuilt` | `ProcessPathReady` |
| `ProcessPathReady` | `PlanMotionCommand` | `MotionPlanBuilt` | `MotionPlanned` |
| `MotionPlanned` | `BuildDispenseTimingPlanCommand` | `DispenseTimingPlanBuilt` | `TimingPlanned` |
| `TimingPlanned` | `AssembleExecutionPackageCommand` | `ExecutionPackageBuilt` | `PackageBuilt` |
| `PackageBuilt` | `ValidateExecutionPackageCommand` | `ExecutionPackageValidated` | `PackageValidated` |
| `PackageValidated` | `CreateExecutionSessionCommand` | `ExecutionSessionCreated` | `ReadyForPreflight` |
| `ReadyForPreflight` | `RunPreflightCommand` | `PreflightPassed` | `MachineReady` |
| `ReadyForPreflight` | `RunPreflightCommand` | `PreflightBlocked` | `PreflightBlocked` |
| `MachineReady` | 首件策略判定 | `FirstArticleNotRequired` | `ReadyForProduction` |
| `MachineReady` | 首件策略判定 | `FirstArticleWaived` | `ReadyForProduction` |
| `MachineReady` | `RunFirstArticleCommand` | `FirstArticleStarted` | `FirstArticlePending` |
| `FirstArticlePending` | `ApproveFirstArticleCommand` | `FirstArticlePassed` | `ReadyForProduction` |
| `FirstArticlePending` | `RejectFirstArticleCommand` | `FirstArticleRejected` | `RollbackPending` |
| `RollbackPending` | `RequestRollbackCommand` | `RollbackRequested` | `RollbackPending` |
| `RollbackPending` | `PerformRollbackCommand` | `RollbackPerformed` | 回退目标对应状态 |
| `ReadyForProduction` | `StartExecutionCommand` | `ExecutionStarted` | `Executing` |
| `Executing` | `PauseExecutionCommand` | `ExecutionPaused` | `Paused` |
| `Paused` | `ResumeExecutionCommand` | `ExecutionResumed` | `Executing` |
| `Executing` | `RecoverExecutionCommand` | `ExecutionRecovered` | `Recovering` |
| `Recovering` | 恢复完成 | `ExecutionResumed` | `Executing` |
| `Executing` | 运行结束 | `ExecutionCompleted` | `Completed` |
| `Executing / Paused / Recovering / PreflightBlocked / FirstArticlePending / RollbackPending` | `AbortExecutionCommand / AbortWorkflowCommand` | `ExecutionAborted / WorkflowAborted` | `Aborted` |
| `Executing / Recovering` | 运行失败 | `ExecutionFaulted` | `Faulted` |
| `Completed / Aborted / Faulted` | `ArchiveWorkflowCommand` | `WorkflowArchived` | `Archived` |

## 2.3 阻断态与旁路态的显式规则

### 规则 A：`Aligned` 不再允许被“隐式跳过”
无对位任务必须通过 `AlignmentNotRequired` 进入 `Aligned`，不再允许 `CoordinatesResolved -> ProcessPathReady` 直接短路。

### 规则 B：`PreflightBlocked` 必须有退出路径
允许：
- `PreflightBlocked -> ReadyForPreflight`（重新发起预检）
- `PreflightBlocked -> Aborted`（人工 / 系统中止）

### 规则 C：`FirstArticleRejected` 不直接把顶层状态改成 `Faulted`
`FirstArticleRejected` 的默认含义是：
- 当前执行会话停止
- `RollbackDecision` 被请求
- 顶层工作流进入 `RollbackPending`

### 规则 D：`ExecutionPackageValidated` 不能被 `PreflightPassed` 代替
前者代表离线规则通过，后者代表当前设备门禁满足，两者必须分别出现、分别持久化。

## 2.4 非法迁移示例

- `Created -> MotionPlanned`
- `SourceAccepted -> PackageValidated`
- `MachineReady -> ProcessPlanned`
- `Executing -> GeometryReady`
- `Completed -> Executing`
- `Faulted -> MotionPlanned`（必须通过显式回退或新任务）
- `PackageBuilt -> ReadyForProduction`（跳过离线校验与执行门禁）
- `FirstArticleRejected -> StartExecutionCommand`（未放行直接开跑）

## 2.5 顶层任务状态机的终止态

正式终止态只有四个：
- `Completed`
- `Aborted`
- `Faulted`
- `Archived`

说明：
- `PreflightBlocked`、`Paused`、`Recovering`、`RollbackPending` 都不是终止态。

---

# 3. 执行状态机

控制对象：`ExecutionSession`

## 3.1 状态定义

| 状态 | 含义 |
|---|---|
| `Idle` | 尚未创建执行会话 |
| `SessionCreated` | 已绑定执行包，尚未预检 |
| `PreflightRunning` | 正在做设备预检 |
| `PreflightBlocked` | 预检未通过，停留等待 |
| `PreflightPassed` | 会话已满足执行门禁 |
| `PreviewRunning` | 正在预览 |
| `DryRunRunning` | 正在空跑 |
| `FirstArticleRunning` | 正在低速首件或首件执行 |
| `AwaitingFirstArticleDecision` | 等待首件审批 / 质检结论 |
| `ReadyToStart` | 可进入正式执行 |
| `Starting` | 正在进入运行 |
| `Running` | 正式运行中 |
| `Pausing` | 正在进入稳定暂停点 |
| `Paused` | 已暂停 |
| `Resuming` | 正在从暂停点恢复 |
| `Recovering` | 正在处理故障恢复 |
| `Completed` | 会话完成 |
| `Aborted` | 会话已中止 |
| `Faulted` | 会话不可恢复失败 |

## 3.2 合法迁移

| From | 命令 | 成功事件 | To |
|---|---|---|---|
| `Idle` | `CreateExecutionSessionCommand` | `ExecutionSessionCreated` | `SessionCreated` |
| `SessionCreated` | `RunPreflightCommand` | `PreflightStarted` | `PreflightRunning` |
| `PreflightRunning` | - | `PreflightPassed` | `PreflightPassed` |
| `PreflightRunning` | - | `PreflightBlocked` | `PreflightBlocked` |
| `PreflightBlocked` | `RunPreflightCommand` | `PreflightStarted` | `PreflightRunning` |
| `PreflightBlocked` | `AbortExecutionCommand` | `ExecutionAborted` | `Aborted` |
| `PreflightPassed` | `RunPreviewCommand` | `PreviewStarted` | `PreviewRunning` |
| `PreviewRunning` | - | `PreviewCompleted` | `PreflightPassed` |
| `PreflightPassed` | `RunDryRunCommand` | `DryRunStarted` | `DryRunRunning` |
| `DryRunRunning` | - | `DryRunCompleted` | `PreflightPassed` |
| `PreflightPassed` | 首件策略判定 | `FirstArticleNotRequired` | `ReadyToStart` |
| `PreflightPassed` | 首件策略判定 | `FirstArticleWaived` | `ReadyToStart` |
| `PreflightPassed` | `RunFirstArticleCommand` | `FirstArticleStarted` | `FirstArticleRunning` |
| `FirstArticleRunning` | - | `FirstArticlePassed` | `ReadyToStart` |
| `FirstArticleRunning` | - | `FirstArticleRejected` | `Aborted` |
| `ReadyToStart` | `StartExecutionCommand` | `ExecutionStarted` | `Starting` |
| `Starting` | - | `ExecutionStarted` | `Running` |
| `Running` | `PauseExecutionCommand` | `ExecutionPaused` | `Pausing` |
| `Pausing` | - | `ExecutionPaused` | `Paused` |
| `Paused` | `ResumeExecutionCommand` | `ExecutionResumed` | `Resuming` |
| `Resuming` | - | `ExecutionResumed` | `Running` |
| `Running` | `RecoverExecutionCommand` | `ExecutionRecovered` | `Recovering` |
| `Recovering` | 恢复完成 | `ExecutionResumed` | `Running` |
| `Recovering` | 需重新门禁检查 | `PreflightStarted` | `PreflightRunning` |
| `Running` | - | `ExecutionCompleted` | `Completed` |
| `Running / Paused / Recovering / Starting` | `AbortExecutionCommand` | `ExecutionAborted` | `Aborted` |
| `Running / Recovering` | - | `ExecutionFaulted` | `Faulted` |

## 3.3 执行状态机的重要约束

### 约束 A：`Paused` 必须是稳定态
进入 `Paused` 前必须满足：
- 轴停止或停在允许边界
- 阀处于安全状态
- 当前段索引已冻结
- 恢复点已记录

### 约束 B：`Recovering` 与 `Resuming` 不同
- `Resuming`：从合法暂停点继续
- `Recovering`：从 fault 或门禁丢失后的恢复流程继续

### 约束 C：`PreflightBlocked` 不是失败终止态
它是“当前不能跑”，不是“任务已死”。

### 约束 D：`StartExecutionCommand` 不允许重入
同一会话已经 `Running / Starting / Completed` 时，重复 `StartExecutionCommand` 必须拒绝或返回现有结果，不得二次启动。

### 约束 E：`RecoverExecutionCommand` 不等价于 `ResumeExecutionCommand`
只有 fault 已被归类为可恢复，且恢复前置检查通过，才允许进入 `Recovering`。

---

# 4. 首件状态机

控制对象：`FirstArticleRun`

## 4.1 状态定义

| 状态 | 含义 |
|---|---|
| `NotRequired` | 当前任务不要求首件 |
| `Pending` | 等待开始首件 |
| `PreviewRunning` | 正在图形预览 |
| `DryRunRunning` | 正在空跑 |
| `LowSpeedDispenseRunning` | 正在低速首件出胶 |
| `AwaitingInspection` | 等待人工或自动质检 |
| `Passed` | 首件通过 |
| `Rejected` | 首件失败 |
| `Waived` | 经授权豁免 |
| `Aborted` | 首件流程中止 |

## 4.2 合法迁移

```text
NotRequired

Pending
 -> PreviewRunning
 -> DryRunRunning
 -> LowSpeedDispenseRunning
 -> AwaitingInspection
 -> Passed / Rejected / Waived / Aborted
```

## 4.3 首件失败与豁免规则

### 失败分流规则
| 首件失败类型 | 推荐回退目标 |
|---|---|
| 空跑路径错误 | `ToProcessPathReady` 或更早 |
| 空跑正常、出胶异常 | `ToTimingPlanned` 或 `ToProcessPlanned` |
| 位置偏差明显 | `ToCoordinatesResolved` 或 `ToAligned` |
| 原因未明需人工诊断 | `AwaitingApproval` 后再定 |

### 条件必填规则
- `FirstArticleRejected` 事件必须带 `failure_code`、`rollback_target`
- `FirstArticleWaived` 事件必须带 `approval_ref`、`approved_by`
- `FirstArticleNotRequired` 事件必须带 `policy_ref` 或等效策略来源
- `FirstArticlePassed` 若依赖人工确认，必须带 `approval_ref`

---

# 5. 回退状态机

控制对象：`RollbackDecision`

## 5.1 回退状态定义

| 状态 | 含义 |
|---|---|
| `None` | 当前无需回退 |
| `Requested` | 已请求回退 |
| `Analyzing` | 正在判断回退归属层 |
| `AwaitingApproval` | 等待人工批准 |
| `Approved` | 回退已批准 |
| `Rejected` | 回退被拒绝 |
| `RollingBack` | 正在执行回退 |
| `SupersedingDownstream` | 正在标记下游对象 `superseded` |
| `RolledBack` | 回退完成 |
| `Superseded` | 回退决策本身被新的请求覆盖 |

## 5.2 推荐回退目标枚举

- `ToSourceAccepted`
- `ToGeometryReady`
- `ToTopologyReady`
- `ToFeaturesReady`
- `ToProcessPlanned`
- `ToCoordinatesResolved`
- `ToAligned`
- `ToProcessPathReady`
- `ToMotionPlanned`
- `ToTimingPlanned`
- `ToPackageBuilt`
- `ToPackageValidated`

## 5.3 回退的硬规则

### 规则 A
回退只能回到已定义状态，禁止回到“半状态”。

### 规则 B
回退完成前，所有回退目标之后的 owner 对象都必须显式发出 `ArtifactSuperseded`。

### 规则 C
回退成功不是“状态改回去”这么简单，必须同时满足：
- 下游旧版本对象已 `superseded`
- 新版本对象已按顺序重建
- 顶层状态已迁移到目标状态
- 追溯系统已能看到旧链与新链的关联

### 规则 D
`FirstArticleRejected`、`OfflineValidationFailed`、`CompensationOutOfRange` 都可以成为回退触发来源，但推荐目标不同。

---

# 6. 命令总线模板

## 6.1 三类消息

### A. Command
表示“请求做一件事”，必须可拒绝、可幂等、可记录。

### B. Event
表示“某件事已经发生”，必须可持久化、可订阅、可驱动状态迁移。

### C. Query
表示“读取事实”，不得引发业务写入。

## 6.2 命令总线职责

- 路由命令到正确 handler
- 提供 `idempotency_key` 判重能力
- 保证命令结果能映射为事件
- 为状态机提供可靠事件源
- 为追溯系统提供 `request_id / correlation_id / causation_id`

## 6.3 命令最小公共字段

| 字段 | 说明 |
|---|---|
| `message_id` | 消息主键 |
| `command_id` | 命令主键 |
| `request_id` | 请求主键 |
| `idempotency_key` | 幂等键 |
| `job_id` | 任务主键 |
| `workflow_id` | 工作流主键 |
| `stage_id` | 目标阶段 |
| `artifact_ref` | 目标或上下文对象引用 |
| `correlation_id` | 关联链主键 |
| `causation_id` | 触发来源 |
| `execution_context` | `execution_package_ref / execution_id / machine_id / segment_index` |
| `issued_by` | 人 / 服务 |
| `issued_at` | 发起时间 |
| `reason` | 原因 |

## 6.4 事件最小公共字段

| 字段 | 说明 |
|---|---|
| `message_id` | 消息主键 |
| `event_id` | 事件主键 |
| `request_id` | 对应请求 |
| `job_id` | 任务主键 |
| `workflow_id` | 工作流主键 |
| `stage_id` | 所属阶段 |
| `artifact_ref` | 事件关联对象 |
| `source_command_id` | 来源命令 |
| `correlation_id` | 关联链主键 |
| `causation_id` | 上一消息主键 |
| `execution_context` | 执行上下文 |
| `producer` | 事件生产模块 |
| `occurred_at` | 发生时间 |
| `status` | `success / failure / blocked / decision` |

## 6.5 决策事件附加字段

对 `FirstArticleWaived / FirstArticlePassed / RollbackApproved / WorkflowArchived` 等决策型事件，建议统一增加：
- `approval_ref`
- `approved_by`
- `approved_at`
- `decision_reason`

---

# 7. 推荐命令清单

## 7.1 任务与输入类命令
- `CreateJobCommand`
- `AttachSourceFileCommand`
- `CreateWorkflowCommand`
- `ArchiveWorkflowCommand`

## 7.2 规划链命令
- `ParseSourceDrawingCommand`
- `BuildTopologyCommand`
- `ExtractFeaturesCommand`
- `PlanProcessCommand`
- `ResolveCoordinateTransformsCommand`
- `RunAlignmentCommand`
- `MarkAlignmentNotRequiredCommand`
- `BuildProcessPathCommand`
- `PlanMotionCommand`
- `BuildDispenseTimingPlanCommand`
- `AssembleExecutionPackageCommand`
- `ValidateExecutionPackageCommand`

## 7.3 执行链命令
- `CreateExecutionSessionCommand`
- `RunPreflightCommand`
- `RunPreviewCommand`
- `RunDryRunCommand`
- `RunFirstArticleCommand`
- `ApproveFirstArticleCommand`
- `RejectFirstArticleCommand`
- `WaiveFirstArticleCommand`
- `StartExecutionCommand`
- `PauseExecutionCommand`
- `ResumeExecutionCommand`
- `RecoverExecutionCommand`
- `AbortExecutionCommand`

## 7.4 回退与恢复类命令
- `RequestRollbackCommand`
- `ApproveRollbackCommand`
- `RejectRollbackCommand`
- `PerformRollbackCommand`
- `SupersedeOwnedArtifactCommand`

## 7.5 追溯类命令
- `RecordExecutionCommand`
- `AppendFaultEventCommand`
- `FinalizeExecutionCommand`
- `QueryTraceCommand`

---

# 8. 推荐事件清单

## 8.1 任务与输入类事件
- `JobCreated`
- `SourceFileAccepted`
- `SourceFileRejected`
- `WorkflowCreated`
- `WorkflowArchived`

## 8.2 规划链成功事件
- `CanonicalGeometryBuilt`
- `TopologyBuilt`
- `FeatureGraphBuilt`
- `ProcessPlanBuilt`
- `CoordinateTransformResolved`
- `AlignmentSucceeded`
- `AlignmentNotRequired`
- `ProcessPathBuilt`
- `MotionPlanBuilt`
- `DispenseTimingPlanBuilt`
- `ExecutionPackageBuilt`
- `ExecutionPackageValidated`

## 8.3 规划链失败事件
- `CanonicalGeometryRejected`
- `TopologyRejected`
- `FeatureClassificationFailed`
- `ProcessRuleRejected`
- `AlignmentFailed`
- `CompensationOutOfRange`
- `ProcessPathRejected`
- `TrajectoryConstraintViolated`
- `DispenseTimingPlanRejected`
- `ExecutionPackageRejected`
- `OfflineValidationFailed`

## 8.4 执行链事件
- `ExecutionSessionCreated`
- `MachineReadySnapshotBuilt`
- `PreflightStarted`
- `PreflightBlocked`
- `PreflightPassed`
- `PreviewStarted`
- `PreviewCompleted`
- `DryRunStarted`
- `DryRunCompleted`
- `FirstArticleStarted`
- `FirstArticlePassed`
- `FirstArticleRejected`
- `FirstArticleWaived`
- `FirstArticleNotRequired`
- `ExecutionStarted`
- `ExecutionPaused`
- `ExecutionResumed`
- `ExecutionRecovered`
- `ExecutionCompleted`
- `ExecutionAborted`
- `ExecutionFaulted`

## 8.5 回退类事件
- `RollbackRequested`
- `RollbackApproved`
- `RollbackRejected`
- `ArtifactSuperseded`
- `RollbackPerformed`

## 8.6 追溯类事件
- `ExecutionRecordBuilt`
- `FaultEventAppended`
- `TraceWriteFailed`
- `TraceQueryResult`

---

# 9. 哪些命令允许重入

## 9.1 允许幂等重入的命令
- `CreateJobCommand`
- `AttachSourceFileCommand`
- `RunPreflightCommand`
- `ApproveFirstArticleCommand`
- `RejectFirstArticleCommand`
- `WaiveFirstArticleCommand`
- `ArchiveWorkflowCommand`
- `QueryTraceCommand`

要求：相同 `idempotency_key` 重放，不得产生新的业务事实。

## 9.2 有条件允许重入的命令
- `PerformRollbackCommand`
- `RecoverExecutionCommand`
- `FinalizeExecutionCommand`

要求：只有在前一次执行未完成或结果可重放时允许幂等重入。

## 9.3 原则上不允许重入的命令
- `StartExecutionCommand`
- `PauseExecutionCommand`
- `ResumeExecutionCommand`
- `RunFirstArticleCommand`

要求：系统必须返回“已启动 / 已暂停 / 已恢复 / 已存在首件流程”的既有事实或拒绝，不得再次触发动作。

---

# 10. 哪些事件必须持久化

## 10.1 必须持久化的生命周期事件
- `SourceFileAccepted`
- `CanonicalGeometryBuilt`
- `FeatureGraphBuilt`
- `ProcessPlanBuilt`
- `ExecutionPackageBuilt`
- `ExecutionPackageValidated`
- `ExecutionSessionCreated`
- `PreflightPassed`
- `ExecutionStarted`
- `ExecutionCompleted`
- `WorkflowArchived`

## 10.2 必须持久化的失败事件
- `SourceFileRejected`
- `CanonicalGeometryRejected`
- `ProcessRuleRejected`
- `AlignmentFailed`
- `CompensationOutOfRange`
- `OfflineValidationFailed`
- `PreflightBlocked`
- `FirstArticleRejected`
- `ExecutionFaulted`
- `ExecutionAborted`
- `TraceWriteFailed`

## 10.3 必须持久化的决策事件
- `FirstArticlePassed`
- `FirstArticleWaived`
- `FirstArticleNotRequired`
- `RollbackApproved`
- `RollbackRejected`
- `ArtifactSuperseded`

---

# 11. 状态机与命令总线的协作模式

## 11.1 标准成功路径

```text
Command -> Handler -> Event -> Persist -> State Transition -> Read Model Update
```

## 11.2 标准失败路径

```text
Command -> Handler -> Failure Event -> Persist -> State Remains / Moves to Blocked or Faulted -> Read Model Update
```

## 11.3 禁止路径

- 仅靠函数返回值驱动状态迁移，不发事件
- UI 本地状态越过总线成为系统事实
- 运行时发现问题后，直接改写上游对象版本而不发 `ArtifactSuperseded`
- 不记录 `ExecutionPackageValidated`，直接从组包成功跳到预检

---

# 12. 命令处理器模板

## 12.1 输入
- `command`
- `current_state`
- `artifact_refs`
- `idempotency_record`

## 12.2 执行
- 校验状态是否合法
- 校验对象引用是否合法
- 校验幂等键是否已处理
- 调用领域服务或适配层
- 生成成功 / 失败 / 阻断 / 决策事件

## 12.3 输出
- 事件列表
- 状态迁移结果
- 只读模型更新请求
- 审计记录

---

# 13. UI 与状态机的对接建议

- UI 只能展示持久化后的系统状态，不能使用本地猜测状态。
- 所有按钮可用性都必须由状态机派生，而不是写死在前端逻辑里。
- `Start` 按钮必须绑定：`PreflightPassed + 首件门已放行 + 非 Running`
- `Approve / Reject / Waive` 必须写入决策事件，不能只更新界面。

---

# 14. 最小命令总线实现建议

## 14.1 进程内命令总线
适合单体版本，只要保证事件先持久化再更新读模型即可。

## 14.2 持久化事件存储
至少需要：
- 事件表
- 幂等表
- 读模型快照表
- 命令处理日志表

## 14.3 运行时适配层独立
设备适配层只输出运行时事实，不得越权决定上游规划状态。

---

# 15. 最容易做错的地方

## 15.1 用函数返回值代替事件
会导致状态机无法复盘。

## 15.2 用 UI 本地状态代替系统事实状态
会导致审批、放行、中止全部不可审计。

## 15.3 把 `PreflightBlocked` 当成 `Faulted`
会污染规划链，也会误伤版本链。

## 15.4 把 `Paused` 和 `Recovering` 混为一谈
会让恢复点、重试逻辑、设备门禁全部混乱。

## 15.5 没有幂等键
重复点击或网络抖动会引发重复动作。

## 15.6 只有 `ExecutionPackageBuilt`，没有 `ExecutionPackageValidated`
会让离线正确与在线可运行被混为一谈。

## 15.7 只有 `FirstArticleCompleted`，没有 `Passed / Rejected / Waived / NotRequired`
会让首件门控无法审计，也无法做稳定测试断言。

---

# 16. 这一版最关键的工程结论

## 16.1 状态机的核心不是“状态多”，而是“每个状态都可由事件确认”

## 16.2 命令总线的核心不是“中间件”，而是“命令、事件、幂等、追溯的一致性”

## 16.3 你这套系统最重要的 6 个阻断点已经在本版落为正式规则
- `ExecutionPackageBuilt` 与 `ExecutionPackageValidated` 分离
- `PreflightBlocked` 具有显式退出路径
- 无对位任务通过 `AlignmentNotRequired` 闭环
- 首件失败必须带 `failure_code + rollback_target`
- `ArtifactSuperseded` 是正式事件而不是日志备注
- `WorkflowArchived` 有唯一 owner 与显式迁移