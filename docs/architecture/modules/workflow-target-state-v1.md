# workflow target-state v1

`M0 workflow` 终态规格文档。

本文是独立于 `docs/architecture/dsp-e2e-spec/` 的目标态说明，只回答 `modules/workflow/` 在长期稳定架构下应该长成什么样；`S01-S10` 仍然是 owner、阶段、状态机和接口语义的 authority。

## 1. 终态定位

`workflow` 的长期身份是：

- `M0` 顶层工作流编排模块
- A 类正式对象链的 process manager / saga owner
- 顶层 `WorkflowRun` 与 `RollbackDecision` 的唯一 owner

`workflow` 不是：

- `M9 runtime-execution` 的替代物
- 设备运行时、预检、首件、执行、恢复的 concrete owner
- `M10 trace-diagnostics` 的归档或追溯 owner
- HMI、TCP 或 CLI 的总入口宿主
- 仓库级代码冗余治理、静态/运行证据评分与删除候选治理的 owner

## 2. 终态目录树

```text
modules/workflow/
├─ README.md
├─ module.yaml
├─ contracts/
│  └─ include/workflow/contracts/
│     ├─ commands/
│     ├─ queries/
│     ├─ events/
│     ├─ dto/
│     └─ failures/
├─ domain/
│  ├─ workflow_run/
│  ├─ stage_transition/
│  ├─ rollback/
│  ├─ policies/
│  └─ ports/
├─ services/
│  ├─ lifecycle/
│  ├─ rollback/
│  ├─ archive/
│  └─ projection/
├─ application/
│  ├─ commands/
│  ├─ queries/
│  └─ facade/
├─ adapters/
│  ├─ persistence/
│  ├─ messaging/
│  └─ projection_store/
├─ runtime/
│  ├─ orchestrators/
│  └─ subscriptions/
├─ tests/
│  ├─ unit/
│  ├─ contract/
│  ├─ integration/
│  └─ regression/
└─ examples/
```

## 2.1 文件级骨架草案

```text
modules/workflow/contracts/include/workflow/contracts/
├─ WorkflowContracts.h
├─ commands/
│  ├─ CreateWorkflow.h
│  ├─ AdvanceStage.h
│  ├─ RetryStage.h
│  ├─ RequestRollback.h
│  ├─ PerformRollback.h
│  ├─ AbortWorkflow.h
│  └─ CompleteWorkflowArchive.h
├─ queries/
│  ├─ GetWorkflowRun.h
│  ├─ GetWorkflowStageHistory.h
│  ├─ GetRollbackDecision.h
│  └─ GetWorkflowTimeline.h
├─ events/
│  ├─ StageStarted.h
│  ├─ StageSucceeded.h
│  ├─ StageFailed.h
│  ├─ RollbackRequested.h
│  ├─ RollbackPerformed.h
│  ├─ WorkflowCompleted.h
│  ├─ WorkflowAborted.h
│  └─ WorkflowArchiveRequested.h
├─ dto/
│  ├─ WorkflowRunView.h
│  ├─ StageTransitionRecordView.h
│  ├─ RollbackDecisionView.h
│  ├─ WorkflowTimelineEntry.h
│  └─ WorkflowArtifactRefSet.h
└─ failures/
   ├─ WorkflowFailureCategory.h
   ├─ WorkflowFailurePayload.h
   ├─ WorkflowRecoveryAction.h
   └─ RollbackFailurePayload.h
```

