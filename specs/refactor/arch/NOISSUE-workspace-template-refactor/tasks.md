# Tasks: 工作区模板化架构重构

**Input**: 设计文档来自 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\`  
**Prerequisites**: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\spec.md`、`D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\plan.md`、`D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\`

**Tests**: 本特性明确要求根级门禁、冻结文档契约、控制语义和 closeout 证据闭环，因此每个用户故事都包含对应的验证任务。  
**Organization**: 任务按独立可验收的用户故事组织；每个故事直接对应一个主迁移风险边界或一个主波次。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可并行执行（不同文件、无未完成前置依赖）
- **[Story]**: 任务所属用户故事（`US1` 到 `US7`）
- 每个任务描述均包含明确文件路径

## Phase 1: Setup (任务索引与工作目录)

**Purpose**: 为本次任务建立独立的波次索引、cutover 清单和门禁矩阵入口

- [X] T001 创建波次迁移索引文件 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\wave-mapping.md`
- [X] T002 [P] 创建模块 cutover 清单文件 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\module-cutover-checklist.md`
- [X] T003 [P] 创建波次门禁矩阵文件 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\validation-gates.md`

---

## Phase 2: Foundational (阻塞性前置条件)

**Purpose**: 建立所有故事共用的 canonical root 分类、桥接策略、脚本入口和门禁机制

**⚠️ CRITICAL**: 本阶段未完成前，不允许开始任何模块 owner 迁移

