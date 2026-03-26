# Tasks: modules 统一骨架对齐

**Input**: 设计文档来自 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\`  
**Prerequisites**: `spec.md`、`design.md`、`plan.md`、`motion-planning-preview.md`

**Tests**: 本特性以模块统一骨架对齐为目标，因此每个故事都必须同时覆盖目录完整性、边界清晰度、构建切换和 bridge 退出条件。  
**Organization**: 任务按阶段和模块组组织，优先保证全模块骨架落位，再逐组收敛实现与验证面。

## Format: `[ID] [P?] [Group] Description`

- **[P]**: 可并行执行（不同模块、不同文件、无未完成前置依赖）
- **[Group]**: 任务所属模块组（`G0` 到 `G4`）
- 每个任务描述均包含明确文件路径

## Phase 1: Setup (工作目录与判定基线)

**Purpose**: 为本次模块统一骨架对齐建立独立的任务清单、模块检查清单和波次映射入口

- [X] T001 创建任务清单 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\tasks.md`
- [X] T002 [P] 创建模块统一骨架检查清单 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\module-checklist.md`
- [X] T003 [P] 创建模块迁移波次映射 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\wave-mapping.md`

---

## Phase 2: Foundational (统一骨架规则落位)

**Purpose**: 固化统一骨架的准入规则、bridge 语义和完成判定，阻塞所有模块实施

**CRITICAL**: 本阶段未完成前，不允许开始任一模块的目录重排

- [X] T004 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\plan.md` 和 `module-checklist.md` 固化统一骨架目录清单、每层职责和 `src/` bridge 退出标准
- [X] T005 [P] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\wave-mapping.md` 建立 `Wave 0-4` 的模块分组、进入条件和退出条件
- [X] T006 [P] 在 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s06-repo-structure-guide.md` 补充“模块内部统一骨架对齐仍待实施”的正式说明
- [X] T007 [P] 在 `D:\Projects\SS-dispense-align\scripts\migration\validate_workspace_layout.py` 设计并补充模块统一骨架断言入口（目录存在性、bridge 状态、module.yaml 存在性）

**Checkpoint**: 全模块统一骨架的评价口径、bridge 语义和验证入口已统一

---

## Phase 3: Group G0 - 全模块骨架落位 (Priority: P1)

**Goal**: 为所有一级模块补齐统一骨架顶层目录，使目录治理从 owner root 提升到模块内骨架级别

**Independent Test**: 检查 `modules/` 下全部一级模块时，都能看到 `module.yaml`、`domain/`、`services/`、`application/`、`adapters/`、`tests/`、`examples/`，且 `src/` 被明确定义为 bridge

### Tests for Group G0

- [X] T008 [P] [G0] 在 `D:\Projects\SS-dispense-align\scripts\migration\validate_workspace_layout.py` 增加全模块统一骨架必备目录断言
- [X] T009 [P] [G0] 在 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\module-checklist.md` 增加每个模块的骨架完成列和 bridge 退出列

### Implementation for Group G0

