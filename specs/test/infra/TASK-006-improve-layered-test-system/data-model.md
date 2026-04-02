# Phase 1 Data Model: Layered Test System

## 1. 模型目标

本数据模型描述的是“如何对仓库测试进行分层、路由、复用资产、注入故障并产出统一证据”的设计实体，不是运行时数据库设计。它服务于根级门禁、模块契约、仓库级场景、性能画像和受限 HIL 的统一规划。

## 2. 实体总览

| 实体 | 作用 | 核心关系 |
|---|---|---|
| `ValidationLayer` | 定义每一层验证的目标、升级条件和默认入口 | 约束 `TestCaseDefinition`、`ExecutionLane`、`ValidationRequest` |
| `TestCaseDefinition` | 描述一条测试的 owner、层级、风险标签和证据要求 | 依赖 `ValidationLayer`、`TestAsset`、`TestFixture`、`FaultScenario` |
| `ValidationRequest` | 表示一次由开发者、CI 或专项验证触发的验证请求 | 选择 `ExecutionLane` 与若干 `ValidationLayer` |
| `TestAsset` | 表示可复用的样本、baseline、协议样例或观测数据 | 被 `TestCaseDefinition` 和 `EvidenceBundle` 引用 |
| `TestFixture` | 表示跨模块复用的 fake、simulator、clock、report emitter 等支撑能力 | 被 `TestCaseDefinition` 使用 |
| `FaultScenario` | 表示命名化的异常或边界场景 | 约束 `TestCaseDefinition` 和 `EvidenceBundle` 的预期结果 |
| `ExecutionLane` | 表示一条按成本与风险分类的运行通道 | 聚合若干 `ValidationLayer`，产出 `EvidenceBundle` |
| `EvidenceBundle` | 表示一次验证执行输出的统一证据包 | 关联 `ValidationRequest`、`TestCaseDefinition`、`TestAsset` |
| `StabilityRecord` | 记录测试稳定性与门禁可用性 | 约束 `TestCaseDefinition` 是否允许承担正式 gate |

## 3. 实体定义

### 3.1 `ValidationLayer`

**说明**: 一类具有固定验证目标、成本边界和升级顺序的测试层。

| 字段 | 类型 | 约束 |
|---|---|---|
| `layer_id` | string | 唯一，如 `L0-structure-gate` |
| `layer_kind` | enum | `structure-gate`, `module-contract`, `offline-integration`, `simulated-e2e`, `performance`, `limited-hil`, `closeout` |
| `owner_surface` | text | 必填，描述本层主要负责的验证面 |
| `default_entry_refs` | list[path] | 至少一个 canonical root entry |
| `allowed_environments` | list[enum] | `offline`, `simulated`, `ci`, `hardware-limited` |
| `upgrade_prerequisites` | list[string] | 高成本层必须显式声明前置层 |
| `required_evidence_profile` | string | 必填 |
| `default_enabled` | boolean | 必填 |

**固定语义**

| 层 | 主要入口 | 目标 |
|---|---|---|
| `L0-structure-gate` | `scripts/migration/validate_workspace_layout.py`、`scripts/validation/run-local-validation-gate.ps1` | 校验布局、根级入口、冻结文档与基本门禁 |
| `L1-module-contract` | `tests/CMakeLists.txt` 聚合的 module test roots | 校验 owner 语义输出、错误分类与状态迁移 |
| `L2-offline-integration` | `tests/integration/`、`protocol-compatibility` suite | 校验跨模块链路和协议边界 |
| `L3-simulated-e2e` | `tests/e2e/simulated-line/` | 校验成功、阻断、回滚、恢复、归档主场景 |
| `L4-performance` | `tests/performance/` | 校验基线、漂移和长时运行画像 |
| `L5-limited-hil` | `tests/e2e/hardware-in-loop/` | 校验模拟无法充分证明的真实硬件链路 |
| `L6-closeout` | `ci.ps1`、closeout 报告 | 汇总已执行层级、残余风险与文档影响 |

**校验规则**

- `limited-hil` 只能在至少一个离线层通过后进入执行计划。
- `performance` 不能替代正确性层。
- `closeout` 只能聚合已执行证据，不能单独作为“已验证”结论来源。

### 3.2 `TestCaseDefinition`

**说明**: 一条可被根级入口或专项验证通道调度的测试定义。

| 字段 | 类型 | 约束 |
|---|---|---|
| `case_id` | string | 唯一 |
| `title` | string | 必填 |
| `owner_scope` | string | 必填，使用稳定模块或责任域名 |
| `primary_layer_ref` | string | 引用 `ValidationLayer.layer_id` |
| `suite_refs` | list[string] | 至少一个 suite 或 surface |
| `scenario_class` | enum | `success`, `block`, `rollback`, `recovery`, `archive`, `compatibility`, `performance`, `fault` |
| `risk_tags` | list[string] | 至少一个风险标签 |
| `required_asset_refs` | list[string] | 引用 `TestAsset.asset_id` |
| `required_fixture_refs` | list[string] | 引用 `TestFixture.fixture_id` |
| `fault_refs` | list[string] | 可空；异常测试时必须引用 `FaultScenario.fault_id` |
| `evidence_profile` | string | 必填 |
| `stability_state` | enum | `stable`, `known-failure`, `flaky`, `quarantined` |

