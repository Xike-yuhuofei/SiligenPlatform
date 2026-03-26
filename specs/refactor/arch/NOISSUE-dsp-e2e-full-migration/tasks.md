# Tasks: DSP E2E 全量迁移与 Legacy 清除

**Input**: 设计文档来自 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-dsp-e2e-full-migration\`  
**Prerequisites**: `plan.md`、`spec.md`、`research.md`、`data-model.md`、`contracts/`、`quickstart.md`

**Tests**: 本特性明确要求通过根级 build/test/CI、本地 gate、layout/docset/legacy-exit 校验来证明 closeout，因此每个用户故事都包含先行的验证任务。  
**Organization**: 任务按用户故事组织，保证 `US1`、`US2`、`US3` 可以按冻结事实源统一、owner 面收敛、legacy 清除与最终验收三步独立推进。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可并行执行（不同文件、无未完成前置依赖）
- **[Story]**: 对应用户故事（`US1`、`US2`、`US3`）
- 每个任务都包含明确文件或目录路径

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: 建立本特性的正式执行账本和 closeout 工作文档

- [ ] T001 Create migration alignment matrix in `D:\Projects\SS-dispense-align\docs\architecture\migration-alignment-clearance-matrix.md`
- [ ] T002 [P] Create bridge exit closeout note in `D:\Projects\SS-dispense-align\docs\architecture\bridge-exit-closeout.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 统一基线解释、矩阵结构和 gate 责任边界；本阶段完成前不得开始任一用户故事

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [ ] T003 Reconcile baseline interpretation in `D:\Projects\SS-dispense-align\docs\architecture\workspace-baseline.md` and `D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md`
- [ ] T004 [P] Seed `M0-M11` rows and cross-root asset columns in `D:\Projects\SS-dispense-align\docs\architecture\migration-alignment-clearance-matrix.md`
- [ ] T005 [P] Define validation responsibility split in `D:\Projects\SS-dispense-align\docs\architecture\bridge-exit-closeout.md`, `D:\Projects\SS-dispense-align\scripts\migration\validate_workspace_layout.py`, `D:\Projects\SS-dispense-align\scripts\migration\validate_dsp_e2e_spec_docset.py`, and `D:\Projects\SS-dispense-align\scripts\migration\legacy-exit-checks.py`
- [ ] T006 [P] Surface matrix/docset/legacy-exit evidence in `D:\Projects\SS-dispense-align\shared\testing\test-kit\src\test_kit\workspace_validation.py`

**Checkpoint**: 基线解释、矩阵骨架和 gate 分工已经固定，用户故事任务可开始

---

## Phase 3: User Story 1 - 统一正式冻结事实源 (Priority: P1) 🎯 MVP

**Goal**: 让 `docs/architecture/dsp-e2e-spec/` 成为唯一正式冻结事实源，并让 docset gate 与根级报告显式证明这一点

**Independent Test**: 运行 `python scripts/migration/validate_dsp_e2e_spec_docset.py`、`powershell -File scripts/validation/run-local-validation-gate.ps1` 和 `powershell -File ci.ps1 -Suite all` 时，冻结文档集无缺轴、无重复事实源、无补充正式入口；评审任意 `M0-M11` 模块时只需回链 `dsp-e2e-spec` 与本特性 docs 即可完成裁决

### Tests for User Story 1

- [ ] T007 [P] [US1] Add freeze-docset contract regression in `D:\Projects\SS-dispense-align\tests\contracts\test_freeze_docset_contract.py`
- [ ] T008 [P] [US1] Wire freeze-docset contract execution into `D:\Projects\SS-dispense-align\shared\testing\test-kit\src\test_kit\workspace_validation.py`, `D:\Projects\SS-dispense-align\scripts\validation\run-local-validation-gate.ps1`, and `D:\Projects\SS-dispense-align\ci.ps1`

### Implementation for User Story 1

- [ ] T009 [US1] Update formal freeze entry guidance in `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\README.md` and `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s10-frozen-directory-index.md`
- [ ] T010 [P] [US1] Align single-track owner terminology in `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s05-module-boundary-interface-contract.md` and `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s06-repo-structure-guide.md`
- [ ] T011 [P] [US1] Align root validation evidence terminology in `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md` and `D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md`
- [ ] T012 [US1] Demote historical `migration-source roots` wording from live owner decisions in `D:\Projects\SS-dispense-align\docs\architecture\workspace-baseline.md` and `D:\Projects\SS-dispense-align\docs\architecture\migration-alignment-clearance-matrix.md`

