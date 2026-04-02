# Tasks: Layered Test System

**Input**: 设计文档来自 `D:\Projects\ss-e\specs\test\infra\TASK-006-improve-layered-test-system\`
**Prerequisites**: `plan.md`、`spec.md`、`research.md`、`data-model.md`、`contracts/`、`quickstart.md`

**Tests**: 本特性本身就是测试体系建设，且规格明确要求通过分层验证、共享资产、故障注入、模拟全链路和受限 HIL 来证明价值，因此每个用户故事与后续治理阶段都包含先行测试或验证任务。
**Organization**: 任务按用户故事组织，先冻结共享分层骨架与根级入口，再分别交付风险分层路由、共享测试资产与故障场景、以及模拟/HIL 证据闭环三个可独立验收的增量，最后补执行治理、迁移清理、故障注入底座与 HIL/evidence 治理收口。

**Follow-up**: Phase 11 后的正式 `nightly-performance` 激活与 `limited-hil` 受控验证已转入 `D:\Projects\ss-e\specs\test\infra\TASK-007-activate-performance-and-controlled-hil\`，本任务不再继续追加新的平台语义。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可并行执行（不同文件、无未完成前置依赖）
- **[Story]**: 对应用户故事（`US1`、`US2`、`US3`）
- 每个任务都包含明确文件或目录路径

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: 建立本特性的执行矩阵、报告根说明和缺失的 integration scenarios surface

- [X] T001 Create layered validation matrix in `D:\Projects\ss-e\docs\validation\layered-test-matrix.md`
- [X] T002 [P] Create evidence root guide in `D:\Projects\ss-e\tests\reports\README.md`
- [X] T003 [P] Create integration scenarios surface guide in `D:\Projects\ss-e\tests\integration\scenarios\README.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 固定分层模型、资产目录和 evidence bundle 骨架；本阶段完成前不得开始任一用户故事

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T004 Create validation layer and execution lane models in `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\validation_layers.py`
- [X] T005 [P] Create evidence bundle helpers in `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\evidence_bundle.py`
- [X] T006 [P] Create shared asset and fault catalog helpers in `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\asset_catalog.py`
- [X] T007 Wire layer and lane metadata through `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\runner.py`
- [X] T008 Wire root routing and report enrichment in `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\workspace_validation.py` and `D:\Projects\ss-e\scripts\validation\invoke-workspace-tests.ps1`
- [X] T009 [P] Freeze root-entry and layer terminology in `D:\Projects\ss-e\docs\architecture\build-and-test.md` and `D:\Projects\ss-e\docs\validation\README.md`

**Checkpoint**: 分层模型、共享资产目录和 evidence bundle 骨架已经固定，用户故事任务可开始

---

## Phase 3: User Story 1 - 按风险分层执行验证 (Priority: P1) 🎯 MVP

**Goal**: 让常规改动先进入低成本离线层，并能通过根级入口得到明确的 layer、lane、跳层和升级验证结论

**Independent Test**: 运行 `D:\Projects\ss-e\build.ps1`、`D:\Projects\ss-e\test.ps1 -Profile Local -Suite contracts,protocol-compatibility` 与 `D:\Projects\ss-e\scripts\validation\run-local-validation-gate.ps1` 后，输出报告能明确显示当前请求所属 layer、lane、跳层理由和是否需要升级到更高成本验证

### Tests for User Story 1

- [X] T010 [P] [US1] Add layered validation routing contract regression in `D:\Projects\ss-e\tests\contracts\test_layered_validation_contract.py`
- [X] T011 [P] [US1] Create layered validation routing smoke in `D:\Projects\ss-e\tests\integration\scenarios\run_layered_validation_smoke.py`

### Implementation for User Story 1

- [X] T012 [US1] Implement request classification, lane selection, and routing smoke execution in `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\validation_layers.py`, `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\workspace_validation.py`, and `D:\Projects\ss-e\tests\integration\scenarios\run_layered_validation_smoke.py`
- [X] T013 [US1] Surface lane flags and skip-justification handling in `D:\Projects\ss-e\build.ps1`, `D:\Projects\ss-e\test.ps1`, and `D:\Projects\ss-e\ci.ps1`
- [X] T014 [US1] Align integration validation guidance in `D:\Projects\ss-e\tests\integration\README.md` and `D:\Projects\ss-e\tests\integration\scenarios\README.md`
- [X] T015 [US1] Publish layer-to-lane escalation rules in `D:\Projects\ss-e\docs\validation\layered-test-matrix.md`

