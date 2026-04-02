# Contract: Layered Validation Routing

## 1. 合同标识

- Contract ID: `layered-validation-routing-v1`
- Scope: `build.ps1`, `test.ps1`, `ci.ps1`, `scripts/validation/run-local-validation-gate.ps1`, `shared/testing/test-kit`, `tests/`

## 2. 目的

定义一次改动如何被路由到合适的验证层和执行通道，确保 quick gate、完整离线回归、性能画像和受限 HIL 使用同一套分类语义，而不是各自发明规则。

## 3. Canonical Validation Layers

| Layer | 目标 | 默认承载面 | 默认进入条件 |
|---|---|---|---|
| `L0-structure-gate` | 校验布局、根级入口和冻结门禁 | `scripts/migration/`、`scripts/validation/` | 任意非平凡改动 |
| `L1-module-contract` | 校验模块 owner 的输入、输出、错误和状态契约 | canonical module `*/tests` | 影响单模块语义输出 |
| `L2-offline-integration` | 校验跨模块链路与协议衔接 | `tests/integration/` | 影响两个及以上 owner，或影响协议 / 执行链 |
| `L3-simulated-e2e` | 校验模拟全链路 acceptance 场景 | `tests/e2e/simulated-line/` | 影响主流程、状态机或故障恢复 |
| `L4-performance` | 校验基线、漂移与长时运行画像 | `tests/performance/` | 影响耗时、缓存、single-flight 或大样本路径 |
| `L5-limited-hil` | 校验模拟无法充分证明的真实硬件风险 | `tests/e2e/hardware-in-loop/` | 离线层已通过且剩余风险仍需真实硬件确认 |
| `L6-closeout` | 汇总已执行层、残余风险和文档影响 | `tests/reports/`、closeout 文档 | 进入任务收尾 |

## 4. Required Test Metadata

每条可调度测试至少必须声明以下字段：

| 字段 | 说明 |
|---|---|
| `case_id` | 稳定测试标识 |
| `owner_scope` | 主要 owner 模块或责任域 |
| `primary_layer` | 主验证层 |
| `suite_ref` | 所属 suite 或 surface |
| `risk_tags` | 风险分类 |
| `required_assets` | 依赖的样本、baseline 或协议资产 |
| `required_fixtures` | 依赖的 fake、simulator、clock 或 report helper |
| `evidence_profile` | 必要证据类型 |
| `stability_state` | `stable`、`known-failure`、`flaky`、`quarantined` |

## 5. Routing Rules

1. 任意非平凡改动必须先经过 `L0-structure-gate`。
2. 只影响单模块纯逻辑或语义输出的改动，默认进入 `L1-module-contract`，不直接跳到 `L3` 或 `L5`。
3. 影响协议边界、跨模块编排、执行准备或状态反馈的改动，必须进入 `L2-offline-integration`。
4. 影响 success / block / rollback / recovery / archive 主场景的改动，必须进入 `L3-simulated-e2e`。
5. 影响耗时、缓存、single-flight 或大样本处理的改动，必须补 `L4-performance`。
6. 只有当模拟层不能充分证明风险时，才允许升级到 `L5-limited-hil`。
7. 任一相关层被跳过时，必须记录 `skip justification` 并在 closeout 中声明残余风险。

## 6. Execution Lanes

| Lane | 允许层 | 默认入口 |
|---|---|---|
| `quick-gate` | `L0`, `L1`, 相关 `L2` | `build.ps1`, `test.ps1`, `run-local-validation-gate.ps1` |
| `full-offline-gate` | `L0-L4` | `test.ps1`, `ci.ps1` |
| `nightly-performance` | `L2-L4` | `ci.ps1`, `tests/performance/*` |
| `limited-hil` | `L5`, `L6` | `test.ps1 -IncludeHardwareSmoke/-IncludeHil*` |

## 7. Forbidden Patterns

以下情况视为违反本合同：

1. 为分层体系新增第二套默认 root runner
2. 让 `flaky` 或 `quarantined` 测试承担正式 gate
3. 在没有离线前置层放行的情况下直接进入 HIL
4. 把仓库级 `tests/` 当作业务 owner，导致 `owner_scope` 缺失
5. 让 quick gate 依赖真实硬件、人工交互或不可重复环境

## 8. Closeout 条件

只有当以下条件同时满足时，分层路由才可视为完成：

1. 相关测试都能映射到唯一 `primary_layer`
2. 相关执行通道都只调用 canonical root entry points
3. 跳层、延后和 HIL 升级理由均已记录
4. 证据输出与 `validation-evidence-bundle-v1` 一致
