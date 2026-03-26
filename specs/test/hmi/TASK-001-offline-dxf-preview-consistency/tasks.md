# Tasks: HMI 在线点胶轨迹预览一致性验证

**Input**: 设计文档来自 `D:\Projects\SiligenSuite\specs\test\hmi\TASK-001-offline-dxf-preview-consistency\`
**Prerequisites**: `plan.md`、`spec.md`、`research.md`、`data-model.md`、`contracts\`、`quickstart.md`

**Tests**: 本特性明确要求以在线 smoke、HMI 单测、协议契约、first-layer 几何回归和 HIL 证据来证明“真实 DXF 在线预览”和“与控制卡下发准备数据语义一致”同时成立，因此每个用户故事都包含先行测试任务。
**Organization**: 任务按用户故事组织，先冻结共享契约与证据骨架，再分别交付在线查看真实轨迹、校验与执行准备语义一致、以及拦截非权威预览结果三个可独立验收的增量。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可并行执行（不同文件、无未完成前置依赖）
- **[Story]**: 对应用户故事（`US1`、`US2`、`US3`）
- 每个任务都包含明确文件或目录路径

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: 建立本特性的报告目录约定和人工验收入口

- [X] T001 Create online preview evidence guide in `D:\Projects\SiligenSuite\tests\reports\online-preview\README.md`
- [X] T002 [P] Align operator acceptance entry points in `D:\Projects\SiligenSuite\docs\validation\dispense-trajectory-preview-real-acceptance-checklist.md` and `D:\Projects\SiligenSuite\tests\e2e\hardware-in-loop\README.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 冻结跨故事共享的正式协议、在线就绪门禁和证据包骨架；本阶段完成前不得开始任一用户故事

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T003 Reconcile preview snapshot protocol fixtures and compatibility guards in `D:\Projects\SiligenSuite\shared\contracts\application\commands\dxf.command-set.json`, `D:\Projects\SiligenSuite\shared\contracts\application\fixtures\requests\dxf.preview.snapshot.request.json`, `D:\Projects\SiligenSuite\shared\contracts\application\fixtures\responses\dxf.preview.snapshot.success.json`, `D:\Projects\SiligenSuite\tests\contracts\test_protocol_compatibility.py`, and `D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\tests\test_transport_gateway_compatibility.py`
- [X] T004 [P] Freeze online-ready diagnostic contract in `D:\Projects\SiligenSuite\apps\hmi-app\scripts\online-smoke.ps1` and `D:\Projects\SiligenSuite\apps\hmi-app\docs\smoke-exit-codes.md`
- [X] T005 [P] Add canonical evidence bundle filenames and report-root handling in `D:\Projects\SiligenSuite\tests\e2e\hardware-in-loop\run_real_dxf_preview_snapshot.py` and `D:\Projects\SiligenSuite\tests\e2e\first-layer\test_real_preview_snapshot_geometry.py`

**Checkpoint**: 正式协议、在线就绪判定和证据包基础输出已固定，用户故事任务可开始

---

## Phase 3: User Story 1 - 在线查看真实轨迹 (Priority: P1) 🎯 MVP

**Goal**: 让 HMI 只在 `online_ready` 且机台已准备好的上下文中加载真实 DXF，并稳定展示来自 runtime 的权威预览

**Independent Test**: 运行 `D:\Projects\SiligenSuite\apps\hmi-app\scripts\online-smoke.ps1` 到达 `online_ready=true` 后，在 HMI 中加载 `D:\Projects\SiligenSuite\samples\dxf\rect_diag.dxf`，连续两次触发预览都能看到来源为 `runtime_snapshot` 的可复现轨迹

### Tests for User Story 1

- [X] T006 [P] [US1] Add online-ready preview UI regressions in `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_main_window.py`
- [X] T007 [P] [US1] Extend preview request/response contract coverage in `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py`
- [X] T008 [P] [US1] Exercise runtime preview launch from GUI smoke in `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\tools\ui_qtest.py` and `D:\Projects\SiligenSuite\apps\hmi-app\scripts\online-smoke.ps1`

### Implementation for User Story 1

- [X] T009 [US1] Update preview snapshot call shaping in `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\client\protocol.py`
- [X] T010 [US1] Enforce `online_ready`-only real DXF preview launch, context reset, and source display in `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\ui\main_window.py`
- [X] T011 [US1] Align preview confirmation and stale-state transitions in `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\features\dispense_preview_gate\preview_gate.py`

**Checkpoint**: HMI 已能在真实在线就绪上下文中独立展示权威轨迹预览，`US1` 可单独验收

