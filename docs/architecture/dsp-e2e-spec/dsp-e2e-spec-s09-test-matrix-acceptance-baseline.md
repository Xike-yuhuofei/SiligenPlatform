# 《点胶机端到端流程规格说明 s09：测试矩阵与验收基线（修订版）》

本版目标是把前面已经确定的：
- 阶段链
- 对象链
- 模块链
- 状态机
- 命令/事件
- 回退与归档
- 关键时序链

全部压到可验证层，从“规格存在”推进到“规格可验收、可回归、可定位”。

本版重点补齐以下缺口：

1. 增加 **L1 对象级测试矩阵**，不再只讲阶段和链路。
2. 显式区分 `ExecutionPackageBuilt` 与 `ExecutionPackageValidated` 的测试断言。
3. 把 `PreflightBlocked` 的退出迁移写进测试矩阵。
4. 把 `FirstArticleRejected / Waived / NotRequired` 的条件必填拉进断言模板。
5. 新增幂等 / 重放 / 双击 / 错序命令测试族。
6. 新增 `ArtifactSuperseded` 与 `WorkflowArchived` 的验收链。

---

# 1. 测试体系总原则

## 1.1 测试分层必须与系统分层一致
测试层级建议固定为：
- L1 对象级测试
- L2 阶段级测试
- L3 模块级测试
- L4 链路级测试
- L5 故障注入测试
- L6 回归测试
- L7 验收测试

## 1.2 每一层测试都必须有明确事实对象
测试不应只断言“函数返回 true”，而应断言对象、状态、事件、版本链是否符合契约。

## 1.3 必须区分“结构正确”和“业务正确”
例如：
- `MotionPlan` 字段齐全，只能说明结构正确；
- 轨迹无越界、段索引连续、插补模式合法，才是业务正确。

## 1.4 必须区分“离线正确”和“在线可运行”
例如：
- `ExecutionPackageValidated` 说明离线规则通过；
- `PreflightPassed` 说明当前设备条件允许执行。

这两者都必须测，不能互相替代。

## 1.5 首件测试必须独立建模
首件本质上是：
- 质量门
- 回退入口
- 工艺与时序的第一现场验证点

## 1.6 故障注入必须覆盖“阻断、恢复、终止”三类结局
不能只注入 fault 然后看报错，必须验证状态、事件、推荐动作、恢复链和终止链。

## 1.7 回归测试必须围绕金样本，不围绕临时输入
DXF 类系统极易出现解析、容差、路径排序、轨迹边界调整的行为漂移，因此回归必须以稳定样例集为中心。

## 1.8 所有测试都必须能回链到规格对象
每条测试用例建议至少关联：
- `stage_id`
- `module_id`
- `artifact_type`
- `workflow_chain`
- `command`
- `expected_event_sequence`
- `expected_state_sequence`
- `failure_code`（失败场景）
- `rollback_target`（回退场景）
- `sample_ref`

## 1.9 不允许用 UI 人工观察代替核心验收
UI 观察只能辅助，不能替代：
- 结构断言
- 状态断言
- 事件断言
- 产物断言
- 失败码断言
- 版本链断言

## 1.10 测试输出应同时服务开发与追溯
每次测试结果最好至少产出：
- 输入样本标识
- 期望产物摘要
- 实际产物摘要
- 差异摘要
- 失败阶段
- 失败码
- 回退建议
- trace 摘要

## 1.11 幂等与错序必须单独成组测试
对于有 HMI、总线、运行时设备的系统，重复点击、网络抖动、消息重放不是边界条件，而是常态风险。

## 1.12 归档验收不是“有一条记录就算通过”
必须验证 `WorkflowArchived`、`ExecutionRecord`、全链路引用、fault 历史和首件结论都可回查。

---

# 2. 测试层级总览

## 2.1 分层矩阵

