# 《点胶机端到端流程规格说明 s05：模块边界与接口契约表（修订版）》

Status: Frozen
Authority: Primary Spec Axis S05
Applies To: M0-M11 module ownership and interface contracts
Owns / Covers: module owner, produced artifacts, consumed artifacts, command/event interfaces, forbidden access, failure responsibility
Must Not Override: S01 stage chain; S04 object schema; S07/S08 state and sequence semantics
Read When: judging module owner, dependency direction, interface boundary, or cross-module violation
Conflict Priority: use S05 for owner and boundary decisions; defer object fields to S04 and stage acceptance to S02
Codex Keywords: M0-M11, module owner, interface contract, artifact_ref, forbidden access, HMI not source of truth

---

## Codex Decision Summary

- 本文裁决 M0-M11 的唯一 owner、接口契约、消费关系、输出事件与越权边界。
- 本文不改变对象字段定义或状态机细节；对象字段以 S04 为准，状态命令闭环以 S07/S08 为准。
- HMI 不是事实源模块；运行时模块不得越权重算规划对象；回退、失效、归档必须有 owner 级接口闭环。

---
本版以“联调可闭环、状态可落地、测试可追溯”为目标，重点修正原稿中的以下关键问题：

- M8 存在 `ValidateExecutionPackage(...)` 输入，但没有 `ExecutionPackageValidated` 成功事件闭环。
- M9 运行时接口粒度不足，无法支撑预检 / 首件 / 恢复 / 中止状态机与测试矩阵。
- `artifact_ref` 与 `artifact_id / artifact_version` 口径未统一。
- 回退后的 `superseded` 规则缺少 owner 级执行机制。
- 归档 `WorkflowArchived` 的 owner 与接口闭环不清。
- 无对位任务缺少正式契约语义。

---

# 1. 模块划分总原则

## 1.1 模块按“产物责任”划分，不按算法名划分
一级模块的首要职责是生成和维护 owner 对象，而不是包办一串算法过程。

## 1.2 每个核心对象必须有唯一 owner 模块
| 对象 | owner 模块 |
|---|---|
| `JobDefinition`、`SourceDrawing` | M1 |
| `CanonicalGeometry` | M2 |
| `TopologyModel`、`FeatureGraph` | M3 |
| `ProcessPlan` | M4 |
| `CoordinateTransformSet`、`AlignmentCompensation` | M5 |
| `ProcessPath` | M6 |
| `MotionPlan` | M7 |
| `DispenseTimingPlan`、`ExecutionPackage` | M8 |
| `MachineReadySnapshot`、`ExecutionSession`、运行时事件流、`FirstArticleResult` | M9 |
| `ExecutionRecord`、`TraceLinkSet` | M10 |

## 1.3 模块间统一传递 `artifact_ref`
模块主接口优先传：
- `job_id`
- `workflow_id`
- `artifact_ref`
- `request_id`
- `idempotency_key`

不再允许在核心链路里只传一组无版本信息的临时字段。

## 1.4 运行时模块不得越权重算规划对象
M9 可以：
- 阻断执行
- 暂停执行
- 恢复执行
- 上报 fault
- 请求回退

M9 不可以：
- 直接重建 `ProcessPlan`
- 直接重建 `MotionPlan`
- 直接修改 `ExecutionPackage`

## 1.5 HMI 不是事实源模块
M11 只能发命令、看状态、做审批、看追溯，不能直接写 owner 对象，也不能绕过 M0/M9 控设备。

## 1.6 回退 / 失效 / 归档都必须是显式接口
- 回退：由 M0 编排，owner 模块执行对象失效与重建。
- 失效：由 owner 模块执行 `SupersedeOwnedArtifactCommand` 并发 `ArtifactSuperseded`。
- 归档：由 M10 执行 `ArchiveWorkflowCommand`，发 `WorkflowArchived`，M0 消费后迁移到 `Archived`。

---

# 2. 建议的一级模块清单

