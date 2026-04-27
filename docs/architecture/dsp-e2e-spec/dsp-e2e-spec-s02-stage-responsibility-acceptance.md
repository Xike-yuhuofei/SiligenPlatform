# 《点胶机端到端流程规格说明 s02：阶段职责与验收准则表（修订版）》

Status: Frozen
Authority: Primary Spec Axis S02
Applies To: S0-S16 stage responsibilities and acceptance
Owns / Covers: stage duties, owner handoff, acceptance criteria, no-shortcut rules
Must Not Override: S01 stage I/O; S04 object fields; S05 module interfaces; S07/S08 state and sequence semantics; S09 test layer ownership
Read When: judging whether a stage is complete, blocked, failed, or allowed to continue
Conflict Priority: use S02 for stage responsibility and acceptance; defer object schema to S04 and module boundary to S05
Codex Keywords: stage responsibility, acceptance, owner handoff, blocked, failed, no shortcut, WorkflowArchived

---

## Codex Decision Summary

- 本文裁决每个阶段的职责、验收条件、禁止短路条件与 owner 交接语义。
- 本文不新增对象链或模块边界；对象链以 S04 为准，模块边界以 S05 为准。
- 成功不得等同于函数返回成功；阻断不得等同于失败终止；回退和归档必须显式进入事件与对象生命周期。

---
本版目标是把每个阶段定义成**可开发、可联调、可验收、可追溯**的工程节点，并与以下口径保持统一：

- 与 s04 的对象字典对齐
- 与 s05 的模块边界与接口契约对齐
- 与 s07 的状态机、命令与事件对齐
- 与 s09 的测试矩阵与验收基线对齐

本版特别修正以下高风险点：

1. S11 拆分为 **S11A 执行包组装** 与 **S11B 离线校验**；
2. S7 明确引入 `AlignmentNotRequired` 与 `AlignmentCompensation(not_required)`；
3. S12 明确为执行门禁层，不允许污染上游规划结果；
4. S13 明确四类首件结论：`Passed / Rejected / Waived / NotRequired`；
5. S16 明确 `WorkflowArchived` 由归档 owner 产出，不由编排层伪造。

---

# 1. 阶段通用验收硬规则

## 1.1 每个阶段都必须同时具备 6 类定义
- 阶段职责
- 输入
- 输出
- 核心命令 / 事件
- 成功/失败/阻断判定
- 最低审计字段

## 1.2 成功不等于“函数返回成功”
只有满足以下三项，阶段才算真正成功：
1. owner 对象已生成并版本冻结；
2. 成功/决策事件已持久化；
3. 编排层可由事件确认推进状态。

## 1.3 阻断不等于失败终止
至少要区分：
- 输入阻断
- 规划阻断
- 执行门禁阻断
- 首件质量阻断
- 运行时故障终止

## 1.4 回退必须显式触发 `ArtifactSuperseded`
只要回到较早阶段，目标之后的旧对象必须被 owner 模块显式标记为 `superseded`。

## 1.5 归档必须显式触发 `WorkflowArchived`
`ExecutionRecordBuilt` 不等于 `WorkflowArchived`，两者必须分别发生并可分别追溯。

---

# 2. 阶段职责与验收准则

## S0｜任务创建与工艺上下文绑定

**owner 模块**  
M1（M0 编排）

**阶段职责**  
建立一次完整制造任务，把产品、工件、治具、材料、配方、设备与操作者绑定成统一上下文。

**主要输入**  
产品型号、工件类型、治具版本、材料批次、配方版本、设备型号、操作员、工单号、DXF 文件引用。

**主要输出**  
`JobDefinition`

**核心命令 / 事件**  
- `CreateJobCommand`
- `JobCreated`

**成功判定**  
- 任务主键生成成功
- 所有必填上下文字段齐全
- 所选配方与设备能力兼容
- `JobDefinition` 可作为后续阶段唯一任务上下文事实源

