# Phase 1 Data Model: ARCH-203 Process Model Owner Boundary Repair

## 1. 模型目标

本数据模型描述的是 owner boundary repair 的治理对象，而不是运行时数据库。它把“谁拥有事实、谁可以消费、哪些只是兼容壳、哪些需要按波次迁移”建模成可验证实体，供后续 `tasks.md` 和实施 evidence 使用。

## 2. 实体总览

| 实体 | 作用 | 核心关系 |
| --- | --- | --- |
| `ReviewBaselineAsset` | 2026-03-31 评审与复核输入资产 | 被 `RemediationWave` 与 `BoundaryEvidenceRecord` 引用 |
| `ModuleBoundaryCharter` | 单个模块的 owner 边界定义 | 拥有 `OwnerArtifactMapEntry`，约束 `ConsumerAccessRule` |
| `OwnerArtifactMapEntry` | 主链交接对象的 canonical owner 映射 | 属于 `ModuleBoundaryCharter`，关联 `ShadowImplementationRecord` |
| `ConsumerAccessRule` | 宿主/消费者允许和禁止的接入规则 | 消费 `ModuleBoundaryCharter` 的 public surface |
| `CompatibilityShellRecord` | 兼容壳、bridge、alias 的登记 | 为 `OwnerArtifactMapEntry` 提供过渡路径 |
| `ShadowImplementationRecord` | 重复实现、镜像类型、影子适配器登记 | 被 `OwnerArtifactMapEntry` 处置 |
| `RemediationWave` | 分波次整改计划 | 聚合 `ModuleBoundaryCharter`、`CompatibilityShellRecord` 和 `BoundaryEvidenceRecord` |
| `BoundaryEvidenceRecord` | 证明边界已收口的证据项 | 归属 `RemediationWave` 并回链 `ReviewBaselineAsset` |

## 3. 实体定义

### 3.1 `ReviewBaselineAsset`

**说明**: 本特性的事实输入文档。

| 字段 | 类型 | 约束 |
| --- | --- | --- |
| `asset_id` | string | 唯一，如 `review-m9-20260331` |
| `asset_type` | enum | `module-review`, `recheck-report` |
| `canonical_path` | path | 必须位于 `docs/process-model/reviews/` |
| `module_scope` | string | `M0`、`M3`...`M11` 或 `cross-module` |
| `git_tracked` | boolean | `false` 时不得进入 implementation |
| `fact_status` | enum | `current-fact`, `corrected-fact`, `superseded` |
| `blocking_level` | enum | `planning-only`, `implementation-blocker`, `closeout-blocker` |

**校验规则**

- `recheck-report` 必须至少有一份，且 `fact_status = corrected-fact`。
- 任一 `module-review` 若 `git_tracked = false`，则对应 `RemediationWave` 最多只能处于 `planned`。

### 3.2 `ModuleBoundaryCharter`

**说明**: 单个模块的正式 owner charter。

| 字段 | 类型 | 约束 |
| --- | --- | --- |
| `module_id` | string | `M0`、`M3`、`M4`、`M5`、`M6`、`M7`、`M8`、`M9`、`M11` |
| `module_name` | string | 唯一 |
| `canonical_roots` | list[path] | 必须全部位于 canonical workspace roots |
| `owner_focus` | list[string] | 至少 1 项 |
| `allowed_consumers` | list[string] | 可为空，但需显式给出 |
| `allowed_dependencies` | list[string] | 必填 |
| `forbidden_dependencies` | list[string] | 必填 |
| `current_drift_summary` | text | 必填 |
| `completion_criteria` | list[text] | 至少 1 项 |

**校验规则**

- `workflow` 的 `owner_focus` 只能包含 orchestration / compatibility，不得重新声明下游模块事实 owner。
- `runtime-execution` 的 `owner_focus` 必须覆盖执行语义与执行状态，不得只保留 compatibility shell。

### 3.3 `OwnerArtifactMapEntry`

**说明**: 主链交接对象与 owner 模块的映射。

| 字段 | 类型 | 约束 |
| --- | --- | --- |
| `artifact_name` | string | 唯一 |
| `canonical_owner_module_id` | string | 必须引用 `ModuleBoundaryCharter.module_id` |
| `upstream_artifacts` | list[string] | 可为空 |
| `downstream_artifacts` | list[string] | 可为空 |
| `allowed_public_surfaces` | list[path or symbol] | 至少 1 项 |
| `current_shadow_locations` | list[path] | 可为空 |
| `migration_status` | enum | `planned`, `in-progress`, `owner-closed`, `consumer-closed` |

**校验规则**

- `ProcessPlan`、`CoordinateTransformSet/AlignmentCompensation`、`ProcessPath`、`MotionPlan`、`Trigger/CMP packaging`、`ExecutionSession`、HMI 前置决策/会话状态必须全部存在。
- 同一 `artifact_name` 只能有一个 `canonical_owner_module_id`。

### 3.4 `ConsumerAccessRule`

**说明**: 模块外消费者的接入约束。