| 层级 | 目标 | 重点 |
|---|---|---|
| L1 对象级 | 核心对象字段与不变量 | 数据字典、条件必填、对象状态 |
| L2 阶段级 | 单阶段输入输出正确性 | 阶段成功/失败/阻断判定 |
| L3 模块级 | 模块内部服务与适配层协作 | owner 边界、越权负测 |
| L4 链路级 | 多阶段连续协作 | 主成功链、阻断链、回退链、归档链 |
| L5 故障注入级 | 异常分流与恢复 | failure_code、状态机、回退/恢复 |
| L6 回归级 | 防止行为漂移 | 金样本、摘要回归 |
| L7 验收级 | 项目级放行标准 | 通过率、关键链稳定性、追溯完整性 |

## 2.2 L1 对象级测试矩阵

| 对象 | 必测不变量 | 典型失败 |
|---|---|---|
| `JobDefinition` | 主键稳定、上下文字段完整 | 缺 baseline、缺设备、缺产品编码 |
| `SourceDrawing` | 文件哈希可追溯、输入不可变 | 空文件、损坏文件、重复文件处理错误 |
| `CanonicalGeometry` | 单位一致、实体数量稳定 | 不支持实体、退化几何 |
| `TopologyModel` | 连通图合法、闭环/开环分类正确 | 自交歧义、闭环不可解释 |
| `FeatureGraph` | 全覆盖、无未分类特征 | 分类冲突、特征遗漏 |
| `ProcessPlan` | 全特征已分配、参数合法 | 模板缺失、参数越界 |
| `CoordinateTransformSet` | 变换链完整、行程合法 | 基准缺失、超行程 |
| `AlignmentCompensation` | `not_required` 语义合法、拒绝场景失败码齐全 | 无对位却无对象、补偿超限未带失败码 |
| `ProcessPath` | 覆盖全部目标特征、顺序完整 | 未覆盖特征、关键区污染 |
| `MotionPlan` | 段索引连续、动态约束满足 | 不可达、动态超限 |
| `DispenseTimingPlan` | 事件索引与轨迹对齐 | 开阀越界、关阀错位 |
| `ExecutionPackage` | `build_result` 与 `validation_result` 区分明确 | 组包通过但未校验、校验失败未带原因 |
| `MachineReadySnapshot` | `blocked` 场景必须带原因 | 阻断无 failure_code |
| `FirstArticleResult` | `rejected/waived/not_required` 条件必填正确 | 缺 `rollback_target`、缺 `approval_ref` |
| `ExecutionRecord` | 可回链到源图、执行包、首件结论、fault | trace 断链、归档字段缺失 |

## 2.3 对象级最小断言模板

每个对象级用例至少断言：
1. `artifact_type / id / version` 合法
2. `input_refs` 可回链
3. 条件必填字段满足
4. `status` 合法
5. 不承载跨层污染字段

---

# 3. 阶段级测试矩阵

## 3.1 阶段级测试表