**校验规则**

- 每条测试只能有一个 `primary_layer_ref`，避免同一用例跨层双重记账。
- `stability_state = flaky` 或 `quarantined` 的测试不得承担正式 gate。
- 仓库级 `tests/` 下的用例仍必须声明 `owner_scope`，不得把 owner 漂移给仓库根。

### 3.3 `ValidationRequest`

**说明**: 一次验证触发的路由请求。

| 字段 | 类型 | 约束 |
|---|---|---|
| `request_id` | string | 唯一 |
| `trigger_source` | enum | `local`, `pull-request`, `merge`, `nightly`, `release`, `manual-investigation` |
| `changed_scopes` | list[string] | 至少一个 |
| `risk_profile` | enum | `low`, `medium`, `high`, `hardware-sensitive` |
| `desired_depth` | enum | `quick`, `full-offline`, `nightly`, `hil` |
| `selected_lane_ref` | string | 引用 `ExecutionLane.lane_id` |
| `selected_layer_refs` | list[string] | 至少一个 `ValidationLayer.layer_id` |
| `skipped_layer_refs` | list[string] | 可空 |
| `skip_justification` | text | 跳过任一相关层时必填 |
| `requested_at` | datetime | 必填 |

**状态迁移**

```text
received -> classified -> routed -> executed -> closed
received/classified/routed -> blocked
blocked -> routed
```

**路由规则**

- `hardware-sensitive` 请求默认不能跳过 `full-offline`。
- `desired_depth = quick` 至少包含 `L0` 和相关 `L1` / `L2`。
- `desired_depth = hil` 必须显式记录被哪些离线层前置放行。

### 3.4 `TestAsset`

**说明**: 可复用的静态测试事实资产。

| 字段 | 类型 | 约束 |
|---|---|---|
| `asset_id` | string | 唯一 |
| `asset_kind` | enum | `sample-input`, `golden-baseline`, `protocol-fixture`, `trace-snapshot`, `performance-baseline`, `manual-checklist` |
| `canonical_path` | path | 必须位于 `samples/`、`tests/baselines/`、`docs/validation/`、`shared/contracts/` 等 canonical roots |
| `owner_scope` | string | 必填 |
| `source_of_truth` | text | 必填 |
| `shared_reuse_allowed` | boolean | 必填 |
| `lifecycle_state` | enum | `active`, `deprecated`, `blocked` |

**校验规则**

- 不允许把 `tests/reports/` 产物声明为 `source_of_truth`。
- `golden-baseline` 与 `performance-baseline` 必须可回链到稳定样本。
- 共享资产若被多个 owner 复用，仍必须声明单一事实来源。

### 3.5 `TestFixture`

**说明**: 用于表达依赖替身、时序控制、故障注入和证据采集的复用型支撑能力。

| 字段 | 类型 | 约束 |
|---|---|---|
| `fixture_id` | string | 唯一 |
| `fixture_kind` | enum | `fake-gateway`, `fake-controller`, `fake-io`, `fake-clock`, `simulator`, `report-emitter`, `assertion-kit` |
| `provider_path` | path | 通常位于 `shared/testing/` 或 `tests/` canonical roots |
| `supported_layer_refs` | list[string] | 至少一个 `ValidationLayer.layer_id` |
| `external_dependency_mode` | enum | `none`, `offline-only`, `simulated`, `hardware-limited` |
| `reusable_across_modules` | boolean | 必填 |
| `notes` | text | 可空 |

**校验规则**

- 跨模块复用 fixture 优先落在 `shared/testing/`。
- `fixture_kind` 不得承载生产业务 owner 逻辑。
- `hardware-limited` fixture 不得成为 quick gate 默认依赖。

### 3.6 `FaultScenario`

**说明**: 命名化的异常或边界场景。

| 字段 | 类型 | 约束 |
|---|---|---|
| `fault_id` | string | 唯一 |
| `scenario_kind` | enum | `not-ready`, `timeout`, `disconnect`, `duplicate-event`, `out-of-order`, `rollback`, `recovery`, `resource-missing` |
| `injection_point` | text | 必填 |
| `applicable_layer_refs` | list[string] | 至少一个 |
| `expected_outcome` | enum | `blocked`, `failed`, `recovered`, `rolled-back`, `deferred` |
| `required_evidence_fields` | list[string] | 至少一个 |
| `safety_level` | enum | `offline-safe`, `simulated-safe`, `hardware-bounded` |

