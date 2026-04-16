# 项目链路标准 V1

更新时间：`2026-03-31`

## 1. 目的

本文用于把 SiligenSuite 当前项目内的“链路”统一收敛成一套可复用、可审查、可追责的定义，避免以下混淆：

1. 把模块当成链路。
2. 把阶段当成链路。
3. 把协议方法名当成链路。
4. 把单个算法或单个类当成链路。

本文同时区分两类事实：

1. **正式对象链**：以 `docs/architecture/dsp-e2e-spec/` 为准的端到端 owner 对象链。
2. **当前 live 链**：以 `apps/*`、`shared/contracts/application/*`、当前 HMI/TCP/CLI 宿主实际暴露的交互链为准。

这两类事实相关，但不等价。正式对象链回答“业务对象如何逐层生成”；live 链回答“当前系统入口如何交互、执行和观测”。

对 A07/A08，本文优先采用当前 canonical owner、已落地 contracts 和边界测试已经收口的 **as-is 口径**。
如果后续要推动 target-state，必须另立迁移文档或目标态规格，不能在本文里把现状和目标态混写。

## 2. “链路”的统一判定标准

项目内只有同时满足以下条件，才定义为一条正式链路：

1. 有稳定的上游输入与下游输出。
2. 有明确的 owner 交接或责任交接。
3. 有可观察的成功/失败/阻断语义。
4. 有固定的边界，能明确说出“不属于本链的内容是什么”。

因此：

1. **模块** 是责任边界，不等于链路。
2. **阶段** 是流程里程碑，不等于链路。
3. **协议方法** 是链路入口或观测面，不等于链路本体。
4. **算法** 是链路内部实现手段，不等于链路本体。

## 3. 总体分类

项目当前可归纳为三大类链路：

1. **A 类：正式制造对象链**
   从任务接入一直到归档追溯，是 `M0-M10` 的正式业务主链。
2. **B 类：当前 live 控制与交互链**
   从 HMI/CLI/TCP 入口到运行时执行与状态回流，是当前真正跑通的应用控制闭环。
3. **C 类：观测与工程支撑链**
   为预览、仿真、追溯、验证和工程数据服务，但不直接等于生产执行主链。

## 4. A 类：正式制造对象链

主要依据：

1. `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s01-stage-io-matrix.md`
2. `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s02-stage-responsibility-acceptance.md`
3. `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md`
4. `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s07-state-machine-command-bus.md`