| 阶段 | 核心测试目标 | 正常用例 | 边界用例 | 失败用例 | 验收断言 |
|---|---|---|---|---|---|
| S0 | 任务上下文建模 | 合法产品/治具/baseline 组合 | 可选字段缺省 | 缺产品、缺 baseline | `JobDefinition` 完整，失败停在 S0 |
| S1 | 文件接收与封存 | 合法 DXF 上传 | 大文件、重复文件 | 空文件、损坏文件 | `SourceDrawing` 可追溯，失败不进入 S2 |
| S2 | DXF 解析标准化 | line/arc/polyline/spline 混合图 | 极小段、极短弧 | 不支持实体、字段缺失 | `CanonicalGeometry` 结构稳定 |
| S3 | 几何修复与拓扑重建 | 闭环/开环可重建 | 轻微断口、轻微重复段 | 自交不可解释、闭环歧义 | `TopologyModel` 连通关系稳定 |
| S4 | 特征提取 | 常规封闭胶圈/开放胶线 | 边界尺寸、邻近区域 | 特征冲突、未分类特征 | `FeatureGraph` 全覆盖、无未分类 |
| S5 | 工艺规则映射 | 标准特征映射标准工艺 | 极限速度/点距 | 模板缺失、参数越界 | `ProcessPlan` 全特征已分配 |
| S6 | 坐标变换 | 标准平移旋转镜像 | 接近行程边界 | 超行程、基准缺失 | `CoordinateTransformSet` 合法 |
| S7 | 对位/补偿 | 正常对位成功 | 小偏差补偿 | 对位失败、补偿超限 | `AlignmentCompensation` 合法；无需对位时必须生成 `not_required` 对象 |
| S8 | 工艺路径生成 | 合理排序与连接 | 多区域、多跳转 | 关键区污染、未覆盖特征 | `ProcessPath` 顺序完整 |
| S9 | 运动轨迹生成 | 标准插补与抬刀 | 高曲率、短段密集 | 动态超限、不可达 | `MotionPlan` 连续可执行 |
| S10 | 点胶时序生成 | 标准开/关阀逻辑 | 极短段、极短延迟 | 触发点越界、索引不一致 | `DispenseTimingPlan` 与轨迹一致 |
| S11A | 执行包组包 | 正常组包 | 大包、复杂工艺 | 缺字段、引用错版 | 必须产生 `ExecutionPackageBuilt`，但不得自动视为已校验 |
| S11B | 离线校验 | 已组包对象校验通过 | 规则边界包 | 越界、干涉、包不一致 | 必须产生 `ExecutionPackageValidated / OfflineValidationFailed` |
| S12 | 设备预检 | 全 ready | 个别慢 ready | 未回零、报警、baseline invalid | `PreflightPassed / PreflightBlocked` 正确，规划对象版本不变化 |
| S13 | 首件确认 | 预览/空跑/首件通过 | 临界质量阈值、豁免、无需首件 | 空跑错、出胶错、位置偏 | 正确进入 `Passed / Rejected / Waived / NotRequired` |
| S14 | 正式执行 | 标准跑通 | 暂停恢复 | 通信中断、段索引异常 | 执行状态迁移正确 |
| S15 | 在线故障分层 | 可恢复 fault | 重复 fault、连锁 fault | 故障不可分类 | 正确给出恢复/终止决策 |
| S16 | 归档与追溯 | 正常归档 | 多报警、多暂停 | 记录缺失、链路断裂 | `ExecutionRecord` 完整，`WorkflowArchived` 正确 |

## 3.2 阶段级最小断言模板

建议每个阶段测试至少断言 6 件事：
1. 输入对象版本正确
2. 输出对象被创建且主键有效
3. 当前阶段状态迁移正确
4. 事件序列正确
5. 失败时 `failure_code` 正确
6. 失败时没有越权进入后续阶段

---

# 4. 模块级测试矩阵

## 4.1 模块级测试表

| 模块 | 必测内容 | 不应发生的行为 |
|---|---|---|
| M0 workflow | 状态迁移、回退编排、归档编排、幂等重试 | 自行改写业务对象 |
| M1 job-ingest | 任务创建、文件封存、重复文件识别 | 偷偷解析 DXF |
| M2 dxf-geometry | 实体解析、单位标准化、unsupported 标记 | 直接做特征分类 |
| M3 topology-feature | 修复、拓扑、特征提取 | 写工艺速度/阀延时 |
| M4 process-planning | 特征到工艺映射、参数校验 | 输出轨迹或设备指令 |
| M5 coordinate-alignment | 坐标链、对位补偿、无补偿显式化、超限判定 | 改写源图或工艺模板 |
| M6 process-path | 路径排序、连接策略、覆盖率 | 写 jerk/加速度 |
| M7 motion-planning | 轨迹生成、动态约束、抬刀逻辑 | 写阀开关时序 |
| M8 dispense-packaging | 时序生成、组包、离线校验、已组包/已校验分离 | 重排路径、改轨迹 |
| M9 runtime-execution | 预检、首件、执行、恢复、中止 | 重建上游规划对象 |
| M10 trace-diagnostics | 归档、日志链、追溯查询 | 回写历史事实 |
| M11 hmi-application | 命令转发、状态展示、审批 | 直接控制设备或写规划事实 |