**校验规则**

- `hardware-bounded` fault 只能绑定 `L5-limited-hil`。
- `duplicate-event`、`out-of-order` 等时序 fault 应优先在 `L2` 或 `L3` 模拟层验证。

### 3.7 `ExecutionLane`

**说明**: 按执行成本、风险与触发时机定义的运行通道。

| 字段 | 类型 | 约束 |
|---|---|---|
| `lane_id` | string | 唯一 |
| `lane_kind` | enum | `quick-gate`, `full-offline-gate`, `nightly-performance`, `limited-hil` |
| `entrypoint_refs` | list[path] | 至少一个 root entry |
| `default_suite_set` | list[string] | 至少一个 suite |
| `allowed_layer_refs` | list[string] | 至少一个 `ValidationLayer.layer_id` |
| `report_root` | path | 位于 `tests/reports/` canonical tree |
| `default_fail_policy` | enum | `fail-fast`, `collect-and-report`, `manual-signoff-required` |
| `human_approval_required` | boolean | 必填 |

**固定通道**

| 通道 | 默认层 | 主要入口 |
|---|---|---|
| `quick-gate` | `L0`, `L1`, 相关 `L2` | `build.ps1`, `test.ps1`, `run-local-validation-gate.ps1` |
| `full-offline-gate` | `L0-L4` | `test.ps1`, `ci.ps1` |
| `nightly-performance` | `L2-L4` | `ci.ps1`, `tests/performance/*` |
| `limited-hil` | `L5`, `L6` | `test.ps1 -IncludeHardwareSmoke/-IncludeHil*`, 受限联机脚本 |

### 3.8 `EvidenceBundle`

**说明**: 一次验证执行后落地的结构化证据集合。

| 字段 | 类型 | 约束 |
|---|---|---|
| `bundle_id` | string | 唯一 |
| `request_ref` | string | 引用 `ValidationRequest.request_id` |
| `producer_lane_ref` | string | 引用 `ExecutionLane.lane_id` |
| `case_refs` | list[string] | 至少一个 `TestCaseDefinition.case_id` |
| `report_root` | path | 指向 `tests/reports/` 下的具体目录 |
| `summary_file` | path | 必填 |
| `machine_file` | path | 必填 |
| `required_trace_fields` | list[string] | 至少包含仓库规定字段 |
| `verdict` | enum | `passed`, `failed`, `blocked`, `known-failure`, `skipped`, `incomplete`, `deferred` |
| `linked_asset_refs` | list[string] | 引用 `TestAsset.asset_id` |
| `generated_at` | datetime | 必填 |

**必要 trace 字段**

- `stage_id`
- `artifact_id`
- `module_id`
- `workflow_state`
- `execution_state`
- `event_name`
- `failure_code`
- `evidence_path`

**状态迁移**

```text
collecting -> summarized -> published
collecting -> failed
summarized -> incomplete
```

### 3.9 `StabilityRecord`

**说明**: 测试稳定性与门禁可用性记录。

| 字段 | 类型 | 约束 |
|---|---|---|
| `record_id` | string | 唯一 |
| `case_ref` | string | 引用 `TestCaseDefinition.case_id` |
| `status` | enum | `stable`, `known-failure`, `flaky`, `quarantined` |
| `first_seen_at` | datetime | 必填 |
| `last_seen_at` | datetime | 必填 |
| `gate_allowed` | boolean | 必填 |
| `owner_scope` | string | 必填 |

**判定规则**

- `status = flaky` 或 `quarantined` 时，`gate_allowed` 必须为 `false`。
- `known-failure` 可以保留为观测性执行，但必须明确不计入可信通过结论。

## 4. 关系约束

```text
ValidationLayer
├── n x TestCaseDefinition
├── n x ExecutionLane
└── n x ValidationRequest

ValidationRequest
├── 1 x ExecutionLane
├── n x ValidationLayer
└── 1 x EvidenceBundle

TestCaseDefinition
├── n x TestAsset
├── n x TestFixture
├── n x FaultScenario
└── 1 x StabilityRecord

EvidenceBundle
├── n x TestCaseDefinition
└── n x TestAsset
```

## 5. 完成判定闭环

一次分层验证只有在以下条件同时满足时，才能进入可信 closeout：

1. `ValidationRequest` 已完成分类与路由，且所有跳层理由明确。
2. 相关 `TestCaseDefinition` 全部绑定到正确 `ValidationLayer` 与 `owner_scope`。
3. 所需 `TestAsset` 与 `TestFixture` 均来自 canonical roots，且没有第二事实源。
4. 若触发了 `FaultScenario`，其结果与 `expected_outcome` 一致。
5. `EvidenceBundle` 已发布到 `tests/reports/`，并包含必要 trace 字段。
6. 进入正式 gate 的测试，其 `StabilityRecord.gate_allowed = true`。
