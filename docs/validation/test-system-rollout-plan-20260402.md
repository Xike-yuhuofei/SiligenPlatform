# Test System Rollout Plan

更新时间：`2026-04-02`

## 1. 任务归类

- 类型：`boundary-audit`
- 目标：把当前“平台已完成、覆盖未收口”的测试体系推进到“基线完全落地”
- 非目标：
  - 不重建 root entry、lane、evidence bundle、controlled HIL、nightly-performance 平台
  - 不把所有专项联机能力立即升级为 blocking gate
  - 不借测试补齐之名重写业务 owner 或设备语义

## 2. 当前判断

当前仓库已经完成以下平台层收口：

- `L0-L6` layer / lane 真值已冻结
- `tests/` canonical roots、`tests/reports/` evidence root、`tests/baselines/` governance 已落地
- `nightly-performance` 已具备 `small/medium/large` blocking gate
- controlled HIL 已具备 signed latest authority

当前主要缺口集中在：

- 第二批模块仍多为占位测试骨架
- 部分 P0 模块只有薄覆盖，未达到基线要求的测试类型组合
- 若干专项 online/HIL authority 仍未收敛到正式 gate policy
- `stress / soak` 尚未形成统一正式体系

## 3. 完成定义

只有同时满足以下条件，才视为“测试体系基准完全落地”：

1. `03-baseline-module-test-matrix.md` 中所有已纳入矩阵的模块，都有正式测试入口和对应最小测试面。
2. 所有 P0 模块满足其基线要求的测试类型组合，而不是只具备少量单测。
3. `P0-05`、`P1-01`、`P1-04`、`P1-05` 等专项能力的 gate 定位已冻结。
4. `2h / 8h / 24h` `stress / soak` 体系至少完成正式 profile、报告和策略定义。
5. closeout 文档能明确说明哪些能力已进入正式 gate，哪些仍是专项 authority。

## 4. 阶段总览

| Phase | 目标 | 主要 owner | 预期验证层级 | 退出标准 |
| --- | --- | --- | --- | --- |
| `Phase A` | 冻结缺口、边界和策略清单 | `docs/validation`、`docs/testing` | `L0` | 所有模块与专项能力都有明确状态和目标口径 |
| `Phase B` | 补齐空心模块的最小正式测试面 | `modules/topology-feature`、`modules/process-planning`、`modules/coordinate-alignment`、`modules/trace-diagnostics`、`modules/hmi-application` | `L0-L2` | 不再存在只有 `README + .gitkeep` 的正式模块测试根 |
| `Phase C` | 加厚 P0 模块矩阵 | `job-ingest`、`dxf-geometry`、`process-path`、`runtime-execution`、`dispense-packaging` | `L1-L3` | P0 模块满足基线要求的测试类型组合 |
| `Phase D` | 收敛专项 authority 到正式策略 | `tests/e2e/hardware-in-loop/`、`apps/hmi-app/scripts/` | `L0-L5` | 每个专项能力都明确是 `blocking`、`advisory` 或 `specialized authority` |
| `Phase E` | 正式化 `stress / soak` 体系 | `tests/performance/`、`tests/e2e/hardware-in-loop/`、`tests/reports/` | `L4-L6` | `soak` 具备正式 profile、报告、趋势字段和运行策略 |

## 5. 可执行任务清单

### Phase A：冻结缺口与策略

#### T001 Gap Matrix Freeze

- 目标：把当前模块与专项能力状态冻结成单一真值矩阵。
- 输入：
  - `docs/testing/03-baseline-module-test-matrix.md`
  - `docs/validation/online-test-matrix-v1.md`
  - 当前 closeout 文档
- 输出：
  - 缺口矩阵文档
  - 模块分组：`empty` / `thin` / `healthy`
  - 专项分组：`blocking` / `advisory` / `specialized authority`
- 依赖：无
- 验证方式：
  - 文档中每个模块和专项项都能映射到现有证据路径
  - 无“状态不明”的模块或专项项
