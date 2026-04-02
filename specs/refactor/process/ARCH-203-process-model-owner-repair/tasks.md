# Tasks: ARCH-203 Process Model Owner Boundary Repair

**Input**: Design documents from `/specs/refactor/process/ARCH-203-process-model-owner-repair/`  
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/`, `quickstart.md`

**Tests**: 本特性在 `spec.md` 中将测试与独立验收定义为 mandatory，因此任务显式覆盖边界门禁、消费者 cutover 回归、review traceability 补档，以及根级 `.\build.ps1` / `.\test.ps1` / `.\ci.ps1` closeout。  
**Organization**: 任务按用户故事组织，保证 `US1`、`US2`、`US3` 能按“唯一 owner 地图冻结 -> live owner 收口 -> 消费者 cutover 与最终证据”逐步落地。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可并行执行（不同文件、无未完成前置依赖）
- **[Story]**: 对应用户故事（`US1`、`US2`、`US3`）
- 每个任务都包含明确文件或目录路径

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: 对齐现有 spec 资产账本，准备后续波次共用的 evidence 容器

- [X] T001 Align requirement-to-wave ledger in `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/traceability-matrix.md`
- [X] T002 [P] Normalize `US1` owner-map evidence ledger in `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/us1-owner-map-evidence.md`
- [X] T003 [P] Normalize `US2` and `US3` evidence ledgers in `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/us2-owner-closure-evidence.md` and `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/us3-consumer-closeout-evidence.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 纳入 `Wave 0` 事实基线，并冻结所有故事共享的 guardrail

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T004 Register the 10 untracked `2026-03-31` review baseline files in `docs/process-model/reviews/` and reconcile their index in `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/README.md`
- [X] T005 [P] Finalize `ARCH-203` boundary guard rules in `scripts/validation/assert-module-boundary-bridges.ps1` for `modules/workflow/CMakeLists.txt`, `modules/job-ingest/CMakeLists.txt`, and `modules/dxf-geometry/CMakeLists.txt`
- [X] T006 [P] Add baseline completeness and review-supplement gates in `scripts/validation/run-local-validation-gate.ps1` and `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/traceability-matrix.md`
- [X] T007 [P] Rewrite imported-baseline execution order in `specs/refactor/process/ARCH-203-process-model-owner-repair/quickstart.md` and `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/remediation-wave-contract.md`

**Checkpoint**: `Wave 0` 基线已被当前分支正式跟踪，且所有后续故事共享同一套 evidence 入口和阻断规则

---

## Phase 3: User Story 1 - 冻结唯一 owner 地图 (Priority: P1) 🎯 MVP

**Goal**: 冻结 9 个在范围模块的 owner charter、禁止依赖规则和唯一 public surface

**Independent Test**: 针对任一主链能力，评审者都能通过模块 `README.md` / `module.yaml`、仓库级架构文档和 boundary guard 直接定位唯一 owner、唯一 public surface 与 forbidden dependency

### Tests for User Story 1

- [X] T008 [P] [US1] Add owner-map guard coverage for `M0/M3/M4/M5/M6/M7/M8/M9/M11` in `scripts/validation/assert-module-boundary-bridges.ps1`
- [X] T009 [P] [US1] Add owner-map acceptance rows for `FR-002`, `FR-003`, and `FR-014` in `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/traceability-matrix.md`

### Implementation for User Story 1

- [X] T010 [P] [US1] Freeze `M0` and `M9` owner charters in `modules/workflow/README.md`, `modules/workflow/module.yaml`, `modules/runtime-execution/README.md`, and `modules/runtime-execution/module.yaml`
- [X] T011 [P] [US1] Freeze `M4`, `M5`, and `M6` owner charters in `modules/process-planning/README.md`, `modules/process-planning/module.yaml`, `modules/coordinate-alignment/README.md`, `modules/coordinate-alignment/module.yaml`, `modules/process-path/README.md`, and `modules/process-path/module.yaml`
- [X] T012 [P] [US1] Freeze `M7`, `M8`, `M3`, and `M11` owner charters in `modules/motion-planning/README.md`, `modules/motion-planning/module.yaml`, `modules/dispense-packaging/README.md`, `modules/dispense-packaging/module.yaml`, `modules/topology-feature/README.md`, `modules/topology-feature/module.yaml`, `modules/hmi-application/README.md`, and `modules/hmi-application/module.yaml`
- [X] T013 [US1] Sync canonical owner map and public-surface notes in `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md`, `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s06-repo-structure-guide.md`, and `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`
- [X] T014 [US1] Record owner-map closure evidence in `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/us1-owner-map-evidence.md` and `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/traceability-matrix.md`
- [X] T015 [US1] Run owner-map boundary validation with `scripts/validation/assert-module-boundary-bridges.ps1` and store outputs in `tests/reports/module-boundary-bridges/`

