# Tasks: SS-dispense-align 工作区模板化原地重构

**Input**: 设计文档来自 `/specs/refactor/arch/NOISSUE-architecture-refactor-spec/`  
**Prerequisites**: `plan.md`、`spec.md`、`research.md`、`data-model.md`、`quickstart.md`、`contracts/`

**Tests**: 本特性明确要求根级门禁、冻结文档契约和验收回链闭环，因此每个用户故事都包含对应的验证任务。  
**Organization**: 任务按用户故事分组，保证每个故事都能独立实施、独立审查、独立验证。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可并行执行（不同文件、无未完成前置依赖）
- **[Story]**: 任务对应的用户故事（`US1`、`US2`、`US3`）
- 每个任务描述都包含可直接落地的精确文件路径

## Phase 1: Setup (模板根与入口骨架)

**Purpose**: 建立模板化顶层根的跟踪文件、根布局解析入口和工作区导航

- [X] T001 创建模板化顶层根骨架文件 `modules/README.md`、`modules/CMakeLists.txt`、`samples/README.md`、`scripts/README.md`、`deploy/README.md`
- [X] T002 [P] 在 `tools/scripts/get-workspace-layout.ps1` 和 `cmake/LoadWorkspaceLayout.cmake` 注册 `modules/`、`samples/`、`scripts/`、`deploy/` 的根布局解析
- [X] T003 [P] 在 `README.md` 和 `WORKSPACE.md` 发布目标工作区拓扑、迁移源根和稳定根级入口说明

---

## Phase 2: Foundational (阻塞性前置条件)

**Purpose**: 建立所有用户故事共用的构建入口、冻结文档门禁和根分类治理

**⚠️ CRITICAL**: 在本阶段完成前，不应开始任何用户故事实现

- [X] T004 在 `build.ps1`、`test.ps1`、`ci.ps1` 保持现有调用方式不变的前提下接入模板化根拓扑解析
- [X] T005 [P] 在 `tools/scripts/run-local-validation-gate.ps1` 和 `tools/migration/validate_workspace_layout.py` 增加目标根存在性、迁移源根排空和 owner 落位断言
- [X] T006 [P] 在 `scripts/migration/validate_dsp_e2e_spec_docset.py` 和 `tools/scripts/legacy_exit_checks.py` 增加冻结文档、规范术语和 legacy 回流门禁
- [X] T007 [P] 在 `docs/architecture/workspace-baseline.md`、`docs/architecture/canonical-paths.md`、`docs/decisions/ADR-001-workspace-baseline.md` 将 `modules/`、`samples/`、`scripts/`、`deploy/` 升级为 canonical roots，并将 `packages/`、`integration/`、`tools/`、`examples/` 标记为迁移源根
- [X] T008 [P] 在 `docs/architecture/removed-legacy-items-final.md` 和 `docs/architecture/system-acceptance-report.md` 建立迁移波次、源根排空和 closeout 台账

**Checkpoint**: 根布局、校验器和治理基线已经就绪，用户故事可以按优先级推进

---

## Phase 3: User Story 1 - 建立统一架构冻结基线 (Priority: P1) 🎯 MVP

**Goal**: 让团队能够以单一阶段链、职责链、成败判定和术语体系审查整个工作区主链路

**Independent Test**: 仅审阅冻结文档即可从任务发起回链到执行归档，并在每个关键阶段看到唯一 owner、输入、输出、成功条件、失败条件和禁止短路规则

### Tests for User Story 1

> **NOTE**: 先让冻结文档门禁能够识别阶段链和术语缺口，再落地正式文档内容

- [X] T009 [P] [US1] 在 `scripts/migration/validate_dsp_e2e_spec_docset.py` 增加 `dsp-e2e-spec-s01`、`dsp-e2e-spec-s02`、`dsp-e2e-spec-s03`、`dsp-e2e-spec-s10` 的必备章节和关键术语断言
- [X] T010 [P] [US1] 在 `docs/architecture/build-and-test.md` 增加冻结基线评审命令、关键术语 grep 检查和阶段回链步骤

### Implementation for User Story 1