**Checkpoint**: `US1` 完成后，常规改动已经可以通过根级入口被正确路由到 quick gate 或更高层验证，且该故事可单独验收

---

## Phase 4: User Story 2 - 复用统一测试资产与故障场景 (Priority: P2)

**Goal**: 让样本、baseline、fixture 和故障场景都回到 canonical shared roots，并能被多个模块复用而不出现第二事实源

**Independent Test**: 运行共享资产复用 smoke 后，至少两个不同 owner scope 的测试都能从 `D:\Projects\ss-e\samples\`、`D:\Projects\ss-e\tests\baselines\` 和 `D:\Projects\ss-e\shared\testing\` 读取相同 canonical 资产，并生成一致 evidence

### Tests for User Story 2

- [X] T016 [P] [US2] Add shared test assets contract regression in `D:\Projects\ss-e\tests\contracts\test_shared_test_assets_contract.py`
- [X] T017 [P] [US2] Create shared asset reuse smoke in `D:\Projects\ss-e\tests\integration\scenarios\run_shared_asset_reuse_smoke.py`

### Implementation for User Story 2

- [X] T018 [US2] Implement shared asset catalog and fixture metadata in `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\asset_catalog.py`
- [X] T019 [P] [US2] Normalize canonical sample and baseline taxonomy in `D:\Projects\ss-e\samples\README.md` and `D:\Projects\ss-e\tests\baselines\README.md`
- [X] T020 [P] [US2] Document shared fixture ownership and reuse boundaries in `D:\Projects\ss-e\shared\testing\README.md` and `D:\Projects\ss-e\tests\e2e\simulated-line\README.md`
- [X] T021 [US2] Wire shared asset reuse smoke and simulated fault consumption in `D:\Projects\ss-e\tests\integration\scenarios\run_shared_asset_reuse_smoke.py`, `D:\Projects\ss-e\tests\e2e\simulated-line\run_simulated_line.py`, and `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\workspace_validation.py`
- [X] T022 [US2] Freeze reusable fault scenario mapping in `D:\Projects\ss-e\samples\simulation\README.md` and `D:\Projects\ss-e\docs\validation\online-test-matrix-v1.md`

**Checkpoint**: `US2` 完成后，样本、baseline、fixture 和故障场景都能被跨模块复用，且不再依赖分散的私有事实源

---

## Phase 5: User Story 3 - 用模拟和受限联机获得可诊断证据 (Priority: P3)

**Goal**: 让 simulated-line 与 limited HIL 都产出统一 evidence bundle，并明确记录前置离线层、观察点、停机条件和失败分类

**Independent Test**: 运行 `D:\Projects\ss-e\tests\e2e\simulated-line\run_simulated_line.py` 与受限 HIL 路径后，`D:\Projects\ss-e\tests\reports\` 下都能看到包含 required trace fields 的 evidence bundle，且 HIL 结果明确声明其离线前置层和 abort metadata

### Tests for User Story 3

- [X] T023 [P] [US3] Add validation evidence bundle contract regression in `D:\Projects\ss-e\tests\contracts\test_validation_evidence_bundle_contract.py`
- [X] T024 [P] [US3] Extend simulated-line evidence assertions in `D:\Projects\ss-e\tests\e2e\simulated-line\verify_controlled_production_test.py`
- [X] T025 [P] [US3] Extend bounded HIL evidence assertions in `D:\Projects\ss-e\tests\e2e\hardware-in-loop\verify_hil_controlled_gate.py`

### Implementation for User Story 3

- [X] T026 [US3] Implement evidence bundle serialization and required trace fields in `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\evidence_bundle.py` and `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\runner.py`
- [X] T027 [US3] Emit structured simulated-line evidence in `D:\Projects\ss-e\tests\e2e\simulated-line\run_simulated_line.py` and `D:\Projects\ss-e\tests\e2e\simulated-line\run_controlled_production_test.py`
- [X] T028 [US3] Emit bounded HIL evidence and abort metadata in `D:\Projects\ss-e\tests\e2e\hardware-in-loop\run_hil_closed_loop.py` and `D:\Projects\ss-e\tests\e2e\hardware-in-loop\run_case_matrix.py`
- [X] T029 [US3] Align limited-HIL operator guidance and evidence semantics in `D:\Projects\ss-e\tests\e2e\hardware-in-loop\README.md` and `D:\Projects\ss-e\docs\validation\online-test-matrix-v1.md`
- [X] T030 [US3] Publish evidence bundle layout and report-root rules in `D:\Projects\ss-e\tests\reports\README.md` and `D:\Projects\ss-e\docs\validation\layered-test-matrix.md`

**Checkpoint**: `US3` 完成后，模拟全链路和受限 HIL 都能独立产出统一证据，且 HIL 不再是无上下文的黑盒执行

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 对齐性能 lane、刷新仓库级验证说明并执行最终 quickstart 验证

- [X] T031 [P] Align performance lane evidence output in `D:\Projects\ss-e\tests\performance\collect_dxf_preview_profiles.py` and `D:\Projects\ss-e\tests\performance\README.md`
- [X] T032 [P] Refresh repo-level validation surface docs in `D:\Projects\ss-e\tests\e2e\README.md`, `D:\Projects\ss-e\tests\integration\README.md`, and `D:\Projects\ss-e\docs\architecture\build-and-test.md`
- [X] T033 Run the quickstart validation flow from `D:\Projects\ss-e\specs\test\infra\TASK-006-improve-layered-test-system\quickstart.md` and record resulting evidence links in `D:\Projects\ss-e\docs\validation\layered-test-matrix.md`

---

## Phase 7: User Story 1 Extension - 执行治理与 Gate Policy 收口 (Priority: P1)

**Purpose**: 把已有 layer/lane routing 从“可用”收紧成“可执行、可阻断、可观察”的 PR / main / nightly / HIL 执行矩阵

**Independent Test**: 运行 `D:\Projects\ss-e\ci.ps1` 的 `quick-gate`、`full-offline-gate`、`nightly-performance` 与显式 HIL lane 后，报告必须明确显示 blocking 判定、timeout/retry/fail-fast 预算、suite/label 分类，以及缺失 metadata 时的 hard-fail 结果

### Tests for User Story 1 Extension

- [X] T034 [P] [US1] Add lane matrix and gate policy contract regression in `D:\Projects\ss-e\tests\contracts\test_layered_validation_lane_policy_contract.py`
- [X] T035 [P] [US1] Create CI lane matrix smoke in `D:\Projects\ss-e\tests\integration\scenarios\run_lane_policy_smoke.py`

### Implementation for User Story 1 Extension

- [X] T036 [US1] Define PR / main / nightly / HIL lane matrix and blocking-vs-advisory policy in `D:\Projects\ss-e\docs\validation\layered-test-matrix.md`, `D:\Projects\ss-e\docs\architecture\build-and-test.md`, and `D:\Projects\ss-e\ci.ps1`
- [X] T037 [US1] Enforce lane-specific timeout, retry, and fail-fast budgets in `D:\Projects\ss-e\ci.ps1`, `D:\Projects\ss-e\test.ps1`, `D:\Projects\ss-e\scripts\validation\run-local-validation-gate.ps1`, and `D:\Projects\ss-e\scripts\validation\invoke-workspace-tests.ps1`
- [X] T038 [US1] Freeze suite / label taxonomy and metadata propagation in `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\validation_layers.py`, `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\runner.py`, and `D:\Projects\ss-e\tests\reports\README.md`
- [X] T039 [US1] Backfill existing `contracts` / `integration` / `e2e` / `performance` tests with lane, suite, and size labels in `D:\Projects\ss-e\tests\contracts\`, `D:\Projects\ss-e\tests\integration\`, `D:\Projects\ss-e\tests\e2e\`, and `D:\Projects\ss-e\tests\performance\`

**Checkpoint**: `US1` 不再只是“路由 helper 存在”，而是已经具备可执行 lane matrix、明确 gate policy 和统一 test taxonomy

---

## Phase 8: User Story 2 Extension - 资产迁移、清理与 Baseline 治理 (Priority: P2)

**Purpose**: 把 shared asset 体系从“可以复用”收紧成“可迁移、可清理、可审计”，消除旧资产路径与第二事实源并存

**Independent Test**: 运行资产治理 smoke 与 workspace validation 后，必须能列出 canonical root inventory、发现 deprecated private roots、阻断 duplicate-source 资产，并验证 baseline update workflow 与 stale baseline 检查

### Tests for User Story 2 Extension

- [X] T040 [P] [US2] Add asset inventory and second-source guard regression in `D:\Projects\ss-e\tests\contracts\test_shared_test_assets_contract.py`
- [X] T041 [P] [US2] Create baseline governance smoke in `D:\Projects\ss-e\tests\integration\scenarios\run_baseline_governance_smoke.py`

### Implementation for User Story 2 Extension

- [X] T042 [US2] Inventory existing samples, baselines, fixtures, and fault roots in `D:\Projects\ss-e\samples\README.md`, `D:\Projects\ss-e\tests\baselines\README.md`, `D:\Projects\ss-e\shared\testing\README.md`, and `D:\Projects\ss-e\docs\validation\layered-test-matrix.md`
- [X] T043 [US2] Migrate scattered assets into canonical roots and backfill ownership in `D:\Projects\ss-e\samples\`, `D:\Projects\ss-e\tests\baselines\`, `D:\Projects\ss-e\shared\testing\`, and `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\asset_catalog.py`
- [X] T044 [US2] Mark deprecated private asset roots and add duplicate-source detection in `D:\Projects\ss-e\scripts\migration\validate_workspace_layout.py`, `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\workspace_validation.py`, and `D:\Projects\ss-e\tests\contracts\test_shared_test_assets_contract.py`
- [X] T045 [US2] Freeze baseline manifest schema and reviewer update workflow in `D:\Projects\ss-e\tests\baselines\baseline-manifest.schema.json`, `D:\Projects\ss-e\tests\baselines\README.md`, and `D:\Projects\ss-e\docs\validation\README.md`
- [X] T046 [US2] Add stale / unused baseline detection and report surfacing in `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\workspace_validation.py`, `D:\Projects\ss-e\scripts\validation\invoke-workspace-tests.ps1`, and `D:\Projects\ss-e\tests\integration\scenarios\run_baseline_governance_smoke.py`

**Checkpoint**: `US2` 不再停留在 catalog/README，现有测试资产已经完成 inventory、迁移、弃用标记和 duplicate-source guard

---

## Phase 9: User Story 3 Extension - 故障注入与模拟底座收口 (Priority: P3)

**Purpose**: 把“故障场景可复用”从文档语义收紧成真正可重放、可注入、可跨 owner scope 复用的 simulated-line 底座

**Independent Test**: 运行 fault matrix smoke 后，至少两个不同 owner scope 的测试必须能复用同一套 fault scenario schema、injector API、deterministic seed/clock，并生成一致 evidence

### Tests for User Story 3 Extension

- [X] T047 [P] [US3] Add fault scenario schema contract regression in `D:\Projects\ss-e\tests\contracts\test_fault_scenario_contract.py`
- [X] T048 [P] [US3] Create reusable fault matrix smoke in `D:\Projects\ss-e\tests\integration\scenarios\run_fault_matrix_smoke.py` and `D:\Projects\ss-e\tests\e2e\simulated-line\verify_controlled_production_test.py`

### Implementation for User Story 3 Extension

- [X] T049 [US3] Define fault scenario schema and deterministic replay metadata in `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\asset_catalog.py`, `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\evidence_bundle.py`, and `D:\Projects\ss-e\samples\simulation\README.md`
- [X] T050 [US3] Implement fault injector API and deterministic clock / seed hooks in `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\fault_injection.py`, `D:\Projects\ss-e\tests\e2e\simulated-line\run_simulated_line.py`, and `D:\Projects\ss-e\tests\e2e\simulated-line\run_controlled_production_test.py`
- [X] T051 [US3] Freeze simulated-line test doubles surface and add fake controller, fake IO, readiness, abort, and disconnect hooks in `D:\Projects\ss-e\tests\e2e\simulated-line\README.md`, `D:\Projects\ss-e\tests\e2e\simulated-line\run_simulated_line.py`, and `D:\Projects\ss-e\tests\e2e\simulated-line\run_controlled_production_test.py`
- [X] T052 [US3] Reuse the fault matrix across at least two owner scopes in `D:\Projects\ss-e\tests\integration\scenarios\run_fault_matrix_smoke.py`, `D:\Projects\ss-e\tests\e2e\simulated-line\run_simulated_line.py`, and `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\workspace_validation.py`

**Checkpoint**: `US3` 不再只是 evidence 输出增强，simulated-line 已具备真正的 fault injector、deterministic replay 和可复用 fake surface

---

## Phase 10: User Story 3 Closure - HIL 准入、失败分类与证据版本治理 (Priority: P3)

**Purpose**: 把 limited HIL 从“有说明、有 evidence”收紧成“有硬门禁、有 override 记录、有 schema version、有失败字典”的受控执行路径

**Independent Test**: 运行 bounded HIL 路径时，若离线前置层、safety preflight、operator override reason、abort metadata、failure classification 或 evidence schema version 缺失，执行必须 hard-fail；满足前置条件时则必须产出带 manifest/index 的统一 evidence bundle

### Tests for User Story 3 Closure

- [X] T053 [P] [US3] Add HIL admission and operator-override contract regression in `D:\Projects\ss-e\tests\e2e\hardware-in-loop\verify_hil_controlled_gate.py` and `D:\Projects\ss-e\tests\contracts\test_validation_evidence_bundle_contract.py`
- [X] T054 [P] [US3] Add evidence schema version and report manifest smoke in `D:\Projects\ss-e\tests\integration\scenarios\run_lane_policy_smoke.py` and `D:\Projects\ss-e\tests\contracts\test_validation_evidence_bundle_contract.py`

### Implementation for User Story 3 Closure

- [X] T055 [US3] Enforce HIL admission predicates, operator override reason capture, and safety preflight in `D:\Projects\ss-e\tests\e2e\hardware-in-loop\run_hil_closed_loop.py`, `D:\Projects\ss-e\tests\e2e\hardware-in-loop\run_case_matrix.py`, and `D:\Projects\ss-e\tests\e2e\hardware-in-loop\README.md`
- [X] T056 [US3] Freeze failure-classification taxonomy and evidence schema version in `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\evidence_bundle.py`, `D:\Projects\ss-e\docs\validation\README.md`, and `D:\Projects\ss-e\tests\reports\README.md`
- [X] T057 [US3] Add report manifest / index generation and compatibility checks in `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\evidence_bundle.py`, `D:\Projects\ss-e\shared\testing\test-kit\src\test_kit\workspace_validation.py`, and `D:\Projects\ss-e\tests\reports\README.md`
- [X] T058 [US3] Add performance lane routing and threshold smoke in `D:\Projects\ss-e\tests\performance\collect_dxf_preview_profiles.py`, `D:\Projects\ss-e\tests\performance\README.md`, and `D:\Projects\ss-e\tests\integration\scenarios\run_lane_policy_smoke.py`

**Checkpoint**: limited HIL 只在离线层放行且安全前提满足时进入，所有高成本验证都带明确的 failure taxonomy、schema version 和 report manifest

---

## Phase 11: Closeout & Acceptance Sync

**Purpose**: 回填 Phase 10 账本、让 quickstart/validation docs 与当前代码 reality 对齐，并补一条可持久化的 controlled HIL synthetic acceptance 证据链

- [X] T059 Backfill Phase 10 completion state in `D:\Projects\ss-e\specs\test\infra\TASK-006-improve-layered-test-system\tasks.md` and refresh Phase 11 scope in this file
- [X] T060 [P] Sync quickstart and root validation truth in `D:\Projects\ss-e\specs\test\infra\TASK-006-improve-layered-test-system\quickstart.md`, `D:\Projects\ss-e\docs\architecture\build-and-test.md`, `D:\Projects\ss-e\docs\validation\README.md`, and `D:\Projects\ss-e\docs\validation\layered-test-matrix.md`
- [X] T061 [P] Sync controlled HIL operator, gate, and release evidence docs in `D:\Projects\ss-e\tests\e2e\hardware-in-loop\README.md`, `D:\Projects\ss-e\docs\validation\online-test-matrix-v1.md`, and `D:\Projects\ss-e\docs\validation\release-test-checklist.md`
- [X] T062 [P] Sync performance authority, threshold gate, and nightly policy docs in `D:\Projects\ss-e\tests\performance\README.md`, `D:\Projects\ss-e\docs\architecture\build-and-test.md`, and `D:\Projects\ss-e\docs\validation\layered-test-matrix.md`
- [X] T063 Create controlled HIL synthetic acceptance smoke in `D:\Projects\ss-e\tests\integration\scenarios\run_hil_controlled_gate_smoke.py`
- [X] T064 Wire the new smoke into scenario guidance in `D:\Projects\ss-e\tests\integration\scenarios\README.md` and `D:\Projects\ss-e\docs\validation\layered-test-matrix.md`
- [X] T065 Run offline closeout acceptance, archive evidence links, and record results in `D:\Projects\ss-e\docs\validation\layered-test-matrix.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 无依赖，可立即开始
- **Foundational (Phase 2)**: 依赖 Setup 完成；阻塞全部用户故事
- **User Story 1 (Phase 3)**: 依赖 Foundational 完成；建立风险分层路由最小闭环
- **User Story 2 (Phase 4)**: 依赖 Foundational 完成；建立共享资产、catalog 和 reuse smoke 的第一版闭环
- **User Story 3 (Phase 5)**: 依赖 Foundational 完成；建立 evidence bundle 与 simulated-line / bounded HIL 的第一版闭环
- **Polish (Phase 6)**: 依赖 Phase 3-5 完成；对齐性能 lane 和 quickstart 证据
- **User Story 1 Extension (Phase 7)**: 依赖 `US1` 已完成最小路由闭环；把 routing 提升为可阻断的执行治理
- **User Story 2 Extension (Phase 8)**: `T040-T042` 仅依赖 Foundational 与现有 `US2` catalog；`T043-T046` 依赖 inventory 结果，并要求 `US1` 的 routing metadata 已稳定
- **User Story 3 Extension (Phase 9)**: `T047-T049` 仅依赖 Foundational 与现有 evidence bundle；`T050-T052` 依赖 fault schema 已冻结，并消费 `US2` 的 canonical asset provenance
- **User Story 3 Closure (Phase 10)**: 依赖 Phase 7 的 gate policy、Phase 9 的 fault/sim 底座，以及 Phase 8 的 provenance / baseline guard，才能把 HIL admission 和 evidence governance 强制化
- **Closeout & Acceptance Sync (Phase 11)**: 依赖 Phase 10 已完成；只做账本回填、文档真值同步、synthetic acceptance 和离线 closeout evidence，不再扩展新的平台语义