## 4.2 模块级越权负测试

### M7 越权负测试
验证：
- 不能生成 `DispenseTimingPlan`
- 不能访问设备 ready
- 不能审批首件

### M9 越权负测试
验证：
- 不能直接修改 `ExecutionPackage`
- 不能直接重建 `MotionPlan`
- 不能直接修正 `ProcessPlan`

### M10 越权负测试
验证：
- 不能改写 `ProcessPlan` 历史版本
- 不能用追溯结果替代业务对象事实

### M11 越权负测试
验证：
- 不能绕过 WF 直调 DEV
- 不能写 owner 对象
- 不能以 UI 本地状态替代系统事实状态

---

# 5. 关键时序链测试矩阵

## 5.1 主成功链测试

### 目标
验证“上传 DXF → 规划 → 组包 → 离线校验 → 预检 → 首件 → 执行 → 归档”整链可跑通。

### 最低断言
- 所有关键对象都被生成
- `ExecutionPackageBuilt` 与 `ExecutionPackageValidated` 均出现，且顺序正确
- 状态链按顺序迁移
- 无非法跳步
- `ExecutionRecord` 能回链到 `SourceDrawing`
- `WorkflowArchived` 正确产生

### 最低通过标准
- 连续 N 次稳定通过
- 关键对象版本链一致
- 无未分类 warning 升级为错误

## 5.2 首件失败回退链测试

### 目标
验证首件失败后，系统能按归属层正确回退，而不是乱回退。

### 最低断言
- `FirstArticleRejected` 被正确记录
- 失败原因被正确分流
- `rollback_target` 正确
- 下游产物被标记 `superseded`
- 新对象重新生成
- 未经首件通过不能进入 `StartExecution`

### 子场景至少覆盖
- 空跑路径错
- 空跑对、出胶错
- 位置偏
- 原因未明需人工诊断

## 5.3 设备未就绪阻断链测试

### 目标
验证 `PreflightBlocked` 仅阻断执行，不污染规划链。

### 最低断言
- `ExecutionPackageValidated` 仍保持有效
- 状态进入 `PreflightBlocked`
- 规划对象版本不变化
- 设备状态恢复后可继续预检
- 中止时走 `Aborted`，不是改写规划失败

### 子场景至少覆盖
- 未回零
- 报警未清
- `baseline invalid`
- 工件未到位
- 门禁 / 安全链未满足

## 5.4 运行时故障恢复 / 终止链测试

### 目标
验证 fault 后系统能区分：
- 可恢复继续跑
- 需重新预检再恢复
- 必须终止

### 最低断言
- `ExecutionFaulted` 带 `fault_code`、`segment_index`
- 恢复前存在检查点
- 不允许对不可恢复 fault 执行 `ResumeExecution`
- 终止后能归档
- fault 事件可追溯

### 子场景至少覆盖
- 运行中短暂通信中断
- 堵针 / 无胶
- 视觉丢失
- 安全链触发
- 段索引错乱

## 5.5 归档与追溯链测试

### 目标
验证 `Completed / Aborted / Faulted` 三类终态都能进入归档，并可完整追溯。

### 最低断言
- `ExecutionRecordBuilt` 正确
- `ArchiveWorkflowCommand` 后产生 `WorkflowArchived`
- 可查询到对应源图、工艺计划、轨迹、时序、执行包、首件结论、fault 记录、最终状态
- 缺任一关键引用都判定失败

---

# 6. 幂等、重放与错序测试矩阵

## 6.1 同幂等键重放

| 命令 | 预期 |
|---|---|
| `CreateJobCommand` | 返回既有结果，不重复创建 |
| `RunPreflightCommand` | 在同一设备条件下不重复产生新的业务事实 |
| `ApproveFirstArticleCommand` | 不重复产生新审批事实 |
| `ArchiveWorkflowCommand` | 不重复归档 |