- [X] T004 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\wave-mapping.md` 填写 legacy roots 到 canonical roots 的总迁移盘点表
- [X] T005 [P] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\module-cutover-checklist.md` 填写 `M0-M11` 的 cutover 判定标准
- [X] T006 [P] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\validation-gates.md` 定义 `Wave 0-6` 的入口/退出门禁
- [X] T007 [P] 对齐 canonical root 分类文档 `D:\Projects\SS-dispense-align\docs\architecture\canonical-paths.md` 和 `D:\Projects\SS-dispense-align\docs\architecture\directory-responsibilities.md`
- [X] T008 [P] 在 `D:\Projects\SS-dispense-align\cmake\workspace-layout.env` 和 `D:\Projects\SS-dispense-align\cmake\LoadWorkspaceLayout.cmake` 注册 canonical roots 变量与解析逻辑
- [X] T009 [P] 在 `D:\Projects\SS-dispense-align\scripts\build\build-validation.ps1`、`D:\Projects\SS-dispense-align\scripts\validation\invoke-workspace-tests.ps1`、`D:\Projects\SS-dispense-align\scripts\migration\legacy-exit-checks.py` 建立正式脚本入口
- [X] T010 [P] 将 `D:\Projects\SS-dispense-align\tools\build\build-validation.ps1`、`D:\Projects\SS-dispense-align\tools\scripts\invoke-workspace-tests.ps1`、`D:\Projects\SS-dispense-align\tools\scripts\legacy_exit_checks.py` 降级为 wrapper
- [X] T011 [P] 在 `D:\Projects\SS-dispense-align\build.ps1`、`D:\Projects\SS-dispense-align\test.ps1`、`D:\Projects\SS-dispense-align\ci.ps1` 保持根级调用接口稳定并切向新脚本根
- [X] T012 [P] 在 `D:\Projects\SS-dispense-align\shared\CMakeLists.txt`、`D:\Projects\SS-dispense-align\shared\kernel\CMakeLists.txt`、`D:\Projects\SS-dispense-align\shared\contracts\CMakeLists.txt`、`D:\Projects\SS-dispense-align\shared\testing\CMakeLists.txt` 建立 shared 根的正式构建入口
- [X] T013 [P] 在 `D:\Projects\SS-dispense-align\tools\migration\validate_workspace_layout.py` 和 `D:\Projects\SS-dispense-align\scripts\migration\validate_dsp_e2e_spec_docset.py` 增加 wave-aware root/freeze 断言

**Checkpoint**: 新任务目录、canonical root 解析、脚本桥和根级门禁已经就绪

---

## Phase 3: User Story 1 - 锁定统一架构冻结基线 (Priority: P1) 🎯 MVP

**Goal**: 让所有后续迁移都基于同一套阶段链、术语和失败语义进行

**Independent Test**: 仅通过正式冻结文档和根级评审入口即可完成端到端基线审阅

### Deliverables

- `dsp-e2e-spec-s01`
- `dsp-e2e-spec-s02`
- `dsp-e2e-spec-s03`
- `dsp-e2e-spec-s10` 的统一索引

### Tests for User Story 1

- [X] T014 [P] [US1] 在 `D:\Projects\SS-dispense-align\scripts\migration\validate_dsp_e2e_spec_docset.py` 增加阶段链、失败链和规范术语断言
- [X] T015 [P] [US1] 在 `D:\Projects\SS-dispense-align\docs\architecture\build-and-test.md` 增加冻结文档评审命令和统一术语检查步骤

### Implementation for User Story 1

- [X] T016 [P] [US1] 对齐阶段输入/输出基线到 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s01-stage-io-matrix.md`
- [X] T017 [P] [US1] 对齐阶段职责与验收基线到 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s02-stage-responsibility-acceptance.md`
- [X] T018 [P] [US1] 对齐失败分层与回退基线到 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s03-stage-errorcode-rollback.md`
- [X] T019 [P] [US1] 同步参考模板到正式冻结索引的映射于 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\README.md` 和 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s10-frozen-directory-index.md`
- [X] T020 [US1] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\wave-mapping.md` 和 `D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md` 记录 US1 closeout 证据

**Checkpoint**: US1 完成后，团队应能仅依赖冻结文档完成统一基线审查

---

## Phase 4: User Story 2 - 固化 canonical roots 与共享支撑面 (Priority: P2)

**Goal**: 让 `shared/`、`scripts/`、`samples/`、`tests/`、`deploy/` 成为正式承载面，并让旧根退出默认 owner 地位

**Independent Test**: 仅检查 canonical root 文档、shared/script/test support 承载面和根级门禁，即可判断新根是否真正可用

### Deliverables

- `dsp-e2e-spec-s06`
- `shared/` 下的正式公共层
- `scripts/` 下的正式自动化入口
- `samples/`、`tests/`、`deploy/` 的正式承载说明

### Tests for User Story 2

- [X] T021 [P] [US2] 在 `D:\Projects\SS-dispense-align\tools\migration\validate_workspace_layout.py` 和 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\validation-gates.md` 增加 canonical roots 与 shared-support 断言
- [X] T022 [P] [US2] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\module-cutover-checklist.md` 增加 `Wave 1` 的 cutover 检查点

### Implementation for User Story 2

- [X] T023 [P] [US2] 对齐仓库结构与 root 处置基线到 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s06-repo-structure-guide.md`
- [X] T024 [P] [US2] 发布 shared 根职责说明到 `D:\Projects\SS-dispense-align\shared\kernel\README.md`、`D:\Projects\SS-dispense-align\shared\contracts\README.md`、`D:\Projects\SS-dispense-align\shared\testing\README.md`
- [X] T025 [P] [US2] 发布 automation/sample/deploy 根职责说明到 `D:\Projects\SS-dispense-align\scripts\README.md`、`D:\Projects\SS-dispense-align\samples\README.md`、`D:\Projects\SS-dispense-align\deploy\README.md`
- [X] T026 [P] [US2] 将 shared-kernel 基元归位到 `D:\Projects\SS-dispense-align\shared\kernel\CMakeLists.txt`、`D:\Projects\SS-dispense-align\shared\ids\README.md`、`D:\Projects\SS-dispense-align\shared\artifacts\README.md`、`D:\Projects\SS-dispense-align\shared\messaging\README.md`、`D:\Projects\SS-dispense-align\shared\logging\README.md`
- [X] T027 [P] [US2] 将稳定跨模块契约归位到 `D:\Projects\SS-dispense-align\shared\contracts\application\README.md`、`D:\Projects\SS-dispense-align\shared\contracts\engineering\README.md`、`D:\Projects\SS-dispense-align\shared\contracts\device\README.md`
- [X] T028 [P] [US2] 将 test-kit 共用支撑归位到 `D:\Projects\SS-dispense-align\shared\testing\CMakeLists.txt` 和 `D:\Projects\SS-dispense-align\shared\testing\README.md`
- [X] T029 [P] [US2] 在 `D:\Projects\SS-dispense-align\docs\architecture\workspace-baseline.md` 和 `D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md` 固化 canonical roots 与 migration-source roots 的正式口径
- [X] T030 [US2] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\wave-mapping.md` 和 `D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md` 记录 US2 closeout 证据

**Checkpoint**: US2 完成后，canonical roots 应已成为后续迁移的正式承载面

---

## Phase 5: User Story 3 - 收敛上游静态工程链 `M1-M3` (Priority: P3)

**Goal**: 让 `job-ingest`、`dxf-geometry`、`topology-feature` 成为真实 owner，并稳定上游输入事实和样本来源

**Independent Test**: 仅检查 `modules/job-ingest`、`modules/dxf-geometry`、`modules/topology-feature` 及其样本/测试入口，即可验证上游链收敛状态

### Deliverables

- `modules/job-ingest/`
- `modules/dxf-geometry/`
- `modules/topology-feature/`
- 上游链对应的样本与验证入口

### Tests for User Story 3

- [X] T031 [P] [US3] 在 `D:\Projects\SS-dispense-align\tools\migration\validate_workspace_layout.py` 和 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\validation-gates.md` 增加 `M1-M3` owner 路径断言
- [X] T032 [P] [US3] 在 `D:\Projects\SS-dispense-align\tests\integration\README.md` 和 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\module-cutover-checklist.md` 增加上游链检查点

### Implementation for User Story 3

- [X] T033 [P] [US3] 将 `M1 job-ingest` owner 入口归位到 `D:\Projects\SS-dispense-align\modules\job-ingest\CMakeLists.txt`、`D:\Projects\SS-dispense-align\modules\job-ingest\README.md`、`D:\Projects\SS-dispense-align\modules\job-ingest\contracts\README.md`
- [X] T034 [P] [US3] 将 `M2 dxf-geometry` owner 入口归位到 `D:\Projects\SS-dispense-align\modules\dxf-geometry\CMakeLists.txt`、`D:\Projects\SS-dispense-align\modules\dxf-geometry\README.md`、`D:\Projects\SS-dispense-align\modules\dxf-geometry\contracts\README.md`
- [X] T035 [P] [US3] 将 `M3 topology-feature` owner 入口归位到 `D:\Projects\SS-dispense-align\modules\topology-feature\CMakeLists.txt`、`D:\Projects\SS-dispense-align\modules\topology-feature\README.md`、`D:\Projects\SS-dispense-align\modules\topology-feature\contracts\README.md`
- [X] T036 [P] [US3] 将上游样本和 golden 输入归位到 `D:\Projects\SS-dispense-align\samples\dxf\README.md`、`D:\Projects\SS-dispense-align\samples\golden\README.md`，并在 `D:\Projects\SS-dispense-align\examples\README.md` 写明退出口径
- [X] T037 [P] [US3] 同步上游对象与模块边界到 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s04-stage-artifact-dictionary.md` 和 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s05-module-boundary-interface-contract.md`
- [X] T038 [US3] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\wave-mapping.md`、`D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\module-cutover-checklist.md`、`D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md` 记录 US3 closeout 证据