| ID | 链路名 | 权威输入 -> 权威输出 | 精准定义 | 明确不等于 |
|---|---|---|---|---|
| A01 | 任务接入链 | 任务上下文、源文件引用 -> `JobDefinition`、`SourceDrawing` | 建立任务事实并封存源文件，使后续阶段拥有唯一任务上下文和可追溯源图事实。 | DXF 解析链、几何链 |
| A02 | 几何规范化链 | `SourceDrawing` -> `CanonicalGeometry` | 把源图转换为系统统一几何表达，收敛单位、方向和基础坐标语义。 | 拓扑修复链、工艺规划链 |
| A03 | 拓扑与特征链 | `CanonicalGeometry` -> `TopologyModel`、`FeatureGraph` | 把规范几何进一步转成可制造拓扑关系和制造特征事实。 | 工艺规则链、运动规划链 |
| A04 | 工艺规划链 | `FeatureGraph`、配方/材料/模板 -> `ProcessPlan` | 把制造特征映射为工艺规则与工艺参数，不负责路径顺序或动态约束。 | 路径链、轨迹链 |
| A05 | 坐标/对位/补偿链 | `ProcessPlan`、治具/标定/视觉/测高输入 -> `CoordinateTransformSet`、`AlignmentCompensation` | 生成当前任务的坐标真相、补偿真相和“无需补偿”的显式事实。 | 路径优化链、速度规划链 |
| A06 | 工艺路径链 | `ProcessPlan`、坐标变换、补偿 -> `ProcessPath` | 按工艺语义组织实际要走的几何段、顺序、连接、进退刀和段级工艺标签。 | 时间参数化链、板卡程序链 |
| A07 | 轨迹生成链 | `ProcessPath`、动态参数（当前 canonical 为 `TimePlanningConfig`） -> `MotionPlan`（代码级 payload = `MotionTrajectory`） | 当前 M7 canonical owner 在 `modules/motion-planning/`。业务阶段名保留 `MotionPlan`，代码级实际通过 `MotionPlanningFacade -> MotionPlanner -> MotionTrajectory` 产出时间化运动描述。 | `InterpolationProgramPlanner`、板卡 API 调用链、HMI 预览渲染链 |
| A08 | 插补程序合成链 | `ProcessPath` + `MotionPlan`（代码级 payload = `MotionTrajectory`） -> `InterpolationData` / `interpolation_segments` | 由 `InterpolationProgramPlanner` 把几何段语义与速度语义合成为板卡段级程序。`interpolation_points` 属于预览 / authority binding 辅助资产，不是 A08 的 canonical 执行程序输出。 | `MotionPlanner` 时间参数化链、板卡执行链、状态回流链 |
| A09 | 点胶时序链 | `MotionPlan`、`ProcessPlan`、阀/泵响应参数 -> `DispenseTimingPlan` | 生成出胶开闭、触发、补偿和供胶协同的时序事实。 | 轨迹生成链、手动阀调试链 |
| A10 | 执行包链 | `MotionPlan`、`DispenseTimingPlan`、配方/模式 -> `ExecutionPackage(built/validated)` | 冻结正式执行输入，并把“已组包”和“已通过离线校验”显式分离。 | 设备预检链、正式执行链 |
| A11 | 执行门禁与首件链 | `ExecutionPackage(validated)`、实时设备状态 -> `MachineReadySnapshot`、`FirstArticleResult`、`ReadyForProduction` | 判断当前是否允许进入执行；其本质是执行前门禁与质量放行，不回写上游规划对象。 | 规划失败链、归档链 |
| A12 | 正式执行与恢复链 | 已放行执行输入、执行会话、设备状态 -> 运行事件流、故障事件、恢复决策 | 负责启动、暂停、恢复、故障分层和中止，是运行时闭环，不重算规划对象。 | 轨迹规划链、预览确认链 |
| A13 | 归档追溯链 | 执行日志、fault 历史、首件结论、上下文快照 -> `ExecutionRecord`、`TraceLinkSet`、`WorkflowArchived` | 固化执行事实并建立可追溯证据链。 | 追溯查看 UI、运行时状态链 |

## 5. A07-A08-A12 的运动控制细分（当前 owner 口径）

“运动控制链路”在项目内不是单点概念，而是以下三条相邻子链的组合：

| 子链 | 权威输入 -> 输出 | 精准定义 | 主要依据 |
|---|---|---|---|
| A07-1 轨迹生成子链 | `ProcessPath` + `TimePlanningConfig` -> `MotionPlan`（代码级 payload = `MotionTrajectory`） | 当前 canonical 规划调用链是 `MotionPlanningFacade -> MotionPlanner -> MotionTrajectory`；业务阶段名保留 `MotionPlan`。jerk 约束仍属于该层的时间化职责，不是板卡插补器。 | `modules/motion-planning/README.md`、`docs/architecture/topics/dxf/dxf-motion-execution-contract-v1.md` |
| A08-1 程序合成子链 | `ProcessPath` + `MotionPlan`（`MotionTrajectory`） -> `InterpolationData` / `interpolation_segments` | `InterpolationProgramPlanner` 根据 `process_path` 的几何段语义和 `motion_trajectory` 的速度语义合成板卡段程序；这是当前 canonical execution program。`interpolation_points` 只是预览 / authority binding 辅助资产。 | `docs/architecture/topics/dxf/dxf-motion-execution-contract-v1.md` |
| A12-1 板卡执行子链 | `InterpolationData` -> MultiCard 坐标系执行状态 | 设备层负责真正的 FIFO / lookahead / 坐标系插补执行和状态反馈。 | `docs/architecture/topics/dxf/dxf-motion-execution-contract-v1.md` |

裁决性定义：