```text
modules/workflow/domain/
├─ workflow_run/
│  ├─ WorkflowRun.h
│  ├─ WorkflowRunState.h
│  ├─ WorkflowArchiveStatus.h
│  └─ WorkflowArtifactRefSet.h
├─ stage_transition/
│  ├─ StageTransitionRecord.h
│  ├─ StageTransitionReason.h
│  └─ StageStateProjectionPolicy.h
├─ rollback/
│  ├─ RollbackDecision.h
│  ├─ RollbackDecisionStatus.h
│  ├─ RollbackTarget.h
│  └─ RollbackPolicy.h
├─ policies/
│  ├─ WorkflowStateTransitionPolicy.h
│  ├─ WorkflowIdempotencyPolicy.h
│  └─ WorkflowCorrelationPolicy.h
└─ ports/
   ├─ IWorkflowRepository.h
   ├─ IStageTransitionStore.h
   ├─ IRollbackDecisionStore.h
   ├─ IStageOwnerCommandPort.h
   └─ IWorkflowEventIngressPort.h
```

```text
modules/workflow/services/
├─ lifecycle/
│  ├─ CreateWorkflowService.h
│  ├─ AdvanceStageService.h
│  ├─ RetryStageService.h
│  ├─ HandleStageSucceededService.h
│  └─ HandleStageFailedService.h
├─ rollback/
│  ├─ RequestRollbackService.h
│  ├─ PerformRollbackService.h
│  └─ RollbackImpactAnalysisService.h
├─ archive/
│  ├─ RequestWorkflowArchiveService.h
│  └─ CompleteWorkflowArchiveService.h
└─ projection/
   ├─ WorkflowProjectionService.h
   └─ WorkflowTimelineProjectionService.h
```

```text
modules/workflow/application/
├─ commands/
│  ├─ CreateWorkflowHandler.h
│  ├─ AdvanceStageHandler.h
│  ├─ RetryStageHandler.h
│  ├─ RequestRollbackHandler.h
│  ├─ PerformRollbackHandler.h
│  ├─ AbortWorkflowHandler.h
│  └─ CompleteWorkflowArchiveHandler.h
├─ queries/
│  ├─ GetWorkflowRunHandler.h
│  ├─ GetWorkflowStageHistoryHandler.h
│  ├─ GetRollbackDecisionHandler.h
│  └─ GetWorkflowTimelineHandler.h
└─ facade/
   └─ WorkflowFacade.h
```

```text
modules/workflow/runtime/
├─ orchestrators/
│  ├─ WorkflowCommandOrchestrator.h
│  ├─ WorkflowEventOrchestrator.h
│  └─ WorkflowArchiveOrchestrator.h
└─ subscriptions/
   ├─ PlanningEventsSubscription.h
   ├─ RuntimeEventsSubscription.h
   └─ TraceEventsSubscription.h
```

## 3. 目录职责冻结

### 3.1 `contracts/`

只暴露 `M0` 自己的稳定接口：

- workflow commands
- workflow queries
- workflow events
- workflow DTO / failure payload

不得暴露：

- 任何 `M1-M10` 的 domain header
- 任何运行时控制 adapter
- 任何规划链或执行链的 concrete service

### 3.2 `domain/`

只放 `workflow` 自己拥有的事实和规则：

- `WorkflowRun`
- `StageTransitionRecord`
- `RollbackDecision`
- 状态迁移规则
- 回退目标判定规则
- 幂等与相关性规则

不得放：

- `ProcessPlan` / `ProcessPath` / `MotionPlan`
- `ExecutionPackage` / `ExecutionSession`
- 设备状态、轴状态、I/O 状态 concrete
- `CodeEntity` / `StaticEvidenceRecord` / `RuntimeEvidenceRecord` / `CandidateRecord` / `DecisionLogRecord`

### 3.3 `services/`

只放 `workflow` 自己的用例编排逻辑：

- 生命周期推进
- 阶段成功/失败投影
- 回退协作
- 归档握手

不得把 `services/` 当成任意业务杂物箱。
其中 `RollbackImpactAnalysisService` 只允许评估 workflow 阶段、失败面与 artifact 引用的回退可达性，不得承接代码冗余治理评分语义。

### 3.4 `application/`

只放外部调用入口：

- command handlers
- query handlers
- module facade

### 3.5 `adapters/`

只放 `workflow` 自己的外部适配：