| 模块编号 | 模块名称 | 核心职责 | owner 对象 |
|---|---|---|---|
| M0 | 工作流编排模块 | 串联阶段、管理状态机、驱动回退与归档 | `WorkflowRun`、`StageTransitionRecord`、`RollbackDecision` |
| M1 | 任务与文件接入模块 | 创建任务、接收 DXF、封存源文件 | `JobDefinition`、`SourceDrawing` |
| M2 | DXF/规范几何模块 | 解析 DXF，生成规范几何 | `CanonicalGeometry` |
| M3 | 拓扑与制造特征模块 | 拓扑修复与特征提取 | `TopologyModel`、`FeatureGraph` |
| M4 | 工艺规划模块 | 特征到工艺规则映射 | `ProcessPlan` |
| M5 | 坐标/对位/补偿模块 | 静态坐标链、动态补偿、无补偿显式化 | `CoordinateTransformSet`、`AlignmentCompensation` |
| M6 | 工艺路径模块 | 路径顺序、连接、空跑/进退刀组织 | `ProcessPath` |
| M7 | 运动规划模块 | 动态约束、轨迹生成 | `MotionPlan` |
| M8 | 点胶时序与执行包模块 | 时序、组包、离线校验 | `DispenseTimingPlan`、`ExecutionPackage` |
| M9 | 设备运行时模块 | 预检、首件、执行、暂停/恢复、fault 分流 | `MachineReadySnapshot`、`ExecutionSession`、`FirstArticleResult`、运行时事件流 |
| M10 | 追溯与诊断模块 | 执行记录、事件归档、查询链 | `ExecutionRecord`、`TraceLinkSet` |
| M11 | HMI/应用服务模块 | 命令入口、审批、展示与查询 | `UserCommand`、`ApprovalDecision`、`UIViewModel` |

---

# 3. 模块边界与接口契约表

## M0｜工作流编排模块

### 模块职责
负责串联阶段、驱动顶层工作流状态机、发起回退与归档、保证跨模块闭环。

### 负责生成的对象
- `WorkflowRun`
- `StageTransitionRecord`
- `RollbackDecision`

### 负责消费的对象
- 全链路对象引用与事件引用

### 对外输入命令
- `CreateWorkflow(job_id)`
- `AdvanceStage(job_id, target_stage)`
- `RetryStage(job_id, stage_id)`
- `RequestRollback(job_id, rollback_target, reason)`
- `PerformRollback(job_id, rollback_target)`
- `AbortWorkflow(job_id)`
- `CompleteWorkflowArchive(job_id, execution_record_ref)`

### 对外输出事件
- `StageStarted`
- `StageSucceeded`
- `StageFailed`
- `RollbackRequested`
- `RollbackPerformed`
- `WorkflowCompleted`
- `WorkflowAborted`
- `WorkflowArchiveRequested`

### 不允许越权访问的对象
- 不允许直接改写任一阶段产物内容
- 不允许在编排层生成 `MotionPlan` 或 `ExecutionPackage`
- 不允许在编排层自行修补工艺参数

### 失败责任边界
流程控制错、状态迁移错、回退目标错、归档编排错，由 M0 负责；具体阶段对象构造失败不归 M0。

---

## M1｜任务与文件接入模块

### 模块职责
创建任务上下文、接收 DXF、封存源文件、生成 `JobDefinition` 与 `SourceDrawing`。

### 负责生成的对象
- `JobDefinition`
- `SourceDrawing`

### 负责消费的对象
- 外部工单系统 / HMI 输入

### 对外输入命令
- `CreateJob(command)`
- `AttachSourceFile(job_ref, file_blob)`
- `ValidateSourceFile(job_ref)`
- `GetSourceDrawing(job_ref)`

### 对外输出事件
- `JobCreated`
- `SourceFileAccepted`
- `SourceFileRejected`

### 不允许越权访问的对象
- 不允许生成 `CanonicalGeometry`
- 不允许做特征提取
- 不允许绑定工艺细节
- 不允许改设备运行状态

### 失败责任边界
文件损坏、封存失败、任务上下文缺失由 M1 负责；DXF 解析失败不归 M1。

---

## M2｜DXF/规范几何模块

### 模块职责
把源图解析为系统统一几何表达。

### 负责生成的对象
- `CanonicalGeometry`

### 负责消费的对象
- `SourceDrawing`

### 对外输入命令
- `ParseSourceDrawing(source_drawing_ref)`
- `RebuildCanonicalGeometry(source_drawing_ref, parse_profile_ref)`
- `SupersedeOwnedArtifact(artifact_ref, superseded_by_ref, reason)`

### 对外输出事件
- `CanonicalGeometryBuilt`
- `CanonicalGeometryRejected`
- `ArtifactSuperseded`