**失败判定**  
- 产品/治具/配方缺失
- 设备能力与任务类型不匹配
- 必填上下文信息不完整

**必须记录**  
- `job_id`
- `request_id`
- `operator_id`
- `product_code`
- `process_template_ref`
- `machine_id`

**验收重点**  
必须明确区分“文件已被引用”和“任务已被建模”两个状态，不允许直接跳过 S0 进入文件接收或解析。

**禁止事项**  
- 不得在 S0 生成任何几何、轨迹、时序对象
- 不得以 UI 本地状态替代 `JobDefinition`

---

## S1｜文件接收、封存与格式校验

**owner 模块**  
M1

**阶段职责**  
接收 DXF 原件，完成封存、哈希、基本格式检查和版本识别。

**主要输入**  
`JobDefinition`、原始 DXF 文件。

**主要输出**  
`SourceDrawing`

**核心命令 / 事件**  
- `AttachSourceFileCommand`
- `SourceFileAccepted`
- `SourceFileRejected`

**成功判定**  
- 文件可读取
- 哈希生成成功
- 文件被稳定封存
- 文件格式被识别为有效 DXF 输入

**失败判定**  
- 文件损坏、空文件、编码异常、结构非法
- 封存失败
- 文件不可追溯

**必须记录**  
- `source_drawing_ref`
- `file_hash`
- `file_name`
- `file_format`
- `blob_ref/storage_uri`

**验收重点**  
同一源文件重复上传时，系统应能识别同源文件；后续阶段不得覆盖原始文件事实。

**禁止事项**  
- 不得在 S1 提前解析几何
- 不得修改 `JobDefinition`

---

## S2｜DXF 解析与几何标准化

**owner 模块**  
M2

**阶段职责**  
把 DXF 图元解析为系统统一几何表达，并统一单位、方向和基础坐标语义。

**主要输入**  
`SourceDrawing`

**主要输出**  
`CanonicalGeometry`

**核心命令 / 事件**  
- `ParseSourceDrawingCommand`
- `CanonicalGeometryBuilt`
- `CanonicalGeometryRejected`

**成功判定**  
- 受支持实体全部解析成功
- 每个几何对象具有明确类型、方向、单位
- 不支持实体被显式记录

**失败判定**  
- 关键实体无法解析
- 单位不明
- 图元退化导致无法构造规范对象
- Block/Insert 展开失败

**必须记录**  
- `canonical_geometry_ref`
- `source_drawing_ref`
- `entity_count`
- `unsupported_entities`
- `unit_mode`

**验收重点**  
解析层只负责“读懂图纸并标准化”，不得混入拓扑修复、工艺语义或设备命令。

**禁止事项**  
- 不得在 S2 做工艺分类
- 不得在 S2 注入设备动态参数

---

## S3｜几何修复与拓扑重建

**owner 模块**  
M3

**阶段职责**  
对规范几何做制造导向修复，建立稳定拓扑关系。

**主要输入**  
`CanonicalGeometry`

**主要输出**  
`TopologyModel`

**核心命令 / 事件**  
- `BuildTopologyCommand`
- `TopologyBuilt`
- `TopologyRejected`

**成功判定**  
- 可识别开环/闭环
- 关键连通关系稳定
- 重复段、零长度段、近似断口等按规则处理

**失败判定**  
- 自交无法解释
- 断口拼接歧义过大
- 吸附/修复后拓扑失真
- 闭环关系不稳定

**必须记录**  
- `topology_model_ref`
- `canonical_geometry_ref`
- `repair_actions`
- `connectivity_graph_ref`

**验收重点**  
必须能区分“解析正确但拓扑不可制造”和“解析本身失败”两类问题。

**禁止事项**  
- 不得将拓扑问题丢给特征层兜底
- 不得在 S3 输出工艺对象

---

## S4｜制造特征提取

