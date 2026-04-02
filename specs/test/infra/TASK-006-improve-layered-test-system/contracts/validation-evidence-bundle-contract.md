# Contract: Validation Evidence Bundle

## 1. 合同标识

- Contract ID: `validation-evidence-bundle-v1`
- Scope: `tests/reports/`, `ci.ps1`, `test.ps1`, `scripts/validation/run-local-validation-gate.ps1`, `tests/performance/`, `tests/e2e/`

## 2. 目的

定义分层测试体系在 quick gate、完整离线回归、性能画像和受限 HIL 下都必须输出的统一证据口径，使失败、阻断、已知失败、延后和通过结论都可追溯。

## 3. Report Roots

| 场景 | 默认报告根 |
|---|---|
| `test.ps1` workspace validation | `tests/reports/workspace-validation.*` |
| `ci.ps1` | `tests/reports/ci/` |
| local validation gate | `tests/reports/local-validation-gate/<timestamp>/` |
| performance | `tests/reports/performance/` |
| 受限 HIL / ad-hoc e2e | `tests/reports/` 下对应时间戳目录 |

## 4. Minimal Evidence Files

每次执行至少必须产出：

| 文件 | 必需 | 说明 |
|---|---|---|
| `summary.md` 或等价 Markdown 报告 | 是 | 面向人工快速审阅 |
| `summary.json` 或等价 machine-readable 报告 | 是 | 面向工具消费 |
| `case-index.json` 或等价明细 | 是 | 列出执行的 case、suite、layer、lane |
| `failure-details.json` | 失败或阻断时必需 | 记录失败分类与关键上下文 |
| `evidence-links.md` | 是 | 指向相关 baseline、样本、附加证据路径 |

## 5. Required Trace Fields

machine-readable 证据至少必须包含以下字段：

| 字段 | 必填 |
|---|---|
| `stage_id` | 是 |
| `artifact_id` | 是 |
| `module_id` | 是 |
| `workflow_state` | 是 |
| `execution_state` | 是 |
| `event_name` | 是 |
| `failure_code` | 失败、阻断或已知失败时必填 |
| `evidence_path` | 是 |

## 6. Verdict Semantics

| verdict | 含义 |
|---|---|
| `passed` | 本次执行达到预期层级通过条件 |
| `failed` | 发生真实回归或断言失败 |
| `blocked` | 前置条件不满足，无法进入目标层 |
| `known-failure` | 已知问题被观测到，不计入可信通过 |
| `skipped` | 有意跳过，必须附带理由 |
| `incomplete` | 证据不完整，结论不可用 |
| `deferred` | 风险被明确延后到更高层或后续执行 |

## 7. Pass And Fail Rules

1. `passed` 只能在必要证据文件齐全且 required trace fields 满足时出现。
2. `blocked` 必须说明是哪一层前置条件未满足。
3. `known-failure` 与 `flaky` 结果不得被聚合为正式门禁通过。
4. `incomplete` 必须被视为失败边界，而不是“基本通过”。
5. 升级到 HIL 的执行必须显式指出其前置离线层结论和残余风险。

## 8. Forbidden Patterns

以下情况视为违反本合同：

1. 只有控制台输出，没有机器可消费证据
2. 不同 lane 之间使用互不兼容的 verdict 语义
3. 失败报告缺少 `failure_code` 或 `evidence_path`
4. 用截图或口头说明替代原始报告文件

## 9. Closeout 条件

只有当以下条件同时满足时，evidence bundle 才可用于任务收尾：

1. 报告位于 canonical report roots
2. required trace fields 齐全
3. verdict 与实际执行层级一致
4. 报告能回链到对应 `ValidationRequest`、`TestCaseDefinition` 和 `TestAsset`