### User Story Dependencies

- **User Story 1 (P1)**: Foundational 完成后即可开始；不依赖其他故事
- **User Story 2 (P2)**: shared asset taxonomy、inventory 和 baseline schema 可在 Foundational 后启动；reuse smoke、deprecated-root guard 和 duplicate-source enforcement 才依赖 `US1` routing metadata 稳定
- **User Story 3 (P3)**: evidence schema、failure taxonomy 与 fault scenario schema 可在 Foundational 后启动；shared asset provenance 回链依赖 `US2`，而 HIL admission enforcement 依赖 `US1` gate policy
- **Cross-Cutting Governance**: lane matrix、baseline governance、fault injector 与 HIL admission 都是同一测试平台的闭环任务，不得被当作单纯文档补充推迟到最后

### Parallel Opportunities

- `T005`、`T006`、`T009` 可并行，但 `T007-T008` 必须等待 evidence 字段和 asset catalog 基本术语冻结
- `US1` 中 `T010` 与 `T011` 可并行；Phase 7 中 `T034` 与 `T035` 可并行，但 `T037-T039` 必须等待 `T036` 先冻结 lane matrix 与 gate policy
- `US2` 中 `T016` 与 `T017` 可并行，`T019` 与 `T020` 可并行；Phase 8 中 `T040` 与 `T041` 可并行，但 `T043-T046` 必须等待 inventory 和 canonical mapping 明确后再推进
- `US3` 中 `T023`、`T024`、`T025` 可并行；Phase 9 中 `T047` 与 `T048` 可并行，但 `T050-T052` 必须等待 `T049` 先冻结 fault scenario schema
- Phase 10 中 `T053` 与 `T054` 只有在 HIL admission predicates、override 字段和 required trace fields 口径冻结后才允许并行推进，避免 HIL verify 与 evidence schema 返工