**owner 模块**  
M3

**阶段职责**  
从拓扑对象中提取制造特征，而不是停留在线段级别。

**主要输入**  
`TopologyModel`、特征识别规则。

**主要输出**  
`FeatureGraph`

**核心命令 / 事件**  
- `ExtractFeaturesCommand`
- `FeatureGraphBuilt`
- `FeatureClassificationFailed`

**成功判定**  
- 主要轮廓、开放胶线、封闭胶圈、点列区域、禁胶区等可被唯一分类
- 特征间关系可追踪
- 无未分类关键特征

**失败判定**  
- 特征分类冲突
- 区域语义不清
- 存在未分类特征
- 禁胶区/点胶区重叠

**必须记录**  
- `feature_graph_ref`
- `feature_count`
- `unclassified_regions`
- `conflicts`

**验收重点**  
系统必须能解释“某条几何为什么被判成某类工艺特征”，而不只是给出分类结果。

**禁止事项**  
- 不得直接给出速度、阀延时等工艺参数
- 不得把未分类特征带病进入 S5

---

## S5｜工艺规则映射

**owner 模块**  
M4

**阶段职责**  
把制造特征映射成具体点胶工艺动作和参数。

**主要输入**  
`FeatureGraph`、工艺模板、材料参数、针嘴参数、配方快照。

**主要输出**  
`ProcessPlan`

**核心命令 / 事件**  
- `PlanProcessCommand`
- `ProcessPlanBuilt`
- `ProcessRuleRejected`

**成功判定**  
- 每个特征都绑定了明确工艺模式、目标参数、起止补偿和优先级
- 无未解释特征
- 工艺模板版本可追溯

**失败判定**  
- 工艺模板缺失
- 参数越界
- 材料/针嘴与工艺模式不兼容
- 起止补偿未定义

**必须记录**  
- `process_plan_ref`
- `feature_graph_ref`
- `material_profile_ref`
- `process_template_ref`

**验收重点**  
工艺层必须显式产出“为什么这样点胶”，不能把参数散落到轨迹层、时序层或设备层。

**禁止事项**  
- 不得直接输出轨迹
- 不得访问设备实时 ready 状态

---

## S6｜坐标体系建立

**owner 模块**  
M5

**阶段职责**  
建立设计坐标、工艺坐标、治具坐标、机器坐标之间的静态变换链。

**主要输入**  
`ProcessPlan`、治具基准、设备标定参数、装夹规则。

**主要输出**  
`CoordinateTransformSet`

**核心命令 / 事件**  
- `ResolveCoordinateTransformsCommand`
- `CoordinateTransformResolved`

**成功判定**  
- 所有工艺对象都能稳定映射到设备工作空间
- 平移、旋转、镜像规则明确
- 变换链可追溯

**失败判定**  
- 基准缺失
- 坐标变换后超行程
- 左右件/镜像方向不明确
- 标定参数非法

**必须记录**  
- `coordinate_transform_set_ref`
- `process_plan_ref`
- `fixture_profile_ref`
- `calibration_profile_ref`

**验收重点**  
必须把“图纸在哪”和“机器去哪”分开建模，不允许默认 DXF 坐标直接等于机器坐标。

**禁止事项**  
- 不得在 S6 写入动态视觉补偿
- 不得修改 `ProcessPlan`

---

## S7｜视觉对位、针嘴校准与高度补偿

**owner 模块**  
M5

**阶段职责**  
对实际工件位置和工具偏置做执行前补偿；对于无需补偿场景，显式生成 `AlignmentCompensation(not_required)`。

**主要输入**  
`CoordinateTransformSet`、视觉图像、对位规则、测高数据、针嘴校准数据、策略引用。

**主要输出**  
`AlignmentCompensation`

**核心命令 / 事件**  
- `RunAlignmentCommand`
- `MarkAlignmentNotRequiredCommand`
- `AlignmentSucceeded`
- `AlignmentNotRequired`
- `AlignmentFailed`
- `CompensationOutOfRange`

