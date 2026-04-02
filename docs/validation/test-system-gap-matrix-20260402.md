# Test System Gap Matrix

更新时间：`2026-04-02`

## 1. 目的

本文件冻结“测试体系基准完全落地”之前的当前缺口、owner 边界和专项策略真值。

它服务于 `Phase A / T001-T003`，用于回答三件事：

1. 哪些模块已经基本达标，哪些仍是空心骨架，哪些只是薄覆盖。
2. 模块内 tests 与仓库级 tests 的 owner 边界应如何保持。
3. 已存在的专项 authority 当前应保持什么策略定位。

## 2. 范围与非范围

### 2.1 范围

- `docs/testing/03-baseline-module-test-matrix.md`
- `docs/validation/online-test-matrix-v1.md`
- `modules/*/tests`
- `apps/*/tests`
- `shared/*/tests`
- `tests/`

### 2.2 非范围

- 不在本文件中新增代码实现。
- 不在本文件中直接修改 gate 行为。
- 不把专项 authority 在本文件中直接提升为 blocking policy。

## 3. 模块状态矩阵

状态定义：

- `empty`
  - 已有正式 `tests/` 根，但当前仍以 `README + .gitkeep` 为主，没有真实测试源码。
- `thin`
  - 已有真实测试源码和正式入口，但未达到基线要求的测试类型组合。
- `healthy`
  - 已形成较明确、较稳定的模块测试面，当前不作为优先补齐对象。

| 模块 | 基线优先级 | 基线最低测试面 | 当前状态 | 当前判断 | 下一步出口 |
| --- | --- | --- | --- | --- | --- |
| `job-ingest` | `P0` | `unit, contracts, golden, integration` | `thin` | 已有正式入口，但当前可见真实测试仍偏薄 | `Phase C / T201` |
| `dxf-geometry` | `P0` | `unit, contracts, golden, integration` | `thin` | 已有正式入口，但当前可见真实测试仍偏薄 | `Phase C / T202` |
| `topology-feature` | `P1` | `unit, contracts, golden, integration` | `empty` | 当前仍是占位骨架 | `Phase B / T101` |
| `process-planning` | `P1` | `unit, contracts, golden` | `empty` | 当前仍是占位骨架 | `Phase B / T102` |
| `coordinate-alignment` | `P1` | `unit, contracts, golden` | `empty` | 当前仍是占位骨架 | `Phase B / T103` |
| `process-path` | `P0` | `unit, contracts, golden, integration` | `thin` | 已接入，但仍缺完整最小矩阵 | `Phase C / T203` |
| `motion-planning` | `P0` | `unit, contracts, golden, performance, integration` | `healthy` | 当前已形成较厚单测面 | 维持，后续按相邻链路回归补强 |
| `dispense-packaging` | `P0` | `unit, contracts, compatibility, golden, integration` | `thin` | 单测厚度可用，但兼容 / golden / integration 仍需补全确认 | `Phase C / T205` |
| `runtime-execution` | `P0` | `unit, contracts, state-machine, fault-injection, recovery` | `thin` | 顶层 `tests/` 偏薄，真实测试分布在 `runtime/host/tests/` | `Phase C / T204` |
| `workflow` | `P0` | `unit, contracts, integration, fault-injection` | `healthy` | 当前是最成熟模块之一 | 维持，做邻近回归 |
| `trace-diagnostics` | `P1` | `contracts, evidence, observability` | `empty` | 当前仍是占位骨架 | `Phase B / T104` |
| `hmi-application` | `P2` | `presenter/view-model, startup-state, protocol` | `thin` | 已有最小单测，但状态投影面仍未收口 | `Phase B / T105` |
| `apps/runtime-service` | `P0` | `unit, contracts, protocol, service-integration` | `healthy` | 已形成较清晰模块测试面 | 维持，做协议邻近回归 |
| `shared/kernel` | `P0` | `unit, contracts` | `healthy` | 已有最小共享能力测试面 | 维持 |

## 4. Owner 边界冻结

### 4.1 模块内 tests 的 owner 责任

- `modules/*/tests`
  - 负责 owner 级 `unit / contracts / golden / fault-injection / recovery` 等证明。
- `apps/*/tests`
  - 负责宿主、bootstrap、service-integration、consumer 侧协议接线。
- `shared/*/tests`
  - 负责共享基础能力和共享 contract，不承担跨 owner 主链拼装。

### 4.2 仓库级 tests 的 consumer-only 责任