---

## Parallel Example: User Story 1

```text
T010 + T011
```

## Parallel Example: User Story 2

```text
T016 + T017 + T019 + T020
```

## Parallel Example: User Story 3

```text
T023 + T024 + T025
```

## Parallel Example: Governance Closure

```text
T034 + T035
T040 + T041
T047 + T048
```

## Implementation Strategy

### MVP First (User Story 1 Only)

1. 完成 Phase 1: Setup
2. 完成 Phase 2: Foundational
3. 完成 Phase 3: User Story 1
4. 运行 quick gate 与 local validation gate，确认 layer/lane 路由闭环成立
5. 在 `D:\Projects\ss-e\docs\validation\layered-test-matrix.md` 记录第一版 evidence

### Incremental Delivery

1. 先完成 `US1`，冻结 root routing 与 quick gate 升级规则
2. 再完成 `US2`，把共享资产、baseline、fixture 和 fault scenario 收敛到 canonical roots
3. 最后完成 `US3`，让 simulated-line、performance 和 limited HIL 输出统一 evidence bundle

### Next-Stage Delivery

1. 先完成 Phase 7，把 lane matrix、gate policy、suite/label taxonomy 和 timeout/retry/fail-fast 收紧到 `ci.ps1` 与 root scripts
2. 再完成 Phase 8，用 `inventory -> migrate -> deprecate -> detect` 的顺序清理现有样本、baseline、fixture 与 fault roots
3. 然后完成 Phase 9，补齐 fault injector、deterministic replay 与 simulated-line fake surface，使 simulated-line 成为高风险场景主战场
4. 最后完成 Phase 10，把 HIL admission、operator override、failure taxonomy、schema version 和 report manifest 强制化