**成功判定**  
- 对位成功，或已被策略明确判定为 `not_required`
- 补偿量在阈值内
- 针嘴/相机偏置有效
- 高度补偿可用或被正确声明为不需要

**失败判定**  
- Fiducial 识别失败
- 补偿超限
- 针嘴校准失败
- 测高异常
- `not_required` 场景缺失策略依据

**必须记录**  
- `alignment_compensation_ref`
- `coordinate_transform_set_ref`
- `compensation_mode`
- `compensation_result`
- `not_required_reason / failure_code`

**验收重点**  
无对位任务不能靠“传空值”绕过本阶段，必须产出正式对象供 S8 强引用。

**禁止事项**  
- 不得把运行时偏置写回源图或工艺模板
- 不得直接生成路径或轨迹

---

## S8｜工艺路径生成

**owner 模块**  
M6

**阶段职责**  
按工艺语义组织路径顺序、起点选择、连线策略和空跑跨越。

**主要输入**  
`ProcessPlan`、`CoordinateTransformSet`、`AlignmentCompensation`

**主要输出**  
`ProcessPath`

**核心命令 / 事件**  
- `BuildProcessPathCommand`
- `ProcessPathBuilt`
- `ProcessPathRejected`
- `UncoveredFeatureDetected`

**成功判定**  
- 所有特征均被纳入路径
- 路径顺序满足工艺优先级
- 出胶段与空跑段区分明确
- `alignment_compensation_ref` 已绑定

**失败判定**  
- 存在不可达连接
- 顺序违反工艺约束
- 空跑策略污染关键区域
- 有特征未纳入路径

**必须记录**  
- `process_path_ref`
- `process_plan_ref`
- `alignment_compensation_ref`
- `path_segment_count`

**验收重点**  
该阶段输出的是“应该怎么走”，不是“电机最终怎么加减速去走”。

**禁止事项**  
- 不得写速度/加速度/jerk
- 不得生成阀时序

---

## S9｜运动轨迹生成

**owner 模块**  
M7

**阶段职责**  
把工艺路径转换成满足设备动态约束的可执行运动轨迹。

**主要输入**  
`ProcessPath`、轴组约束、动态参数、插补策略。

**主要输出**  
`MotionPlan`

**核心命令 / 事件**  
- `PlanMotionCommand`
- `MotionPlanBuilt`
- `TrajectoryConstraintViolated`
- `MotionPlanRejected`

**成功判定**  
- 轨迹连续可执行
- 动态参数在设备能力内
- 抬刀/落刀、连接段和插补段完整

**失败判定**  
- 轨迹动态超限
- 局部路径不可行
- Z 过渡冲突
- 当前路径不支持所选插补模式

**必须记录**  
- `motion_plan_ref`
- `process_path_ref`
- `dynamic_profile_ref`
- `interpolation_modes`

**验收重点**  
轨迹层必须独立持有速度、加速度、jerk、插补方式等信息，不得隐式塞回工艺层。

**禁止事项**  
- 不得直接埋阀开关逻辑
- 不得读写设备实时报警

---

## S10｜点胶 I/O 时序生成

**owner 模块**  
M8

**阶段职责**  
生成与轨迹绑定的阀、泵、回抽、等待、预开、预关等工艺时序。

**主要输入**  
`MotionPlan`、`ProcessPlan`、阀/泵响应参数。

**主要输出**  
`DispenseTimingPlan`

**核心命令 / 事件**  
- `BuildDispenseTimingPlanCommand`
- `DispenseTimingPlanBuilt`
- `DispenseTimingPlanRejected`

**成功判定**  
- 每个出胶段存在明确触发点
- 空跑段无误触发
- 起止补偿逻辑完整
- 时序索引与运动段一致