### 不允许越权访问的对象
- 不允许直接做特征分类
- 不允许改写 `SourceDrawing`
- 不允许加入设备动态参数

### 失败责任边界
不支持实体、单位解析异常、几何退化等由 M2 负责。

---

## M3｜拓扑与制造特征模块

### 模块职责
负责拓扑修复、连通性建模与制造特征提取。

### 负责生成的对象
- `TopologyModel`
- `FeatureGraph`

### 负责消费的对象
- `CanonicalGeometry`

### 对外输入命令
- `BuildTopology(canonical_geometry_ref)`
- `ExtractFeatures(topology_model_ref, feature_profile_ref)`
- `SupersedeOwnedArtifact(artifact_ref, superseded_by_ref, reason)`

### 对外输出事件
- `TopologyBuilt`
- `TopologyRejected`
- `FeatureGraphBuilt`
- `FeatureClassificationFailed`
- `ArtifactSuperseded`

### 不允许越权访问的对象
- 不允许写工艺速度/阀延时
- 不允许生成 `ProcessPlan`

### 失败责任边界
断口修复失败、闭环歧义、自交不可解释、特征冲突由 M3 负责。

---

## M4｜工艺规划模块

### 模块职责
把制造特征映射为工艺规则，生成 `ProcessPlan`。

### 负责生成的对象
- `ProcessPlan`

### 负责消费的对象
- `FeatureGraph`
- 工艺模板 / 材料画像

### 对外输入命令
- `PlanProcess(feature_graph_ref, material_profile_ref, process_template_ref)`
- `ReplanProcess(process_context)`
- `SupersedeOwnedArtifact(artifact_ref, superseded_by_ref, reason)`

### 对外输出事件
- `ProcessPlanBuilt`
- `ProcessRuleRejected`
- `ArtifactSuperseded`

### 不允许越权访问的对象
- 不允许输出轨迹或设备指令
- 不允许直接查询设备实时门禁

### 失败责任边界
模板缺失、参数越界、工艺规则冲突由 M4 负责。

---

## M5｜坐标/对位/补偿模块

### 模块职责
负责静态坐标变换、动态对位/补偿，以及“无需补偿”场景的显式化。

### 负责生成的对象
- `CoordinateTransformSet`
- `AlignmentCompensation`

### 负责消费的对象
- `ProcessPlan`
- 标定参数
- 治具规则
- 视觉输入
- 测高输入

### 对外输入命令
- `ResolveCoordinateTransforms(process_plan_ref, fixture_profile_ref, calibration_profile_ref)`
- `RunAlignment(transform_set_ref, alignment_profile_ref)`
- `RunHeightCompensation(transform_set_ref, probe_profile_ref)`
- `MarkAlignmentNotRequired(transform_set_ref, reason, policy_ref)`
- `SupersedeOwnedArtifact(artifact_ref, superseded_by_ref, reason)`

### 对外输出事件
- `CoordinateTransformResolved`
- `AlignmentSucceeded`
- `AlignmentFailed`
- `AlignmentNotRequired`
- `CompensationOutOfRange`
- `ArtifactSuperseded`

### 不允许越权访问的对象
- 不允许改写 `ProcessPlan`
- 不允许重新定义特征分类
- 不允许生成轨迹动态参数
- 不允许直接下发设备执行

### 失败责任边界
基准缺失、补偿超限、对位失败、针嘴校准失败由 M5 负责；路径顺序错误不归 M5。

---

## M6｜工艺路径模块

### 模块职责
负责按工艺语义组织路径顺序、进退刀、空跑连接和分组。

### 负责生成的对象
- `ProcessPath`

### 负责消费的对象
- `ProcessPlan`
- `CoordinateTransformSet`
- `AlignmentCompensation`

### 对外输入命令
- `BuildProcessPath(process_plan_ref, transform_set_ref, alignment_compensation_ref)`
- `ReorderProcessPath(process_path_request)`
- `SupersedeOwnedArtifact(artifact_ref, superseded_by_ref, reason)`

### 对外输出事件
- `ProcessPathBuilt`
- `ProcessPathRejected`
- `UncoveredFeatureDetected`
- `ArtifactSuperseded`

### 关键约束
- `alignment_compensation_ref` 必填。
- 无对位任务必须引用 M5 生成的 `AlignmentCompensation(not_required)` 对象，不允许直接传空值。

### 不允许越权访问的对象
- 不允许决定 jerk、插补模式、加速度
- 不允许生成阀时序
- 不允许直接与设备通信
- 不允许修改对位补偿结果