- 停止条件：
  - 若某模块 owner 不清或测试落点不清，先停止并回退到边界冻结

#### T002 Owner And Scope Freeze

- 目标：确认模块内测试与仓库级测试的 owner 边界，防止仓库级 tests 代替模块 owner 责任。
- 输入：
  - `docs/testing/02-baseline-directory-and-ownership.md`
  - 当前 `modules/*/tests`、`apps/*/tests`
- 输出：
  - 模块内测试 owner 声明
  - 仓库级 tests consumer-only 说明
- 依赖：`T001`
- 验证方式：
  - P0/P1/P2 模块均有清晰 owner 落点
  - 无新增隐藏 owner
- 停止条件：
  - 若发现 bridge / compat / host 层正在充当真实测试 owner，停止并先修正文档口径

#### T003 Special Authority Policy Freeze

- 目标：冻结 `P0-05`、`P1-01`、`P1-04`、`P1-05` 的正式策略。
- 输入：
  - `docs/validation/online-test-matrix-v1.md`
  - `phase14` / `phase15` closeout
- 输出：
  - 每个专项能力的定位结论：
    - `blocking`
    - `advisory`
    - `specialized authority`
- 依赖：`T001`
- 验证方式：
  - 每个专项能力都有唯一定位和理由
- 停止条件：
  - 未先冻结失败分类、override 规则和 evidence contract 时，不进入实现

### Phase B：补齐空心模块

#### T101 Topology Feature Minimal Test Surface

- 目标：为 `topology-feature` 补齐 `unit + contracts + golden + integration` 的最小正式测试面。
- 输入：
  - `modules/topology-feature/tests/`
  - 上游几何样本
- 输出：
  - 最小单测
  - 最小 contract
  - 至少一组 golden
  - 至少一条最小 integration
- 依赖：`T002`
- 验证方式：
  - 模块内 tests 存在真实测试源码
  - 至少覆盖 `small` 和 `failure` 两类样本
- 停止条件：
  - 若拓扑 authority artifact 未冻结，停止补测试

#### T102 Process Planning Minimal Test Surface

- 目标：为 `process-planning` 补齐 `unit + contracts + golden` 的最小正式测试面。
- 输入：`modules/process-planning/tests/`
- 输出：最小测试集合与模块 README 说明
- 依赖：`T002`
- 验证方式：
  - 至少具备真实测试源码
  - 至少覆盖 `missing-config` 或 `mode-mismatch`
- 停止条件：
  - 若工艺模板选择语义未冻结，停止

#### T103 Coordinate Alignment Minimal Test Surface

- 目标：为 `coordinate-alignment` 补齐 `unit + contracts + golden` 的最小正式测试面。
- 输入：`modules/coordinate-alignment/tests/`
- 输出：最小测试集合与对齐基线样本
- 依赖：`T002`
- 验证方式：
  - 至少覆盖 `origin offset`、`rotation`、`failure`
- 停止条件：
  - 若标定 authority 不清，停止

#### T104 Trace Diagnostics Minimal Test Surface

- 目标：为 `trace-diagnostics` 补齐 `contracts + evidence + observability` 的最小正式测试面。
- 输入：
  - `modules/trace-diagnostics/tests/`
  - 现有 `validation-evidence-bundle` contract
- 输出：
  - 最小 evidence 结构 contract
  - 最小 observability 回归
- 依赖：`T002`
- 验证方式：
  - 能校验成功场景和失败场景的最小证据结构
- 停止条件：
  - 若 evidence owner 漂移到 `tests/reports/`，停止

#### T105 HMI Application Minimal Projection Surface

- 目标：为 `hmi-application` 补齐 `presenter/view-model + startup-state + protocol` 的最小正式测试面。
- 输入：`modules/hmi-application/tests/`
- 输出：
  - 状态投影测试
  - 启动态测试
  - 协议消费测试
- 依赖：`T002`
- 验证方式：
  - 覆盖 `offline / connecting / ready / failure`