- [X] T010 [P] [G0] 为 `D:\Projects\SS-dispense-align\modules\job-ingest\`、`dxf-geometry\`、`topology-feature\` 补齐统一骨架顶层目录与 `module.yaml`
- [X] T011 [P] [G0] 为 `D:\Projects\SS-dispense-align\modules\process-planning\`、`coordinate-alignment\`、`process-path\` 补齐统一骨架顶层目录与 `module.yaml`
- [X] T012 [P] [G0] 为 `D:\Projects\SS-dispense-align\modules\motion-planning\`、`dispense-packaging\`、`workflow\` 补齐统一骨架顶层目录与 `module.yaml`
- [X] T013 [P] [G0] 为 `D:\Projects\SS-dispense-align\modules\runtime-execution\`、`trace-diagnostics\`、`hmi-application\` 补齐统一骨架顶层目录与 `module.yaml`
- [X] T014 [G0] 在所有模块 `README.md` 中显式声明：统一骨架为终态结构，`src/` 为迁移期 bridge，不再接收新增终态实现

**Checkpoint**: 全部一级模块都具备统一骨架顶层结构

---

## Phase 4: Group G1 - 上游与规划链模块收敛 (Priority: P2)

**Goal**: 优先在低到中等复杂度模块中完成从 `src/` 到统一骨架的真实实现迁移，建立可复制样板

**Independent Test**: 抽查 `M1-M8` 中任意模块时，其 owner 事实和主要实现已进入 `domain/services/application/adapters`，而不是继续停留在 `src/`

### Tests for Group G1

- [X] T015 [P] [G1] 为 `job-ingest`、`dxf-geometry`、`topology-feature` 增加模块级骨架迁移断言和样本入口断言
- [X] T016 [P] [G1] 为 `process-planning`、`coordinate-alignment`、`process-path`、`motion-planning`、`dispense-packaging` 增加模块级骨架迁移断言和构建来源断言

### Implementation for Group G1

- [X] T017 [P] [G1] 将 `job-ingest`、`dxf-geometry`、`topology-feature` 中 `src/` 的 owner 事实迁入 `domain/`、`services/`、`application/`
- [X] T018 [P] [G1] 将 `process-planning`、`coordinate-alignment`、`process-path` 中 `src/` 的 owner 事实迁入统一骨架
- [X] T019 [P] [G1] 按 `motion-planning-preview.md` 将 `modules/motion-planning/src/motion/` 的纯 `M7` 资产迁入 `domain/`、`services/`、`application/`、`adapters/`
- [X] T020 [P] [G1] 将 `dispense-packaging` 中 `src/` 的规划与打包逻辑迁入统一骨架，并与 `M7/M9` 明确边界
- [X] T021 [G1] 为 `M1-M8` 对应模块补齐 `tests/` 和 `examples/` 的正式承载入口，并把 README 回链到新结构

**Checkpoint**: 上游与规划链模块形成第一批真实对齐样板

---

## Phase 5: Group G2 - workflow 模块职责净化 (Priority: P3)

**Goal**: 将 `workflow` 从混合型历史聚合结构收敛为可治理的统一骨架，同时避免把 `process-runtime-core` 原样改名保留

**Independent Test**: 检查 `modules/workflow/` 时，能够区分编排层、领域规则、应用入口和 bridge 资产，且 `process-runtime-core` 不再作为默认终态 owner 面

### Tests for Group G2

- [X] T022 [P] [G2] 在 `scripts/migration/validate_workspace_layout.py` 增加 `modules/workflow/` 的 bridge 资产与统一骨架并存断言
- [X] T023 [P] [G2] 在 `module-checklist.md` 为 `workflow` 增加“历史聚合目录已降级”专项判定列

### Implementation for Group G2

- [X] T024 [P] [G2] 为 `modules/workflow/` 建立 `domain/`、`services/`、`application/`、`adapters/` 的正式入口与说明
- [X] T025 [P] [G2] 将仍属于 `M0 workflow` 的编排语义从 `process-runtime-core/` 中迁入统一骨架
- [X] T026 [P] [G2] 将不属于 `M0` 的资产标注为迁出目标或 bridge 资产，并更新 `README.md` 与 `module.yaml`
- [X] T027 [G2] 切换 `modules/workflow/CMakeLists.txt` 的主实现来源，降低 `process-runtime-core/` 为 bridge

**Checkpoint**: `workflow` 不再以历史聚合目录作为默认终态结构

---

## Phase 6: Group G3 - 运行态与宿主耦合模块收敛 (Priority: P4)

**Goal**: 对 `runtime-execution`、`trace-diagnostics`、`hmi-application` 完成统一骨架对齐，同时保留运行态模块必要的受控特化

**Independent Test**: 检查 `M9-M11` 时，既能看到统一骨架，又不会把宿主壳、设备适配和运行态实现混入同一层

### Tests for Group G3

- [X] T028 [P] [G3] 在 `scripts/migration/validate_workspace_layout.py` 增加 `runtime-execution` 允许 `runtime/`、禁止把运行态实现继续挂在 bridge 根下的断言
- [X] T029 [P] [G3] 在 `module-checklist.md` 增加 `runtime-execution` 的受控特化列，以及 `trace-diagnostics`、`hmi-application` 的宿主分离列

### Implementation for Group G3

- [X] T030 [P] [G3] 将 `modules/runtime-execution/` 重排为统一骨架加受控 `runtime/`，并把 `runtime-host/`、`device-adapters/`、`device-contracts/` 的终态归位写清
- [X] T031 [P] [G3] 将 `modules/trace-diagnostics/` 的实现和验证入口迁入统一骨架
- [X] T032 [P] [G3] 将 `modules/hmi-application/` 的业务 owner 实现迁入统一骨架，并保持 `apps/hmi-app/` 仅承担宿主职责
- [X] T033 [G3] 为 `M9-M11` 补齐模块内 `tests/` 与 `examples/` 的正式入口，并回链文档

**Checkpoint**: 高复杂度模块完成统一骨架对齐且保留必要特化

---

## Phase 7: Group G4 - 构建切换与 bridge 退出 (Priority: P5)

**Goal**: 逐模块把主构建图切换到统一骨架，并关闭 `src/` 及其他桥接目录的默认实现地位

**Independent Test**: 任一模块的主实现来源已不再依赖 `src/`；`src/` 若存在，仅作为 tombstone、wrapper 或迁移说明目录

### Tests for Group G4

- [X] T034 [P] [G4] 在 `scripts/migration/validate_workspace_layout.py` 增加“新增代码不得落到 `src/`”和“bridge 已退出主实现来源”的断言
- [X] T035 [P] [G4] 在 `module-checklist.md` 为全部模块补齐 closeout 判定列

### Implementation for Group G4

- [X] T036 [P] [G4] 逐模块切换 `CMakeLists.txt` 到统一骨架目录，停止以 `src/` 作为主实现来源
- [X] T037 [P] [G4] 将 `src/` 降级为 README、wrapper 或 tombstone，并在模块 README 中回写 closeout 状态
- [X] T038 [P] [G4] 将模块级测试和样例回链到统一骨架完成态，并更新 `module.yaml` 的测试目标与 sample refs
- [X] T039 [G4] 在 `wave-mapping.md`、`module-checklist.md` 和相关模块 README 中记录每个模块的 closeout 证据

**Checkpoint**: 全部模块完成统一骨架对齐，bridge 退出主实现来源

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 无依赖，可立即开始
- **Foundational (Phase 2)**: 依赖 Setup 完成，阻塞所有模块实施
- **G0 (Phase 3)**: 依赖 Foundational 完成，是所有后续迁移的共同前提
- **G1 (Phase 4)**: 依赖 G0 完成
- **G2 (Phase 5)**: 依赖 G1 的样板与边界收敛结果
- **G3 (Phase 6)**: 依赖 G2，且运行态模块可复用前置分层规则
- **G4 (Phase 7)**: 依赖 G1-G3 全部完成

### Module Order

- `M1-M8` 必须先于 `M0`
- `M0` 必须先于 `M9-M11`
- `M9-M11` 必须先于全模块 bridge exit

### Parallel Opportunities

- G0 阶段可按模块批次并行补齐顶层骨架
- G1 阶段可按模块并行迁移，但 `motion-planning` 需先完成边界复核
- G3 阶段 `trace-diagnostics` 与 `hmi-application` 可并行，`runtime-execution` 单独推进

## Notes

- 本任务清单只服务 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-module-skeleton-alignment\`
- 本任务关注的是模块内部统一骨架对齐，不重复 root-level canonical roots 迁移
- `src/` 在迁移期可保留，但默认视为 `legacy-source bridge`