## 6.2 不允许重入命令的双击测试

| 命令 | 预期 |
|---|---|
| `StartExecutionCommand` | 拒绝二次启动或返回既有执行会话 |
| `PauseExecutionCommand` | 已暂停时不得再次进入新暂停流程 |
| `ResumeExecutionCommand` | 未暂停时不得恢复 |
| `RunFirstArticleCommand` | 已存在首件流程时不得再次创建 |

## 6.3 错序命令测试

必须覆盖：
- 在 `PackageBuilt` 直接发 `StartExecutionCommand`
- 在 `PreflightBlocked` 直接发 `StartExecutionCommand`
- 在 `FirstArticleRejected` 后直接发 `StartExecutionCommand`
- 在 `Running` 期间再次发 `CreateExecutionSessionCommand`

期望：全部被拒绝，且不产生错误状态迁移。

---

# 7. 故障注入矩阵

## 7.1 故障注入分类

| 注入层 | 故障类型 | 预期结局 |
|---|---|---|
| 文件层 | 文件损坏、结构非法 | 阻断在 S1/S2 |
| 几何层 | 不支持实体、退化几何 | 阻断在 S2 |
| 拓扑层 | 自交、闭环歧义 | 阻断在 S3 |
| 特征层 | 分类冲突、未分类 | 阻断在 S4 |
| 工艺层 | 模板缺失、参数越界 | 阻断在 S5 |
| 坐标层 | 基准丢失、补偿超限 | 阻断在 S6/S7 |
| 路径层 | 顺序冲突、未覆盖特征 | 阻断在 S8 |
| 轨迹层 | 动态超限、不可达 | 阻断在 S9 |
| 时序层 | 开关阀越界、索引错位 | 阻断在 S10 |
| 组包层 | 越界、干涉、包不一致 | 阻断在 S11B |
| 设备门禁层 | 未回零、报警、baseline invalid | 阻断在 S12 |
| 首件层 | 空跑错、出胶错、位置偏 | 回退在 S13 |
| 运行层 | 通信中断、堵针、安全链 | 恢复/终止在 S15 |
| 追溯层 | 归档失败、主链断裂 | 阻断在 S16 |

## 7.2 故障注入结果断言

每次 fault injection 至少断言：
1. 进入了预期状态
2. 产生了预期失败码
3. 没有越权进入错误阶段
4. 给出了预期推荐动作
5. 若允许恢复，恢复路径正确
6. 若必须终止，终止路径正确

---

# 8. 金样本回归矩阵

## 8.1 金样本的组织原则

每个金样本建议包含：
- `input/`
  - DXF
  - 任务上下文
  - 治具 / 标定样本
  - fixed-parameter baseline 样本
- `expected/`
  - 特征摘要
  - 工艺计划摘要
  - 路径摘要
  - 轨迹摘要
  - 时序摘要
  - 事件序列摘要
  - 状态链摘要
- `metadata/`
  - 样本目的
  - 覆盖阶段
  - 覆盖故障
  - 创建日期
  - owner

## 8.2 建议金样本集合

| 样本名 | 目标 |
|---|---|
| `simple_closed_bead` | 最简单闭环胶路 |
| `open_path_with_travel` | 开放路径 + 空跑 |
| `mixed_line_arc_polyline` | 混合图元解析 |
| `small_gap_topology_repair` | 断口修复 |
| `feature_conflict_case` | 特征冲突 |
| `mirrored_part_case` | 镜像 / 左右件 |
| `alignment_offset_case` | 对位补偿 |
| `alignment_not_required_case` | 无需对位闭环 |
| `corner_slowdown_case` | 转角轨迹 |
| `start_stop_quality_case` | 起停时序 |
| `dense_short_segments_case` | 短段密集 |
| `package_boundary_case` | 离线校验边界 |
| `first_article_reject_case` | 首件失败 |
| `first_article_waive_case` | 首件豁免 |
| `first_article_not_required_case` | 无需首件 |
| `preflight_block_case` | 设备未 ready |
| `runtime_fault_recovery_case` | 可恢复 fault |
| `runtime_fault_abort_case` | 不可恢复 fault |
| `archive_trace_case` | 归档与追溯 |