- repository / store
- event bus / outbox / inbox
- timeline / projection store

### 3.6 `runtime/`

`workflow/runtime/` 允许存在，但只能承载轻量编排运行面：

- 事件订阅
- 命令分发协调
- correlation / idempotency routing
- 顶层状态投影刷新

不得承载：

- `ExecutionSession` 状态机
- preflight / preview / dry-run / first-article / run / recover concrete
- 设备轮询、板卡控制、I/O 适配

## 4. `workflow/contracts` 终态清单

### 4.1 Commands

- `CreateWorkflow`
- `AdvanceStage`
- `RetryStage`
- `RequestRollback`
- `PerformRollback`
- `AbortWorkflow`
- `CompleteWorkflowArchive`

### 4.2 Queries

- `GetWorkflowRun`
- `GetWorkflowStageHistory`
- `GetRollbackDecision`
- `GetWorkflowTimeline`

### 4.3 Events

- `StageStarted`
- `StageSucceeded`
- `StageFailed`
- `RollbackRequested`
- `RollbackPerformed`
- `WorkflowCompleted`
- `WorkflowAborted`
- `WorkflowArchiveRequested`

### 4.4 不属于 `workflow/contracts` 的对象

以下对象只允许以 `artifact_ref` 或事件引用方式被 `workflow` 消费，不允许在 `workflow/contracts` 里重新 owner 化：

- `JobDefinition`
- `CanonicalGeometry`
- `TopologyModel`
- `FeatureGraph`
- `ProcessPlan`
- `CoordinateTransformSet`
- `AlignmentCompensation`
- `ProcessPath`
- `MotionPlan`
- `DispenseTimingPlan`
- `ExecutionPackage`
- `MachineReadySnapshot`
- `ExecutionSession`
- `FirstArticleResult`
- `ExecutionRecord`
- `TraceLinkSet`

## 4.5 Commands payload 草案

所有 command 都复用 `shared` 统一消息底板：

- `request_id`
- `idempotency_key`
- `correlation_id`
- `causation_id`
- `workflow_id`
- `job_id`
- `issued_by`
- `issued_at`

各 command 自身最小 payload 建议为：

### `CreateWorkflow`

- `job_id`
- `initial_stage = Created`
- `workflow_template_version`
- `context_snapshot_ref`

### `AdvanceStage`

- `workflow_id`
- `from_stage`
- `target_stage`
- `expected_state`
- `trigger_reason`

### `RetryStage`

- `workflow_id`
- `stage_id`
- `retry_reason`
- `expected_failed_event_id`

### `RequestRollback`

- `workflow_id`
- `rollback_target`
- `reason_code`
- `reason_message`
- `requested_from_stage`
- `requested_from_state`

### `PerformRollback`

- `workflow_id`
- `rollback_id`
- `rollback_target`
- `supersede_scope`

### `AbortWorkflow`

- `workflow_id`
- `abort_reason_code`
- `abort_reason_message`
- `abort_scope`

### `CompleteWorkflowArchive`

- `workflow_id`
- `execution_record_ref`
- `archive_request_event_id`

## 4.6 Queries / DTO 草案

### `GetWorkflowRun`

返回 `WorkflowRunView`：

- `workflow_id`
- `job_id`
- `current_state`
- `current_stage`
- `target_stage`
- `last_completed_stage`
- `active_artifact_refs`
- `archive_status`
- `updated_at`

### `GetWorkflowStageHistory`

返回 `StageTransitionRecordView[]`：

- `transition_id`
- `from_state`
- `to_state`
- `from_stage`
- `to_stage`
- `source_command_id`
- `source_event_id`
- `artifact_ref`
- `transition_reason`
- `occurred_at`

### `GetRollbackDecision`

返回 `RollbackDecisionView`：

- `rollback_id`
- `workflow_id`
- `rollback_target`
- `decision_status`
- `reason_code`
- `reason_message`
- `affected_artifact_refs`
- `requested_at`
- `resolved_at`