### Parallel Team Strategy

1. 一名成员负责 `US1` 的 routing、root scripts 与 integration smoke
2. 一名成员负责 `US2` 的 asset catalog、samples/baselines 与 simulated-line 复用
3. 一名成员负责 `US3` 的 evidence bundle、HIL 证据与报告收尾

## Acceptance Thresholds & Failure Policy

- `quick-gate` / PR lane: blocking；缺少 layer/lane/suite labels、skip justification、required trace fields 或超过 fail-fast 预算时必须直接失败
- `full-offline-gate` / main lane: blocking；若 duplicate-source 资产、baseline manifest 校验、fault matrix smoke 或 simulated-line evidence 任一失败，禁止继续宣称离线闭环成立
- `nightly-performance` lane: 必须记录 threshold 配置与 evidence manifest；阈值缺失、schema 不兼容或 evidence 不完整时 hard-fail，性能 gate 由 `threshold_gate` 直接给出 blocking 判定
- `limited-hil` lane: 永远不是默认入口；离线前置层未通过、safety preflight 缺失、operator override reason 缺失、abort metadata 缺失时必须 hard-fail
- baseline refresh: 未同步 manifest、reviewer guidance 和 provenance 说明时不得接受 baseline 更新
- deprecated private roots: inventory、迁移与 duplicate-source guard 未落地前，不得删除旧根；旧根一旦标记 deprecated，就不得继续新增事实资产
- fault / simulated-line tasks: deterministic seed/clock 与 injector API 未冻结前，不得把新的故障场景接入正式 gate
- HIL tasks: admission predicates 未代码化前，不得把任何 bounded HIL 路径列入正式验收清单

---

## Notes

- 所有测试或验证任务都必须先写并先失败，再进行对应实现任务
- `shared/testing/` 只承载跨模块复用测试支撑，不承接业务 owner
- `tests/reports/` 产物只能作为 evidence，不能回写为 baseline
- 受限 HIL 只能在离线层放行后进入，不得成为默认 gate
- 现有测试资产必须按 `inventory -> migrate -> deprecate -> detect -> delete` 顺序治理，禁止新旧体系长期双轨并存
- 任何新增 lane、baseline schema、fault schema 或 HIL 路径都必须同时带上 failure policy、compatibility 口径和报告 manifest
- 分支必须持续保持 `<type>/<scope>/<ticket>-<short-desc>` 合规