**Checkpoint**: US3 完成后，上游静态工程链应已从 legacy roots 收敛到 `modules/` 与 `samples/`

---

## Phase 6: User Story 4 - 收敛工作流与规划链 `M0`、`M4-M8` (Priority: P4)

**Goal**: 把 `process-runtime-core` 的混合 owner 拆成真实的 `workflow/process-planning/coordinate-alignment/process-path/motion-planning/dispense-packaging`

**Independent Test**: 仅检查 `M0/M4-M8` 的模块入口、依赖方向和对象 owner 文档，即可判断规划链是否已完成 owner 收敛

### Deliverables

- `modules/workflow/`
- `modules/process-planning/`
- `modules/coordinate-alignment/`
- `modules/process-path/`
- `modules/motion-planning/`
- `modules/dispense-packaging/`

### Tests for User Story 4

- [X] T039 [P] [US4] 在 `D:\Projects\SS-dispense-align\tools\migration\validate_workspace_layout.py` 和 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\validation-gates.md` 增加 `M0/M4-M8` owner 路径断言
- [X] T040 [P] [US4] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\module-cutover-checklist.md` 增加规划链 cutover 检查点

### Implementation for User Story 4

- [X] T041 [P] [US4] 将 `M0 workflow` owner 入口归位到 `D:\Projects\SS-dispense-align\modules\workflow\CMakeLists.txt`、`D:\Projects\SS-dispense-align\modules\workflow\README.md`、`D:\Projects\SS-dispense-align\modules\workflow\contracts\README.md`
- [X] T042 [P] [US4] 将 `M4 process-planning` owner 入口归位到 `D:\Projects\SS-dispense-align\modules\process-planning\CMakeLists.txt`、`D:\Projects\SS-dispense-align\modules\process-planning\README.md`、`D:\Projects\SS-dispense-align\modules\process-planning\contracts\README.md`
- [X] T043 [P] [US4] 将 `M5 coordinate-alignment` owner 入口归位到 `D:\Projects\SS-dispense-align\modules\coordinate-alignment\CMakeLists.txt`、`D:\Projects\SS-dispense-align\modules\coordinate-alignment\README.md`、`D:\Projects\SS-dispense-align\modules\coordinate-alignment\contracts\README.md`
- [X] T044 [P] [US4] 将 `M6 process-path` owner 入口归位到 `D:\Projects\SS-dispense-align\modules\process-path\CMakeLists.txt`、`D:\Projects\SS-dispense-align\modules\process-path\README.md`、`D:\Projects\SS-dispense-align\modules\process-path\contracts\README.md`
- [X] T045 [P] [US4] 将 `M7 motion-planning` owner 入口归位到 `D:\Projects\SS-dispense-align\modules\motion-planning\CMakeLists.txt`、`D:\Projects\SS-dispense-align\modules\motion-planning\README.md`、`D:\Projects\SS-dispense-align\modules\motion-planning\contracts\README.md`
- [X] T046 [P] [US4] 将 `M8 dispense-packaging` owner 入口归位到 `D:\Projects\SS-dispense-align\modules\dispense-packaging\CMakeLists.txt`、`D:\Projects\SS-dispense-align\modules\dispense-packaging\README.md`、`D:\Projects\SS-dispense-align\modules\dispense-packaging\contracts\README.md`
- [X] T047 [P] [US4] 同步规划链对象与模块边界到 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s04-stage-artifact-dictionary.md` 和 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s05-module-boundary-interface-contract.md`
- [X] T048 [US4] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\wave-mapping.md`、`D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\module-cutover-checklist.md`、`D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md` 记录 US4 closeout 证据

**Checkpoint**: US4 完成后，`process-runtime-core` 不应再承担 `M0/M4-M8` 的终态 owner 职责

---

## Phase 7: User Story 5 - 收敛运行时执行链 `M9` 与运行时入口 (Priority: P5)

**Goal**: 让 `runtime-execution`、`runtime-service`、`runtime-gateway` 成为运行时链的正式 owner/入口，并与上游规划链断开越权回写

**Independent Test**: 仅检查运行时模块入口、宿主壳职责和控制语义文档，即可判断运行时链是否完成受控收敛

### Deliverables

- `modules/runtime-execution/`
- `apps/runtime-service/`
- `apps/runtime-gateway/`
- 运行态设备契约与适配的正式归位

### Tests for User Story 5

- [X] T049 [P] [US5] 在 `D:\Projects\SS-dispense-align\scripts\validation\invoke-workspace-tests.ps1` 和 `D:\Projects\SS-dispense-align\tools\migration\validate_workspace_layout.py` 增加运行时链 owner/验收断言
- [X] T050 [P] [US5] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\module-cutover-checklist.md` 和 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\validation-gates.md` 增加运行时链检查点

### Implementation for User Story 5

- [X] T051 [P] [US5] 将 `M9 runtime-execution` owner 入口归位到 `D:\Projects\SS-dispense-align\modules\runtime-execution\CMakeLists.txt`、`D:\Projects\SS-dispense-align\modules\runtime-execution\README.md`、`D:\Projects\SS-dispense-align\modules\runtime-execution\contracts\README.md`
- [X] T052 [P] [US5] 将 runtime service 宿主职责归位到 `D:\Projects\SS-dispense-align\apps\runtime-service\CMakeLists.txt`，并在 `D:\Projects\SS-dispense-align\apps\control-runtime\CMakeLists.txt` 降级旧 owner 路径
- [X] T053 [P] [US5] 将 runtime gateway 宿主职责归位到 `D:\Projects\SS-dispense-align\apps\runtime-gateway\CMakeLists.txt`，并在 `D:\Projects\SS-dispense-align\apps\control-tcp-server\CMakeLists.txt` 降级旧 owner 路径
- [X] T054 [P] [US5] 将设备契约和适配归位到 `D:\Projects\SS-dispense-align\shared\contracts\device\README.md`、`D:\Projects\SS-dispense-align\modules\runtime-execution\README.md`、`D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\README.md`
- [X] T055 [P] [US5] 同步运行时控制语义到 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s07-state-machine-command-bus.md` 和 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s08-system-sequence-template.md`
- [X] T056 [US5] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\wave-mapping.md`、`D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\module-cutover-checklist.md`、`D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md` 记录 US5 closeout 证据