**Checkpoint**: owner 地图、模块 charter 和 forbidden dependency 规则已冻结，`US1` 可独立复核

---

## Phase 4: User Story 2 - 收口主链 live owner 实现 (Priority: P1)

**Goal**: 让 `workflow/runtime-execution` 与 `M4-M8` 主链交接恢复到唯一 owner，并处置重复实现、影子实现和旧 build spine

**Independent Test**: 检查主链对象和关键 seam 时，消费者只能命中 canonical owner；`workflow` 不再默认承载下游 live owner，`job-ingest/dxf-geometry` 不再直连 concrete，旧 build spine/helper target 不再是默认依赖

### Tests for User Story 2

- [X] T016 [P] [US2] Add `M0/M9` seam regressions in `modules/runtime-execution/runtime/host/tests/CMakeLists.txt`, `modules/runtime-execution/tests/regression/CMakeLists.txt`, and `modules/workflow/tests/process-runtime-core/CMakeLists.txt`
- [X] T017 [P] [US2] Add planning-chain seam regressions in `modules/process-path/tests/CMakeLists.txt`, `modules/motion-planning/tests/CMakeLists.txt`, and `modules/dispense-packaging/tests/CMakeLists.txt`

### Implementation for User Story 2

- [X] T018 [US2] Close the `workflow <-> runtime-execution` owner seam in `modules/runtime-execution/application/CMakeLists.txt`, `modules/runtime-execution/contracts/runtime/CMakeLists.txt`, `modules/runtime-execution/runtime/host/CMakeLists.txt`, `modules/workflow/application/CMakeLists.txt`, and `modules/workflow/domain/CMakeLists.txt`
- [X] T019 [P] [US2] Recover `M4` planning ownership in `modules/process-planning/CMakeLists.txt`, `modules/process-planning/domain/configuration/CMakeLists.txt`, `apps/runtime-service/container/ApplicationContainer.cpp`, and `apps/runtime-service/container/ApplicationContainer.Dispensing.cpp`
- [X] T020 [P] [US2] Recover `M5` alignment ownership in `modules/coordinate-alignment/CMakeLists.txt`, `modules/coordinate-alignment/domain/machine/CMakeLists.txt`, and `modules/workflow/domain/domain/machine/CMakeLists.txt`
- [X] T021 [P] [US2] Recover `M6` input/path ownership in `modules/process-path/CMakeLists.txt`, `modules/process-path/domain/trajectory/CMakeLists.txt`, `modules/workflow/adapters/infrastructure/adapters/planning/dxf/CMakeLists.txt`, and `modules/dxf-geometry/adapters/dxf/CMakeLists.txt`
- [X] T022 [P] [US2] Recover `M7` planning ownership in `modules/motion-planning/CMakeLists.txt`, `modules/motion-planning/domain/motion/CMakeLists.txt`, `modules/runtime-execution/runtime/host/CMakeLists.txt`, and `modules/dispense-packaging/application/CMakeLists.txt`
- [X] T023 [P] [US2] Recover `M8` packaging ownership in `modules/dispense-packaging/CMakeLists.txt`, `modules/dispense-packaging/domain/dispensing/CMakeLists.txt`, `modules/workflow/domain/domain/dispensing/CMakeLists.txt`, and `modules/workflow/tests/process-runtime-core/CMakeLists.txt`
- [X] T024 [US2] Retire default legacy build spine usage in `modules/workflow/CMakeLists.txt`, `modules/workflow/tests/regression/CMakeLists.txt`, `modules/workflow/tests/process-runtime-core/CMakeLists.txt`, `modules/process-path/domain/trajectory/CMakeLists.txt`, `modules/motion-planning/domain/motion/CMakeLists.txt`, and `modules/dispense-packaging/domain/dispensing/CMakeLists.txt`
- [X] T025 [US2] Cut reverse concrete dependencies in `modules/job-ingest/CMakeLists.txt`, `modules/job-ingest/tests/CMakeLists.txt`, `modules/dxf-geometry/CMakeLists.txt`, and `modules/dxf-geometry/tests/CMakeLists.txt`
- [X] T026 [US2] Record owner-closure evidence in `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/us2-owner-closure-evidence.md` and `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/traceability-matrix.md`
- [X] T027 [US2] Run Wave 1-2 validation via `build.ps1`, `test.ps1`, and `scripts/validation/assert-module-boundary-bridges.ps1`, then store reports in `tests/reports/arch-203/`