**失败判定**  
- 开阀/关阀点越界
- 时序与段索引不一致
- 阀响应参数缺失
- 起止策略明显不合理

**必须记录**  
- `dispense_timing_plan_ref`
- `motion_plan_ref`
- `process_plan_ref`
- `timing_event_count`

**验收重点**  
必须把“走轨迹”和“什么时候出胶”分成两个对象。

**禁止事项**  
- 不得修改轨迹几何
- 不得重排 `ProcessPath`

---

## S11A｜执行包组装

**owner 模块**  
M8

**阶段职责**  
将轨迹、时序、配方、运行模式等冻结为一次待校验执行包。

**主要输入**  
`MotionPlan`、`DispenseTimingPlan`、配方快照、运行模式、机型快照。

**主要输出**  
`ExecutionPackage(build_result=built)`

**核心命令 / 事件**  
- `AssembleExecutionPackageCommand`
- `ExecutionPackageBuilt`
- `ExecutionPackageRejected`

**成功判定**  
- 执行包结构完整
- 关键引用版本被冻结
- 包 hash 可计算并可追溯

**失败判定**  
- 轨迹与时序版本不一致
- 包字段缺失
- 配方/模式上下文不完整

**必须记录**  
- `execution_package_ref`
- `motion_plan_ref`
- `dispense_timing_plan_ref`
- `package_hash`
- `run_mode`

**验收重点**  
S11A 只代表“包已组装”，不代表“包已离线验证通过”。

**禁止事项**  
- 不得把 S11A 成功当成可直接执行
- 不得跳过 S11B

---

## S11B｜离线规则校验

**owner 模块**  
M8

**阶段职责**  
对已组装执行包做越界、干涉、配方一致性、估算等离线规则校验。

**主要输入**  
`ExecutionPackage(build_result=built)`、离线规则集、机型约束。

**主要输出**  
`ExecutionPackage(validation_result=passed/rejected)`、校验报告。

**核心命令 / 事件**  
- `ValidateExecutionPackageCommand`
- `ExecutionPackageValidated`
- `OfflineValidationFailed`
- `ExecutionPackageRejected`

**成功判定**  
- 离线规则全部通过
- `validated_at / validated_by / offline_rule_profile_ref` 完整
- 允许进入执行门禁层

**失败判定**  
- 越界、干涉、规则失配
- 包哈希或版本不一致
- 胶量/节拍异常达到阻断阈值

**必须记录**  
- `execution_package_ref`
- `validation_result`
- `offline_rule_profile_ref`
- `reject_reasons / failure_code`

**验收重点**  
`ExecutionPackageValidated` 不能被 `PreflightPassed` 代替，两者含义完全不同。

**禁止事项**  
- 不得把设备 ready 检查混到 S11B
- 不得在校验阶段偷偷改包后不升版本

---

## S12｜执行会话创建与设备预检

**owner 模块**  
M9

**阶段职责**  
创建执行会话并检查设备是否具备执行条件。

**主要输入**  
`ExecutionPackage`、实时设备状态、报警状态、recipe 状态、工件到位状态。

**主要输出**  
`ExecutionSession`、`MachineReadySnapshot`

**核心命令 / 事件**  
- `CreateExecutionSessionCommand`
- `RunPreflightCommand`
- `ExecutionSessionCreated`
- `PreflightStarted`
- `PreflightPassed`
- `PreflightBlocked`

**成功判定**  
- 会话创建成功
- 伺服、回零、安全链、气压、工件、配方、材料状态全部满足门禁
- `MachineReadySnapshot.ready_result = passed`

**失败/阻断判定**  
- 任一关键就绪条件不满足
- 存在活动报警
- 设备不在合法模式
- `MachineReadySnapshot.ready_result = blocked`

**必须记录**  
- `execution_id`
- `execution_package_ref`
- `machine_ready_snapshot_ref`
- `failure_code / blocking_reasons`