---

## Phase 4: User Story 2 - 判断预览是否与执行准备一致 (Priority: P2)

**Goal**: 让预览结果能够回链到同一 `plan_id / plan_fingerprint / snapshot_hash`，并证明其与上位机准备下发到运动控制卡的数据在几何、顺序和点胶运动语义上一致

**Independent Test**: 对同一 `D:\Projects\SiligenSuite\samples\dxf\rect_diag.dxf` 运行 preview/prepare/confirm 链路后，`D:\Projects\SiligenSuite\tests\e2e\first-layer\test_real_preview_snapshot_geometry.py` 与 HIL 证据包同时通过，且 `preview-verdict.json` 明确记录三类语义匹配均为 `true`

### Tests for User Story 2

- [X] T012 [P] [US2] Add preview/dispatch alignment unit coverage in `D:\Projects\SiligenSuite\modules\workflow\tests\process-runtime-core\unit\dispensing\DispensingWorkflowUseCaseTest.cpp`
- [X] T013 [P] [US2] Add online preview evidence bundle contract coverage in `D:\Projects\SiligenSuite\tests\contracts\test_online_preview_evidence_contract.py`
- [X] T014 [P] [US2] Extend canonical geometry regression to assert plan/context correlation in `D:\Projects\SiligenSuite\tests\e2e\first-layer\test_real_preview_snapshot_geometry.py`

### Implementation for User Story 2

- [X] T015 [US2] Align preview snapshot hash and dispatch fingerprints in `D:\Projects\SiligenSuite\modules\workflow\application\usecases\dispensing\DispensingWorkflowUseCase.cpp` and `D:\Projects\SiligenSuite\modules\workflow\application\include\application\usecases\dispensing\DispensingWorkflowUseCase.h`
- [X] T016 [US2] Surface plan/context correlation and preview cache guards in `D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\src\tcp\TcpCommandDispatcher.cpp` and `D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\src\tcp\TcpCommandDispatcher.h`
- [X] T017 [US2] Emit `plan-prepare.json`, `preview-verdict.json`, and `preview-evidence.md` in `D:\Projects\SiligenSuite\tests\e2e\hardware-in-loop\run_real_dxf_preview_snapshot.py`
- [X] T018 [US2] Reuse preview-confirm and `dxf.job.start` correlation from `D:\Projects\SiligenSuite\tests\e2e\hardware-in-loop\run_real_dxf_machine_dryrun.py` inside `D:\Projects\SiligenSuite\tests\e2e\hardware-in-loop\run_real_dxf_preview_snapshot.py`
- [X] T019 [US2] Align manual verdict wording with dispatch-semantics comparison in `D:\Projects\SiligenSuite\docs\validation\dispense-trajectory-preview-real-acceptance-checklist.md`

**Checkpoint**: 预览与执行准备数据的语义对齐证据已经闭环，`US2` 可独立通过几何回归和 HIL 验证

---

## Phase 5: User Story 3 - 识别非权威预览结果 (Priority: P3)

**Goal**: 明确区分 `runtime_snapshot` 与 mock、未知来源、历史残留或上下文不匹配结果，并阻止后者被记为真实预览通过

**Independent Test**: 对 `mock_synthetic`、来源缺失、`online_ready=false`、DXF/plan 不匹配和历史缓存场景分别执行验证时，HMI、transport/gateway 和 HIL 证据都只能输出 `invalid-source`、`not-ready`、`mismatch` 或 `incomplete`，不得出现 `passed`

### Tests for User Story 3

- [X] T020 [P] [US3] Add source-classification and stale-preview regressions in `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_main_window.py` and `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_dispense_preview_gate.py`
- [X] T021 [P] [US3] Add invalid-source protocol and transport regressions in `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py` and `D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\tests\test_transport_gateway_compatibility.py`
- [X] T022 [P] [US3] Add non-authoritative evidence verdict coverage in `D:\Projects\SiligenSuite\tests\contracts\test_online_preview_evidence_contract.py` and `D:\Projects\SiligenSuite\tests\e2e\hardware-in-loop\run_real_dxf_preview_snapshot.py`

### Implementation for User Story 3