**Checkpoint**: 主链 live owner 已收口，重复实现和旧 build spine 已有明确处置并通过针对性验证

---

## Phase 5: User Story 3 - 统一消费者接入与验收证据 (Priority: P2)

**Goal**: 让宿主、CLI、gateway、runtime-service、测试入口和评审证据全部切到 canonical public surface，并完成 closeout 证据

**Independent Test**: 宿主和消费者不再依赖内部路径、compat re-export、无关链接依赖或 sibling worktree 未跟踪资产；review traceability 补档完成后，closeout 可只凭当前 worktree 复查

### Tests for User Story 3

- [X] T028 [P] [US3] Add consumer-cutover regressions in `apps/runtime-gateway/transport-gateway/tests/test_transport_gateway_compatibility.py`, `apps/hmi-app/tests/unit/test_main_window.py`, `apps/hmi-app/tests/unit/test_m11_owner_imports.py`, and `shared/contracts/application/tests/test_application_contracts_compatibility.py`
- [X] T029 [P] [US3] Add review-traceability supplement gates in `scripts/validation/run-local-validation-gate.ps1` and `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/traceability-matrix.md`

### Implementation for User Story 3

- [X] T030 [P] [US3] Cut over CLI, gateway, and runtime-service consumers in `apps/planner-cli/CommandHandlers.Dxf.cpp`, `apps/runtime-gateway/transport-gateway/src/tcp/TcpCommandDispatcher.cpp`, `apps/runtime-gateway/transport-gateway/src/wiring/TcpServerHost.cpp`, `apps/runtime-service/bootstrap/ContainerBootstrap.cpp`, and `apps/runtime-service/container/ApplicationContainer.cpp`
- [X] T031 [P] [US3] Cut over HMI host imports and remove compat re-export in `apps/hmi-app/src/hmi_client/module_paths.py`, `apps/hmi-app/src/hmi_client/client/__init__.py`, `apps/hmi-app/src/hmi_client/client/preview_session.py`, `apps/hmi-app/src/hmi_client/ui/main_window.py`, and `modules/hmi-application/application/hmi_application/preview_session.py`
- [X] T032 [P] [US3] Retire duplicate owner tests and stale link dependencies in `apps/hmi-app/tests/unit/test_m11_owner_imports.py`, `apps/planner-cli/CMakeLists.txt`, `apps/runtime-service/tests/CMakeLists.txt`, `modules/workflow/tests/process-runtime-core/CMakeLists.txt`, and `modules/dispense-packaging/tests/CMakeLists.txt`
- [X] T033 [P] [US3] Backfill hash, diff, command, and precise-index supplements in `docs/process-model/reviews/coordinate-alignment-module-architecture-review-20260331-074844.md`, `docs/process-model/reviews/dispense-packaging-module-architecture-review-20260331-074840.md`, `docs/process-model/reviews/topology-feature-module-architecture-review-20260331-075200.md`, and `docs/process-model/reviews/process-planning-module-architecture-review-20260331-075201.md`
- [X] T034 [P] [US3] Sync inventory and consumer-surface docs in `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`, `docs/README.md`, and `docs/process-model/reviews/README.md`
- [X] T035 [US3] Record consumer-cutover and closeout evidence in `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/us3-consumer-closeout-evidence.md` and `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/traceability-matrix.md`
- [X] T036 [US3] Run root-entry closeout via `build.ps1`, `test.ps1`, `ci.ps1`, and `scripts/validation/run-local-validation-gate.ps1`, then store reports in `tests/reports/arch-203/`