**验收重点**  
这是门禁层，不是规划层。设备未 ready 不应触发重算轨迹或工艺。

**禁止事项**  
- 不得把 `PreflightBlocked` 解释为 `ExecutionPackage` 失效
- 不得修改 S5-S11 产物版本

---

## S13｜试运行、首件与放行决策

**owner 模块**  
M9

**阶段职责**  
通过预览、空跑、低速首件及审批，确认是否允许进入正式执行。

**主要输入**  
`ExecutionPackage`、`MachineReadySnapshot`、首件策略、审批策略。

**主要输出**  
`FirstArticleResult`

**核心命令 / 事件**  
- `RunPreviewCommand`
- `RunDryRunCommand`
- `RunFirstArticleCommand`
- `ApproveFirstArticleCommand`
- `RejectFirstArticleCommand`
- `WaiveFirstArticleCommand`
- `PreviewCompleted`
- `DryRunCompleted`
- `FirstArticleStarted`
- `FirstArticlePassed`
- `FirstArticleRejected`
- `FirstArticleWaived`
- `FirstArticleNotRequired`

**成功/决策判定**  
- 预览与空跑正确
- 首件通过，或按授权流程豁免，或策略明确无需首件
- `FirstArticleResult` 条件必填字段完整

**失败判定**  
- 空跑路径错误
- 首件堆胶、断胶、拉丝、位置偏
- 审批缺失或豁免条件不满足

**必须记录**  
- `first_article_result_ref`
- `execution_package_ref`
- `machine_ready_snapshot_ref`
- `result_status`
- `failure_code / rollback_target / approval_ref`

**验收重点**  
必须强制区分：
- 空跑就错
- 空跑对、出胶错
- 位置偏
- 豁免 / 无需首件

**禁止事项**  
- `FirstArticleRejected` 后不得直接 `StartExecutionCommand`
- 不得只输出 `FirstArticleCompleted` 这种粗粒度结果

---

## S14｜正式执行

**owner 模块**  
M9

**阶段职责**  
按执行包驱动设备完成正式点胶任务，并维护执行状态机。

**主要输入**  
`ExecutionPackage`、`MachineReadySnapshot`、`execution_id`

**主要输出**  
运行状态流、段执行进度、局部结果。

**核心命令 / 事件**  
- `StartExecutionCommand`
- `PauseExecutionCommand`
- `ResumeExecutionCommand`
- `ExecutionStarted`
- `ExecutionPaused`
- `ExecutionResumed`
- `ExecutionCompleted`
- `ExecutionFaulted`
- `ExecutionAborted`

**成功判定**  
- 按执行包完成任务
- 暂停/恢复逻辑正确
- 段索引、工艺对象与运行进度一致

**失败判定**  
- 运行中断
- 段索引错乱
- 设备故障
- 通信中断
- 执行包一致性失配

**必须记录**  
- `execution_id`
- `segment_index`
- `execution_package_ref`
- `checkpoint_ref`
- `runtime_state`

**验收重点**  
正式执行只能消费冻结包，不能在运行中临时改规划语义。

**禁止事项**  
- 不得在运行时改写 `ExecutionPackage`
- 不得重复 `StartExecutionCommand` 触发二次启动

---

## S15｜在线故障分层与恢复/终止决策

**owner 模块**  
M9（TR 旁路记录 fault）

**阶段职责**  
实时监控运动、工艺、对位、控制等故障，并给出恢复或终止决策。

**主要输入**  
实时轴状态、I/O 状态、报警、视觉反馈、过程传感器、检查点信息。

**主要输出**  
`FaultEvent`、恢复决策、检查点更新、终止决策。

**核心命令 / 事件**  
- `RecoverExecutionCommand`
- `AbortExecutionCommand`
- `RunPreflightCommand`（若恢复前需重检）
- `ExecutionRecovered`
- `ExecutionResumed`
- `ExecutionFaulted`
- `ExecutionAborted`

