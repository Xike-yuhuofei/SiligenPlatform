# Tasks: M7 MotionPlan Owner Boundary Repair

**Input**: Design documents from `/specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/`  
**Prerequisites**: plan.md (required), spec.md (required for user stories)  
**Tests**: 本特性明确要求验证 owner 边界与回归稳定性，包含必要测试任务。  
**Organization**: 任务按用户故事分组，保证每个故事可独立实现与验证。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可并行（不同文件、无未完成前置依赖）
- **[Story]**: 用户故事标签（US1/US2/US3/US4）
- 所有任务包含明确文件路径

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: 建立本特性的证据目录、验证手册和边界基线

- [X] T001 建立边界修复研究记录 `specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/research.md`
- [X] T002 [P] 建立执行与验收说明 `specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/quickstart.md`
- [X] T003 [P] 建立特性契约目录与说明 `specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/contracts/README.md`
- [X] T004 记录审查基线与风险项 `docs/architecture/m7-owner-boundary-audit-baseline.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 提供 owner 收口所需的最小骨架与边界门禁

**⚠️ CRITICAL**: 该阶段完成前禁止进入任何用户故事实现

- [X] T005 在 M7 建立 `MotionPlanner` 头文件骨架 `modules/motion-planning/domain/motion/domain-services/MotionPlanner.h`
- [X] T006 在 M7 建立 `MotionPlanner` 源文件骨架 `modules/motion-planning/domain/motion/domain-services/MotionPlanner.cpp`
- [X] T007 建立旧路径兼容转发头 `modules/process-path/domain/trajectory/domain-services/MotionPlanner.h`
- [X] T008 扩展模块边界门禁规则 `scripts/validation/assert-module-boundary-bridges.ps1`
- [X] T009 在特性契约中登记 owner 收口目标 `specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/contracts/m7-owner-boundary-report.md`

**Checkpoint**: owner 收口前置骨架与边界门禁已就绪

---

## Phase 3: User Story 1 - M7 Owner 事实收口 (Priority: P1) 🎯 MVP

**Goal**: `MotionPlanner` 的源码归属、构建归属、调用归属全部收敛到 `modules/motion-planning`

**Independent Test**: `siligen_motion` 不再编译 `modules/process-path/.../MotionPlanner.cpp`，并且 `MotionPlanningFacade` 仍可输出 `MotionTrajectory`

### Tests for User Story 1

- [X] T010 [P] [US1] 新增 owner 归属单测 `modules/motion-planning/tests/unit/domain/motion/MotionPlannerOwnerPathTest.cpp`
- [X] T011 [P] [US1] 更新规划器单测 include 与命名空间 `modules/motion-planning/tests/unit/domain/trajectory/MotionPlannerTest.cpp`
- [X] T012 [P] [US1] 更新约束单测 include 与命名空间 `modules/motion-planning/tests/unit/domain/trajectory/MotionPlannerConstraintTest.cpp`

### Implementation for User Story 1

- [X] T013 [US1] 迁移完整规划求解实现到 M7 `modules/motion-planning/domain/motion/domain-services/MotionPlanner.cpp`
- [X] T014 [US1] 更新 M7 motion target 源清单 `modules/motion-planning/domain/motion/CMakeLists.txt`
- [X] T015 [US1] 清理 M6 反向 include 暴露 `modules/process-path/domain/trajectory/CMakeLists.txt`
- [X] T016 [US1] 切换 Facade 到 M7 owner 实现 `modules/motion-planning/application/services/motion_planning/MotionPlanningFacade.cpp`
- [X] T017 [US1] 同步 Facade 对外头文件依赖 `modules/motion-planning/application/include/application/services/motion_planning/MotionPlanningFacade.h`
- [X] T018 [US1] 将旧路径 `MotionPlanner.cpp` 改为兼容薄层并标记退出条件 `modules/process-path/domain/trajectory/domain-services/MotionPlanner.cpp`
- [X] T019 [US1] 更新 M7 模块测试入口 `modules/motion-planning/tests/CMakeLists.txt`
- [X] T020 [US1] 更新模块声明说明 owner 已收口 `modules/motion-planning/module.yaml`
- [X] T021 [US1] 记录 US1 验收证据 `specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/contracts/us1-owner-closure-evidence.md`

**Checkpoint**: M7 owner 收口完成且可独立验证

---

## Phase 4: User Story 2 - Runtime/Control 语义剥离到 M9 (Priority: P2)

**Goal**: 回零、点动、控制、状态、IO 语义归位 `runtime-execution`，M7 不再持有 runtime/control 主入口

**Independent Test**: M7 不再定义 `IMotionRuntimePort` 与 `IIOControlPort`；M9 可承接对应控制流程

### Tests for User Story 2

- [ ] T022 [P] [US2] 新增 M7 防越界测试 `modules/motion-planning/tests/unit/domain/motion/NoRuntimeControlLeakTest.cpp`
- [ ] T023 [P] [US2] 新增运行时迁移单测 `modules/runtime-execution/runtime/host/tests/unit/runtime/motion/MotionControlMigrationTest.cpp`

### Implementation for User Story 2

- [ ] T024 [US2] 新增 M9 runtime 控制契约 `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/IMotionRuntimePort.h`
- [ ] T025 [US2] 新增 M9 IO 控制契约 `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/IIOControlPort.h`
- [ ] T026 [P] [US2] 新增 M9 motion use case 对外头 `modules/runtime-execution/application/include/runtime_execution/application/usecases/motion/MotionControlUseCase.h`
- [ ] T027 [P] [US2] 新增 M9 motion use case 实现 `modules/runtime-execution/application/usecases/motion/MotionControlUseCase.cpp`
- [ ] T028 [P] [US2] 迁移回零流程接口到 M9 `modules/runtime-execution/runtime/host/services/motion/HomingProcessService.h`
- [ ] T029 [P] [US2] 迁移回零流程实现到 M9 `modules/runtime-execution/runtime/host/services/motion/HomingProcessService.cpp`
- [ ] T030 [P] [US2] 迁移点动流程接口到 M9 `modules/runtime-execution/runtime/host/services/motion/JogControllerService.h`
- [ ] T031 [P] [US2] 迁移点动流程实现到 M9 `modules/runtime-execution/runtime/host/services/motion/JogControllerService.cpp`
- [ ] T032 [US2] 在 M9 汇聚控制/状态编排 `modules/runtime-execution/runtime/host/services/motion/MotionRuntimeControlOrchestrator.cpp`
- [ ] T033 [US2] 关闭 M7 控制实现入口 `modules/motion-planning/domain/motion/domain-services/MotionControlServiceImpl.cpp`
- [ ] T034 [US2] 关闭 M7 状态实现入口 `modules/motion-planning/domain/motion/domain-services/MotionStatusServiceImpl.cpp`
- [ ] T035 [US2] 移除 M7 runtime port 定义 `modules/motion-planning/domain/motion/ports/IMotionRuntimePort.h`
- [ ] T036 [US2] 移除 M7 IO port 定义 `modules/motion-planning/domain/motion/ports/IIOControlPort.h`
- [ ] T037 [US2] 更新 runtime-execution application 链接关系 `modules/runtime-execution/application/CMakeLists.txt`
- [ ] T038 [US2] 更新 runtime host 链接关系 `modules/runtime-execution/runtime/host/CMakeLists.txt`
- [ ] T039 [US2] 更新 motion-planning motion target 链接关系 `modules/motion-planning/domain/motion/CMakeLists.txt`
- [ ] T040 [US2] 更新 M9 owner 说明 `modules/runtime-execution/README.md`
- [ ] T041 [US2] 更新 M7 domain 边界说明 `modules/motion-planning/domain/motion/README.md`
- [ ] T042 [US2] 记录 US2 验收证据 `specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/contracts/us2-runtime-extraction-evidence.md`

**Checkpoint**: M9 承接 runtime/control，M7 回归规划职责

---

## Phase 5: User Story 3 - Public Surface 与目录骨架一致化 (Priority: P3)

**Goal**: 让 `contracts/application/domain/adapters/tests` 与真实实现边界一致，消除“转发壳/空骨架”误导

**Independent Test**: 上层仅通过 M7 application public surface 集成，不再跨模块抓内部实现

### Tests for User Story 3

- [ ] T043 [P] [US3] 新增 M7 public surface 编译测试 `modules/motion-planning/tests/unit/application/services/motion_planning/MotionPlanningFacadePublicSurfaceTest.cpp`
- [ ] T044 [P] [US3] 新增边界基线样例 `tests/reports/baselines/module-boundary/m7-public-surface.json`

### Implementation for User Story 3

- [ ] T045 [US3] 收敛 M7 模块根装配逻辑 `modules/motion-planning/CMakeLists.txt`
- [ ] T046 [US3] 收敛 M7 application public 入口装配 `modules/motion-planning/application/CMakeLists.txt`
- [ ] T047 [US3] 对齐 M7 contracts 边界说明 `modules/motion-planning/contracts/README.md`
- [ ] T048 [US3] 对齐 M7 services 目录职责说明 `modules/motion-planning/services/README.md`
- [ ] T049 [US3] 对齐 M7 adapters 目录职责说明 `modules/motion-planning/adapters/README.md`
- [ ] T050 [US3] 对齐 M7 模块测试组织 `modules/motion-planning/tests/CMakeLists.txt`
- [ ] T051 [US3] 对齐 M7 测试入口说明 `modules/motion-planning/tests/README.md`
- [ ] T052 [US3] 产出结构一致性矩阵 `specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/data-model.md`
- [ ] T053 [US3] 记录 US3 验收证据 `specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/contracts/us3-surface-alignment-evidence.md`

**Checkpoint**: M7 目录骨架、构建入口、文档声明一致

---

## Phase 6: User Story 4 - CMP/触发边界显式化 (Priority: P3)

**Goal**: 明确 M7 规划事实与 M9 执行触发包装边界，防止语义继续漂移

**Independent Test**: CMP 规划与执行包装职责分离，契约可直接审查

### Tests for User Story 4

- [ ] T054 [P] [US4] 新增 CMP 边界契约测试 `modules/motion-planning/tests/unit/domain/motion/CMPBoundaryContractTest.cpp`
- [ ] T055 [P] [US4] 新增触发映射集成测试 `modules/runtime-execution/runtime/host/tests/integration/CMPTriggerMappingIntegrationTest.cpp`

### Implementation for User Story 4

- [ ] T056 [US4] 编写 CMP 边界契约文档 `modules/motion-planning/contracts/cmp-trigger-boundary.md`
- [ ] T057 [US4] 收敛规划侧 CMP 字段处理 `modules/motion-planning/domain/motion/CMPCoordinatedInterpolator.cpp`
- [ ] T058 [US4] 下沉执行触发包装映射到 M9 `modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.Control.cpp`
- [ ] T059 [US4] 更新 CMP 约束校验规则 `modules/motion-planning/domain/motion/CMPValidator.cpp`
- [ ] T060 [US4] 更新业务树与 owner 表述 `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`
- [ ] T061 [US4] 记录 US4 验收证据 `specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/contracts/us4-cmp-boundary-evidence.md`

**Checkpoint**: CMP 规划/执行边界可审计且可验证

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: 汇总验证结果、补充跨故事追踪与最终门禁

- [ ] T062 [P] 汇总 contracts 套件执行结果 `tests/reports/m7-owner-boundary-repair/contracts/summary.md`
- [ ] T063 汇总本地验证门禁结果 `tests/reports/m7-owner-boundary-repair/local-validation-gate/summary.md`
- [ ] T064 [P] 更新最终执行手册与命令顺序 `specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/quickstart.md`
- [ ] T065 完成需求-任务-证据追踪矩阵 `specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/contracts/traceability-matrix.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: 无依赖，可立即开始
- **Phase 2 (Foundational)**: 依赖 Phase 1 完成，阻塞所有用户故事
- **Phase 3 (US1)**: 依赖 Phase 2 完成，是后续 US2/US3/US4 的输入
- **Phase 4 (US2)**: 依赖 US1 完成
- **Phase 5 (US3)**: 依赖 US1 完成，可与 US2 并行推进
- **Phase 6 (US4)**: 依赖 US2 和 US3 完成
- **Phase 7 (Polish)**: 依赖所有用户故事完成

