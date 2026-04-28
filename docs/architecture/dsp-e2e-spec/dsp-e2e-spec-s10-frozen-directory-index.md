# 《点胶机端到端流程规格说明：全套冻结版目录索引 v1.0》

Status: Frozen
Authority: Primary Spec Axis S10 Index
Applies To: dsp-e2e-spec directory navigation and cross-document consistency
Owns / Covers: reading order, consistency axes, formal axis list, supplement standard list
Must Not Override: S01-S09 domain rules; supplement standards cannot override S01-S10
Read When: choosing which frozen document to read first, validating cross-document consistency, resolving docset scope
Conflict Priority: use S10 for index and navigation only; defer domain decisions to the referenced authority document
Codex Keywords: frozen index, S01-S10, supplement standard, consistency axis, reading order, authority routing

---

## Codex Decision Summary

- 本文裁决冻结目录的正式轴、补充标准、阅读顺序与一致性主轴。
- 本文不裁决具体对象字段、模块 owner、状态机或测试细节；这些必须回到对应 S 轴文档。
- 不得继续使用旧数量口径；正式结构统一为 S01-S10 正式规格轴加冻结补充标准。

---
适用范围：
- 点胶机端到端方案架构设计
- 规划链 / 执行链 / 追溯链协同设计
- 模块拆分、接口定义、状态机设计、测试设计、联调与验收
- 项目启动、方案评审、研发拆解、测试冻结、上线前审查

本索引对应的正式结构为：`S01-S10` 正式规格轴 + 冻结补充标准。

`S01-S10` 正式规格轴：

1. s01《阶段输入输出矩阵（修订版）》
2. s02《阶段职责与验收准则表（修订版）》
3. s03《阶段失败码与回退策略表（修订版）》
4. s04《阶段产物数据字典（对象清单，修订版）》
5. s05《模块边界与接口契约表（修订版）》
6. s06《仓库目录结构建议（单仓多模块模板，修订版）》
7. s07《状态机与命令总线模板（修订版）》
8. s08《系统时序图模板（修订版）》
9. s09《测试矩阵与验收基线（修订版）》
10. s10《全套冻结版目录索引》

冻结补充标准：

1. `project-chain-standard-v1.md`
   用于冻结项目内正式对象链、当前 live 控制链与观测支撑链的统一定义，作为 `S01/S02/S05/S07` 的横向收敛标准，不单独占用新的 `S` 轴编号。
2. `internal_execution_contract_v_1.md`
   用于冻结内部执行契约与执行语义边界。
3. `dxf-input-governance-standard-v1.md`
   用于冻结 DXF 输入治理、质量门禁、诊断报告与拒绝/修复边界，作为 `S01/S02/S04/S09` 的专项补充标准。

---

# 1. 整体定位

`S01-S10` 不是彼此独立的说明材料，而是一套必须联动使用的“架构冻结模板”。

其中 `S01-S09` 共同回答 9 个领域问题，`S10` 负责目录索引、阅读顺序与一致性主轴：

- s01：系统一共有哪些阶段，每阶段输入输出是什么
- s02：每阶段到底负责什么，什么算通过，什么算失败
- s03：失败了如何归类，如何回退，哪些能重试，哪些绝不能放行
- s04：系统里有哪些核心对象，每个对象的边界、字段、引用和生命周期是什么
- s05：模块怎么切，owner 是谁，接口怎么定义，越权边界在哪里
- s06：代码仓库和目录怎么落，依赖如何管控
- s07：状态机怎么设计，命令/事件怎么闭环，幂等和归档怎么统一
- s08：端到端时序怎么串，主链、回退链、阻断链、恢复链怎么表现
- s09：测试怎么覆盖，验收怎么定，回归怎么防漂移

---

# 2. 文档使用顺序（推荐）

## 2.1 首次阅读顺序

建议团队首次阅读按以下顺序：

### 第 1 组：先建立系统骨架
1. s01 阶段输入输出矩阵
2. s04 阶段产物数据字典
3. s05 模块边界与接口契约表

目的：
先看清楚：
- 阶段链
- 对象链
- 模块链

### 第 2 组：再冻结控制与协作方式
4. s07 状态机与命令总线模板
5. s08 系统时序图模板
6. s03 阶段失败码与回退策略表

目的：
再看清楚：
- 状态怎么迁移
- 命令和事件怎么闭环
- 失败怎么分层
- 回退怎么落地