1. jerk 受限时间化属于 **轨迹生成子链**。
2. 当前业务阶段名保留 `MotionPlan`，但代码级 canonical payload 是 `MotionTrajectory`；`modules/motion-planning/contracts/MotionPlan.h` 只是 alias 封装。
3. `InterpolationAlgorithm` 在当前仓库不构成 `MotionPlanner` 的 canonical 公开输入；它主要服务于 `TrajectoryInterpolatorFactory` / `InterpolationPlanningUseCase` / preview `interpolation_points` 辅助分支。
4. `InterpolationProgramPlanner` 属于 **程序合成子链**。
5. `interpolation_points` 属于预览和 authority binding 辅助资产，不等于板卡下发程序。
6. `InterpolationAdapter -> MultiCard SDK` 才属于 **板卡执行子链**。

## 6. B 类：当前 live 控制与交互链

主要依据：

1. `shared/contracts/application/commands/*.json`
2. `shared/contracts/application/queries/*.json`
3. `shared/contracts/application/mappings/protocol-mapping.md`
4. `apps/runtime-gateway/transport-gateway/docs/protocol-mapping.md`
5. `docs/architecture/topics/dispense-preview/dispense-trajectory-preview-sequence-text-v1.md`
6. `docs/architecture/topics/runtime-hmi/hmi-online-startup-state-machine-v1.md`
7. `docs/architecture/topics/runtime-hmi/runtime-supervision-state-contract-boundary-v1.md`

| ID | 链路名 | 权威输入 -> 权威输出 | 精准定义 | 明确不等于 |
|---|---|---|---|---|
| B01 | HMI online 启动链 | launcher contract、backend/gateway/config -> `online_ready` 会话快照 | 把 HMI 从离线宿主推进到 online 可交互状态，阶段固定为 `backend -> tcp -> hardware -> online_ready`。 | 真实机台放行链、生产执行链 |
| B02 | 应用传输命令链 | HMI/CLI 方法调用 -> JSON envelope -> dispatcher -> facade/usecase -> 响应 envelope | 当前所有 TCP/CLI 应用命令的统一传输与分发链。它是 live 入口，不是业务对象 owner 链。 | `M0-M10` 正式对象链 |
| B03 | DXF 预览确认执行链 | `artifact_id`、`plan_id`、`snapshot_hash`、`plan_fingerprint` -> `job_id`、`job.status` | 当前 DXF 交互式执行的 canonical live 链：`artifact.create -> plan.prepare -> preview.snapshot -> preview.confirm -> job.start -> job.status`。 | 完整 S0-S16 正式对象链 |
| B04 | 手动运动控制链 | `home` / `home.go` / `home.auto` / `jog` / `move` / `stop` / `estop` -> 轴动作或拒绝结果 | 当前人工轴控链，用于回零、点动、点到点移动与安全停机；门禁与监督态会在入口侧直接拦截。 | DXF 任务执行链 |
| B05 | 单阀/供胶调试链 | `dispenser.*`、`purge`、`supply.*` -> 阀/供胶状态变化 | 当前单阀、供胶、purge 调试链，用于维护与调试，不等于 DXF 正式点胶执行。 | 点胶时序规划链 |
| B06 | 状态回流监督链 | runtime/device/raw io -> `status` / `effective_interlocks` / `supervision` / `alarms` | 当前 HMI、smoke、诊断统一观察面的权威状态链，用于能力门禁、在线判定和错误诊断。 | 规划链、板卡程序链 |
| B07 | 告警处置链 | `alarms.list` / `alarms.acknowledge` / `alarms.clear` -> 告警视图与清理结果 | 当前运行态告警查询与处置链。 | 急停复位链、归档链 |
| B08 | 配方生命周期链 | `recipe.*` 请求 -> recipe/version/audit/bundle 结果 | 当前配方的查询、建模、草稿、发布、激活、比对、导入导出与审计链。 | DXF 任务执行链 |

## 7. C 类：观测与工程支撑链

主要依据：

1. `apps/planner-cli/README.md`
2. `apps/trace-viewer/README.md`
3. `docs/architecture/topics/sim-observer/sim-observer-data-contract-v1.md`
4. `docs/architecture/topics/dispense-preview/dispense-trajectory-preview-scope-baseline-v1.md`
5. `docs/architecture/workspace-baseline.md`
6. `README.md`

