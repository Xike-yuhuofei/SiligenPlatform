# Phase 1 Data Model: DSP E2E 全量迁移与 Legacy 清除

## 1. 模型目标

本数据模型描述的是“整仓迁移归位、bridge 清除、冻结事实源校验和根级 gate 收口”的设计实体，不是运行时业务数据库。它服务于本特性的计划、实施、验证和 closeout 证据。

## 2. 实体总览

| 实体 | 作用 | 核心关系 |
|---|---|---|
| `CanonicalRootSet` | 声明正式目标根与非 owner 根分类 | 约束 `ModuleOwnerSurface`、`CrossRootOwnerAsset`、`ValidationEntrypoint` |
| `ModuleOwnerSurface` | 定义 `M0-M11` 的终态 owner 面 | 聚合 `CrossRootOwnerAsset`、`LegacyAsset`，被 `MigrationAlignmentMatrixEntry` 引用 |
| `CrossRootOwnerAsset` | 表示分布在 `apps/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/` 中但仍由某模块 owner 的直接资产 | 归属 `ModuleOwnerSurface`，被 `MigrationAlignmentMatrixEntry` 引用 |
| `LegacyAsset` | 表示仍需清除的 bridge/source/fallback 资产 | 归属 `ModuleOwnerSurface` 或 `CrossRootOwnerAsset`，被 `ValidationRule` 检测 |
| `MigrationAlignmentMatrixEntry` | 每条迁移归位与清除账本记录 | 关联 `ModuleOwnerSurface`、`CrossRootOwnerAsset`、`LegacyAsset`、`AcceptanceEvidence` |
| `ValidationEntrypoint` | 根级验证入口或关键脚本入口 | 产出 `AcceptanceEvidence`，执行 `ValidationRule` |
| `ValidationRule` | 具体的 fail-fast 规则 | 约束 `LegacyAsset` 和 `MigrationAlignmentMatrixEntry` |
| `AcceptanceEvidence` | closeout 证据及其报告路径 | 由 `ValidationEntrypoint` 产出，回链矩阵记录 |

## 3. 实体定义

### 3.1 `CanonicalRootSet`

**说明**: 当前工作区的正式路径分类。

| 字段 | 类型 | 约束 |
|---|---|---|
| `root_id` | string | 唯一，例如 `apps`, `modules`, `shared` |
| `root_path` | path | 必须是仓库根下实际存在路径 |
| `classification` | enum | `target-canonical`, `support`, `vendor`, `generated`, `feature-artifact` |
| `owner_surface_allowed` | boolean | 只有 `target-canonical` 为 `true` |
| `default_entry_allowed` | boolean | 根级正式入口所依赖的根必须为 `true` |
| `notes` | text | 可空 |

**固定分类**

- `target-canonical`: `apps/`, `modules/`, `shared/`, `docs/`, `samples/`, `tests/`, `scripts/`, `config/`, `data/`, `deploy/`
- `support`: `cmake/`
- `vendor`: `third_party/`
- `generated`: `build/`, `logs/`, `uploads/`
- `feature-artifact`: `specs/`

### 3.2 `ModuleOwnerSurface`

**说明**: `M0-M11` 在单轨 canonical workspace 中的终态 owner 面。

| 字段 | 类型 | 约束 |
|---|---|---|
| `module_id` | string | `M0` 到 `M11` |
| `module_name` | string | 采用 `dsp-e2e-spec-s05` 规范名 |
| `module_root` | path | 必须位于 `modules/` 下 |
| `canonical_surface_paths` | list[path] | 只允许指向 canonical skeleton 或明确模块特化路径 |
| `cross_root_asset_refs` | list[string] | 引用 `CrossRootOwnerAsset.asset_id`，至少覆盖所有与模块 owner 直接相关的跨根资产 |
| `build_entry_paths` | list[path] | 至少一个 CMake 或脚本入口 |
| `manifest_files` | list[path] | 至少包含 `module.yaml`、`README.md`、模块级 `CMakeLists.txt` |
| `closeout_state` | enum | `bridge-active`, `bridge-collapsing`, `bridge-removed`, `verified` |

**校验规则**

- `canonical_surface_paths` 不得包含 `src/`、`process-runtime-core/`、`engineering-data/`、`runtime-host/`、`device-adapters/`、`device-contracts/` 等 bridge root。
- `closeout_state = verified` 前，相关 `CrossRootOwnerAsset` 必须全部达到 `verified`，相关 `LegacyAsset` 必须全部达到 `removed` 或 `not-applicable`。

### 3.3 `CrossRootOwnerAsset`

**说明**: 不位于 `modules/<module>/` 主目录下，但仍由某个 `M0-M11` 模块 owner 直接负责的 canonical 资产。