- `tests/contracts/`
  - 冻结 repo-level contract、治理规则、证据结构和 shared asset contract。
- `tests/integration/`
  - 承担跨 owner 装配和共享样本消费一致性验证。
- `tests/e2e/`
  - 承担 simulated-line、limited-HIL 和专项联机 authority。
- `tests/performance/`
  - 承担 `performance / stress / soak` 的正式入口和报告。

### 4.3 当前禁止的 owner 漂移

- 禁止把仓库级 `tests/` 当成 `topology-feature`、`process-planning`、`coordinate-alignment`、`trace-diagnostics` 的长期 owner 级补丁场。
- 禁止把 `apps/hmi-app/scripts/` 当成 `modules/hmi-application` 状态投影测试的替代 owner。
- 禁止把 `tests/reports/` 的最新输出反写成 baseline 或 golden 真值。

## 5. 专项 authority 策略冻结

策略定义：

- `blocking`
  - 当前或后续应进入正式 gate，失败会阻断发布或正式验证通道。
- `advisory`
  - 当前保留为正式专项入口，但不直接阻断正式 gate。
- `specialized authority`
  - 当前只作为专项真值和 closeout 证据，不接 release blocking policy。

| 项目 | 当前事实 | 冻结策略 | 原因 | 升级条件 |
| --- | --- | --- | --- | --- |
| `P0-05 canonical dry-run negative matrix` | 已存在正式入口和 summary，但 `phase14` 明确未接 `limited-hil` formal blocking gate | `advisory` | 已具备较稳定 authority，但仍属于受控阻断专项，不应在未补齐策略前直接阻断 controlled release | failure classification、override 规则、release policy 三者冻结一致 |
| `P1-04 HMI runtime action matrix` | 已存在 matrix runner 和 summary authority，但未接正式 gate | `advisory` | 适合作为持续专项回归，不适合当前直接变成 release blocker | 运行 profile、失败分类、CI/nightly 调度冻结完成 |
| `P1-01 HIL TCP recovery` | 已具备 `10` 轮验证和 `30` 轮 soak，但当前语义仅覆盖 `TCP session disconnect/reconnect recovery` | `specialized authority` | 语义范围仍窄，不应外推到 broader recovery gate | recovery 语义扩展到 crash / power / physical disconnect，并补足失败分类与 operator 策略 |
| `P1-05 30min+ soak` | 已有专项 closeout，但未形成统一 soak 体系 | `specialized authority` | 当前仍是过渡性专项，不是正式 profile | `2h / 8h / 24h` soak profile、report contract、lane policy 全冻结后再升级 |

## 6. 当前执行边界

基于本文件冻结结果，后续各阶段的默认边界如下：

### 6.1 `Phase B`

- 允许修改：
  - 目标模块自身 `tests/`
  - 必要的模块 README
  - 必要样本 / golden / contract 工件
- 禁止修改：
  - root lane policy
  - controlled HIL 脚本行为
  - release gate 逻辑

### 6.2 `Phase C`

- 允许修改：
  - P0 模块 owner 测试
  - 必要相邻 integration
  - 必要 repo-level contract
- 禁止修改：
  - 用仓库级 E2E 替代模块内 contract
  - 借补测试顺手改设备语义 owner

### 6.3 `Phase D`

- 允许修改：
  - `docs/validation/online-test-matrix-v1.md`
  - `docs/validation/README.md`
  - 专项 README
  - 必要 gate 文档
- 禁止修改：
  - 在未冻结策略前直接调整 blocking 行为

### 6.4 `Phase E`

- 允许修改：
  - `tests/performance/`
  - `tests/e2e/hardware-in-loop/` 中 soak 入口
  - `tests/reports/` soak report contract
- 禁止修改：
  - 把专项 soak 直接冒充正式 blocking baseline

## 7. Phase A 完成判定

当满足以下条件时，`Phase A` 视为完成：

1. 所有模块都已被归类到 `empty / thin / healthy`。
2. `tests/` 与模块内 `tests/` 的 owner 边界已冻结。
3. `P0-05`、`P1-01`、`P1-04`、`P1-05` 都有唯一策略定位。
4. 后续阶段的默认变更边界已写明。

## 8. 下一步

- 进入 `Phase B`
- 优先顺序：
  1. `T101 topology-feature`
  2. `T102 process-planning`
  3. `T103 coordinate-alignment`
  4. `T104 trace-diagnostics`
  5. `T105 hmi-application`
