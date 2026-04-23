# 《点胶机端到端流程规格说明 s01：阶段输入输出矩阵（修订版）》

本版用于把端到端流程固定为一组**可编排、可审计、可回退、可测试**的阶段节点，并与以下修订口径保持一致：

1. `ExecutionPackageBuilt` 与 `ExecutionPackageValidated` 必须分离，因此 S11 拆为 **S11A 执行包组装** 与 **S11B 离线校验**。
2. 无对位任务不得“隐式跳过补偿层”，必须通过 `AlignmentNotRequired` 产生 `AlignmentCompensation(not_required)`。
3. `PreflightBlocked` 是执行门禁阻断，不是规划失败，不得污染 S5-S11 的产物版本。
4. 首件必须区分 `Passed / Rejected / Waived / NotRequired` 四类结果。
5. 所有回退都必须显式触发下游对象 `ArtifactSuperseded`，而不是只把状态“改回去”。
6. `WorkflowArchived` 由归档 owner 模块产出，不能由编排层伪造。

---

# 1. 阶段输入输出总矩阵

## 1.1 主矩阵

| 阶段 | 阶段名称 | owner 模块 | 进入门槛 | 主要输入 | 核心命令 | 主要输出/事实对象 | 成功/决策事件 | 失败/阻断事件 | 默认处理 |
|---|---|---|---|---|---|---|---|---|---|
| S0 | 任务创建与工艺上下文绑定 | M1（M0 编排） | 尚未存在活动 `job_id` | 产品型号、工件类型、治具版本、材料批次、production baseline、设备型号、操作员、工单号、源文件引用 | `CreateJobCommand` | `JobDefinition` | `JobCreated` | 命令拒绝 + `S0_CTX_xxx` | 停留 S0，补齐上下文 |
| S1 | 文件接收、封存与格式校验 | M1 | `JobDefinition` 已存在 | `job_ref`、原始 DXF | `AttachSourceFileCommand` | `SourceDrawing` | `SourceFileAccepted` | `SourceFileRejected` | 停留 S1，重新接收文件 |
| S2 | DXF 解析与几何标准化 | M2 | `SourceDrawing` 已封存 | `source_drawing_ref` | `ParseSourceDrawingCommand` | `CanonicalGeometry` | `CanonicalGeometryBuilt` | `CanonicalGeometryRejected` | 回到 S1 或留在 S2 处理解析规则 |
| S3 | 几何修复与拓扑重建 | M3 | `CanonicalGeometry` 有效 | `canonical_geometry_ref` | `BuildTopologyCommand` | `TopologyModel` | `TopologyBuilt` | `TopologyRejected` | 回到 S2/S3 |
| S4 | 制造特征提取 | M3 | `TopologyModel` 有效 | `topology_model_ref`、特征规则 | `ExtractFeaturesCommand` | `FeatureGraph` | `FeatureGraphBuilt` | `FeatureClassificationFailed` | 回到 S3/S4 |
| S5 | 工艺规则映射 | M4 | `FeatureGraph` 已就绪 | `feature_graph_ref`、工艺模板、材料/针嘴参数、production baseline 快照 | `PlanProcessCommand` | `ProcessPlan` | `ProcessPlanBuilt` | `ProcessRuleRejected` | 回到 S4/S5 |
| S6 | 坐标体系建立 | M5 | `ProcessPlan` 已就绪 | `process_plan_ref`、治具基准、标定参数、装夹规则 | `ResolveCoordinateTransformsCommand` | `CoordinateTransformSet` | `CoordinateTransformResolved` | 命令失败 + `S6_COORD_xxx` | 停留 S6 或回到 S5 |
| S7 | 对位、针嘴校准与高度补偿 | M5 | `CoordinateTransformSet` 已就绪 | `coordinate_transform_set_ref`、视觉图像、测高数据、校准数据、策略引用 | `RunAlignmentCommand` / `MarkAlignmentNotRequiredCommand` | `AlignmentCompensation` | `AlignmentSucceeded` / `AlignmentNotRequired` | `AlignmentFailed` / `CompensationOutOfRange` | 识别失败可停留 S7 重试；补偿超限回到 S6/S7 |
| S8 | 工艺路径生成 | M6 | `ProcessPlan`、`CoordinateTransformSet`、`AlignmentCompensation` 已就绪 | `process_plan_ref`、`coordinate_transform_set_ref`、`alignment_compensation_ref` | `BuildProcessPathCommand` | `ProcessPath` | `ProcessPathBuilt` | `ProcessPathRejected` / `UncoveredFeatureDetected` | 回到 S5/S8 |
| S9 | 运动轨迹生成 | M7 | `ProcessPath` 已就绪 | `process_path_ref`、设备动态参数、插补策略、机型约束 | `PlanMotionCommand` | `MotionPlan` | `MotionPlanBuilt` | `TrajectoryConstraintViolated` / `MotionPlanRejected` | 回到 S8/S9 |
| S10 | 点胶 I/O 时序生成 | M8 | `MotionPlan` 已就绪 | `motion_plan_ref`、`process_plan_ref`、阀/泵响应参数 | `BuildDispenseTimingPlanCommand` | `DispenseTimingPlan` | `DispenseTimingPlanBuilt` | `DispenseTimingPlanRejected` | 回到 S9/S10 或 S5 |
| S11A | 执行包组装 | M8 | `MotionPlan` 与 `DispenseTimingPlan` 已就绪 | `motion_plan_ref`、`dispense_timing_plan_ref`、production baseline 快照、运行模式 | `AssembleExecutionPackageCommand` | `ExecutionPackage(build_result=built)` | `ExecutionPackageBuilt` | `ExecutionPackageRejected` | 停留 S11A 或回到 S9/S10 |
| S11B | 离线规则校验 | M8 | 已存在 `ExecutionPackage` 且 `build_result=built` | `execution_package_ref`、离线规则集、机型快照 | `ValidateExecutionPackageCommand` | `ExecutionPackage(validation_result=passed/rejected)`、校验报告 | `ExecutionPackageValidated` | `OfflineValidationFailed` / `ExecutionPackageRejected` | 留在 S11B 或回到 S9/S10/S5 |
| S12 | 执行会话创建与设备预检 | M9 | `ExecutionPackageValidated` 已完成 | `execution_package_ref`、实时设备状态、报警状态、fixed-parameter baseline 状态、工件到位状态 | `CreateExecutionSessionCommand`、`RunPreflightCommand` | `ExecutionSession`、`MachineReadySnapshot` | `ExecutionSessionCreated`、`PreflightStarted`、`PreflightPassed` | `PreflightBlocked` | 停留 S12，修复门禁后重试；禁止回退规划层 |
| S13 | 试运行、首件与放行决策 | M9 | `PreflightPassed`，已绑定执行会话 | `execution_package_ref`、`machine_ready_snapshot_ref`、首件策略、审批策略 | `RunPreviewCommand` / `RunDryRunCommand` / `RunFirstArticleCommand` / `ApproveFirstArticleCommand` / `RejectFirstArticleCommand` / `WaiveFirstArticleCommand` | `FirstArticleResult` | `PreviewCompleted`、`DryRunCompleted`、`FirstArticleStarted`、`FirstArticlePassed`、`FirstArticleRejected`、`FirstArticleWaived`、`FirstArticleNotRequired` | `FirstArticleRejected`（失败决策） | 回退到对应规划层，或在授权策略下放行；未放行不得进 S14 |
| S14 | 正式执行 | M9 | `ReadyForProduction` | `execution_package_ref`、`execution_id`、`machine_ready_snapshot_ref` | `StartExecutionCommand`、`PauseExecutionCommand`、`ResumeExecutionCommand` | 运行状态流、段进度、局部结果 | `ExecutionStarted`、`ExecutionPaused`、`ExecutionResumed`、`ExecutionCompleted` | `ExecutionFaulted` / `ExecutionAborted` | 转入 S15 分层或进入终态 |
| S15 | 在线故障分层与恢复/终止决策 | M9（TR 旁路固化） | 执行中存在 fault / alarm / condition loss | `execution_id`、轴状态、I/O 状态、报警、视觉反馈、过程传感器 | `RecoverExecutionCommand` / `AbortExecutionCommand` / `RunPreflightCommand` | `FaultEvent`、恢复决策、检查点信息 | `ExecutionRecovered`、`ExecutionResumed` | `ExecutionFaulted` / `ExecutionAborted` | 可恢复则回到 S14；需门禁重检则回 S12；不可恢复终止 |
| S16 | 执行固化、归档与追溯 | M10 | `Completed / Aborted / Faulted` 已成立 | 执行日志、首件结论、补偿快照、fault 历史、执行上下文 | `FinalizeExecutionCommand`、`ArchiveWorkflowCommand`、`QueryTraceCommand` | `ExecutionRecord`、`TraceLinkSet` | `ExecutionRecordBuilt`、`WorkflowArchived` | `TraceWriteFailed` | 留在 S16 修复追溯系统；不得篡改已发生事实 |