### `GetWorkflowTimeline`

返回 `WorkflowTimelineEntry[]`：

- `entry_id`
- `entry_type`
- `workflow_id`
- `stage_id`
- `state`
- `source_command_id`
- `source_event_id`
- `summary`
- `occurred_at`

## 4.7 Events payload 草案

所有 event 都复用 `shared` 事件底板：

- `event_id`
- `request_id`
- `correlation_id`
- `causation_id`
- `workflow_id`
- `job_id`
- `stage_id`
- `artifact_ref`
- `producer`
- `occurred_at`

各 event 最小 payload 建议为：

### `StageStarted`

- `from_state`
- `to_state`
- `from_stage`
- `to_stage`
- `trigger_command`

### `StageSucceeded`

- `completed_stage`
- `resulting_state`
- `owner_module_id`
- `owner_event_id`

### `StageFailed`

- `failed_stage`
- `resulting_state`
- `failure_category`
- `failure_code`
- `retryable`
- `rollback_recommended`

### `RollbackRequested`

- `rollback_id`
- `rollback_target`
- `reason_code`

### `RollbackPerformed`

- `rollback_id`
- `rollback_target`
- `superseded_artifact_refs`
- `resulting_state`

### `WorkflowCompleted`

- `final_state = Completed`
- `completion_source_event_id`

### `WorkflowAborted`

- `final_state = Aborted`
- `abort_reason_code`

### `WorkflowArchiveRequested`

- `archive_scope`
- `requested_execution_record_ref`

## 4.8 Failure / recovery 契约草案

### `WorkflowFailureCategory`

保留并收口为以下稳定枚举：

- `Validation`
- `OwnerCommandRejected`
- `OwnerEventTimeout`
- `StateTransitionViolation`
- `RollbackPolicyViolation`
- `ArchiveHandshakeFailed`
- `Infrastructure`
- `Unknown`

### `WorkflowRecoveryAction`

收口为：

- `RetryCurrentStage`
- `RequestRollback`
- `AbortWorkflow`
- `EscalateToOperator`
- `AwaitOwnerResolution`

### `WorkflowFailurePayload`

- `failure_category`
- `failure_code`
- `severity`
- `retryable`
- `rollback_target`
- `recommended_action`
- `owner_module_id`
- `owner_event_id`
- `message`

## 5. `WorkflowRun` 草案

### 5.1 核心字段

- `workflow_id`
- `job_id`
- `current_state`
- `current_stage`
- `target_stage`
- `last_completed_stage`
- `active_artifact_refs`
- `pending_command_id`
- `pending_request_id`
- `current_execution_context`
- `archive_status`
- `version`
- `created_at`
- `updated_at`
- `completed_at`
- `aborted_at`
- `faulted_at`
- `archived_at`

### 5.2 字段语义

- `current_state`：顶层工作流状态机状态
- `current_stage`：当前停留或正在推进的正式阶段
- `target_stage`：本次推进意图的目标阶段
- `last_completed_stage`：最近一次已成功闭环的阶段
- `active_artifact_refs`：各阶段当前有效版本引用，不保存 owner payload
- `current_execution_context`：仅保存与顶层协作相关的统一消息底板字段，不保存 runtime private state
- `archive_status`：`none / requested / archived / failed`

### 5.2.1 字段表