**Checkpoint**: US5 完成后，运行时链应已具备独立 owner 与入口壳，且不再回写上游规划事实

---

## Phase 8: User Story 6 - 收敛追溯、HMI 与验证承载面 `M10-M11` (Priority: P6)

**Goal**: 让 `trace-diagnostics`、`hmi-application`、`tests/`、`samples/` 成为正式承载面，并为最终 cutover 准备完整证据面

**Independent Test**: 仅检查 trace/HMI 模块、`tests/`、`samples/` 和验收基线文档，即可判断展示、追溯和验证资产是否完成正式落位

### Deliverables

- `modules/trace-diagnostics/`
- `modules/hmi-application/`
- `tests/`
- `samples/`

### Tests for User Story 6

- [X] T057 [P] [US6] 在 `D:\Projects\SS-dispense-align\packages\test-kit\src\test_kit\workspace_validation.py`、`D:\Projects\SS-dispense-align\docs\validation\README.md`、`D:\Projects\SS-dispense-align\tools\migration\validate_workspace_layout.py` 增加 trace/HMI/validation 断言
- [X] T058 [P] [US6] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\module-cutover-checklist.md` 和 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\validation-gates.md` 增加 `M10-M11/tests/samples` 检查点

### Implementation for User Story 6

- [X] T059 [P] [US6] 将 `M10 trace-diagnostics` owner 入口归位到 `D:\Projects\SS-dispense-align\modules\trace-diagnostics\CMakeLists.txt`、`D:\Projects\SS-dispense-align\modules\trace-diagnostics\README.md`、`D:\Projects\SS-dispense-align\modules\trace-diagnostics\contracts\README.md`
- [X] T060 [P] [US6] 将 `M11 hmi-application` owner 入口归位到 `D:\Projects\SS-dispense-align\modules\hmi-application\CMakeLists.txt`、`D:\Projects\SS-dispense-align\modules\hmi-application\README.md`、`D:\Projects\SS-dispense-align\modules\hmi-application\contracts\README.md`，并在 `D:\Projects\SS-dispense-align\apps\hmi-app\README.md` 标注宿主职责
- [X] T061 [P] [US6] 将仓库级验证承载面归位到 `D:\Projects\SS-dispense-align\tests\CMakeLists.txt`、`D:\Projects\SS-dispense-align\tests\integration\README.md`、`D:\Projects\SS-dispense-align\tests\e2e\README.md`、`D:\Projects\SS-dispense-align\tests\performance\README.md`
- [X] T062 [P] [US6] 将样本承载面归位到 `D:\Projects\SS-dispense-align\samples\README.md`、`D:\Projects\SS-dispense-align\samples\golden\README.md`，并在 `D:\Projects\SS-dispense-align\examples\README.md` 标注退出口径
- [X] T063 [P] [US6] 同步验收与索引基线到 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md` 和 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s10-frozen-directory-index.md`
- [X] T064 [US6] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\wave-mapping.md`、`D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\module-cutover-checklist.md`、`D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md` 记录 US6 closeout 证据

**Checkpoint**: US6 完成后，trace/HMI/validation/sample 应已全部迁入正式承载面

---

## Phase 9: User Story 7 - 完成根级构建图切换与 legacy exit (Priority: P7)

**Goal**: 将 canonical roots 变成唯一真实构建图，并关闭 legacy roots 的默认 owner 地位

**Independent Test**: 仅运行根级 `build.ps1`、`test.ps1`、`ci.ps1`、local validation gate 并审查 legacy exit 报告，即可验证终态切换是否完成

### Deliverables

- 切换后的根级 `CMakeLists.txt`
- 切换后的 `apps/CMakeLists.txt`
- 切换后的 `tests/CMakeLists.txt`
- 切换后的 `cmake/workspace-layout.env`
- legacy roots 的 tombstone/wrapper

### Tests for User Story 7

- [X] T065 [P] [US7] 在 `D:\Projects\SS-dispense-align\scripts\migration\legacy-exit-checks.py`、`D:\Projects\SS-dispense-align\tools\scripts\legacy_exit_checks.py`、`D:\Projects\SS-dispense-align\scripts\build\build-validation.ps1` 增加 canonical graph 与 legacy exit 断言
- [X] T066 [P] [US7] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\validation-gates.md` 和 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\module-cutover-checklist.md` 增加 `Wave 6` 验收检查点

### Implementation for User Story 7

- [X] T067 [US7] 切换根级 canonical build graph 于 `D:\Projects\SS-dispense-align\CMakeLists.txt`、`D:\Projects\SS-dispense-align\apps\CMakeLists.txt`、`D:\Projects\SS-dispense-align\tests\CMakeLists.txt`、`D:\Projects\SS-dispense-align\cmake\workspace-layout.env`
- [X] T068 [P] [US7] 将 legacy roots 降级为 tombstone 或 wrapper 于 `D:\Projects\SS-dispense-align\packages\README.md`、`D:\Projects\SS-dispense-align\integration\README.md`、`D:\Projects\SS-dispense-align\tools\README.md`、`D:\Projects\SS-dispense-align\examples\README.md`
- [X] T069 [US7] 在 `D:\Projects\SS-dispense-align\docs\architecture\removed-legacy-items-final.md`、`D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md`、`D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\wave-mapping.md` 发布 `Wave 6` closeout 证据

**Checkpoint**: US7 完成后，canonical roots 应已成为唯一真实构建图，legacy roots 不再承担终态 owner 职责

---

## Phase 10: Polish & Cross-Cutting Concerns

**Purpose**: 运行最终门禁、汇总证据并冻结完成判定

- [X] T070 [P] 运行 `D:\Projects\SS-dispense-align\build.ps1` 并将构建证据写入 `D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md`
- [X] T071 [P] 运行 `D:\Projects\SS-dispense-align\test.ps1` 并将测试证据写入 `D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md`
- [X] T072 [P] 运行 `D:\Projects\SS-dispense-align\ci.ps1` 并将 CI 证据写入 `D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md`
- [X] T073 [P] 运行 `D:\Projects\SS-dispense-align\tools\scripts\run-local-validation-gate.ps1` 并将 gate 证据写入 `D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md`
- [X] T074 在 `D:\Projects\SS-dispense-align\docs\architecture\workspace-baseline.md` 和 `D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md` 冻结终态 closeout 结论

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 无依赖，可立即开始
- **Foundational (Phase 2)**: 依赖 Setup 完成，阻塞所有故事
- **US1 (Phase 3)**: 依赖 Foundational 完成，是建议 MVP
- **US2 (Phase 4)**: 依赖 US1 的统一术语与评审入口
- **US3 (Phase 5)**: 依赖 US2 的 canonical roots 与 shared/script/test support 已稳定
- **US4 (Phase 6)**: 依赖 US3 已稳定上游输入事实
- **US5 (Phase 7)**: 依赖 US4 已稳定 `M0/M4-M8`
- **US6 (Phase 8)**: 依赖 US5 已稳定运行时链
- **US7 (Phase 9)**: 依赖 US6 已稳定 trace/HMI/validation/sample 承载面
- **Polish (Phase 10)**: 依赖 US1-US7 全部完成

### Wave Dependencies

- **Wave 0**: 对应 `T001-T020`，无前置迁移依赖
- **Wave 1**: 依赖 Wave 0，对应 `T021-T030`
- **Wave 2**: 依赖 Wave 1，对应 `T031-T038`
- **Wave 3**: 依赖 Wave 2，对应 `T039-T048`
- **Wave 4**: 依赖 Wave 3，对应 `T049-T056`
- **Wave 5**: 依赖 Wave 4，对应 `T057-T064`
- **Wave 6**: 依赖 Wave 5，对应 `T065-T069`

### User Story Dependencies

- **US1 (P1)**: 无故事级前置依赖，是统一架构冻结基线的 MVP
- **US2 (P2)**: 依赖 US1 的统一术语与评审入口
- **US3 (P3)**: 依赖 US2 的 canonical roots 已正式可用
- **US4 (P4)**: 依赖 US3 的上游输入事实已经稳定
- **US5 (P5)**: 依赖 US4 的规划链 owner 已完成拆分
- **US6 (P6)**: 依赖 US5 的运行时链与入口壳已稳定
- **US7 (P7)**: 依赖 US6 的追溯/HMI/validation/sample 正式落位

### Within Each User Story

- 验证任务必须先于同故事的迁移与 cutover 任务
- `M1-M3` 必须先于 `M0/M4-M8`
- `M0/M4-M8` 必须先于 `M9`
- `M9` 必须先于 `M10-M11` 和 root build-graph cutover
- `T067-T069` 只能在所有 owner 已经真实迁入 canonical roots 后执行

### Parallel Opportunities

- Setup 阶段的 `T002`、`T003` 可并行
- Foundational 阶段的 `T005-T013` 中标记 `[P]` 的任务可并行
- US1 的 `T014-T019` 可并行，`T020` 最后执行
- US2 的 `T021-T029` 中标记 `[P]` 的任务可并行，`T030` 最后执行
- US3 的 `T031-T037` 中标记 `[P]` 的任务可按模块并行，`T038` 最后执行
- US4 的 `T039-T047` 中标记 `[P]` 的任务可按模块并行，`T048` 最后执行
- US5 的 `T049-T055` 中标记 `[P]` 的任务可按运行时/设备面并行，`T056` 最后执行
- US6 的 `T057-T063` 中标记 `[P]` 的任务可按 trace/HMI/tests/samples 并行，`T064` 最后执行
- US7 的 `T065-T066` 可并行，`T067-T069` 顺序收口
- Polish 阶段的 `T070-T073` 可并行收集证据，`T074` 最后冻结结论

---

## Parallel Example: User Story 4

```bash
Task: "将 M0 workflow owner 入口归位到 modules/workflow/CMakeLists.txt、modules/workflow/README.md、modules/workflow/contracts/README.md"
Task: "将 M4 process-planning owner 入口归位到 modules/process-planning/CMakeLists.txt、modules/process-planning/README.md、modules/process-planning/contracts/README.md"
Task: "将 M5 coordinate-alignment owner 入口归位到 modules/coordinate-alignment/CMakeLists.txt、modules/coordinate-alignment/README.md、modules/coordinate-alignment/contracts/README.md"
Task: "将 M6 process-path owner 入口归位到 modules/process-path/CMakeLists.txt、modules/process-path/README.md、modules/process-path/contracts/README.md"
```

---

## Parallel Example: User Story 6

```bash
Task: "将 M10 trace-diagnostics owner 入口归位到 modules/trace-diagnostics/CMakeLists.txt、modules/trace-diagnostics/README.md、modules/trace-diagnostics/contracts/README.md"
Task: "将 M11 hmi-application owner 入口归位到 modules/hmi-application/CMakeLists.txt、modules/hmi-application/README.md、modules/hmi-application/contracts/README.md"
Task: "将仓库级验证承载面归位到 tests/CMakeLists.txt、tests/integration/README.md、tests/e2e/README.md、tests/performance/README.md"
Task: "将样本承载面归位到 samples/README.md、samples/golden/README.md，并在 examples/README.md 标注退出口径"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. 完成 Phase 1: Setup
2. 完成 Phase 2: Foundational
3. 完成 Phase 3: US1
4. 验证统一阶段链、术语和失败语义已经冻结
5. 仅在 US1 稳定后进入 canonical roots 与 owner 迁移

### Incremental Delivery

1. 先交付 `US1-US2`，锁定基线并让 canonical roots 真正可用
2. 再交付 `US3-US4`，完成上游与规划链 owner 收敛
3. 然后交付 `US5-US6`，完成运行时链、trace/HMI 和验证承载面收敛
4. 最后交付 `US7`，切换根级构建图并完成 legacy exit
5. 以 Polish 阶段运行全量门禁并冻结 closeout 结论

### Suggested MVP Scope

1. 建议 MVP 仅包含 `US1`
2. `US2` 是后续所有迁移的根级承载前提
3. `US3-US7` 按真实迁移风险边界逐步交付，不再压缩成少数粗粒度故事

---

## Notes

- 本任务清单只服务 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\`，不再引用旧特性目录作为正式输入
- `plan.md` 保留设计基线；模块级任务分解、波次依赖顺序和每波交付件清单全部以本 `tasks.md` 为准
- `[P]` 仅表示可并行，不表示可以跨越 wave 前置依赖
- 根级入口必须始终保持 `build.ps1`、`test.ps1`、`ci.ps1` 稳定
- `packages/`、`integration/`、`tools/`、`examples/` 在 closeout 前只允许作为迁移来源或兼容壳存在