### User Story Dependencies

- **US1 (P1)**: 必须最先完成（owner 收口是基础）
- **US2 (P2)**: 依赖 US1（runtime/control 剥离）
- **US3 (P3)**: 依赖 US1（surface/骨架收敛）
- **US4 (P3)**: 依赖 US2 + US3（CMP 边界最终收敛）

### Within Each User Story

- 测试任务先写并先运行，确保迁移前后行为可比较
- 先改契约/入口，再改实现，再改构建，再补文档
- 每个故事完成后必须输出对应 evidence 文档

### Parallel Opportunities

- Phase 1 中标记 `[P]` 的文档任务可并行
- US1 中测试更新任务 `T010-T012` 可并行
- US2 中迁移类任务 `T028-T031` 可并行
- US3 中文档与测试任务 `T043-T044`, `T047-T051` 可并行
- US4 中测试任务 `T054-T055` 可并行

---

## Parallel Example: User Story 1

```bash
Task: "新增 owner 归属单测 modules/motion-planning/tests/unit/domain/motion/MotionPlannerOwnerPathTest.cpp"
Task: "更新规划器单测 include 与命名空间 modules/motion-planning/tests/unit/domain/trajectory/MotionPlannerTest.cpp"
Task: "更新约束单测 include 与命名空间 modules/motion-planning/tests/unit/domain/trajectory/MotionPlannerConstraintTest.cpp"
```