| 字段 | 类型建议 | 必填 | 说明 |
|---|---|---:|---|
| `workflow_id` | `WorkflowId` | 是 | 工作流主键 |
| `job_id` | `JobId` | 是 | 上游任务主键 |
| `current_state` | `WorkflowRunState` | 是 | 顶层状态机当前状态 |
| `current_stage` | `StageId` | 是 | 当前停留阶段 |
| `target_stage` | `StageId` | 否 | 当前推进目标阶段 |
| `last_completed_stage` | `StageId` | 否 | 最近成功闭环阶段 |
| `active_artifact_refs` | `WorkflowArtifactRefSet` | 是 | 各 owner 当前有效对象引用 |
| `pending_command_id` | `CommandId` | 否 | 尚未完成闭环的顶层命令 |
| `pending_request_id` | `RequestId` | 否 | 当前命令对应请求 |
| `current_execution_context` | `ExecutionContextEnvelope` | 否 | 顶层协作使用的统一上下文 |
| `archive_status` | `WorkflowArchiveStatus` | 是 | 归档握手状态 |
| `version` | `uint64` | 是 | 乐观锁版本 |
| `created_at` | `Timestamp` | 是 | 创建时间 |
| `updated_at` | `Timestamp` | 是 | 最后更新时间 |
| `completed_at` | `Timestamp` | 否 | 完成时间 |
| `aborted_at` | `Timestamp` | 否 | 中止时间 |
| `faulted_at` | `Timestamp` | 否 | 不可恢复失败时间 |
| `archived_at` | `Timestamp` | 否 | 归档完成时间 |

### 5.2.2 强约束

- `workflow_id` 与 `job_id` 创建后不可变。
- `active_artifact_refs` 只存 `artifact_ref`，不得内嵌 owner payload。
- `pending_command_id` 在收到对应成功/失败/阻断事件后必须清空或替换。
- `archived_at` 只有在收到 `WorkflowArchived` 后才允许写入。
- `current_execution_context` 可以引用 `execution_package_ref / execution_id / machine_id / segment_index`，但不得替代 `runtime-execution` 的私有运行态存储。

### 5.3 状态集合

- `Created`
- `SourceAccepted`
- `GeometryReady`
- `TopologyReady`
- `FeaturesReady`
- `ProcessPlanned`
- `CoordinatesResolved`
- `Aligned`
- `ProcessPathReady`
- `MotionPlanned`
- `TimingPlanned`
- `PackageBuilt`
- `PackageValidated`
- `ReadyForPreflight`
- `PreflightBlocked`
- `MachineReady`
- `FirstArticlePending`
- `RollbackPending`
- `ReadyForProduction`
- `Executing`
- `Paused`
- `Recovering`
- `Completed`
- `Aborted`
- `Faulted`
- `Archived`

### 5.4 关键约束

- `WorkflowRun` 只记录顶层事实投影，不复制 `ExecutionSession` 内部状态明细
- `PreflightBlocked` 只表示门禁阻断，不改写 `S5-S11` 产物有效性
- `Archived` 只能由收到 `WorkflowArchived` 后进入
- `Completed / Aborted / Faulted` 是归档前终态，不是对象删除或历史抹除

### 5.5 `WorkflowRun` 状态迁移表