### 失败责任边界
路径顺序冲突、起点选择错误、空跑污染关键区由 M6 负责；角点动态超限不归 M6。

---

## M7｜运动规划模块

### 模块职责
把工艺路径转换成设备可执行的运动轨迹。

### 负责生成的对象
- `MotionPlan`

### 负责消费的对象
- `ProcessPath`
- 设备动态约束
- 轴组模型
- 插补策略

### 对外输入命令
- `PlanMotion(process_path_ref, machine_profile_ref, dynamics_profile_ref)`
- `ReplanMotion(motion_plan_request)`
- `SupersedeOwnedArtifact(artifact_ref, superseded_by_ref, reason)`

### 对外输出事件
- `MotionPlanBuilt`
- `MotionPlanRejected`
- `TrajectoryConstraintViolated`
- `ArtifactSuperseded`

### 不允许越权访问的对象
- 不允许直接嵌入阀开关逻辑
- 不允许改写工艺规则
- 不允许读写设备实时报警
- 不允许审批首件质量

### 失败责任边界
轨迹不可行、动态超限、抬刀冲突、插补方式不支持由 M7 负责。

---

## M8｜点胶时序与执行包模块

### 模块职责
负责生成点胶时序、组装执行包、做离线规则校验，并把“已组包”与“已校验通过”显式区分。

### 负责生成的对象
- `DispenseTimingPlan`
- `ExecutionPackage`

### 负责消费的对象
- `MotionPlan`
- `ProcessPlan`
- `production_baseline`
- 运行模式
- 设备配置快照

### 对外输入命令
- `BuildDispenseTimingPlan(motion_plan_ref, process_plan_ref, valve_profile_ref)`
- `AssembleExecutionPackage(motion_plan_ref, timing_plan_ref, production_baseline_ref, run_mode)`
- `ValidateExecutionPackage(execution_package_ref)`
- `SupersedeOwnedArtifact(artifact_ref, superseded_by_ref, reason)`

### 对外输出事件
- `DispenseTimingPlanBuilt`
- `DispenseTimingPlanRejected`
- `ExecutionPackageBuilt`
- `ExecutionPackageValidated`
- `ExecutionPackageRejected`
- `OfflineValidationFailed`
- `ArtifactSuperseded`

### 关键约束
- `ExecutionPackageBuilt` 只表示“组包完成”，不等于“离线规则通过”。
- 只有 `ExecutionPackageValidated` 才允许驱动上层进入 `PackageValidated / ReadyForPreflight`。

### 不允许越权访问的对象
- 不允许重排 `ProcessPath`
- 不允许重建 `MotionPlan`
- 不允许直接宣布设备 ready
- 不允许在运行时改写冻结包

### 失败责任边界
阀时序非法、轨迹/时序版本不一致、离线校验失败由 M8 负责；设备未回零不归 M8。

---

## M9｜设备运行时模块

### 模块职责
负责预检、预览、空跑、首件、正式执行、暂停恢复、故障分层与执行中止。

### 负责生成的对象
- `MachineReadySnapshot`
- `ExecutionSession`
- 运行时状态流
- 局部 fault 事件
- `FirstArticleResult`

### 负责消费的对象
- `ExecutionPackage`
- 设备实时状态
- 首件审批结果

### 对外输入命令
- `CreateExecutionSession(execution_package_ref)`
- `RunPreflight(execution_package_ref)`
- `RunPreview(execution_package_ref)`
- `RunDryRun(execution_package_ref)`
- `RunFirstArticle(execution_package_ref)`
- `ApproveFirstArticle(first_article_result_ref)`
- `RejectFirstArticle(first_article_result_ref)`
- `WaiveFirstArticle(first_article_result_ref, approval_ref)`
- `StartExecution(execution_package_ref)`
- `PauseExecution(execution_id)`
- `ResumeExecution(execution_id)`
- `RecoverExecution(execution_id)`
- `AbortExecution(execution_id)`

### 对外输出事件
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

### 关键约束
- `PreflightBlocked` 仅阻断执行，不得污染规划对象。
- `StartExecution(...)` 仅当存在 `PreflightPassed` 且首件门已放行时才允许成功。
- `FirstArticleRejected` 必须带可追溯 `failure_code` 与 `rollback_target`。
- `RecoverExecution(...)` 仅允许在可恢复 fault 场景下成功。