## Parallel Example: User Story 2

```bash
Task: "迁移回零流程接口 modules/runtime-execution/runtime/host/services/motion/HomingProcessService.h"
Task: "迁移回零流程实现 modules/runtime-execution/runtime/host/services/motion/HomingProcessService.cpp"
Task: "迁移点动流程接口 modules/runtime-execution/runtime/host/services/motion/JogControllerService.h"
Task: "迁移点动流程实现 modules/runtime-execution/runtime/host/services/motion/JogControllerService.cpp"
```

## Parallel Example: User Story 3

```bash
Task: "对齐 contracts 边界说明 modules/motion-planning/contracts/README.md"
Task: "对齐 services 目录职责说明 modules/motion-planning/services/README.md"
Task: "对齐 adapters 目录职责说明 modules/motion-planning/adapters/README.md"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. 完成 Phase 1 + Phase 2
2. 完成 US1（Phase 3）
3. 立即验证 owner 收口与构建可用性
4. 若 US1 验收失败，禁止进入 US2/US3/US4

### Incremental Delivery

1. US1：先收口 owner（消除最高风险）
2. US2：剥离 runtime/control（恢复 M7/M9 边界）
3. US3：收敛 public surface 与骨架（降低维护成本）
4. US4：清理 CMP 漂移（防止继续越界）
5. Final：统一门禁与追踪矩阵

### Parallel Team Strategy

1. 小组 A：US1 owner 收口与 M6/M7 构建修复
2. 小组 B：US2 runtime/control 迁移与 M9 入口承接
3. 小组 C：US3 文档/入口/测试骨架对齐
4. 小组 D：US4 CMP 边界契约与执行映射治理

---

## Notes

- `[P]` 任务仅表示文件级可并行，不代表可跳过依赖门禁
- 每个故事必须有独立 evidence 文档（`contracts/us*-*.md`）
- 不允许新增双向模块依赖或隐式 legacy fallback
- 若发现额外越界点，先补 `assert-module-boundary-bridges.ps1` 再改实现