- 停止条件：
  - 若测试依赖 GUI 截图作为主判定物，停止并改回 presenter/protocol 层

### Phase C：加厚 P0 模块矩阵

#### T201 Job Ingest Matrix Thickening

- 目标：把 `job-ingest` 从薄单测补到 `unit + contracts + golden + integration`。
- 输入：`modules/job-ingest/tests/`
- 输出：P0 最小完整矩阵
- 依赖：`Phase B` 完成
- 验证方式：
  - 至少有 `contracts`、`golden`、`integration`
- 停止条件：
  - 若需要仓库级 E2E 代替模块 contract，停止

#### T202 DXF Geometry Matrix Thickening

- 目标：把 `dxf-geometry` 补到基线要求的完整最小矩阵。
- 输入：`modules/dxf-geometry/tests/`
- 输出：P0 最小完整矩阵
- 依赖：`Phase B`
- 验证方式：
  - 覆盖 `small / dirty / boundary / failure` 中的最小集
- 停止条件：
  - 若没有 golden 仍试图宣称完成，停止

#### T203 Process Path Matrix Thickening

- 目标：把 `process-path` 从当前薄覆盖补到 `unit + contracts + golden + integration`。
- 输入：`modules/process-path/tests/`
- 输出：P0 最小完整矩阵
- 依赖：`T102`、`T103`
- 验证方式：
  - 至少一条 `process-path -> motion-planning` integration
- 停止条件：
  - 若上游 `process-planning` / `coordinate-alignment` 未冻结完成，不继续扩充集成链

#### T204 Runtime Execution Matrix Thickening

- 目标：把 `runtime-execution` 补到 `unit + contracts + state-machine + fault-injection + recovery`。
- 输入：
  - `modules/runtime-execution/tests/`
  - `modules/runtime-execution/runtime/host/tests/`
- 输出：
  - 状态机回归
  - 故障注入回归
  - 恢复路径回归
- 依赖：`Phase A`
- 验证方式：
  - 至少覆盖 `timeout`、`disconnect-mid-flight`、`not-ready`、`feedback-missing`
- 停止条件：
  - 若执行真值落在错误 owner 层，先停边界审查

#### T205 Dispense Packaging Matrix Thickening

- 目标：把 `dispense-packaging` 补到 `unit + contracts + compatibility + golden + integration`。
- 输入：`modules/dispense-packaging/tests/`
- 输出：
  - 兼容性 contract
  - golden payload
  - 最小 integration
- 依赖：`T203`
- 验证方式：
  - 至少有一组 compatibility 样本和一组 golden
- 停止条件：
  - 若 compatibility 仅靠人工说明，没有正式样本，停止

### Phase D：收敛专项 authority 到正式策略

#### T301 P0-05 Gate Decision

- 目标：决定 `P0-05` negative matrix 是否进入 formal gate，还是保持 required evidence。
- 输入：
  - `docs/validation/online-test-matrix-v1.md`
  - `phase14` closeout
- 输出：明确 gate 结论与执行入口
- 依赖：`T003`
- 验证方式：
  - 文档、脚本入口、release policy 三者一致
- 停止条件：
  - 未形成统一失败归类前不进入 blocking

#### T302 P1-04 Gate Decision

- 目标：决定 `P1-04` HMI runtime action matrix 的长期定位。
- 输入：`phase14` closeout、HMI scripts
- 输出：`advisory` 或 `blocking` 结论
- 依赖：`T003`
- 验证方式：
  - `online-test-matrix-v1.md` 与 README 同步
- 停止条件：
  - 若 HMI matrix 仍缺明确 authority artifact，不进入 blocking

#### T303 P1-01 Gate Decision

- 目标：决定 `P1-01` HIL TCP recovery 是否继续保持专项 authority。
- 输入：`phase15` closeout
- 输出：长期策略结论
- 依赖：`T003`
- 验证方式：
  - gate policy 与 closeout 口径一致
- 停止条件：
  - 若 recovery 语义仍仅覆盖 TCP session reconnect，不外推到 broader gate