**成功判定**  
- 故障被正确分层
- 可恢复与不可恢复故障区分明确
- 恢复前置条件可验证

**失败判定**  
- 故障无法分类
- 无稳定检查点
- 恢复条件不满足
- 安全类故障仍试图自动恢复

**必须记录**  
- `fault_code`
- `execution_id`
- `segment_index`
- `retryable`
- `recommended_action`
- `checkpoint_ref`

**验收重点**  
至少要区分：
- 可恢复继续执行
- 需重新预检再恢复
- 必须终止

**禁止事项**  
- 不得把运行时 fault 直接当作上游规划失败
- 不得对不可恢复 fault 直接 `ResumeExecution`

---

## S16｜执行固化、归档与追溯

**owner 模块**  
M10

**阶段职责**  
固化一次任务的执行结果、关键快照、异常记录和追溯数据，并执行正式归档。

**主要输入**  
最终执行状态、首件结论、补偿快照、fault 历史、运行日志、执行上下文。

**主要输出**  
`ExecutionRecord`、`TraceLinkSet`

**核心命令 / 事件**  
- `FinalizeExecutionCommand`
- `ArchiveWorkflowCommand`
- `QueryTraceCommand`
- `ExecutionRecordBuilt`
- `WorkflowArchived`
- `TraceWriteFailed`

**成功判定**  
- 可查询、可复盘、可关联到源文件、工艺、轨迹、时序和设备状态
- 归档时间、归档人、最终状态可追溯

**失败判定**  
- 关键记录缺失
- 结果与包版本无法关联
- 日志主链断裂
- 归档写入失败

**必须记录**  
- `execution_record_ref`
- `execution_package_ref`
- `workflow_final_state`
- `archived_at`
- `archived_by`

**验收重点**  
`ExecutionRecordBuilt` 与 `WorkflowArchived` 必须分开验证，归档层只负责固化事实，不得修正事实。

**禁止事项**  
- 不得回写上游对象版本
- 不得用追溯结果替代业务事实对象

---

# 3. 跨阶段验收硬约束

## 3.1 对象链一致性
必须满足：
- `SourceDrawing -> CanonicalGeometry -> TopologyModel -> FeatureGraph -> ProcessPlan -> CoordinateTransformSet -> AlignmentCompensation -> ProcessPath -> MotionPlan -> DispenseTimingPlan -> ExecutionPackage -> MachineReadySnapshot / FirstArticleResult -> ExecutionRecord`
- 任一节点缺失都不得伪造下游成功

## 3.2 事件链一致性
至少以下事件必须在链路上真实出现：
- `ExecutionPackageBuilt`
- `ExecutionPackageValidated`
- `PreflightPassed / PreflightBlocked`
- `FirstArticlePassed / Rejected / Waived / NotRequired`
- `ExecutionCompleted / Faulted / Aborted`
- `WorkflowArchived`

## 3.3 回退一致性
发生回退时必须同时满足：
- `RollbackRequested`
- `RollbackApproved / Rejected`
- `ArtifactSuperseded`
- `RollbackPerformed`
- 新版本对象重建完成

## 3.4 门禁一致性
必须满足：
- `PreflightBlocked` 不失效规划对象
- `FirstArticleRejected` 不直接进入 `Faulted`
- `WorkflowArchived` 只能从 `Completed / Aborted / Faulted` 迁移而来

---

# 4. 这版文档对研发拆任务的直接用法

每个阶段都可以直接拆成一组实现任务：

1. 输入结构定义  
2. 输出对象定义  
3. 命令 handler  
4. 成功事件实现  
5. 失败/阻断事件实现  
6. 最低审计字段实现  
7. 单阶段测试  
8. 跨阶段对接测试  

这比只按“做个 DXF 模块 / 做个轨迹模块”拆任务更稳，因为它天然带验收口径。