## 8.3 金样本回归断言层级

建议分 5 层：
- L1 结构摘要回归：对象存在且字段完整
- L2 关键行为摘要回归：特征数量、路径段数量、出胶段数量
- L3 关键事件序列回归：`ExecutionPackageBuilt -> ExecutionPackageValidated -> PreflightPassed ...`
- L4 关键状态迁移回归：阻断、回退、恢复、归档是否停在正确状态
- L5 故障分流摘要回归：首件失败时回退到哪、preflight block 停在哪、fault 是 recover 还是 abort

---

# 9. 验收基线

## 9.1 阶段验收基线

建议所有 S0-S16 满足：
- 正常用例通过率 = 100%
- 明确失败用例命中正确失败码率 = 100%
- 非法输入未越级流入后续阶段率 = 100%

## 9.2 主成功链验收基线

建议：
- 连续稳定跑通 >= 30 次
- 无状态错迁移
- 无对象版本链断裂
- `ExecutionRecord` 全部可追溯到源文件与执行包
- `WorkflowArchived` 全部正确出现

## 9.3 首件链验收基线

建议：
- `Passed / Rejected / Waived / NotRequired` 四类首件结论全部覆盖
- 首件失败后不能误启动正式执行
- 回退后新包与旧包版本区分明确
- `superseded` 标记正确

## 9.4 Preflight 验收基线

建议：
- 主要阻断原因全部有明确 `failure_code`
- `PreflightBlocked` 绝不污染规划对象
- 修复后重试可恢复到 `MachineReady`
- 中止时进入 `Aborted`

## 9.5 运行时恢复 / 终止验收基线

建议：
- 至少 3 类可恢复 fault 能恢复成功
- 至少 3 类不可恢复 fault 被正确终止
- 所有 fault 都能关联：
  - `execution_id`
  - `package_id`
  - `segment_index`
  - `fault_code`

## 9.6 追溯验收基线

建议任意一次执行完成后，必须能查询出：
- 对应源图
- 对应工艺计划
- 对应轨迹计划
- 对应时序计划
- 对应执行包
- 对应首件结论
- 对应故障记录
- 对应最终状态
- 对应归档时间与归档人

查不全，就不算追溯合格。

## 9.7 幂等与错序验收基线

建议：
- 同幂等键重放 100% 不重复产生业务事实
- 不允许重入命令 100% 被拒绝或返回既有结果
- 错序命令 100% 不触发非法状态迁移

## 9.8 当前无机台补充基线（北京时间 `2026-03-26 00:31`，对应 UTC `2026-03-25T16:31Z`）

- `s5-door-interlock`：门禁场景当前以 `dispenser.start` / `supply.open` 拦截为准；`mock.io.set(door=...)` 后必须轮询 `status` 收敛到 `machine_state=Fault/Idle`，不能对第一帧硬断言。证据：`tests/reports/verify/release-validation-20260326-002954/phase4-first-layer-rereview/20260325T163158Z/s5-door-interlock/tcp-door-interlock.md`
- `s6-negative-limit-chain`：当前验收口径是 `limit_x_neg/limit_y_neg` 驱动 HOME boundary，负向移动返回 `2401` 且同轴位置不变，正向逃逸移动仍允许；`status.io.limit_x_neg/limit_y_neg` 不暴露该状态，现阶段视为产品既有行为。实现依据：`modules/runtime-execution/adapters/device/src/adapters/motion/controller/control/MultiCardMotionAdapter.Status.cpp`、`modules/runtime-execution/adapters/device/src/adapters/motion/controller/control/MultiCardMotionAdapter.Helpers.cpp`
- `hmi online smoke`：当前只作为链路 smoke，验证 `home / point move / jog / supply / dispenser / estop / door interlock` 能通过 external gateway mock 链路；不作为轨迹精度、停止距离、真实 IO、伺服响应验收依据。证据：`tests/reports/verify/release-validation-20260326-002954/phase4-hmi-online-smoke/online-smoke.log`