### 不允许越权访问的对象
- 不允许重写 `ExecutionPackage`
- 不允许重建 `MotionPlan`
- 不允许修改 `ProcessPlan`
- 不允许把设备未 ready 解释成上游规划失败

### 失败责任边界
预检失败、运行时通信中断、设备报警、堵针/无胶、执行中断由 M9 负责；若根因复盘指向上游规划，M9 只能上报，不负责修规划。

---

## M10｜追溯与诊断模块

### 模块职责
负责固化执行事实、构建日志主链、统计故障、执行归档、提供查询。

### 负责生成的对象
- `ExecutionRecord`
- `TraceLinkSet`
- `FaultDigest`
- `QualitySummary`

### 负责消费的对象
- 全链路对象引用
- 运行时事件流
- 报警流
- 首件结果
- 操作员动作

### 对外输入命令
- `RecordExecution(execution_context)`
- `AppendFaultEvent(fault_event)`
- `FinalizeExecution(execution_id)`
- `ArchiveWorkflow(job_id, execution_record_ref)`
- `QueryTrace(job_id | execution_package_ref | failure_code)`
- `SupersedeOwnedArtifact(artifact_ref, superseded_by_ref, reason)`

### 对外输出事件
- `FaultEventAppended`
- `ExecutionRecordBuilt`
- `WorkflowArchived`
- `TraceWriteFailed`
- `TraceQueryResult`
- `ArtifactSuperseded`

### 关键约束
- `WorkflowArchived` 的 owner 固定为 M10。
- M0 只能消费 `WorkflowArchived` 来迁移顶层状态，不能伪造归档完成。

### 不允许越权访问的对象
- 不允许改写上游对象
- 不允许用追溯结果“自动修复”工艺计划
- 不允许成为实时控制链的一部分

### 失败责任边界
归档失败、日志链断裂、版本关联丢失由 M10 负责；实际执行失败不归 M10。

---

## M11｜HMI/应用服务模块

### 模块职责
负责用户交互、任务发起、参数选择、状态展示、异常提示、人工审批入口。

### 负责生成的对象
- `UserCommand`
- `ApprovalDecision`
- `UIViewModel`

### 负责消费的对象
- 所有上游对象的只读视图
- 运行状态
- 故障事件
- 追溯结果

### 对外输入接口
- 用户操作命令
- 审批操作
- 查询请求

### 对外输出接口
- 向 M0 / M1 / M9 / M10 发命令
- 展示视图
- 人工确认结果

### 不允许越权访问的对象
- 不允许直接写 `ProcessPlan`
- 不允许直接写 `MotionPlan`
- 不允许直接写 `ExecutionPackage`
- 不允许绕过 M9 直控设备

### 失败责任边界
显示错误、交互误导、审批入口设计错误由 M11 负责；制造逻辑失败不归 M11。

---

# 4. 模块依赖关系图（建议拆成三条主链）

## 4.1 规划主链

```text
M1 -> M2 -> M3 -> M4 -> M5 -> M6 -> M7 -> M8
```

## 4.2 执行主链

```text
M8 -> M9 -> M10
```

## 4.3 控制与展示平面

```text
M11 -> M0
M0 -> M1/M2/M3/M4/M5/M6/M7/M8/M9/M10
```

说明：
- M10 是追溯旁路，不属于规划主链的一部分。
- M11 只在控制平面存在，不拥有制造事实源。

---

# 5. 模块间接口类型建议

## 5.1 Command
表示“请求系统做一件事”。必须可追溯、可拒绝、可幂等。

## 5.2 Event
表示“某件事已经发生”。必须可审计、可持久化、可驱动状态迁移。

## 5.3 Query
表示“读取事实”。不得借 Query 触发写操作。

## 5.4 Decision
表示“授权 / 审批 / 回退建议”。Decision 自身也必须持久化。

---

# 6. 统一消息契约（替代旧版最小字段）

## 6.1 Command 最小公共字段

| 字段 | 说明 |
|---|---|
| `message_id` | 消息主键 |
| `command_id` | 命令主键 |
| `request_id` | 本次请求主键 |
| `idempotency_key` | 幂等键 |
| `job_id` | 任务主键 |
| `workflow_id` | 工作流主键 |
| `stage_id` | 目标阶段 |
| `artifact_ref` | 目标或上下文对象引用，可空 |
| `correlation_id` | 关联链主键 |
| `causation_id` | 上一事件或命令主键，可空 |
| `execution_context` | 执行上下文：`execution_package_ref / execution_id / machine_id / segment_index` |
| `issued_by` | 发起人 / 服务 |
| `issued_at` | 发起时间 |
| `reason` | 发起原因，可空 |