### 第 3 组：最后落实工程与质量
7. s02 阶段职责与验收准则表
8. s09 测试矩阵与验收基线
9. s06 仓库目录结构建议

目的：
最后落到：
- 怎么拆任务
- 怎么测
- 怎么建仓库
- 怎么真正推进联调与交付

---

# 3. S01-S10 的一致性主轴

整套模板必须围绕以下 8 条主轴保持一致，不允许任一文档单独变更口径。

## 3.1 主轴 A：阶段链一致
统一阶段为：

- S0 任务创建与工艺上下文绑定
- S1 文件接收、封存与格式校验
- S2 DXF 解析与几何标准化
- S3 几何修复与拓扑重建
- S4 制造特征提取
- S5 工艺规则映射
- S6 坐标体系建立
- S7 视觉对位、针嘴校准与高度补偿
- S8 工艺路径生成
- S9 运动轨迹生成
- S10 点胶 I/O 时序生成
- S11A 执行包组装
- S11B 离线规则校验
- S12 执行会话创建与设备预检
- S13 试运行、首件与放行决策
- S14 正式执行
- S15 在线故障分层与恢复/终止决策
- S16 执行固化、归档与追溯

说明：
- S11 必须拆为 S11A / S11B
- 不允许任何文档再把 S11 合并成一个模糊阶段

## 3.2 主轴 B：对象链一致
统一核心对象链为：

- JobDefinition
- SourceDrawing
- CanonicalGeometry
- TopologyModel
- FeatureGraph
- ProcessPlan
- CoordinateTransformSet
- AlignmentCompensation
- ProcessPath
- MotionPlan
- DispenseTimingPlan
- ExecutionPackage
- MachineReadySnapshot
- FirstArticleResult
- ExecutionRecord

说明：
- 无对位场景也必须生成 `AlignmentCompensation(not_required)`
- 不允许通过“传空引用”跳过对象链

## 3.3 主轴 C：模块 owner 一致
统一 owner 模块为：

- M0 workflow
- M1 job-ingest
- M2 dxf-geometry
- M3 topology-feature
- M4 process-planning
- M5 coordinate-alignment
- M6 process-path
- M7 motion-planning
- M8 dispense-packaging
- M9 runtime-execution
- M10 trace-diagnostics
- M11 hmi-application

说明：
- 每个核心对象必须有唯一 owner
- 不允许两个模块共同持有同一对象事实源

## 3.4 主轴 D：关键事件集一致
统一关键事件至少包括：

### 规划链成功事件
- SourceFileAccepted
- CanonicalGeometryBuilt
- TopologyBuilt
- FeatureGraphBuilt
- ProcessPlanBuilt
- CoordinateTransformResolved
- AlignmentSucceeded
- AlignmentNotRequired
- ProcessPathBuilt
- MotionPlanBuilt
- DispenseTimingPlanBuilt
- ExecutionPackageBuilt
- ExecutionPackageValidated

### 规划链失败事件
- SourceFileRejected
- CanonicalGeometryRejected
- TopologyRejected
- FeatureClassificationFailed
- ProcessRuleRejected
- AlignmentFailed
- CompensationOutOfRange
- ProcessPathRejected
- TrajectoryConstraintViolated
- DispenseTimingPlanRejected
- ExecutionPackageRejected
- OfflineValidationFailed

### 执行链事件
- ExecutionSessionCreated
- MachineReadySnapshotBuilt
- PreflightStarted
- PreflightBlocked
- PreflightPassed
- PreviewStarted
- PreviewCompleted
- DryRunStarted
- DryRunCompleted
- FirstArticleStarted
- FirstArticlePassed
- FirstArticleRejected
- FirstArticleWaived
- FirstArticleNotRequired
- ExecutionStarted
- ExecutionPaused
- ExecutionResumed
- ExecutionRecovered
- ExecutionCompleted
- ExecutionAborted
- ExecutionFaulted

### 回退与归档事件
- RollbackRequested
- RollbackApproved
- RollbackRejected
- ArtifactSuperseded
- RollbackPerformed
- ExecutionRecordBuilt
- WorkflowArchived

## 3.5 主轴 E：关键状态机语义一致
必须统一以下核心状态语义：

- `PackageBuilt` != `PackageValidated`
- `PreflightBlocked` != `Faulted`
- `FirstArticleRejected` != `ExecutionFaulted`
- `ExecutionRecordBuilt` != `WorkflowArchived`