## 9.9 本轮未覆盖项与下一阶段任务

1. 评审是否将 HOME boundary 映射到 `status.io.limit_x_neg/limit_y_neg`；若采纳，补协议契约、HMI 状态展示和回归断言。
2. 把 door 状态收敛规则固化为显式契约，补“首帧非权威、需轮询收敛”的协议/HMI 自动化测试。
3. 规划真实机台或高保真仿真验收，补运动精度、停止距离、真实 IO、伺服响应与安全链的正式放行证据。

---

# 10. 推荐测试用例模板

## 10.1 用例头
建议固定以下字段：
- `case_id`
- `title`
- `layer`
- `stage_id`
- `module_id`
- `artifact_type`
- `workflow_chain`
- `command`
- `expected_event_sequence`
- `expected_state_sequence`
- `failure_code`
- `rollback_target`
- `idempotency_key`
- `approval_ref`
- `sample_ref`

## 10.2 前置条件
- 输入对象版本
- 设备状态
- fixed-parameter baseline 状态
- 对位 / 补偿状态
- 是否首件必需
- 是否允许恢复

## 10.3 操作
- 发什么命令
- 是否注入 fault
- 是否人工审批
- 是否重试 / 回退
- 是否重放相同 `idempotency_key`

## 10.4 期望
- 期望状态迁移
- 期望事件序列
- 期望生成对象
- 期望 `failure_code`
- 期望推荐动作
- 期望 trace 结果

---

# 11. 推荐自动化优先级

## P0 必须自动化
- S2 解析
- S3 拓扑
- S5 工艺规则
- S8 路径
- S9 轨迹
- S10 时序
- S11A 组包
- S11B 离线校验
- 主成功链
- Preflight 阻断链
- 首件失败分流
- 运行时 fault 分流核心场景
- 幂等 / 重放 / 错序核心场景

## P1 尽快自动化
- 镜像件 / 左右件
- 对位补偿边界
- `alignment_not_required_case`
- `first_article_waive_case`
- `first_article_not_required_case`
- 恢复链更多变体
- 追溯查询完整性
- `ArtifactSuperseded` 与 `WorkflowArchived` 验证链

## P2 可后补
- UI 展示细节
- 报表导出
- 统计聚合类功能

---

# 12. 最容易被忽略的测试点

## 12.1 “对象没有生成错误，但生成错版本”
必须断言对象版本链，而不是只断言对象存在。

## 12.2 “PreflightBlocked 后偷偷重算包”
必须专门防回归。

## 12.3 “FirstArticleRejected 后还能点 StartExecution”
必须专门防回归。

## 12.4 “Pause / Resume 后段索引漂移”
运行时系统常见问题，必须专门测。

## 12.5 “ExecutionRecord 写出来了，但回链不全”
追溯系统常见伪通过，必须专门测。

## 12.6 “只有 `ExecutionPackageBuilt`，没有 `ExecutionPackageValidated`”
这是典型的流程伪通过，必须专门防回归。

## 12.7 “`Waived / NotRequired` 分支永远没人测”
这些不是低频点，而是首件门控真实存在的规格分支。

---

# 13. 这一版最关键的工程结论

## 13.1 模板已经从“理论架构”进入“可验证规格”
它可以直接指导：
- 测试设计
- 开发验收
- 回归基线
- 故障注入
- 放行标准

## 13.2 关键验收主轴已经从 4 条扩展到 5 条
不要只验“主成功链”，还必须验：
- 首件失败回退链
- 设备未 ready 阻断链
- 运行时 fault 恢复 / 终止链
- 归档与追溯链

## 13.3 真正高价值的验收，不是“功能都能点”
而是：
- 错误能否停在正确层
- fault 能否走正确分流
- 结果能否稳定追溯
- 重复命令能否不误动作