#### T304 Matrix And Gate Sync

- 目标：把 `Phase D` 的策略结果同步到矩阵文档和验证入口说明。
- 输入：`T301-T303` 输出
- 输出：
  - 更新后的 `online-test-matrix-v1.md`
  - 更新后的 `docs/validation/README.md`
  - 必要时更新专项 README
- 依赖：`T301`、`T302`、`T303`
- 验证方式：
  - 文档口径一致
  - 不存在同一专项项多种定位
- 停止条件：
  - 若入口脚本行为与文档不一致，先修正文档或脚本，不发布 closeout

### Phase E：正式化 `stress / soak`

#### T401 Soak Profile Freeze

- 目标：冻结正式 `2h / 8h / 24h` `soak` profile 的样本、入口、指标和结论口径。
- 输入：
  - `docs/testing/08-baseline-performance-and-soak.md`
  - 现有 `phase15` soak 证据
- 输出：
  - soak profile 定义
  - blocking / advisory 策略
- 依赖：`Phase D`
- 验证方式：
  - 每个 profile 都有固定入口和固定指标
- 停止条件：
  - 仍只有“跑过了”而没有趋势字段时，不算完成

#### T402 Soak Evidence Contract

- 目标：为 soak 输出正式 report / bundle / trend summary。
- 输入：现有 `tests/reports/` contract
- 输出：
  - soak 报告结构
  - 趋势摘要字段
  - 失败/异常点摘要
- 依赖：`T401`
- 验证方式：
  - soak 结果能输出结构化趋势和异常摘要
- 停止条件：
  - 若 soak 结果仍无法复盘，停止

#### T403 Soak Lane Integration

- 目标：决定 soak 如何接入 `nightly` 或专项 lane。
- 输入：`T401`、`T402`
- 输出：
  - lane 策略
  - 调度入口
  - closeout 口径
- 依赖：`T402`
- 验证方式：
  - root entry、profile、report root 一致
- 停止条件：
  - 若策略尚未冻结，不将 soak 接入默认 root entry

## 6. 串并行关系

- 串行：
  - `Phase A -> Phase B -> Phase C -> Phase D -> Phase E`
  - `T001 -> T002 -> T101/T102/T103/T104/T105`
  - `T003 -> T301/T302/T303 -> T304`
  - `T401 -> T402 -> T403`
- 可并行：
  - `T101`、`T102`、`T103`、`T104`、`T105`
  - `T201`、`T202`
  - `T301`、`T302`、`T303`

## 7. 高风险任务

- `T204 Runtime Execution Matrix Thickening`
  - 原因：涉及状态机、故障注入、恢复语义，最容易误改 owner 与 authority artifact。
- `T304 Matrix And Gate Sync`
  - 原因：一旦专项策略与 gate 口径不一致，会直接造成“脚本行为”和“发布口径”双事实源。
- `T403 Soak Lane Integration`
  - 原因：容易把专项验证误提升为默认 blocking，或把高成本长稳验证错误塞进常规入口。

## 8. 阶段验收标准

### Phase A

- 所有模块和专项项都有明确状态
- 所有 owner 与 consumer-only 边界清晰
- 无“以后再说”的未定项

### Phase B

- 目标模块不再只有占位骨架
- 每个模块都有真实测试源码
- 每个模块都有最小样本与最小失败场景

### Phase C

- 所有 P0 模块达到基线要求的测试类型组合
- 不再依赖仓库级 E2E 代替模块 contract

### Phase D

- `P0-05`、`P1-01`、`P1-04`、`P1-05` 都有唯一策略定位
- README、矩阵文档、gate 文档一致

### Phase E

- `2h / 8h / 24h` soak 至少完成 profile 和报告 contract
- soak 能输出趋势摘要与异常摘要
- 是否 blocking 已冻结

## 9. 收尾要求

每完成一个 Phase，都必须额外产出：

- 一份 closeout 文档
- 已执行验证层级
- 残余风险
- 文档同步说明
- 下一阶段准入条件