---

## 1.2 阶段输出的硬规则

### 规则 A：每个阶段输出都必须是“对象 + 事件”双闭环
阶段完成不只意味着生成了对象，还必须满足：
1. 产物版本已落地；
2. 成功/决策事件已持久化；
3. 工作流状态已可由事件确认推进。

### 规则 B：S11A 与 S11B 不可合并
- `ExecutionPackageBuilt` 只表示“冻结包组装完成”。
- `ExecutionPackageValidated` 才表示“离线规则通过，可进入执行门禁层”。

### 规则 C：S12 阻断不改写 S5-S11 产物
`PreflightBlocked` 发生时，必须保留：
- `ProcessPlan`
- `ProcessPath`
- `MotionPlan`
- `DispenseTimingPlan`
- `ExecutionPackage(validated)`

### 规则 D：S13 的 4 类首件结果必须都能落对象
`FirstArticleResult.result_status` 必须明确区分：
- `passed`
- `rejected`
- `waived`
- `not_required`

### 规则 E：回退必须伴随 `ArtifactSuperseded`
当回退到较早阶段时，回退目标之后的旧对象必须由各自 owner 显式标为 `superseded`。

### 规则 F：S16 归档由 M10 产出 `WorkflowArchived`
编排层只能消费归档结果，不能把“写了一条日志”当成“归档成功”。