- [X] T023 [US3] Harden non-authoritative source messaging and block-pass conditions in `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\ui\main_window.py`
- [X] T024 [US3] Extend preview gate reason mapping for `invalid-source`, `not-ready`, and `mismatch` flows in `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\features\dispense_preview_gate\preview_gate.py`
- [X] T025 [US3] Reject cached, unknown, and mismatched preview contexts in `D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\src\tcp\TcpCommandDispatcher.cpp`
- [X] T026 [US3] Emit `invalid-source`, `not-ready`, `mismatch`, and `incomplete` verdicts with `failure_reason` in `D:\Projects\SiligenSuite\tests\e2e\hardware-in-loop\run_real_dxf_preview_snapshot.py`
- [X] T027 [US3] Document source rejection boundaries in `D:\Projects\SiligenSuite\docs\validation\dispense-trajectory-preview-real-acceptance-checklist.md` and `D:\Projects\SiligenSuite\apps\hmi-app\docs\smoke-exit-codes.md`

**Checkpoint**: 非权威结果已被系统化标识并拦截，`US3` 可独立完成错误来源验收

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 刷新执行说明并运行最终整体验证

- [X] T028 [P] Update feature execution guide in `D:\Projects\SiligenSuite\specs\test\hmi\TASK-001-offline-dxf-preview-consistency\quickstart.md` and `D:\Projects\SiligenSuite\tests\e2e\hardware-in-loop\README.md`
- [X] T029 Run the online preview validation suite from `D:\Projects\SiligenSuite\specs\test\hmi\TASK-001-offline-dxf-preview-consistency\quickstart.md` and archive evidence under `D:\Projects\SiligenSuite\tests\reports\online-preview\` and `D:\Projects\SiligenSuite\tests\reports\adhoc\real-dxf-preview-snapshot-canonical\`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 无依赖，可立即开始
- **Foundational (Phase 2)**: 依赖 Setup 完成；阻塞全部用户故事
- **User Story 1 (Phase 3)**: 依赖 Foundational 完成；建立真实在线预览最小闭环
- **User Story 2 (Phase 4)**: 依赖 `US1` 的真实预览上下文已经稳定
- **User Story 3 (Phase 5)**: 依赖 `US1` 的权威预览链已经可复用；可在 `US2` 期间并行推进
- **Polish (Phase 6)**: 依赖全部目标用户故事完成

### User Story Dependencies

- **User Story 1 (P1)**: Foundational 完成后即可开始；不依赖其他用户故事
- **User Story 2 (P2)**: 依赖 `US1` 已能稳定生成真实 `runtime_snapshot`，否则无从比较执行准备语义
- **User Story 3 (P3)**: 依赖 `US1` 的在线权威预览基线；可与 `US2` 并行，只要不覆盖同一未完成文件

### Parallel Opportunities

- `T004` 与 `T005` 可并行，因为分别落在在线 smoke 文档面和 HIL/first-layer 证据面
- `US1` 中 `T006`、`T007`、`T008` 可并行
- `US2` 中 `T012`、`T013`、`T014` 可并行
- `US3` 中 `T020`、`T021`、`T022` 可并行

---

## Parallel Example: User Story 1

```text
T006 + T007 + T008
```

## Parallel Example: User Story 2

```text
T012 + T013 + T014
```

## Parallel Example: User Story 3

```text
T020 + T021 + T022
```

## Implementation Strategy

### MVP First (User Story 1 Only)

1. 完成 Phase 1: Setup
2. 完成 Phase 2: Foundational
3. 完成 Phase 3: User Story 1
4. 运行 `online-smoke.ps1` 与 HMI 单测，确认真实 DXF 在线预览已成立
5. 在 `tests/reports/online-preview\` 记录第一版证据

### Incremental Delivery

1. 先交付 `US1`，冻结“在线就绪 + 真实 DXF + runtime_snapshot”最小闭环
2. 再交付 `US2`，补齐与控制卡下发准备数据的语义一致性证据
3. 最后交付 `US3`，把 mock、历史残留和上下文不匹配结果全部降为非通过

### Parallel Team Strategy

1. 一名成员负责 HMI/UI 与 online smoke 入口（`US1`）
2. 一名成员负责 workflow/gateway 语义对齐与契约测试（`US2`）
3. 一名成员负责非权威来源拦截与 HIL verdict 扩展（`US3`）

---

## Notes

- 所有测试任务都必须先写并先失败，再进行对应实现任务
- `US2` 和 `US3` 只接受 `preview_source == runtime_snapshot` 作为真实验收通过来源
- `D:\Projects\SiligenSuite\samples\dxf\rect_diag.dxf` 是本特性的强制基线输入，不得替换为 synthetic/mock 几何
- `D:\Projects\SiligenSuite\tests\reports\` 下的证据必须由脚本运行生成，不手工伪造
- 分支必须持续保持 `<type>/<scope>/<ticket>-<short-desc>` 合规