**Checkpoint**: `dsp-e2e-spec` 已成为唯一正式冻结事实源，`US1` 可独立通过 docset 和根级 gate 验证

---

## Phase 4: User Story 2 - 收敛真实承载与 owner 面 (Priority: P2)

**Goal**: 建立覆盖 `M0-M11` 和跨根 direct owner 资产的权威迁移矩阵，并把模块/跨根 owner 面全部映射到单轨 canonical roots

**Independent Test**: 抽查任意 `M0-M11` 模块时，`docs/architecture/migration-alignment-clearance-matrix.md` 能指出其唯一 owner 面、跨根 direct owner 资产、当前 live surface 和阻断项；`validate_workspace_layout.py` 与 contract regression 能对未收敛 owner 面直接报错

### Tests for User Story 2

- [ ] T013 [P] [US2] Add migration matrix schema regression in `D:\Projects\SS-dispense-align\tests\contracts\test_migration_alignment_matrix.py`
- [ ] T014 [P] [US2] Extend unresolved-owner detection in `D:\Projects\SS-dispense-align\scripts\migration\validate_workspace_layout.py` and `D:\Projects\SS-dispense-align\shared\testing\test-kit\src\test_kit\workspace_validation.py`

### Implementation for User Story 2

- [ ] T015 [US2] Populate `D:\Projects\SS-dispense-align\docs\architecture\migration-alignment-clearance-matrix.md` with `M0-M11` module owner surfaces, cross-root assets, and blocking states
- [ ] T016 [P] [US2] Align verified owner surfaces in `D:\Projects\SS-dispense-align\modules\workflow\README.md`, `D:\Projects\SS-dispense-align\modules\workflow\module.yaml`, `D:\Projects\SS-dispense-align\modules\workflow\CMakeLists.txt`, `D:\Projects\SS-dispense-align\modules\job-ingest\README.md`, `D:\Projects\SS-dispense-align\modules\job-ingest\module.yaml`, and `D:\Projects\SS-dispense-align\modules\job-ingest\CMakeLists.txt`
- [ ] T017 [P] [US2] Align verified owner surfaces in `D:\Projects\SS-dispense-align\modules\dxf-geometry\README.md`, `D:\Projects\SS-dispense-align\modules\dxf-geometry\module.yaml`, `D:\Projects\SS-dispense-align\modules\dxf-geometry\CMakeLists.txt`, `D:\Projects\SS-dispense-align\modules\topology-feature\README.md`, `D:\Projects\SS-dispense-align\modules\topology-feature\module.yaml`, and `D:\Projects\SS-dispense-align\modules\topology-feature\CMakeLists.txt`
- [ ] T018 [P] [US2] Align verified owner surfaces in `D:\Projects\SS-dispense-align\modules\process-planning\README.md`, `D:\Projects\SS-dispense-align\modules\process-planning\module.yaml`, `D:\Projects\SS-dispense-align\modules\process-planning\CMakeLists.txt`, `D:\Projects\SS-dispense-align\modules\coordinate-alignment\README.md`, `D:\Projects\SS-dispense-align\modules\coordinate-alignment\module.yaml`, `D:\Projects\SS-dispense-align\modules\coordinate-alignment\CMakeLists.txt`, `D:\Projects\SS-dispense-align\modules\process-path\README.md`, `D:\Projects\SS-dispense-align\modules\process-path\module.yaml`, and `D:\Projects\SS-dispense-align\modules\process-path\CMakeLists.txt`
- [ ] T019 [P] [US2] Align verified owner surfaces in `D:\Projects\SS-dispense-align\modules\motion-planning\README.md`, `D:\Projects\SS-dispense-align\modules\motion-planning\module.yaml`, `D:\Projects\SS-dispense-align\modules\motion-planning\CMakeLists.txt`, `D:\Projects\SS-dispense-align\modules\dispense-packaging\README.md`, `D:\Projects\SS-dispense-align\modules\dispense-packaging\module.yaml`, and `D:\Projects\SS-dispense-align\modules\dispense-packaging\CMakeLists.txt`
- [ ] T020 [P] [US2] Align verified owner surfaces in `D:\Projects\SS-dispense-align\modules\runtime-execution\README.md`, `D:\Projects\SS-dispense-align\modules\runtime-execution\module.yaml`, `D:\Projects\SS-dispense-align\modules\runtime-execution\CMakeLists.txt`, `D:\Projects\SS-dispense-align\modules\trace-diagnostics\README.md`, `D:\Projects\SS-dispense-align\modules\trace-diagnostics\module.yaml`, `D:\Projects\SS-dispense-align\modules\trace-diagnostics\CMakeLists.txt`, `D:\Projects\SS-dispense-align\modules\hmi-application\README.md`, `D:\Projects\SS-dispense-align\modules\hmi-application\module.yaml`, and `D:\Projects\SS-dispense-align\modules\hmi-application\CMakeLists.txt`
- [ ] T021 [P] [US2] Align cross-root owner assets in `D:\Projects\SS-dispense-align\apps\hmi-app\`, `D:\Projects\SS-dispense-align\apps\planner-cli\`, `D:\Projects\SS-dispense-align\apps\runtime-gateway\`, `D:\Projects\SS-dispense-align\apps\runtime-service\`, `D:\Projects\SS-dispense-align\shared\`, `D:\Projects\SS-dispense-align\scripts\engineering-data-bridge\`, `D:\Projects\SS-dispense-align\tests\`, `D:\Projects\SS-dispense-align\samples\`, `D:\Projects\SS-dispense-align\config\`, `D:\Projects\SS-dispense-align\data\`, and `D:\Projects\SS-dispense-align\deploy\`
- [ ] T022 [US2] Reflect verified module and cross-root owner surfaces in `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s05-module-boundary-interface-contract.md` and `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s06-repo-structure-guide.md`

**Checkpoint**: `M0-M11` 与跨根 owner 资产都已进入同一矩阵和同一 canonical owner 口径，`US2` 可独立完成 owner 面审查

---

## Phase 5: User Story 3 - 物理清除 legacy 并完成最终验收 (Priority: P3)

**Goal**: 删除所有 active bridge/source/fallback 资产，收紧根级 gate，并用零 bridge/零 fallback 证据完成最终 closeout

**Independent Test**: 运行 `build.ps1`、`test.ps1`、`ci.ps1`、`scripts/validation/run-local-validation-gate.ps1`、`validate_workspace_layout.py`、`validate_dsp_e2e_spec_docset.py`、`legacy-exit-checks.py` 时，所有入口都对 active bridge 和 fallback 返回失败；清除后全部通过且仓库中不再存在 live bridge root

### Tests for User Story 3

- [ ] T023 [P] [US3] Add bridge-exit regression in `D:\Projects\SS-dispense-align\tests\contracts\test_bridge_exit_contract.py`
- [ ] T024 [P] [US3] Hard-fail active bridge and fallback references in `D:\Projects\SS-dispense-align\scripts\migration\legacy-exit-checks.py`, `D:\Projects\SS-dispense-align\build.ps1`, `D:\Projects\SS-dispense-align\test.ps1`, `D:\Projects\SS-dispense-align\ci.ps1`, and `D:\Projects\SS-dispense-align\scripts\validation\run-local-validation-gate.ps1`

### Implementation for User Story 3

- [ ] T025 [P] [US3] Remove workflow bridge roots and forwarders in `D:\Projects\SS-dispense-align\modules\workflow\src\`, `D:\Projects\SS-dispense-align\modules\workflow\process-runtime-core\`, `D:\Projects\SS-dispense-align\modules\workflow\application\usecases-bridge\`, and `D:\Projects\SS-dispense-align\modules\workflow\CMakeLists.txt`
- [ ] T026 [P] [US3] Remove DXF bridge roots and launch forwards in `D:\Projects\SS-dispense-align\modules\dxf-geometry\src\`, `D:\Projects\SS-dispense-align\modules\dxf-geometry\engineering-data\`, `D:\Projects\SS-dispense-align\scripts\engineering-data-bridge\`, and `D:\Projects\SS-dispense-align\modules\dxf-geometry\CMakeLists.txt`
- [ ] T027 [P] [US3] Remove runtime bridge roots and forwarders in `D:\Projects\SS-dispense-align\modules\runtime-execution\runtime-host\`, `D:\Projects\SS-dispense-align\modules\runtime-execution\device-adapters\`, `D:\Projects\SS-dispense-align\modules\runtime-execution\device-contracts\`, and `D:\Projects\SS-dispense-align\modules\runtime-execution\CMakeLists.txt`
- [ ] T028 [P] [US3] Remove remaining `src` bridge roots and bridge metadata in `D:\Projects\SS-dispense-align\modules\job-ingest\`, `D:\Projects\SS-dispense-align\modules\topology-feature\`, `D:\Projects\SS-dispense-align\modules\process-planning\`, `D:\Projects\SS-dispense-align\modules\coordinate-alignment\`, `D:\Projects\SS-dispense-align\modules\process-path\`, `D:\Projects\SS-dispense-align\modules\motion-planning\`, `D:\Projects\SS-dispense-align\modules\dispense-packaging\`, `D:\Projects\SS-dispense-align\modules\trace-diagnostics\`, and `D:\Projects\SS-dispense-align\modules\hmi-application\`
- [ ] T029 [US3] Publish zero-bridge and zero-fallback closeout evidence in `D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md`, `D:\Projects\SS-dispense-align\docs\architecture\migration-alignment-clearance-matrix.md`, and `D:\Projects\SS-dispense-align\docs\architecture\bridge-exit-closeout.md`
- [ ] T030 [US3] Refresh final validation evidence under `D:\Projects\SS-dispense-align\tests\reports\legacy-exit-current\`, `D:\Projects\SS-dispense-align\tests\reports\baseline-contracts\`, and `D:\Projects\SS-dispense-align\tests\reports\baseline-e2e-performance\` by running the zero-bridge closeout suite

**Checkpoint**: 所有 active bridge/fallback 已被物理清除，`US3` 可独立通过根级 closeout 验证

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 清理跨故事残留文档和执行最终整体验证

- [ ] T031 [P] Update release and migration guidance in `D:\Projects\SS-dispense-align\docs\runtime\release-process.md` and `D:\Projects\SS-dispense-align\docs\runtime\external-migration-observation.md` to remove compat-shell rollback guidance
- [ ] T032 [P] Sweep remaining bridge/compat terminology in `D:\Projects\SS-dispense-align\modules\`, `D:\Projects\SS-dispense-align\docs\architecture\workspace-baseline.md`, and `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\`
- [ ] T033 Run the full quickstart closeout commands from `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-dsp-e2e-full-migration\quickstart.md` and attach resulting evidence paths to `D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md` and `D:\Projects\SS-dispense-align\docs\architecture\bridge-exit-closeout.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 无依赖，可立即开始
- **Foundational (Phase 2)**: 依赖 Setup 完成；阻塞全部用户故事
- **User Story 1 (Phase 3)**: 依赖 Foundational 完成；建立唯一冻结事实源
- **User Story 2 (Phase 4)**: 依赖 User Story 1；在统一冻结口径下收敛 owner 面
- **User Story 3 (Phase 5)**: 依赖 User Story 2；在 owner 面收敛后执行物理清除与根级 closeout
- **Polish (Phase 6)**: 依赖全部用户故事完成