| 字段 | 类型 | 约束 |
|---|---|---|
| `asset_id` | string | 唯一 |
| `owner_module_id` | string | 引用 `ModuleOwnerSurface.module_id`；跨模块公共项可使用 `ROOT` |
| `canonical_root_ref` | string | 引用 `CanonicalRootSet.root_id`，且必须是 `target-canonical` |
| `asset_path` | path | 必须位于 `apps/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/` 之一 |
| `asset_role` | enum | `app-entry`, `shared-contract`, `shared-support`, `repo-test`, `sample`, `script`, `doc`, `config`, `data`, `deploy` |
| `current_live_surface` | text | 记录当前真实入口、真实 owner 面或真实消费面 |
| `legacy_dependency_refs` | list[string] | 引用 `LegacyAsset.legacy_id`；无遗留依赖时为空 |
| `status` | enum | `identified`, `aligned`, `verified`, `blocked` |
| `notes` | text | 可空 |

**校验规则**

- 与 module owner 直接相关的 `apps/`、`shared/` 资产必须作为一等迁移对象建模，不得只在备注里出现。
- `status = verified` 前，`legacy_dependency_refs` 中引用的 `LegacyAsset` 不能仍处于 `identified`、`planned` 或 `in-progress`。

### 3.4 `LegacyAsset`

**说明**: 必须被物理清除或去 owner 化的 bridge/fallback/compatibility 资产。

| 字段 | 类型 | 约束 |
|---|---|---|
| `legacy_id` | string | 唯一 |
| `module_id` | string | 可为空；为空时表示跨模块或根级资产 |
| `legacy_path` | path | 必须指向当前仓内实际资产或被规则引用的路径 |
| `legacy_kind` | enum | `bridge-root`, `forwarder`, `compat-cmake`, `fallback-var`, `bridge-script`, `doc-alias`, `test-alias` |
| `canonical_replacement_paths` | list[path] | 至少一项，若为纯删除则指向承接 owner 面或正式入口 |
| `detection_rules` | list[string] | 引用 `ValidationRule.rule_id` |
| `removal_state` | enum | `identified`, `planned`, `in-progress`, `removed`, `not-applicable` |
| `notes` | text | 可空 |

**典型样例**

- `modules/workflow/src`
- `modules/workflow/process-runtime-core`
- `modules/workflow/application/usecases-bridge`
- `modules/dxf-geometry/engineering-data`
- `modules/runtime-execution/runtime-host`
- `modules/runtime-execution/device-adapters`
- `modules/runtime-execution/device-contracts`
- `scripts/engineering-data-bridge`
- `SILIGEN_BRIDGE_ROOTS`
- `SILIGEN_MIGRATION_SOURCE_ROOTS`

### 3.5 `MigrationAlignmentMatrixEntry`

**说明**: 迁移归位与清除矩阵中的一行，代表一项可验证的 closeout 责任。

| 字段 | 类型 | 约束 |
|---|---|---|
| `entry_id` | string | 唯一 |
| `module_id` | string | 引用 `ModuleOwnerSurface.module_id`；跨模块项可使用 `ROOT` |
| `canonical_root_ref` | string | 引用 `CanonicalRootSet.root_id`，用于显式标识该行属于哪个 canonical root |
| `asset_family` | enum | `implementation`, `build-entry`, `tests`, `contracts`, `samples`, `scripts`, `docs`, `config`, `data`, `deploy` |
| `target_owner_surface` | list[path] | 只允许 canonical 路径 |
| `owner_asset_refs` | list[string] | 引用 `CrossRootOwnerAsset.asset_id`；若该行描述模块外 direct owner 资产则至少一项 |
| `legacy_asset_refs` | list[string] | 引用 `LegacyAsset.legacy_id` |
| `validation_entry_refs` | list[string] | 引用 `ValidationEntrypoint.entry_id` |
| `required_evidence_refs` | list[string] | 引用 `AcceptanceEvidence.evidence_id` |
| `blocking_condition` | text | 必填 |
| `status` | enum | `identified`, `planned`, `migrating`, `cleared`, `verified`, `blocked` |

**说明补充**

- 同一 `asset_family` 可以出现在不同 `canonical_root_ref` 下；例如 `build-entry` 可用于 `modules/` 下模块构建面，也可用于 `apps/` 下应用入口。
- 该字段组合的目的是防止 `apps/`、`shared/` 等跨根 owner 资产在任务分解和验收中被静默丢失。

**状态迁移**

```text
identified -> planned -> migrating -> cleared -> verified
identified/planned/migrating -> blocked
blocked -> migrating
```

### 3.6 `ValidationEntrypoint`

**说明**: 根级验证入口或关键校验脚本。

| 字段 | 类型 | 约束 |
|---|---|---|
| `entry_id` | string | 唯一 |
| `entry_path` | path | 仓库内脚本路径 |
| `entry_type` | enum | `root-build`, `root-test`, `root-ci`, `local-gate`, `layout-check`, `legacy-exit`, `freeze-docset-check` |
| `profiles_or_suites` | list[string] | 至少一个可执行 profile/suite |
| `coverage_scope` | text | 必填，明确该入口实际证明什么，不得泛化为其未覆盖的退出维度 |
| `required_reports` | list[path pattern] | 若会产出报告则必填 |
| `must_reject_bridge_state` | boolean | 对本特性必须为 `true` |
| `must_reject_fallback_state` | boolean | 对本特性必须为 `true` |

