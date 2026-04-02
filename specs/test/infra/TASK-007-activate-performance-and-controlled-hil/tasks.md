# Tasks: Activate Nightly Performance And Controlled HIL

**Input**: 设计文档来自 `D:\Projects\ss-e\specs\test\infra\TASK-007-activate-performance-and-controlled-hil\`  
**Prerequisites**: `spec.md`、`plan.md`、`D:\Projects\ss-e\docs\validation\layered-test-matrix.md`、`D:\Projects\ss-e\docs\architecture\build-and-test.md`

**Tests**: 本波次以正式 authority 运行结果为主，因此合同测试、正式 `nightly-performance` 运行和正式 `limited-hil` quick gate 都必须形成可回链证据。  
**Organization**: 任务按“性能正式化 -> 受控 HIL 激活 -> closeout 真值同步”组织，不再扩展新的平台语义。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可并行执行（不同文件、无未完成前置依赖）
- **[Story]**: 对应用户故事（`US1`、`US2`）

## Phase 1: Follow-up Freeze

- [X] T001 Create `TASK-007` follow-up spec/plan/tasks in `D:\Projects\ss-e\specs\test\infra\TASK-007-activate-performance-and-controlled-hil\`
- [X] T002 Point completed `TASK-006` ledger to the new follow-up in `D:\Projects\ss-e\specs\test\infra\TASK-006-improve-layered-test-system\tasks.md`

---

## Phase 2: User Story 1 - Formalize Nightly Performance (Priority: P1)

**Goal**: 让 `nightly-performance` 能在当前工作区直接运行，并由正式 threshold config 给出 blocking verdict。

**Independent Test**: 运行 `collect_dxf_preview_profiles.py --include-start-job --gate-mode nightly-performance --threshold-config tests/baselines/performance/dxf-preview-profile-thresholds.json` 后，`threshold_gate` 为 `passed`。

- [X] T003 [P] [US1] Add default gateway executable fallback contract in `D:\Projects\ss-e\tests\contracts\test_performance_threshold_gate_contract.py`
- [X] T004 [US1] Align default gateway executable resolution in `D:\Projects\ss-e\tests\performance\collect_dxf_preview_profiles.py`
- [X] T005 [US1] Freeze calibrated threshold config in `D:\Projects\ss-e\tests\baselines\performance\dxf-preview-profile-thresholds.json`
- [X] T006 [US1] Run formal `nightly-performance` calibration and archive evidence in `D:\Projects\ss-e\tests\reports\performance\dxf-preview-profiles\`

---

## Phase 3: User Story 2 - Formalize Controlled HIL Quick Gate (Priority: P2)

**Goal**: 让 `limited-hil` quick gate 无论通过还是阻塞，都能落下完整 `gate + release-summary` 证据链，并据此决定是否允许 formal `1800s` gate。

**Independent Test**: 运行 `run_hil_controlled_test.ps1 -HilDurationSeconds 60` 后，要么得到 `passed`，要么得到带 `hil-controlled-gate-summary.*` 与 `hil-controlled-release-summary.md` 的 blocked evidence。

- [X] T007 [US2] Fix offline prerequisite suite binding in `D:\Projects\ss-e\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1`
- [X] T008 [US2] Continue gate/release closeout on blocked HIL steps in `D:\Projects\ss-e\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1`
- [X] T009 [US2] Run `limited-hil` quick gate (`60s`) and archive evidence in `D:\Projects\ss-e\tests\reports\verify\phase12-limited-hil\`
- [X] T010 [US2] Run `limited-hil` formal gate (`1800s`) after quick gate passes
  - 2026-04-02 rerun after control-card power restore:
  - quick gate `passed`: `D:\Projects\ss-e\tests\reports\verify\phase12-limited-hil\20260402T005612Z\`
  - formal gate `passed`: `D:\Projects\ss-e\tests\reports\verify\phase12-limited-hil\20260402T010321Z\`

---

## Phase 4: Closeout

- [X] T011 Sync performance / HIL authority truth in `D:\Projects\ss-e\docs\architecture\build-and-test.md`, `D:\Projects\ss-e\docs\validation\README.md`, `D:\Projects\ss-e\docs\validation\layered-test-matrix.md`, `D:\Projects\ss-e\tests\performance\README.md`, `D:\Projects\ss-e\tests\e2e\hardware-in-loop\README.md`, and `D:\Projects\ss-e\docs\validation\release-test-checklist.md`
- [X] T012 Create Phase 12 closeout doc in `D:\Projects\ss-e\docs\validation\phase12-layered-test-activation-closeout-20260401.md`

## Acceptance

- `nightly-performance`：`passed`
- `limited-hil quick gate`：`passed`
- `limited-hil formal gate`：`passed`