| 字段 | 类型 | 约束 |
| --- | --- | --- |
| `consumer_id` | string | 唯一 |
| `consumer_root` | path | 必须位于 `apps/`、`modules/`、`scripts/` 或 `tests/` |
| `allowed_surfaces` | list[path or symbol] | 至少 1 项 |
| `forbidden_surfaces` | list[path or symbol] | 至少 1 项 |
| `migration_target` | text | 必填 |
| `verification_gate` | list[text] | 至少 1 项 |

**校验规则**

- `apps/hmi-app` 不得直接成为运行时核心事实 owner。
- `apps/runtime-gateway`、`apps/runtime-service`、`apps/planner-cli` 不得继续把内部 helper target 视为长期公开接口。

### 3.5 `CompatibilityShellRecord`

**说明**: 被允许短期保留的兼容壳。

| 字段 | 类型 | 约束 |
| --- | --- | --- |
| `shell_id` | string | 唯一 |
| `shell_path` | path | 必填 |
| `shell_type` | enum | `alias-header`, `compat-facade`, `bridge-target`, `duplicate-contract-tree` |
| `source_owner_module_id` | string | 当前消费者眼中的旧 owner |
| `target_owner_module_id` | string | 最终 canonical owner |
| `allowed_behavior` | text | 必须明确为只读/转发/装配 |
| `exit_condition` | text | 必填 |
| `expiry_wave_id` | string | 必须引用 `RemediationWave.wave_id` |

**校验规则**

- `allowed_behavior` 不得允许新增业务逻辑。
- 缺少 `exit_condition` 的兼容壳视为无效设计。

### 3.6 `ShadowImplementationRecord`

**说明**: 重复实现或镜像类型。

| 字段 | 类型 | 约束 |
| --- | --- | --- |
| `shadow_id` | string | 唯一 |
| `canonical_artifact_name` | string | 必须引用 `OwnerArtifactMapEntry.artifact_name` |
| `shadow_paths` | list[path] | 至少 1 项 |
| `shadow_kind` | enum | `duplicate-source`, `mirror-type`, `shadow-adapter`, `re-export-shell` |
| `disposition` | enum | `merge-into-owner`, `degrade-to-shell`, `remove`, `freeze` |
| `evidence_required` | boolean | 默认 `true` |
| `status` | enum | `identified`, `planned`, `handled`, `verified` |

**校验规则**

- `status = verified` 前必须有至少 1 条 `BoundaryEvidenceRecord`。
- `workflow` 中的影子实现若归属 `M3-M11`，其 `disposition` 不能是 `freeze`。

### 3.7 `RemediationWave`

**说明**: 边界修复的执行波次。

| 字段 | 类型 | 约束 |
| --- | --- | --- |
| `wave_id` | string | `wave-0` ... `wave-4` |
| `wave_name` | string | 唯一 |
| `focus_seam` | text | 必填 |
| `prerequisites` | list[string] | 可为空 |
| `target_modules` | list[string] | 至少 1 项 |
| `exit_criteria` | list[text] | 至少 1 项 |
| `blocked_by` | list[string] | 可为空 |
| `status` | enum | `planned`, `ready`, `executing`, `verified`, `closed` |

**校验规则**

- `wave-0` 必须负责 baseline import / tracking。
- `wave-1` 必须包含 `workflow` 和 `runtime-execution`。
- 后续波次若依赖上一波次的 owner seam，则不能先于上一波次进入 `executing`。

### 3.8 `BoundaryEvidenceRecord`

**说明**: 用于证明边界收口的证据。

| 字段 | 类型 | 约束 |
| --- | --- | --- |
| `evidence_id` | string | 唯一 |
| `wave_id` | string | 必须引用 `RemediationWave.wave_id` |
| `evidence_type` | enum | `doc-review`, `build-gate`, `test-gate`, `boundary-script`, `traceability` |
| `source_path` | path | 必填 |
| `assertion` | text | 必填 |
| `result` | enum | `pending`, `passed`, `failed`, `blocked` |
| `related_assets` | list[string] | 可为空 |

**校验规则**

- 每个 `RemediationWave` 至少需要 1 条 `boundary-script` 或 `traceability` 证据。
- 若 `result = blocked`，必须在 `assertion` 中写明阻断项与责任归属。

## 4. 关系约束

```text
ReviewBaselineAsset
└── feeds RemediationWave

ModuleBoundaryCharter
├── owns OwnerArtifactMapEntry
└── constrains ConsumerAccessRule

OwnerArtifactMapEntry
├── may have CompatibilityShellRecord
└── may have ShadowImplementationRecord

RemediationWave
├── targets ModuleBoundaryCharter
├── consumes ReviewBaselineAsset
└── produces BoundaryEvidenceRecord
```

## 5. 最小闭环判定

当且仅当以下条件同时满足时，单个模块可被标记为 `owner-closed`:

1. `ModuleBoundaryCharter` 已冻结。
2. 关联 `OwnerArtifactMapEntry` 的 `migration_status` 至少为 `owner-closed`。
3. 所有相关 `CompatibilityShellRecord` 都有退出条件。
4. 所有相关 `ShadowImplementationRecord` 至少达到 `planned`，主阻断项达到 `handled`。
5. 至少有 1 条 `BoundaryEvidenceRecord` 证明消费者接入不再走 forbidden surface。