- [X] T011 [P] [US1] 重写 `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s01-stage-io-matrix.md`，将当前工作区端到端主链映射到统一阶段链和输入输出矩阵
- [X] T012 [P] [US1] 重写 `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s02-stage-responsibility-acceptance.md`，冻结阶段职责、owner、验收口径和禁止短路规则
- [X] T013 [P] [US1] 重写 `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s03-stage-errorcode-rollback.md`，冻结失败分层、错误码、回退入口和人工放行边界
- [X] T014 [P] [US1] 在 `docs/architecture/dsp-e2e-spec/README.md` 和 `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s10-frozen-directory-index.md` 建立参考模板到正式文档的一一映射和一次性 legacy 术语迁移表
- [X] T015 [US1] 在 `docs/architecture/workspace-baseline.md` 和 `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s10-frozen-directory-index.md` 固化阶段基线的审阅顺序、事实来源和评审角色

**Checkpoint**: US1 完成后，团队应能仅依赖冻结文档审查统一架构基线，而不再依赖历史命名和口头解释

---

## Phase 4: User Story 2 - 收敛对象责任与模块边界 (Priority: P2)

**Goal**: 冻结对象 owner、模块契约和工作区真实落位，让结构收敛不再停留在命名映射层

**Independent Test**: 仅检查对象字典、模块契约、仓库结构指南和路径台账，就能判断每个核心对象的唯一 owner、每条关键跨模块协作的允许边界和每个灰区路径的最终处置

### Tests for User Story 2

> **NOTE**: 先让边界门禁能够识别 owner/path 缺口，再迁移模块、共享层和样例根

- [X] T016 [P] [US2] 在 `tools/migration/validate_workspace_layout.py` 增加 `modules/`、`shared/`、`samples/` 以及迁移源根排空规则的 owner/path 断言
- [X] T017 [P] [US2] 在 `tools/scripts/legacy_exit_checks.py` 增加 `packages/`、`integration/`、`tools/`、`examples/` 仍作为默认 owner 面时的回流扫描

### Implementation for User Story 2

- [X] T018 [P] [US2] 重写 `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s04-stage-artifact-dictionary.md`，冻结核心事实对象、生命周期、引用约束和唯一 owner
- [X] T019 [P] [US2] 重写 `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md`，冻结 M0-M11 模块边界、命令/事件和禁止越权规则
- [X] T020 [P] [US2] 重写 `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s06-repo-structure-guide.md`，将模板化目标拓扑、owner 路径和迁移源根退出条件落到真实工作区结构
- [X] T021 [P] [US2] 创建目标应用入口骨架 `apps/planner-cli/README.md`、`apps/runtime-service/README.md`、`apps/runtime-gateway/README.md`、`apps/trace-viewer/README.md`，并为替代 `apps/control-cli/`、`apps/control-runtime/`、`apps/control-tcp-server/` 建立迁移说明
- [X] T022 [P] [US2] 创建规划侧模块骨架 `modules/workflow/README.md`、`modules/job-ingest/README.md`、`modules/dxf-geometry/README.md`、`modules/topology-feature/README.md`、`modules/process-planning/README.md`、`modules/coordinate-alignment/README.md`、`modules/process-path/README.md`、`modules/motion-planning/README.md`、`modules/dispense-packaging/README.md`
- [X] T023 [P] [US2] 在 `shared/contracts/README.md`、`shared/kernel/README.md`、`shared/testing/README.md`、`shared/ids/.gitkeep`、`shared/artifacts/.gitkeep`、`shared/commands/.gitkeep`、`shared/events/.gitkeep`、`shared/failures/.gitkeep`、`shared/messaging/.gitkeep`、`shared/config/.gitkeep` 扩展共享层目标承载面
- [X] T024 [P] [US2] 通过更新 `packages/engineering-data/pyproject.toml`、`packages/process-runtime-core/CMakeLists.txt`、`modules/CMakeLists.txt` 和目标模块 `README.md`/`CMakeLists.txt` 将离线规划 owner 从 `packages/` 拆分到 `modules/`
- [X] T025 [P] [US2] 在 `samples/README.md`、`samples/golden/.gitkeep` 和 `examples/README.md` 建立样例资产从 `examples/` 迁移到 `samples/` 的归位规则和退出条件
- [X] T026 [P] [US2] 在 `docs/architecture/directory-responsibilities.md`、`docs/architecture/canonical-paths.md`、`docs/architecture/system-acceptance-report.md` 发布根目录分类、owner 落位和非主链模块 disposition 矩阵
- [X] T027 [US2] 在 `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s10-frozen-directory-index.md` 和 `docs/architecture/system-acceptance-report.md` 更新对象 owner、模块边界和迁移波次的评审链路

**Checkpoint**: US2 完成后，团队应能仅通过对象字典、模块契约和仓库结构判断任何能力应保留、迁移、合并、冻结还是移除

---