### User Story Dependencies

- **User Story 1 (P1)**: Foundational 完成后即可开始；不依赖其他故事
- **User Story 2 (P2)**: 依赖 `US1` 的冻结事实源与基线解释已经稳定
- **User Story 3 (P3)**: 依赖 `US2` 的矩阵、owner 面和跨根资产归位完成，否则无法安全删桥

### Parallel Opportunities

- `T004`、`T005`、`T006` 可并行，因为它们分别落在矩阵、bridge closeout/gate 脚本和 test-kit 聚合层
- `US1` 中 `T007`、`T008` 可并行，`T010`、`T011` 可并行
- `US2` 中 `T016` 到 `T021` 可按模块族并行
- `US3` 中 `T025` 到 `T028` 可按 bridge 家族并行

---

## Parallel Example: User Story 1

```text
T007 + T008 + T010 + T011
```

## Parallel Example: User Story 2

```text
T016 + T017 + T018 + T019 + T020 + T021
```

## Parallel Example: User Story 3

```text
T025 + T026 + T027 + T028
```

## Implementation Strategy

### MVP First (User Story 1 Only)

1. 完成 Phase 1: Setup
2. 完成 Phase 2: Foundational
3. 完成 Phase 3: User Story 1
4. 运行 docset + local gate 验证，确认唯一冻结事实源成立
5. 在 `docs/architecture/system-acceptance-report.md` 记录 `US1` 证据

### Incremental Delivery

1. 先完成 `US1`，冻结唯一事实源和 gate 口径
2. 在同一口径下完成 `US2`，建立矩阵并收敛 owner 面
3. 最后执行 `US3`，物理清除 bridge/fallback 并跑根级 closeout

### Parallel Team Strategy

1. 一名成员负责 `US1` 的 docset/gate 收紧
2. 两到三名成员并行处理 `US2` 的模块族和跨根资产
3. 一名成员负责 `US3` 的 gate hard-fail 和 closeout 聚合

---

## Notes

- 所有测试类任务必须先写并先失败，再进行对应实现任务
- `US2` 和 `US3` 只能在 `US1` 固化后的术语和 gate 口径下执行
- `tests/reports/` 下的 closeout 证据以重新运行根级入口生成，不手工伪造
- 分支必须持续保持 `<type>/<scope>/<ticket>-<short-desc>` 合规