| From | Trigger command / source event | To | 约束 |
|---|---|---|---|
| `Created` | `CreateWorkflow` / `JobCreated` | `Created` | 创建只建立 M0 对象，不越权推进 |
| `Created` | `AdvanceStage` / `SourceFileAccepted` | `SourceAccepted` | 必须绑定 `job_id` |
| `SourceAccepted` | `AdvanceStage` / `CanonicalGeometryBuilt` | `GeometryReady` | 只消费 `artifact_ref` |
| `GeometryReady` | `AdvanceStage` / `TopologyBuilt` | `TopologyReady` | - |
| `TopologyReady` | `AdvanceStage` / `FeatureGraphBuilt` | `FeaturesReady` | - |
| `FeaturesReady` | `AdvanceStage` / `ProcessPlanBuilt` | `ProcessPlanned` | - |
| `ProcessPlanned` | `AdvanceStage` / `CoordinateTransformResolved` | `CoordinatesResolved` | - |
| `CoordinatesResolved` | `AdvanceStage` / `AlignmentSucceeded or AlignmentNotRequired` | `Aligned` | 不允许隐式跳过 |
| `Aligned` | `AdvanceStage` / `ProcessPathBuilt` | `ProcessPathReady` | - |
| `ProcessPathReady` | `AdvanceStage` / `MotionPlanBuilt` | `MotionPlanned` | - |
| `MotionPlanned` | `AdvanceStage` / `DispenseTimingPlanBuilt` | `TimingPlanned` | - |
| `TimingPlanned` | `AdvanceStage` / `ExecutionPackageBuilt` | `PackageBuilt` | 不得跳过 S11A |
| `PackageBuilt` | `AdvanceStage` / `ExecutionPackageValidated` | `PackageValidated` | 不得跳过 S11B |
| `PackageValidated` | `AdvanceStage` / `ExecutionSessionCreated` | `ReadyForPreflight` | M9 owner |
| `ReadyForPreflight` | `AdvanceStage` / `PreflightPassed` | `MachineReady` | M9 owner |
| `ReadyForPreflight` | `AdvanceStage` / `PreflightBlocked` | `PreflightBlocked` | 不改写上游对象 |
| `PreflightBlocked` | `RetryStage` / `PreflightStarted` | `ReadyForPreflight` | 重新门禁，不回退规划链 |
| `MachineReady` | M9 首件策略事件 | `ReadyForProduction` | `FirstArticleNotRequired` / `FirstArticleWaived` |
| `MachineReady` | `AdvanceStage` / `FirstArticleStarted` | `FirstArticlePending` | M9 owner |
| `FirstArticlePending` | `ApproveFirstArticle` / `FirstArticlePassed` | `ReadyForProduction` | - |
| `FirstArticlePending` | `RejectFirstArticle` / `FirstArticleRejected` | `RollbackPending` | 不得直接进 `Faulted` |
| `RollbackPending` | `RequestRollback` / `RollbackRequested` | `RollbackPending` | 等待执行 |
| `RollbackPending` | `PerformRollback` / `RollbackPerformed` | `rollback_target` 对应状态 | 需伴随 `ArtifactSuperseded` |
| `ReadyForProduction` | `AdvanceStage` / `ExecutionStarted` | `Executing` | M9 owner |
| `Executing` | `PauseExecution` / `ExecutionPaused` | `Paused` | M9 owner |
| `Paused` | `ResumeExecution` / `ExecutionResumed` | `Executing` | M9 owner |
| `Executing` | `RecoverExecution` / `ExecutionRecovered` | `Recovering` | 可恢复 fault 场景 |
| `Recovering` | M9 恢复完成事件 | `Executing` | 不得直接改写规划链 |
| `Executing` | M9 运行完成事件 | `Completed` | - |
| `Executing / Paused / Recovering / PreflightBlocked / FirstArticlePending / RollbackPending` | `AbortWorkflow or AbortExecution` / `WorkflowAborted or ExecutionAborted` | `Aborted` | 统一终止态 |
| `Executing / Recovering` | M9 `ExecutionFaulted` | `Faulted` | 不可恢复终止 |
| `Completed / Aborted / Faulted` | `CompleteWorkflowArchive` / `WorkflowArchiveRequested` | `Completed / Aborted / Faulted` | 仅发起归档握手 |
| `Completed / Aborted / Faulted` | M10 `WorkflowArchived` | `Archived` | 只能由 M10 归档结果驱动 |

## 6. `StageTransitionRecord` 草案

### 6.1 核心字段

- `transition_id`
- `workflow_id`
- `from_state`
- `to_state`
- `from_stage`
- `to_stage`
- `source_command_id`
- `source_event_id`
- `artifact_ref`
- `execution_context`
- `transition_reason`
- `occurred_at`

### 6.2 作用

- 固化每次阶段推进、停留、失败、回退与终止迁移
- 为时序审计、幂等回放、trace 回链提供顶层证据

### 6.3 字段表