---

# 2. 失败归属层映射

| 阶段范围 | 失败归属层 | 典型问题 | 默认治理方式 |
|---|---|---|---|
| S0-S1 | 任务/文件层 | 上下文缺失、文件损坏、格式非法 | 补输入、重传文件，不进入解析链 |
| S2-S3 | 几何/拓扑层 | 解析失败、自交、闭环歧义、断口修复失败 | 回到 S1/S2/S3，禁止下游补丁兜底 |
| S4-S5 | 特征/工艺层 | 特征冲突、模板缺失、参数越界、工艺未绑定 | 回到 S4/S5，重建工艺相关对象 |
| S6-S7 | 坐标/对位/补偿层 | 基准错误、镜像不明、对位失败、补偿超限 | 回到 S6/S7，必要时连带重建下游 |
| S8 | 工艺路径层 | 路径顺序冲突、关键区污染、特征未覆盖 | 回到 S8 或 S5 |
| S9 | 运动轨迹层 | 轨迹不可达、动态超限、Z 过渡冲突 | 回到 S9 或 S8 |
| S10 | 点胶时序层 | 开关阀越界、起止补偿异常、索引不一致 | 回到 S10 或 S5 |
| S11A-S11B | 执行包与离线规则层 | 冻结包不完整、版本不一致、离线规则失败 | 回到 S11A/S11B 或更早规划层 |
| S12 | 执行门禁层 | 未回零、报警、baseline invalid、工件未到位 | 停留 S12，修门禁，不回退规划层 |
| S13 | 首件质量门 | 空跑错、出胶错、位置偏、审批缺失 | 回退到对应规划/补偿层 |
| S14-S15 | 运行时执行层 | 通信中断、堵针、无胶、安全链触发、恢复失败 | 运行内恢复 / 重新预检 / 中止 |
| S16 | 追溯归档层 | 记录缺失、主链断裂、归档失败 | 停留 S16 修复追溯系统，不改生产事实 |

