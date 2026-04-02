# Contract: Shared Test Assets And Fixtures

## 1. 合同标识

- Contract ID: `shared-test-assets-fixtures-v1`
- Scope: `samples/`, `tests/baselines/`, `shared/testing/`, `docs/validation/`, `tests/reports/`

## 2. 目的

定义分层测试体系中共享样本、baseline、fixture 和故障注入能力的 canonical 放置规则与复用边界，避免重复维护第二事实源。

## 3. Canonical Locations

| 根路径 | 允许内容 | 禁止内容 |
|---|---|---|
| `samples/` | 真实输入样本、代表性场景输入、可复用静态工程事实 | 运行报告、临时日志、模块私有 scratch 数据 |
| `tests/baselines/` | golden baseline、对照快照、性能基线 | 动态生成的临时输出 |
| `shared/testing/` | 跨模块复用 fixture、assertion、workspace validation toolkit | 业务 owner 逻辑、生产运行代码 |
| `docs/validation/` | 联机 / 手工 / 受限验证清单、观察点说明、停机条件 | 机器生成报告主存储 |
| `tests/reports/` | 机器生成的 evidence bundle、summary、JSON/Markdown 报告 | 被当作 source of truth 的 baseline |

## 4. Asset Catalog Requirements

每个共享资产至少必须具备以下信息：

| 字段 | 说明 |
|---|---|
| `asset_id` | 稳定标识 |
| `asset_kind` | `sample-input`、`golden-baseline`、`protocol-fixture`、`performance-baseline`、`manual-checklist` |
| `canonical_path` | 资产路径 |
| `owner_scope` | 主要 owner |
| `source_of_truth` | 事实来源 |
| `reuse_policy` | `shared` 或 `owner-only` |
| `lifecycle_state` | `active`、`deprecated`、`blocked` |

## 5. Fixture Requirements

共享 fixture 至少必须声明：

| 字段 | 说明 |
|---|---|
| `fixture_id` | 稳定标识 |
| `fixture_kind` | `fake-gateway`、`fake-controller`、`fake-io`、`fake-clock`、`simulator`、`report-emitter` |
| `provider_path` | 代码路径 |
| `supported_layers` | 可用于哪些验证层 |
| `dependency_mode` | `offline-only`、`simulated`、`hardware-limited` |
| `fault_points` | 可注入哪些异常 |

## 6. Reuse Rules

1. 多模块共用的样本、baseline 和 fixture 必须优先放在 canonical shared roots。
2. 模块私有测试可以有私有样本，但不得把它们描述为仓库级 canonical fact。
3. 共享 fixture 只能表达测试支撑，不得悄悄承接业务 owner 语义。
4. `tests/reports/` 产物只能作为执行证据，不能回写成 baseline。
5. 新增 fault scenario 时，必须明确 injection point、expected outcome 和 evidence requirement。

## 7. Forbidden Patterns

以下情况视为违反本合同：

1. 在 `shared/testing/` 中堆积业务实现
2. 在多个模块各自维护同名 canonical sample 或 baseline
3. 使用 `tests/reports/` 最近一次输出替代正式 baseline
4. 为了临时通过测试，把真实样本替换成无法回链的 synthetic 数据而不声明

## 8. Closeout 条件

只有当以下条件同时满足时，资产与夹具治理才可视为完成：

1. 共享样本、baseline 和 fixture 已全部归位到 canonical roots
2. 没有第二套 canonical fact source
3. 复用规则与 owner 边界都已明确
4. 相关执行证据都能回链到对应资产与 fixture