**Checkpoint**: 消费者接入、评审补档和 closeout evidence 已统一到当前分支与当前 worktree

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 收尾规划资产与最终一致性检查

- [X] T037 [P] Sweep remaining `ARCH-203` blocker and TODO wording in `specs/refactor/process/ARCH-203-process-model-owner-repair/plan.md`, `specs/refactor/process/ARCH-203-process-model-owner-repair/quickstart.md`, `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/README.md`, and `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/traceability-matrix.md`
- [X] T038 Run full quickstart verification from `specs/refactor/process/ARCH-203-process-model-owner-repair/quickstart.md` and finalize `specs/refactor/process/ARCH-203-process-model-owner-repair/contracts/traceability-matrix.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 无依赖，可立即开始
- **Foundational (Phase 2)**: 依赖 Setup 完成；阻塞全部用户故事
- **User Story 1 (Phase 3)**: 依赖 Foundational 完成；冻结唯一 owner 地图
- **User Story 2 (Phase 4)**: 依赖 `US1`；在 owner 地图冻结后收口 live owner 与旧 build spine
- **User Story 3 (Phase 5)**: 依赖 `US2`；在 canonical public surface 明确后执行消费者 cutover 和 closeout
- **Polish (Phase 6)**: 依赖全部用户故事完成

### User Story Dependencies

- **User Story 1 (P1)**: Foundational 完成后即可开始；不依赖其他故事
- **User Story 2 (P1)**: 依赖 `US1` 的 owner charter、forbidden dependency 和 canonical public surface 已冻结
- **User Story 3 (P2)**: 依赖 `US2` 已完成主要 owner seam 收口，否则消费者无法稳定切换

### Within Each User Story

- 测试任务先行，用来固定失败条件和回归门禁
- owner charter / seam 定义先于消费者 cutover
- evidence 回写必须在每个故事收尾前完成
- 根级或波次验证通过后，才能把对应故事视为可关闭

### Parallel Opportunities

- `T002` 与 `T003` 可并行
- `T005`、`T006`、`T007` 可并行
- `T010`、`T011`、`T012` 可并行
- `T016` 与 `T017` 可并行
- `T019`、`T020`、`T021`、`T022`、`T023` 可并行
- `T028` 与 `T029` 可并行
- `T030`、`T031`、`T032`、`T033`、`T034` 可并行

---

## Parallel Example: User Story 1

```text
T010 + T011 + T012
```

## Parallel Example: User Story 2

```text
T019 + T020 + T021 + T022 + T023
```

## Parallel Example: User Story 3

```text
T030 + T031 + T032 + T033 + T034
```

## Implementation Strategy

### MVP First (User Story 1 Only)

1. 完成 Phase 1: Setup
2. 完成 Phase 2: Foundational
3. 完成 Phase 3: User Story 1
4. 运行 owner-map guard 和 evidence 回写
5. 在 `contracts/us1-owner-map-evidence.md` 中冻结 MVP 证据

### Incremental Delivery

1. 先完成 `US1`，建立唯一 owner 地图
2. 再完成 `US2`，收口主链 live owner 和旧 build spine
3. 最后完成 `US3`，统一消费者接入并完成 closeout

### Parallel Team Strategy

1. 一名成员负责 `Wave 0` 基线跟踪与 guardrail
2. 两到三名成员并行处理 `US2` 的 `M0/M9`、`M4-M6`、`M7-M8` 子链
3. 一名成员负责 `US3` 的宿主/消费者 cutover 与 review supplement 补档

---

## Notes

- 所有验证任务都必须把输出落到 `tests/reports/` 或对应 evidence 文件，避免出现“执行过但不可复查”的状态
- `job-ingest` 与 `dxf-geometry` 是正式消费者，不允许在执行时被排除
- `workflow/tests`、app 侧 owner 单测、compat re-export、无关链接依赖必须显式处置，不能默认遗留
- 复核报告里的 4 个可追溯性补强项属于 closeout 必做项
- 分支必须持续保持 `<type>/<scope>/<ticket>-<short-desc>` 合规
