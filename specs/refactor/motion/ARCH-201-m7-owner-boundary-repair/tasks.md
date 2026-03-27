# Tasks: ARCH-201 Stage B - 边界收敛与 US2 最小闭环

**Input**: Design documents from `/specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/`
**Prerequisites**: `spec.md`, `plan.md`
**Tests**: Stage B 明确要求覆盖 owner 边界、防回流 shim、host/gateway 最小闭环与门禁脚本
**Organization**: 任务按阶段分组，聚焦 Stage B 单主线，不并行展开 US3 / US4

## Format: `[ID] [P?] Description`

- **[P]**: 文件级可并行
- 所有任务必须落在 Stage B 白名单或 compile-only collateral 中

## Phase 1: 范围收敛与口径对齐

**Purpose**: 收敛当前工作树、锁定 Stage B 白名单，并让文档与门禁脚本指向真实 canonical 路径

- [X] T001 形成当前未提交文件的 `keep / park` 边界，只保留 Stage B 白名单目录进入活动工作树
- [X] T002 同步 `spec.md`、`plan.md`、`tasks.md`、`quickstart.md` 到 Stage B 范围与 canonical 路径
- [X] T003 更新 `contracts/README.md`、`m7-owner-boundary-report.md`、模块 README，使 `M7=规划 owner`、`M9=runtime/control owner` 口径一致

**Checkpoint**: Stage B 的活动范围、说明文档与 canonical 路径已收敛

---

## Phase 2: US2 最小闭环实现

**Purpose**: 把 runtime/control owner 收到 `runtime-execution`，同时保留最小兼容 shim

### Tests for Phase 2

- [X] T010 [P] 新增 `modules/motion-planning/tests/unit/domain/motion/NoRuntimeControlLeakTest.cpp`
- [X] T011 [P] 新增 `modules/runtime-execution/runtime/host/tests/unit/runtime/motion/MotionControlMigrationTest.cpp`

### Implementation for Phase 2

- [X] T012 新增 `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/IMotionRuntimePort.h`
- [X] T013 新增 `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/IIOControlPort.h`
- [X] T014 新增 `modules/runtime-execution/application/include/runtime_execution/application/services/motion/` 与对应 `MotionControlServiceImpl` / `MotionStatusServiceImpl`
- [X] T015 新增 `modules/runtime-execution/application/include/runtime_execution/application/usecases/motion/MotionControlUseCase.h` 与 `MotionControlUseCase.cpp`
- [X] T016 将 `modules/motion-planning/domain/motion/*` 与 `modules/workflow/domain/include/domain/motion/*` 的 runtime/control 公开头改为 shim/alias
- [X] T017 重接 `modules/runtime-execution/runtime/host/runtime/motion/WorkflowMotionRuntimeServicesProvider.*` 与 `ApplicationContainer` motion/system 装配
- [X] T018 重接 `apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h` 与 `TcpMotionFacade.*` 到 `MotionControlUseCase`
- [X] T019 更新 `modules/runtime-execution/application/CMakeLists.txt`、`modules/runtime-execution/contracts/runtime/CMakeLists.txt`、`modules/runtime-execution/runtime/host/tests/CMakeLists.txt`、`modules/motion-planning/tests/CMakeLists.txt`

**Checkpoint**: M9 承接 runtime/control 最小 owner surface，M7/workflow 仅保留兼容 shim

---

## Phase 3: 门禁、证据与验证

**Purpose**: 补齐 Stage B 的 guardrail、evidence 与实际验证口径

- [X] T020 扩展 `scripts/validation/assert-module-boundary-bridges.ps1`，覆盖 Stage B 的 M7 -> M9 required/forbidden 规则
- [X] T021 记录 `contracts/us2-runtime-extraction-evidence.md`
- [X] T022 更新 `contracts/traceability-matrix.md` 与 `quickstart.md`
- [X] T023 执行 `assert-module-boundary-bridges.ps1`，最新报告 `finding_count=0`
- [X] T024 在独立验证构建目录 `bsc/` 完成 `siligen_motion_planning_unit_tests`、`siligen_runtime_host_unit_tests`、`siligen_transport_gateway`、`siligen_planner_cli`、`siligen_process_path_unit_tests` 定向 build，并补跑 `ProcessPathFacadeTest.*`
- [X] T025 执行 `run-local-validation-gate.ps1`，报告 `tests/reports/local-validation-gate/20260327-204804/` 为 `passed`

**Verification Note (2026-03-27)**:
- `cmake -S . -B bsc -DSILIGEN_BUILD_TESTS=ON -DSILIGEN_BUILD_TARGET=tests` configure / reconfigure 成功
- `tests/reports/module-boundary-bridges/module-boundary-bridges.md` 为 `status: passed`, `finding_count: 0`
- `D:\Projects\SiligenSuite\bsc\bin\Debug\siligen_process_path_unit_tests.exe --gtest_filter=ProcessPathFacadeTest.*` 通过
- `tests/reports/local-validation-gate/20260327-204804/local-validation-gate-summary.md` 为 `overall_status: passed`

---

## Dependencies & Execution Order

- Phase 1 完成前，不进入 Phase 2
- Phase 2 完成前，不进入 evidence/traceability 收尾
- `T024-T025` 已在独立验证构建目录 `bsc/` 完成，不再依赖旧 `build/` 状态

## Notes

- 本文件只记录 Stage B 实际完成项，不再展开 US3 / US4 计划
- 若后续进入 US3 / US4，应新开对应阶段任务，不回写本阶段的 done list