**正式入口集合**

- `build.ps1`
- `test.ps1`
- `ci.ps1`
- `scripts/validation/run-local-validation-gate.ps1`
- `scripts/migration/validate_workspace_layout.py`
- `scripts/migration/legacy-exit-checks.py`
- `scripts/migration/validate_dsp_e2e_spec_docset.py`

**责任分工**

- `validate_workspace_layout.py`: 负责 active bridge、bridge metadata 和 live owner graph 违规。
- `legacy-exit-checks.py`: 负责旧顶层根是否存在，以及旧路径文本引用是否回流。
- `validate_dsp_e2e_spec_docset.py`: 负责唯一冻结文档集完整性、一致性和最小 acceptance 轴覆盖。
- `run-local-validation-gate.ps1` / `ci.ps1`: 负责聚合上述维度并验证根级正式入口链路。

### 3.7 `ValidationRule`

**说明**: gate 中的单条 fail-fast 规则。

| 字段 | 类型 | 约束 |
|---|---|---|
| `rule_id` | string | 唯一 |
| `source_entry_id` | string | 引用 `ValidationEntrypoint.entry_id` |
| `observed_scope` | list[path or file pattern] | 至少一项 |
| `fail_condition` | text | 必填 |
| `allowed_exception_scope` | text | 默认 `none` |
| `remediation_owner` | string | 模块或平台 owner |

**关键规则类别**

- bridge root 仍存在或仍处于 live owner graph
- `bridge_mode: active` 仍存在
- `SILIGEN_BRIDGE_ROOTS` 或 `SILIGEN_MIGRATION_SOURCE_ROOTS` 仍被 live CMake 使用
- 旧顶层根仍存在或旧路径文本引用仍回流
- 根级入口仍引用 bridge 或 fallback 变量
- 文档和脚本仍把 bridge 路径写成默认正式入口
- `dsp-e2e-spec` 正式文档集缺轴、缺章节或 acceptance 词汇不完整

### 3.8 `AcceptanceEvidence`

**说明**: root gate 或 closeout 生成的可审计证据。

| 字段 | 类型 | 约束 |
|---|---|---|
| `evidence_id` | string | 唯一 |
| `producer_entry_id` | string | 引用 `ValidationEntrypoint.entry_id` |
| `report_path` | path | 指向 `tests/reports/` 或 canonical docs 报告 |
| `proof_of` | enum | `single-track-layout`, `zero-legacy-root`, `zero-bridge-root`, `freeze-docset-valid`, `root-gate-pass`, `module-closeout`, `acceptance-sync` |
| `run_frequency` | enum | `per-change`, `closeout`, `release` |
| `pass_predicate` | text | 必填 |

**标准证据样例**

- `tests/reports/**/workspace-validation.md`
- `tests/reports/**/dsp-e2e-spec-docset/dsp-e2e-spec-docset.md`
- `tests/reports/**/legacy-exit-checks.md`
- `tests/reports/**/local-validation-gate-summary.md`
- `docs/architecture/system-acceptance-report.md`

## 4. 关系约束

```text
CanonicalRootSet
├── n x ModuleOwnerSurface
├── n x CrossRootOwnerAsset
├── n x ValidationEntrypoint
└── n x AcceptanceEvidence

ModuleOwnerSurface
├── n x CrossRootOwnerAsset
├── n x LegacyAsset
└── n x MigrationAlignmentMatrixEntry

CrossRootOwnerAsset
├── n x LegacyAsset
└── n x MigrationAlignmentMatrixEntry

ValidationEntrypoint
├── n x ValidationRule
└── n x AcceptanceEvidence

MigrationAlignmentMatrixEntry
├── 1 x ModuleOwnerSurface
├── n x CrossRootOwnerAsset
├── n x LegacyAsset
└── n x AcceptanceEvidence
```

## 5. 完成判定闭环

当且仅当以下条件同时满足时，整仓迁移可从 `planned` 进入 `verified`:

1. 所有 `ModuleOwnerSurface.closeout_state = verified`
2. 所有 `CrossRootOwnerAsset.status = verified`
3. 所有 `LegacyAsset.removal_state` 都为 `removed` 或 `not-applicable`
4. 所有 `MigrationAlignmentMatrixEntry.status = verified`
5. 所有正式 `ValidationEntrypoint` 最近一次执行都满足其 `coverage_scope` 所声明的证明义务，且 `must_reject_bridge_state = true`、`must_reject_fallback_state = true`
6. `AcceptanceEvidence` 能同时证明单轨 canonical layout、冻结文档集有效、零 legacy root、零 bridge root、根级 gate 通过
