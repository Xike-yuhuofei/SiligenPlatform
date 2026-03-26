# Phase 1 Data Model: 工作区架构模板对齐重构

## 1. 模型目标

本数据模型描述的不是运行时数据库，而是本特性在“规范冻结 + 仓库收敛”层面的核心实体。它为 9 轴正式文档、模块边界契约、差距决策和验收证据提供统一结构。

## 2. 实体总览

| 实体 | 作用 | 核心关系 |
|---|---|---|
| `ArchitectureBaseline` | 当前工作区的冻结架构基线 | 聚合 `ReferenceAxisMapping`、`StageDefinition`、`ArtifactDefinition`、`ModuleContract`、`ControlSemantics`、`AcceptanceBaseline`、`GapResolutionItem` |
| `ReferenceAxisMapping` | 参考模板轴与当前工作区正式文档的映射 | 归属 `ArchitectureBaseline`，关联 `GapResolutionItem` |
| `StageDefinition` | 在范围阶段的输入/输出/成败/追溯定义 | 由 `ModuleContract` 拥有或消费，引用 `ArtifactDefinition` |
| `ArtifactDefinition` | 事实对象的 owner、生命周期和引用规则 | 被 `StageDefinition` 生产/消费，被 `ModuleContract` 拥有 |
| `ModuleContract` | 模块 owner、路径落位、允许/禁止协作 | 生产/消费 `ArtifactDefinition`，受 `WorkspaceAreaDecision` 约束 |
| `ControlSemantics` | 状态机、命令、事件、阻断、回退、恢复、归档规则 | 约束 `StageDefinition` 与 `ModuleContract` |
| `AcceptanceBaseline` | 测试与验收矩阵、证据路径、阻断规则 | 验证 `StageDefinition`、`ArtifactDefinition`、`ModuleContract` |
| `GapResolutionItem` | 当前事实与目标模板之间的一条差距及其处理决策 | 关联 `ReferenceAxisMapping`、`ModuleContract`、`WorkspaceAreaDecision` |
| `WorkspaceAreaDecision` | 工作区路径的归属、边界和去留决策 | 约束 `ModuleContract` 与仓库收敛动作 |

## 3. 实体定义

### 3.1 `ArchitectureBaseline`

**说明**: 当前工作区的唯一正式冻结基线。