## Phase 5: User Story 3 - 建立可验收的控制与测试闭环 (Priority: P3)

**Goal**: 让状态机、命令/事件、失败分层、回退恢复和根级验证链路形成可执行验收基线

**Independent Test**: 仅检查控制语义、关键时序、失败归因和测试矩阵，即可验证主成功链、阻断链、回退链、恢复链和归档链都能回链到对象、状态、事件和失败码

### Tests for User Story 3

> **NOTE**: 先扩展验收矩阵与报告聚合，再迁移运行时、验证和自动化承载面

- [X] T028 [P] [US3] 在 `packages/test-kit/src/test_kit/workspace_validation.py` 和 `tools/scripts/invoke-workspace-tests.ps1` 增加 success/block/rollback/recovery/archive 五类验收场景和报告汇总
- [X] T029 [P] [US3] 在 `tools/scripts/run-local-validation-gate.ps1` 和 `ci.ps1` 增加冻结控制语义、根级门禁和报告发布断言

### Implementation for User Story 3

- [X] T030 [P] [US3] 重写 `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s07-state-machine-command-bus.md`，冻结状态机、命令总线、幂等字段和关键事件集
- [X] T031 [P] [US3] 重写 `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s08-system-sequence-template.md`，冻结主成功链、阻断链、回退链、恢复链和归档链时序
- [X] T032 [P] [US3] 重写 `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md`，冻结测试矩阵、证据路径、阻断规则和通过标准
- [X] T033 [P] [US3] 创建运行时侧模块和应用承载骨架 `modules/runtime-execution/README.md`、`modules/trace-diagnostics/README.md`、`modules/hmi-application/README.md`、`apps/runtime-service/CMakeLists.txt`、`apps/runtime-gateway/CMakeLists.txt`、`apps/planner-cli/CMakeLists.txt`、`apps/trace-viewer/README.md`
- [X] T034 [P] [US3] 通过更新 `packages/runtime-host/CMakeLists.txt`、`packages/device-adapters/CMakeLists.txt`、`packages/transport-gateway/CMakeLists.txt`、`packages/traceability-observability/CMakeLists.txt`、`modules/runtime-execution/CMakeLists.txt`、`modules/trace-diagnostics/CMakeLists.txt`、`apps/runtime-service/CMakeLists.txt`、`apps/runtime-gateway/CMakeLists.txt` 将运行时、网关和追溯 owner 从 `packages/` 收敛到目标拓扑
- [X] T035 [P] [US3] 在 `tests/integration/README.md`、`tests/e2e/README.md`、`tests/performance/README.md` 和 `integration/README.md` 建立验证资产从 `integration/` 迁移到 `tests/` 的正式承载关系
- [X] T036 [P] [US3] 在 `scripts/validation/.gitkeep`、`scripts/migration/.gitkeep`、`scripts/README.md`、`deploy/README.md`、`docs/runtime/deployment.md` 将仓库自动化从 `tools/` 收敛到 `scripts/` 并建立部署面
- [X] T037 [P] [US3] 在 `packages/traceability-observability/README.md`、`docs/validation/README.md`、`docs/architecture/build-and-test.md`、`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s10-frozen-directory-index.md` 对齐证据 owner、审阅入口和回链规则
- [X] T038 [US3] 在 `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s10-frozen-directory-index.md` 和 `docs/architecture/system-acceptance-report.md` 更新控制语义、验收基线和根级命令序列的最终评审流转

**Checkpoint**: US3 完成后，验收方应能从根级验证入口直接回链到冻结状态机、关键时序和正式测试矩阵

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 运行全量验证、排空迁移源根并收口最终 closeout 结论

- [X] T039 [P] 运行 `build.ps1` 和 `test.ps1` 的模板化根验证，并将证据记录到 `docs/architecture/system-acceptance-report.md`
- [X] T040 [P] 运行 `ci.ps1`、`tools/scripts/run-local-validation-gate.ps1`、`tools/scripts/legacy_exit_checks.py` 的 closeout 门禁，并将结果记录到 `docs/architecture/system-acceptance-report.md`
- [X] T041 [P] 在 `docs/architecture/removed-legacy-items-final.md` 和 `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s06-repo-structure-guide.md` 完成迁移源根排空、bridge 退出和剩余风险归零说明
- [X] T042 在 `docs/architecture/workspace-baseline.md` 和 `docs/architecture/system-acceptance-report.md` 冻结完成判定、accepted exceptions 和终态 closeout 结论

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 无依赖，可立即开始
- **Foundational (Phase 2)**: 依赖 Setup 完成，阻塞所有用户故事
- **User Story 1 (Phase 3)**: 依赖 Foundational 完成，是建议 MVP
- **User Story 2 (Phase 4)**: 依赖 Foundational 完成，并复用 US1 的正式术语和阶段口径
- **User Story 3 (Phase 5)**: 依赖 Foundational 完成，且建议在 US1/US2 的术语、对象 owner 和目标拓扑稳定后定稿
- **Polish (Phase 6)**: 依赖所有目标用户故事完成后执行