| ID | 链路名 | 权威输入 -> 权威输出 | 精准定义 | 明确不等于 |
|---|---|---|---|---|
| C01 | 离线工程数据链 | DXF / 工程数据输入 -> `.pb`、离线 preview、simulation input、trajectory export | 为规划、预览、仿真和排查准备离线中间资产的工程链。 | 在线正式执行链 |
| C02 | 仿真观察链 | `summary`、`snapshots`、`timeline`、`trace`、`motion_profile` -> 六类观察视图对象 | 把 recording/export 事实标准化成对象、锚点和视图，用于可视化观察与解释。 | 真实机台状态链 |
| C03 | 追溯查看链 | `trace-diagnostics` 事实、验证报告、测试 evidence -> trace-viewer 只读展示 | 当前追溯与证据查看链，只消费事实，不生成 owner 事实。 | 归档 owner 链 |
| C04 | 构建验证门禁链 | `build.ps1` / `test.ps1` / `ci.ps1` / validation scripts -> reports | 当前工程门禁与验证闭环，用于证明工作区结构、契约和测试基线是否成立。 | 业务执行链 |

## 8. 当前最容易混淆的五组概念

### 8.1 模块 vs 链路

例如 `modules/runtime-execution/` 是模块边界，不是单一链路。它同时参与：

1. B04 手动运动控制链
2. B05 单阀/供胶调试链
3. B06 状态回流监督链
4. A12 正式执行与恢复链

### 8.2 阶段 vs 链路

例如 S9“运动轨迹生成”是正式流程阶段，但当前工程讨论时最容易被混成一团的是以下相邻能力：

1. A07 轨迹生成链：M7 canonical owner，产出 `MotionPlan`（代码级 payload = `MotionTrajectory`）
2. A08 插补程序合成链：由 `InterpolationProgramPlanner` 消费 `MotionPlan` 生成 `interpolation_segments`
3. 与 B03 预览链共享的 `interpolation_points` / 快照辅助分支

因此，S9 不能直接等于“所有运动相关实现的总称”。

### 8.3 预览链 vs 执行链

`dxf.preview.snapshot` 属于 B03 预览确认执行链的一部分，只负责执行前确认，不等于板卡执行本身。

### 8.4 状态链 vs 控制链

`status` / `effective_interlocks` / `supervision` 属于 B06 状态回流监督链。  
它回答“系统当前是什么状态”，不回答“上游规划对象是否应该重算”。

### 8.5 HMI vs 事实源

HMI 只属于入口、展示、门禁消费和确认动作层。  
HMI 不是 `M0-M10` 的业务事实 owner，不直接生成 `MotionPlan`、`ExecutionPackage` 或 `ExecutionRecord`。

## 9. 本文的裁决性结论

1. 项目内不存在“只有一条总链路”的说法；至少应区分正式对象链、live 控制链和观测支撑链。
2. “运动控制链路”不是整个项目主链，而是 **A07 轨迹生成链 + A08 插补程序合成链 + A12 板卡执行子链** 的组合叫法。
3. `MotionTrajectory` 属于 **轨迹生成层**；`InterpolationAlgorithm` 在当前仓库主要属于插补器 / 预览点能力，不应直接等同为 M7 canonical `MotionPlanner` 输入。
4. `status` 是当前上位机统一观察面，不是规划对象真源。
5. DXF 当前 live 执行链的正式入口不是笼统的“执行 DXF”，而是 **B03 DXF 预览确认执行链**。

## 10. 建议的后续使用方式

后续做全局审查与优化时，建议固定按以下顺序讨论：

1. 先声明在审哪一条链路，不要只报模块名。
2. 先声明该链路属于 A/B/C 哪一类。
3. 先写清楚这条链的权威输入、权威输出和不变量。
4. 再定位异常首次出现层，不要直接从 HMI 表象跳到运动算法或板卡层。
5. 若讨论 A07/A08，先声明当前是在说 **as-is** 还是 **to-be**。

如果需要进入下一步，应基于本文继续输出《项目链路审查矩阵》，逐条补齐：

1. 当前 owner
2. 真实入口
3. 关键不变量
4. 已知契约漂移
5. 最小观测点
6. 优先优化项