## 3.6 主轴 F：失败码与回退目标一致
所有失败码必须：
- 能映射到阶段
- 能映射到失败类别
- 能映射到推荐动作
- 能映射到正式 `rollback_target` 枚举

## 3.7 主轴 G：统一消息底板一致
统一公共字段：
- artifact_ref
- request_id
- idempotency_key
- correlation_id
- causation_id
- execution_context

## 3.8 主轴 H：测试与验收口径一致
必须统一验证：
- 对象
- 事件
- 状态
- 失败码
- 回退目标
- 版本链
- superseded / archived

---

# 4. 每份文档的角色定义

## 4.1 s01《阶段输入输出矩阵》
角色：
- 端到端流程总目录
- 用于回答“系统有哪些阶段、每阶段输入输出是什么”

适合使用者：
- 架构师
- 项目经理
- 研发负责人
- 联调负责人

它不负责：
- 详细字段定义
- 模块内部实现细节
- 具体测试步骤

---

## 4.2 s02《阶段职责与验收准则表》
角色：
- 每阶段的职责说明书
- 用于回答“该阶段到底负责什么、什么算通过”

适合使用者：
- 模块 owner
- 测试负责人
- 审查人

它不负责：
- 全局失败码体系
- 代码目录结构

---

## 4.3 s03《阶段失败码与回退策略表》
角色：
- 失败分层与回退主规范
- 用于回答“失败了怎么办、能不能重试、退到哪一层”

适合使用者：
- 控制架构设计者
- 运行时设计者
- 测试与异常处理设计者
- HMI 告警与审批设计者

它不负责：
- 对象字段设计
- 具体仓库目录

---

## 4.4 s04《阶段产物数据字典》
角色：
- 对象事实源总表
- 用于回答“系统到底有哪些核心对象、字段、引用和状态”

适合使用者：
- 领域建模人员
- 接口设计者
- 存储模型设计者
- 追溯系统设计者

它不负责：
- 模块交互流程
- 测试覆盖策略

---

## 4.5 s05《模块边界与接口契约表》
角色：
- 模块切分与接口规范
- 用于回答“模块怎么拆、谁是 owner、命令/事件怎么定义”

适合使用者：
- 架构师
- 服务边界设计者
- 联调负责人

它不负责：
- 代码具体目录
- 用例细节

---

## 4.6 s06《仓库目录结构建议》
角色：
- 工程落地模板
- 用于回答“仓库怎么建、依赖怎么控、共享层怎么防腐化”

适合使用者：
- Tech Lead
- 平台/基础架构工程师
- 仓库治理负责人

它不负责：
- 业务状态机语义
- 失败码规则

---

## 4.7 s07《状态机与命令总线模板》
角色：
- 控制平面主规范
- 用于回答“状态怎么迁移、命令和事件如何闭环、幂等怎么做”

适合使用者：
- 工作流编排设计者
- 运行时控制设计者
- 平台中间件设计者

它不负责：
- 每个对象的完整字段定义
- 具体 DXF 解析算法

---

## 4.8 s08《系统时序图模板》
角色：
- 协作链路模板
- 用于回答“模块之间如何按时间顺序协作”

适合使用者：
- 联调负责人
- 研发负责人
- 测试架构师

它不负责：
- 失败码全表
- 目录治理细则

---

## 4.9 s09《测试矩阵与验收基线》
角色：
- 质量冻结主规范
- 用于回答“怎么测、测什么、什么算通过、什么阻断上线”

适合使用者：
- QA
- SDET
- 验收负责人
- 项目经理

它不负责：
- 模块内部代码结构

---

# 5. 文档之间的引用关系

建议固定以下依赖关系，不要交叉混乱引用。

## 5.1 第一层：基础事实
- s01
- s04
- s05

## 5.2 第二层：控制与协作
- s03
- s07
- s08

## 5.3 第三层：实施与质量
- s02
- s06
- s09

## 5.4 推荐引用规则
- s02 引阶段编号时，引用 s01
- s03 引失败归属层时，引用 s01 / s02 / s07
- s05 引对象 owner 时，引用 s04
- s07 引回退目标与失败字段时，引用 s03
- s08 引命令/事件/状态时，引用 s05 / s07
- s09 引对象、事件、状态和失败码时，引用全部前述文档

---

# 6. 团队协作建议分工

## 6.1 架构组
主责：
- s01
- s04
- s05
- s07
- s08