### User Story Dependencies

- **US1 (P1)**: 无故事级前置依赖，是统一冻结基线的 MVP
- **US2 (P2)**: 依赖 US1 的正式阶段名和术语，不依赖 US3
- **US3 (P3)**: 依赖 US1 的阶段语义和 US2 的 owner/边界结果，形成最终验收闭环

### Within Each User Story

- 先完成验证任务，再修改正式文档和结构落位
- 同一故事内，标记 `[P]` 的任务在满足前置依赖后可并行推进
- `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s10-frozen-directory-index.md` 必须在对应轴文档完成后再更新
- 每个故事完成后先按 Independent Test 评审，再进入下一优先级故事

### Parallel Opportunities

- Setup 阶段的 `T002`、`T003` 可并行
- Foundational 阶段的 `T005`、`T006`、`T007`、`T008` 可并行
- US1 的 `T009`、`T010`、`T011`、`T012`、`T013`、`T014` 可并行
- US2 的 `T016`、`T017`、`T018`、`T019`、`T020`、`T021`、`T022`、`T023`、`T024`、`T025`、`T026` 可并行
- US3 的 `T028`、`T029`、`T030`、`T031`、`T032`、`T033`、`T034`、`T035`、`T036`、`T037` 可并行
- Polish 阶段的 `T039`、`T040`、`T041` 可并行，`T042` 必须最后执行

---

## Parallel Example: User Story 1

```bash
Task: "重写 docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s01-stage-io-matrix.md"
Task: "重写 docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s02-stage-responsibility-acceptance.md"
Task: "重写 docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s03-stage-errorcode-rollback.md"
Task: "在 docs/architecture/build-and-test.md 增加冻结基线评审命令"
```

---

## Parallel Example: User Story 2

```bash
Task: "重写 docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s04-stage-artifact-dictionary.md"
Task: "重写 docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md"
Task: "重写 docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s06-repo-structure-guide.md"
Task: "创建 modules/workflow/README.md 至 modules/dispense-packaging/README.md 的规划侧模块骨架"
Task: "在 shared/ 扩展 contracts/kernel/testing/ids/artifacts/commands/events/failures/messaging/config 目标承载面"
```

---

## Parallel Example: User Story 3

```bash
Task: "重写 docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s07-state-machine-command-bus.md"
Task: "重写 docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s08-system-sequence-template.md"
Task: "重写 docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md"
Task: "创建 tests/integration/README.md、tests/e2e/README.md、tests/performance/README.md"
Task: "在 scripts/README.md 和 deploy/README.md 建立自动化与部署承载面"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. 完成 Phase 1: Setup
2. 完成 Phase 2: Foundational
3. 完成 Phase 3: US1
4. 按 US1 的 Independent Test 审阅阶段链、职责链和失败链
5. 确认统一术语和评审入口成立后再继续

### Incremental Delivery

1. 先完成 Setup + Foundational，建立模板化根、稳定入口和统一门禁
2. 交付 US1，冻结统一架构基线
3. 交付 US2，冻结对象 owner、模块边界和工作区真实落位
4. 交付 US3，冻结控制语义和验收闭环
5. 执行 Polish，跑通全量验证并收口终态 closeout

### Suggested MVP Scope

1. 建议 MVP 仅包含 `US1`
2. `US2` 负责把文档冻结推进到结构/owner 收敛
3. `US3` 负责把控制语义和测试闭环固化为正式验收入口

---

## Notes

- 本任务清单以 `plan.md` 的模板化目标拓扑为实施主线
- `research.md`、`data-model.md`、`contracts/` 中可复用的验证与术语约束已经体现在 Phase 2 和各故事测试任务中
- `[P]` 仅表示可并行，不表示可以跳过前置校验
- 根级入口保持 `build.ps1`、`test.ps1`、`ci.ps1` 稳定
- 分支必须持续保持 `<type>/<scope>/<ticket>-<short-desc>` 合规