---

# 3. 回退目标判定表（统一口径）

为与状态机和失败码表对齐，建议所有回退目标使用正式枚举，而不是写自然语言。

| 观测问题 | 首选回退目标 | 次选回退目标 | 不建议回退到 |
|---|---|---|---|
| DXF 无法解析 | `ToSourceAccepted` | 无 | S8 之后 |
| 拓扑闭环/开环判定错误 | `ToGeometryReady` | `ToTopologyReady` | S10 之后 |
| 特征分类错误 | `ToTopologyReady` | `ToFeaturesReady` | S9 之后 |
| 工艺模板缺失/参数不合理 | `ToFeaturesReady` 或 `ToProcessPlanned` | 无 | S2/S3 |
| 镜像/基准映射错误 | `ToProcessPlanned` 或 `ToCoordinatesResolved` | 无 | S9/S10 |
| 对位失败/补偿超限 | `ToCoordinatesResolved` | 无 | S10 之后 |
| 路径顺序异常 | `ToProcessPlanned` 或 `ToProcessPathReady` | 无 | S12 |
| 轨迹动态超限 | `ToProcessPathReady` 或 `ToMotionPlanned` | 无 | S2/S3 |
| 起止点堆胶/拉丝 | `ToTimingPlanned` | `ToProcessPlanned` | S2/S3 |
| 组包字段缺失 | `ToPackageBuilt` | 无 | S12 之后 |
| 离线校验不过 | `ToMotionPlanned` / `ToTimingPlanned` / `ToProcessPlanned` | `ToPackageBuilt` | S12 之后 |
| 设备未 ready | 无回退 | 停留 S12 | 任意规划层 |
| 首件空跑就错 | `ToProcessPathReady` / `ToMotionPlanned` | 无 | `ToTimingPlanned` |
| 首件空跑对、出胶错 | `ToTimingPlanned` | `ToProcessPlanned` | S2/S3 |
| 首件位置偏 | `ToCoordinatesResolved` | `ToProcessPlanned` | `ToTimingPlanned` |
| 运行时轴报警 | 无直接规划回退 | 回 S12 重预检或终止 | S2 |
| 运行时堵针/无胶 | 无直接规划回退 | S12 重预检 / 终止 / 新版本链 | S2 |
| 归档失败 | 无业务回退 | 停留 S16 | 任意规划层 |

---

# 4. 最小日志主链

建议所有阶段至少能拼出下面这条主链：

```text
job_id
 -> source_drawing_ref
 -> canonical_geometry_ref
 -> topology_model_ref
 -> feature_graph_ref
 -> process_plan_ref
 -> coordinate_transform_set_ref
 -> alignment_compensation_ref
 -> process_path_ref
 -> motion_plan_ref
 -> dispense_timing_plan_ref
 -> execution_package_ref
 -> execution_id
 -> execution_record_ref
```

补充要求：
- 发生回退时，必须同时能看到 `rollback_id` 和 `ArtifactSuperseded` 链。
- 发生运行时 fault 时，必须能定位到 `execution_id + segment_index + fault_code`。
- 发生首件失败时，必须能定位到 `first_article_result_ref + rollback_target`。

---

# 5. 这一版最关键的工程结论

## 5.1 端到端阶段必须围绕“对象 + 事件 + 状态”三元组定义
只有对象没有事件，无法审计；只有事件没有对象，无法回链；只有状态没有对象和事件，无法落地。

## 5.2 S11A/S11B 的拆分是执行闭环的基础
不拆分，就会把“已组包”和“已可执行”混为一谈。

## 5.3 `PreflightBlocked` 是门禁问题，不是规划问题
这是整套模板最重要的隔离边界之一。

## 5.4 `FirstArticleRejected` 后必须先回退或人工决策
绝不允许直接 `StartExecutionCommand`。

## 5.5 回退不是“把状态改回去”
而是“显式失效旧对象、重建新版本链、重新进入目标状态”。