## 6.2 控制/运行时组
主责：
- s03
- s07
- s08
- s09 中 S12-S16

## 6.3 规划算法组
主责：
- s02 中 S2-S11
- s04
- s05
- s09 中 S2-S11

## 6.4 测试与质量组
主责：
- s02
- s03
- s09

## 6.5 平台/工程效能组
主责：
- s06
- s07 中总线/事件存储部分
- s09 中 contract-lint / regression 工具链

---

# 7. 冻结版管理规则

## 7.1 冻结级别
建议定义 3 个级别：

### L0 草稿
允许变更：
- 阶段
- 对象
- 接口
- 状态机
- 失败码
- 仓库结构

### L1 方案冻结
不再允许随意改：
- 阶段编号
- 核心对象名
- 核心事件名
- 关键回退目标枚举

允许变更：
- 字段补充
- 用例补充
- 说明优化

### L2 联调冻结
不再允许随意改：
- 命令/事件 schema
- owner 模块归属
- 状态迁移主链
- 失败码语义
- 测试基线

只允许：
- 向后兼容补充
- 文案/注释修正
- 新增不破坏旧语义的辅助字段

## 7.2 当前建议冻结状态
建议当前整套文档作为：

**L1.5：方案已收敛，允许补字段，不建议再改主链**

也就是：
- 可以再细化字段和测试
- 不建议再改阶段主链、核心对象、核心事件和回退主规则

---

# 8. 真正必须统一冻结的关键词

以下词汇一旦进入研发联调，不建议再变：

## 8.1 阶段名
- S11A
- S11B
- S12
- S13
- S15
- S16

## 8.2 对象名
- AlignmentCompensation
- ExecutionPackage
- MachineReadySnapshot
- FirstArticleResult
- ExecutionRecord

## 8.3 关键事件名
- ExecutionPackageBuilt
- ExecutionPackageValidated
- PreflightBlocked
- PreflightPassed
- FirstArticlePassed
- FirstArticleRejected
- FirstArticleWaived
- FirstArticleNotRequired
- ArtifactSuperseded
- WorkflowArchived

## 8.4 关键回退目标
- ToCoordinatesResolved
- ToProcessPathReady
- ToMotionPlanned
- ToTimingPlanned
- ToPackageBuilt
- ToPackageValidated

---

# 9. 交付团队时的建议附件清单

建议把 `S01-S10` 正式规格轴与冻结补充标准连同以下附件一起交付：

1. 一页式总览图  
   内容：阶段链 + 模块链 + 对象链

2. 命令/事件总表  
   从 s05 + s07 抽出一张总表

3. 失败码速查表  
   从 s03 抽出一张速查表

4. 金样本目录索引  
   从 s09 抽出一张样本总表

5. 联调检查清单  
   建议包括：
   - 包已组装？
   - 包已校验？
   - 预检通过？
   - 首件结论明确？
   - 运行状态正确？
   - 归档完成？

---

# 10. 一页式阅读建议（给不同角色）

## 给老板/项目经理
先看：
- s01
- s02
- s09

## 给架构师
先看：
- s04
- s05
- s07
- s08

## 给模块负责人
先看：
- s02
- s04
- s05
- s09

## 给控制/联调负责人
先看：
- s03
- s07
- s08
- s09

## 给测试负责人
先看：
- s02
- s03
- s07
- s09

## 给平台/工程效能负责人
先看：
- s05
- s06
- s07
- s09

---

# 11. 最终冻结结论

`S01-S10` 正式规格轴与冻结补充标准已经构成一套完整、可执行、可联调、可验收的点胶机端到端架构模板。

它最核心的价值不在于“文档多”，而在于它已经把以下 9 件事闭合了：

1. 阶段链闭合
2. 对象链闭合
3. 模块边界闭合
4. 命令/事件闭合
5. 状态机闭合
6. 失败码与回退闭合
7. 回退与 superseded 闭合
8. 执行与归档闭合
9. 测试与验收闭合

如果后续团队严格按这套模板推进，最容易避免的就是 4 类常见灾难：

- 联调时才发现事件名和状态机对不上
- 运行时阻断误伤上游规划对象
- 首件失败后不知道该退哪一层
- 执行完成后没有完整追溯链

因此，建议将本索引与 9 份正文一起作为：
**《点胶机端到端方案冻结版 v1.0》**
正式进入项目实施阶段。