| 字段 | 类型 | 约束 |
|---|---|---|
| `baseline_id` | string | 唯一，例如 `dsp-e2e-spec-v1` |
| `reference_pack_root` | path | 必须指向 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\` |
| `target_docset_root` | path | 必须指向 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\` |
| `workspace_scope` | list[path] | 必须覆盖 `apps\`, `packages\`, `integration\`, `tools\`, `docs\`, `config\`, `data\`, `examples\` |
| `formal_term_policy` | enum | 固定为 `reference-terms-only` |
| `freeze_status` | enum | `draft`, `aligned`, `reviewed`, `frozen` |
| `repo_alignment_status` | enum | `not-started`, `in-progress`, `aligned`, `verified` |
| `created_at` | datetime | 必填 |
| `updated_at` | datetime | 必填 |

**校验规则**

- `freeze_status = frozen` 前，9 个 `ReferenceAxisMapping` 必须全部存在。
- `repo_alignment_status = verified` 前，所有 `GapResolutionItem` 必须为 `verified` 或 `accepted-exception`。
- 不允许存在第二个并行 `ArchitectureBaseline` 作为默认规范源。

### 3.2 `ReferenceAxisMapping`

**说明**: 参考模板单轴与当前工作区正式文档的对应关系。

| 字段 | 类型 | 约束 |
|---|---|---|
| `axis_id` | string | `S01` ... `S09`; `S10` 仅作陪伴索引 |
| `reference_doc_path` | path | 指向 `docs\architecture\dsp-e2e-spec\*.md` |
| `target_doc_path` | path | 指向 `docs\architecture\dsp-e2e-spec\*.md` |
| `coverage_scope` | text | 必须说明覆盖的阶段/对象/模块/测试维度 |
| `current_fact_sources` | list[path] | 至少一个工作区事实来源 |
| `primary_review_role` | enum | `architecture`, `business`, `test`, `cross-functional` |
| `mapping_status` | enum | `draft`, `mapped`, `reviewed`, `frozen` |

**校验规则**

- `axis_id` 唯一。
- `reference_doc_path` 与 `target_doc_path` 必须一一对应，不允许多轴合并成一个目标文档。
- `mapping_status = frozen` 前必须具备至少一个 `current_fact_sources` 和至少一个关联的 `GapResolutionItem` 或显式声明“无差距”。

### 3.3 `StageDefinition`

**说明**: 业务阶段的正式职责与验收定义。

| 字段 | 类型 | 约束 |
|---|---|---|
| `stage_id` | string | 采用参考模板阶段编号，如 `S0`、`S11A`、`S16` |
| `stage_name` | string | 只能使用参考模板规范名 |
| `owner_module_id` | string | 必须引用 `ModuleContract.module_id` |
| `input_artifact_refs` | list[string] | 引用 `ArtifactDefinition.artifact_name` |
| `output_artifact_refs` | list[string] | 引用 `ArtifactDefinition.artifact_name` |
| `success_criteria` | list[text] | 至少 1 条 |
| `failure_criteria` | list[text] | 至少 1 条 |
| `minimum_trace_fields` | list[string] | 至少包含 `job_id`、`workflow_id`、阶段上下文主键 |
| `forbidden_shortcuts` | list[text] | 必填 |

**校验规则**

- `S11A` 与 `S11B` 必须分别建模，不允许合并。
- `S12` 的失败语义必须建模为执行门禁阻断，不能改写上游规划对象有效性。
- `S16` 必须显式区分 `ExecutionRecordBuilt` 与 `WorkflowArchived`。

### 3.4 `ArtifactDefinition`

**说明**: 参考模板中的事实对象或冻结文档要求的对象 owner 规则。

| 字段 | 类型 | 约束 |
|---|---|---|
| `artifact_name` | string | 采用参考模板规范名 |
| `owner_module_id` | string | 必须引用 `ModuleContract.module_id` |
| `category` | enum | `runtime-artifact`, `control-fact`, `governance-artifact` |
| `upstream_refs` | list[string] | 引用其他对象 |
| `downstream_refs` | list[string] | 引用其他对象 |
| `required_fields` | list[string] | 必填 |
| `conditional_required_rules` | list[text] | 条件必填规则 |
| `lifecycle_statuses` | list[string] | 至少覆盖 `valid`, `superseded`, `archived` 或明确说明不适用 |
| `mutation_policy` | text | 必须说明是否只允许新版本 |
| `trace_requirements` | list[string] | 必填 |

**校验规则**

- 所有核心运行时对象必须有唯一 owner，不能多 owner 并存。
- 对象进入 `superseded` 或 `archived` 时，必须由 owner 模块负责并产生可追溯事件。
- HMI/应用服务不得直接成为核心运行时对象的 owner。

**参考运行时对象链**

```text
JobDefinition
-> SourceDrawing
-> CanonicalGeometry
-> TopologyModel
-> FeatureGraph
-> ProcessPlan
-> CoordinateTransformSet
-> AlignmentCompensation
-> ProcessPath
-> MotionPlan
-> DispenseTimingPlan
-> ExecutionPackage
-> MachineReadySnapshot / FirstArticleResult
-> ExecutionRecord
```

### 3.5 `ModuleContract`

**说明**: 模块边界、目录落位和越权禁止规则。

| 字段 | 类型 | 约束 |
|---|---|---|
| `module_id` | string | `M0` ... `M11` |
| `canonical_name` | string | 采用参考模板规范名 |
| `current_path_cluster` | list[path] | 必须落在 canonical roots 或经批准的 support root |
| `target_owner_paths` | list[path] | 目标 owner 落位 |
| `owned_artifacts` | list[string] | 引用 `ArtifactDefinition` |
| `allowed_dependencies` | list[string] | 模块或 contract 名称 |
| `forbidden_dependencies` | list[string] | 模块或路径模式 |
| `commands_in` | list[string] | 对外输入命令 |
| `events_out` | list[string] | 对外输出事件 |
| `migration_notes` | text | 若当前为过渡态则必填 |

**校验规则**

- 不能把 `shared\`、`third_party\`、`build\`、`logs\`、`uploads\` 设为新的 owner 路径。
- `apps -> packages -> contracts/shared-kernel` 是默认方向；反向依赖必须显式拒绝或单向 contract 化。
- `M11` 只能发命令、做审批、看状态，不能直接改写 owner 对象。

### 3.6 `ControlSemantics`

**说明**: 状态机、命令、事件和失败分层的冻结规则。

| 字段 | 类型 | 约束 |
|---|---|---|
| `semantic_id` | string | 唯一 |
| `workflow_states` | list[string] | 必须覆盖 `Created` 到 `Archived` 的顶层状态 |
| `execution_states` | list[string] | 必须覆盖 `Idle` 到 `Faulted` 的执行态 |
| `command_catalog` | list[string] | 关键命令清单 |
| `event_catalog` | list[string] | 关键事件清单 |
| `blocking_layers` | list[string] | 至少覆盖 `L1`~`L5` 或等价分层 |
| `rollback_rules` | list[text] | 必填 |
| `recovery_rules` | list[text] | 必填 |
| `archive_rules` | list[text] | 必填 |
| `idempotency_fields` | list[string] | 至少包含 `request_id`, `idempotency_key`, `correlation_id`, `causation_id` |

**校验规则**

- `ExecutionPackageBuilt` 与 `ExecutionPackageValidated` 必须是两个独立事件。
- `PreflightBlocked` 不能替代规划失败。
- `WorkflowArchived` 与 `ArtifactSuperseded` 必须进入正式事件集合。

### 3.7 `AcceptanceBaseline`

**说明**: 统一的测试与验收闭环。

| 字段 | 类型 | 约束 |
|---|---|---|
| `baseline_id` | string | 唯一 |
| `scenario_classes` | list[string] | 至少覆盖主成功链、阻断链、回退链、恢复链、归档链 |
| `coverage_layers` | list[string] | 对象级、阶段级、模块级、链路级、故障注入级、回归级、验收级 |
| `required_root_gates` | list[path] | 必须包含 `build.ps1`, `test.ps1`, `ci.ps1` |
| `evidence_paths` | list[path] | 指向 `integration\reports\`、`docs\validation\` 等 |
| `pass_criteria` | list[text] | 必填 |
| `block_criteria` | list[text] | 必填 |
| `traceback_fields` | list[string] | 必须覆盖阶段、对象、状态、事件、失败码 |

**校验规则**

- 不能只覆盖主成功链。
- 每条关键用例必须能回链到阶段、对象、状态、事件和失败码。
- 未走根级入口的证据不能替代根级门禁结论。

### 3.8 `GapResolutionItem`

**说明**: 当前工作区与参考模板之间的一条差距及其处置闭环。

| 字段 | 类型 | 约束 |
|---|---|---|
| `gap_id` | string | 唯一 |
| `axis_id` | string | 引用 `ReferenceAxisMapping.axis_id` |
| `current_fact` | text | 必填 |
| `target_fact` | text | 必填 |
| `impact_scope` | list[path or module] | 至少 1 项 |
| `resolution_type` | enum | `retain`, `adjust`, `split`, `merge`, `add`, `remove`, `freeze`, `migrate` |
| `owner` | string | 角色或模块 owner |
| `completion_criteria` | list[text] | 必填 |
| `verification_points` | list[text] | 必填 |
| `status` | enum | `draft`, `proposed`, `approved`, `implementing`, `verified`, `closed`, `accepted-exception` |

**状态迁移**

```text
draft -> proposed -> approved -> implementing -> verified -> closed
draft/proposed/approved -> accepted-exception
```

### 3.9 `WorkspaceAreaDecision`

**说明**: 为整个工作区中的路径给出归属和去留判断，覆盖非主链模块。

| 字段 | 类型 | 约束 |
|---|---|---|
| `area_path` | path | 唯一 |
| `classification` | enum | `effective-canonical`, `transitional-canonical`, `support`, `vendor`, `generated`, `archived`, `removed` |
| `main_chain_relation` | enum | `main-chain`, `adjacent`, `non-main-chain` |
| `owner_module_id` | string | 可为空；为空时必须说明原因 |
| `disposition` | enum | `keep`, `migrate`, `merge`, `freeze`, `remove`, `archive` |
| `rationale` | text | 必填 |
| `legacy_default_allowed` | boolean | 默认 `false` |

**校验规则**

- 非主链模块也必须有 `disposition`。
- `legacy_default_allowed` 只能在时间受限的迁移说明中为 `true`，且必须附带截止条件。

## 4. 关系约束

```text
ArchitectureBaseline
├── 9 x ReferenceAxisMapping
├── n x StageDefinition
├── n x ArtifactDefinition
├── 12 x ModuleContract (M0-M11)
├── 1 x ControlSemantics
├── 1 x AcceptanceBaseline
├── n x GapResolutionItem
└── n x WorkspaceAreaDecision
```

## 5. 冻结状态闭环

```text
ArchitectureBaseline.freeze_status:
draft -> aligned -> reviewed -> frozen

ArchitectureBaseline.repo_alignment_status:
not-started -> in-progress -> aligned -> verified
```

当且仅当以下条件同时满足时，`ArchitectureBaseline` 才可从 `reviewed` 进入 `frozen`:

1. 9 个 `ReferenceAxisMapping` 全部 `frozen`。
2. 所有主链 `ModuleContract` 已完成 owner 和禁止越权规则冻结。
3. 所有关键 `GapResolutionItem` 至少达到 `approved`，且主阻断项达到 `verified`。
4. `AcceptanceBaseline` 明确根级入口与证据路径。