## 6.2 Event 最小公共字段

| 字段 | 说明 |
|---|---|
| `message_id` | 消息主键 |
| `event_id` | 事件主键 |
| `request_id` | 对应请求 |
| `job_id` | 任务主键 |
| `workflow_id` | 工作流主键 |
| `stage_id` | 所属阶段 |
| `artifact_ref` | 事件关联对象 |
| `source_command_id` | 触发该事件的命令 |
| `correlation_id` | 关联链主键 |
| `causation_id` | 上一命令或事件主键 |
| `execution_context` | 执行上下文 |
| `producer` | 产生事件的模块 |
| `occurred_at` | 发生时间 |
| `status` | 成功 / 失败 / 阻断 / 决策 |

## 6.3 失败载荷统一结构

```text
failure = {
  failure_code,
  severity,
  retryable,
  rollback_target,
  recommended_action,
  blocking_scope,
  requires_manual_decision
}
```

说明：
- `failure_code` 对接 s03。
- `rollback_target` 枚举必须与 s07 回退目标一致。
- 运行时 fault 还必须补 `segment_index`、`execution_id`、`execution_package_ref`。

---

# 7. 回退、失效与归档执行机制

## 7.1 回退执行原则

1. M0 负责判定回退目标与编排顺序。
2. 对象 owner 负责执行 `SupersedeOwnedArtifact(...)`。
3. M10 负责记录每一次失效与版本切换。
4. 新对象版本生成后，M0 才能推进状态回到目标阶段。

## 7.2 `SupersedeOwnedArtifact(...)` 统一语义

### 输入
- `artifact_ref`
- `superseded_by_ref`（触发本次替代的新对象或回退决策引用）
- `reason`
- `rollback_id`

### 输出
- `ArtifactSuperseded`

### 结果要求
- 旧对象 `status = superseded`
- `diagnostics.superseded_reason` 写明原因
- 旧对象保留历史可查，不可再被当作活动版本使用

## 7.3 归档闭环

1. M10 在 `ExecutionRecordBuilt` 后可执行 `ArchiveWorkflow(...)`
2. M10 成功后发 `WorkflowArchived`
3. M0 消费 `WorkflowArchived`，将顶层工作流迁移到 `Archived`

---

# 8. 模块越权红线

- M9 不得直接生成 `ProcessPlan`
- M8 不得重排 `ProcessPath`
- M7 不得直接写阀时序
- M5 不得改写 `SourceDrawing`
- M11 不得绕过 M0 / M9 直接写控制设备
- M10 不得反向修正历史对象

---

# 9. 模块责任与故障归属矩阵

| 典型问题 | 首要责任模块 | 不应归责模块 |
|---|---|---|
| DXF 结构非法 | M1 / M2 | M7 / M9 |
| 断口拼接失败 | M3 | M8 / M9 |
| 特征分类冲突 | M3 | M7 |
| 工艺模板缺失 | M4 | M9 |
| 镜像方向错误 | M5 | M8 |
| 补偿超限 | M5 | M8 / M9 |
| 路径顺序异常 | M6 | M9 |
| 轨迹动态超限 | M7 | M4 |
| 起止时序非法 | M8 | M7（除非明确由轨迹几何引发） |
| 离线校验失败 | M8 | M9 |
| 设备未回零 / 门禁不满足 | M9 | M8 |
| 运行中堵针 / 无胶 | M9 | M7 |
| 归档失败 | M10 | M9 |
| UI 误显示放行状态 | M11 | M4 |

---

# 10. 本版最关键的工程结论

## 10.1 M8 与 M9 的事件集必须成为联调冻结接口
如果这些事件再继续保留粗粒度表述，状态机与测试矩阵仍然无法真正落地。

## 10.2 `artifact_ref + request_id + idempotency_key + execution_context` 是统一消息底板
后续任何消息 schema 都不得绕开这一底板另起一套。

## 10.3 `superseded` 和 `WorkflowArchived` 已经变成正式接口能力
这意味着 s07 必须有对应状态迁移，s09 必须有对应验证链。