| 字段 | 类型建议 | 必填 | 说明 |
|---|---|---:|---|
| `transition_id` | `TransitionId` | 是 | 迁移主键 |
| `workflow_id` | `WorkflowId` | 是 | 归属工作流 |
| `from_state` | `WorkflowRunState` | 是 | 起始状态 |
| `to_state` | `WorkflowRunState` | 是 | 目标状态 |
| `from_stage` | `StageId` | 否 | 起始阶段 |
| `to_stage` | `StageId` | 否 | 目标阶段 |
| `source_command_id` | `CommandId` | 否 | 触发迁移的命令 |
| `source_event_id` | `EventId` | 否 | 确认迁移的事件 |
| `artifact_ref` | `ArtifactRef` | 否 | 相关 owner 对象引用 |
| `execution_context` | `ExecutionContextEnvelope` | 否 | 统一执行上下文 |
| `transition_reason` | `StageTransitionReason` | 是 | 迁移原因 |
| `occurred_at` | `Timestamp` | 是 | 发生时间 |

### 6.4 `StageTransitionReason` 建议枚举

- `AdvanceAccepted`
- `AdvanceRejected`
- `OwnerSucceeded`
- `OwnerFailed`
- `OwnerBlocked`
- `RetryRequested`
- `RollbackRequested`
- `RollbackPerformed`
- `AbortRequested`
- `ArchiveRequested`
- `ArchiveCompleted`

## 7. `RollbackDecision` 草案

### 7.1 核心字段

- `rollback_id`
- `workflow_id`
- `requested_from_state`
- `requested_from_stage`
- `rollback_target`
- `reason_code`
- `reason_message`
- `affected_artifact_refs`
- `decision_status`
- `requested_by`
- `requested_at`
- `resolved_at`

### 7.2 状态集合

- `Requested`
- `Evaluating`
- `Rejected`
- `Executing`
- `Performed`
- `Failed`

### 7.3 关键约束

- `rollback_target` 必须映射到正式回退枚举，不允许自然语言随意表达
- `affected_artifact_refs` 只记录将被 supersede 的对象引用，不保存对象内容
- `Performed` 必须以 owner 模块的 `ArtifactSuperseded` 与 `RollbackPerformed` 协同闭环

### 7.4 `rollback_target` 正式枚举

与 `s03` 对齐，仅允许：

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

### 7.5 `RollbackDecision` 状态迁移表

| From | Trigger | To | 约束 |
|---|---|---|---|
| `Requested` | M0 接收 `RequestRollback` | `Evaluating` | 开始分析影响面 |
| `Evaluating` | 策略判定允许回退 | `Executing` | 必须给出 `rollback_target` |
| `Evaluating` | 策略判定拒绝回退 | `Rejected` | 必须保留拒绝原因 |
| `Executing` | owner 模块已完成 supersede 与回退动作 | `Performed` | 需有 `RollbackPerformed` 证据 |
| `Executing` | 任一关键 owner 回退失败 | `Failed` | 保留失败模块和原因 |

### 7.6 `RollbackDecision` 字段表

| 字段 | 类型建议 | 必填 | 说明 |
|---|---|---:|---|
| `rollback_id` | `RollbackId` | 是 | 回退决策主键 |
| `workflow_id` | `WorkflowId` | 是 | 归属工作流 |
| `requested_from_state` | `WorkflowRunState` | 是 | 请求发起时的顶层状态 |
| `requested_from_stage` | `StageId` | 是 | 请求发起时的阶段 |
| `rollback_target` | `RollbackTarget` | 是 | 正式枚举回退目标 |
| `reason_code` | `FailureCode` | 是 | 触发回退的机器可读原因 |
| `reason_message` | `string` | 否 | 面向人的补充说明 |
| `affected_artifact_refs` | `ArtifactRef[]` | 是 | 将被 supersede 的对象链 |
| `decision_status` | `RollbackDecisionStatus` | 是 | 当前决策状态 |
| `requested_by` | `ActorRef` | 是 | 发起者 |
| `requested_at` | `Timestamp` | 是 | 发起时间 |
| `resolved_at` | `Timestamp` | 否 | 结束时间 |

## 8. `workflow` 依赖冻结

### 8.1 允许依赖

- `shared`
- `M1-M10` 的 `contracts`

### 8.2 禁止依赖

- 任意模块的 `domain/`
- 任意模块的 `services/`
- 任意模块的 `adapters/`
- 任意模块的 `runtime/`
- `apps/`

## 8.3 现有 `WorkflowContracts.h` 向目标态的映射

当前单文件聚合头：

- `WorkflowCommand`
- `WorkflowFailureCategory`
- `WorkflowRecoveryAction`
- `WorkflowStageLifecycle`
- `WorkflowRecoveryDirective`
- `WorkflowStageState`
- `WorkflowPlanningTriggerRequest`
- `WorkflowPlanningTriggerResponse`

目标态映射建议：

- `WorkflowCommand`
  - 不再作为唯一总枚举继续扩张
  - 对外拆入 `contracts/commands/*.h`
  - 若需要兼容聚合，可在 `WorkflowContracts.h` 中仅保留 `#include` 聚合
- `WorkflowFailureCategory`
  - 迁入 `contracts/failures/WorkflowFailureCategory.h`
- `WorkflowRecoveryAction`
  - 迁入 `contracts/failures/WorkflowRecoveryAction.h`
- `WorkflowStageLifecycle`
  - 不再承担顶层状态机真值
  - 若保留，仅作为历史 bridge；终态以 `WorkflowRunState` 与 `RollbackDecisionStatus` 为准
- `WorkflowRecoveryDirective`
  - 拆分为 `WorkflowFailurePayload` 中的恢复建议字段
- `WorkflowStageState`
  - 拆分为 `WorkflowRunView` + `StageTransitionRecordView`
- `WorkflowPlanningTriggerRequest`
  - 不再作为 `workflow` 长期 canonical 请求
  - 规划触发属于 `AdvanceStage` 到具体 owner command 的编排结果，不在 `workflow/contracts` 里长期 owner 化 `M4` 特定请求
- `WorkflowPlanningTriggerResponse`
  - 不再作为 `workflow` 长期 canonical 响应
  - 统一由 owner 模块事件 + `StageSucceeded/StageFailed` 顶层投影替代

## 9. 与其他模块的终态接口关系

### 9.1 `workflow -> runtime-execution`

`workflow` 只通过 `runtime-execution/contracts`：

- 下发执行前与执行中命令
- 消费执行结果事件
- 更新顶层 `WorkflowRun`

不直接依赖 `runtime-execution/application`、`runtime-execution/runtime`、`runtime-execution/adapters`

### 9.2 `workflow -> trace-diagnostics`

`workflow` 只：

- 请求归档
- 消费 `WorkflowArchived`

不直接生成 `ExecutionRecord`

### 9.3 `hmi-application -> workflow`

`hmi-application` 只消费：

- `workflow/contracts`
- `workflow` 查询结果

不进入 `workflow` 内部实现层。

## 10. 终态禁止项

- 不允许保留 `application/usecases/` 作为长期 public surface
- 不允许保留 `domain/domain/` 作为 bridge-domain root
- 不允许把 motion、dispensing、runtime、trace 的 concrete 回流到 `workflow`
- 不允许把 `workflow` 变成 “M0 + M9 + M10” 三合一模块
- 不允许把 `PreflightBlocked`、`FirstArticleRejected`、`ExecutionFaulted` 混写成一个模糊失败态

## 11. 文档关系

本文不覆盖以下 authority 文档，只对它们在 `modules/workflow/` 上的最终物理落点作补充冻结：

- `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md`
- `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s06-repo-structure-guide.md`
- `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s07-state-machine-command-bus.md`
- `docs/architecture/dsp-e2e-spec/project-chain-standard-v1.md`
