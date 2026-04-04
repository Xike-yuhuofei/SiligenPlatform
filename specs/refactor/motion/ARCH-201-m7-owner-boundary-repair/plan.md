# Implementation Plan: ARCH-201 Stage B - 边界收敛与 US2 最小闭环

**Branch**: `refactor/motion/ARCH-201-m7-owner-boundary-repair` | **Date**: 2026-03-27 | **Spec**: `/specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/spec.md`
**Input**: Feature specification from `/specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/spec.md`

## Summary

本阶段只做一条主线：先把当前混合工作树收敛回 `ARCH-201 Stage B` 允许范围，再完成 US2 的最小闭环，让 `M7 motion-planning` 只保留规划事实与规划输出，让 `M9 runtime-execution` 承接 runtime/control owner。`workflow` 在本阶段只允许保留 orchestration / compatibility shell，不并行推进 US3 全量骨架一致化或 US4 CMP 最终收口。

## Technical Context

**Language/Version**: C++17、CMake 3.20+、PowerShell 7、Python 3.11
**Primary Dependencies**: GoogleTest、spdlog、仓库根级验证脚本
**Storage**: N/A（仅 Git 跟踪文件与验证报告目录）
**Testing**: `modules/motion-planning/tests`、`modules/runtime-execution/runtime/host/tests`、`scripts/validation/assert-module-boundary-bridges.ps1`、`scripts/validation/run-local-validation-gate.ps1`
**Target Platform**: Windows 开发环境 + CI（无机台 / mock）
**Project Type**: 多模块 C++ 工程（module owner + app host 装配）
**Constraints**:
- 只允许修改 `modules/motion-planning`、`modules/process-path`、`modules/runtime-execution`、`modules/workflow`、`scripts/validation`，以及 compile-only collateral：`shared/contracts/device`、`apps/runtime-gateway`
- 不恢复白名单外改动，不回滚既有用户改动
- 不新增双向模块依赖
- gateway 只能做最小重接，不改协议字段与外部行为

## Constitution Check

*GATE: Must pass before implementation. Re-check after documentation/validation sync.*

- [x] Branch name is compliant with `<type>/<scope>/<ticket>-<short-desc>`
- [x] Stage B canonical paths are explicit and avoid the deprecated `runtime/host/services/motion` owner path
- [x] Validation entry points are explicit, even when the current workspace state blocks complete execution
- [x] Compatibility behavior is explicit: legacy headers may remain only as shim/alias with退出条件

## Scope & Structure

### Active Stage B Roots

```text
modules/
├── motion-planning/
├── process-path/
├── runtime-execution/
└── workflow/

scripts/
└── validation/

shared/
└── contracts/device/          # compile-only collateral

apps/
└── runtime-gateway/           # compile-only collateral
```

### Canonical Motion Owner Paths

```text
modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/
modules/runtime-execution/application/include/runtime_execution/application/usecases/motion/
modules/runtime-execution/application/include/runtime_execution/application/services/motion/
modules/runtime-execution/runtime/host/runtime/motion/
```

### Out of Scope for Stage B

- US3 全量 public surface / 目录骨架整理
- US4 CMP 最终语义收口
- 白名单外目录恢复、提交或验证

## Design Decisions

1. **M9 owner contracts 固定在 runtime-execution/contracts/runtime**
   `IMotionRuntimePort` 与 `IIOControlPort` 迁入 `runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/`，旧 `M7/workflow` 公开头只保留 shim/alias。

2. **MotionControlUseCase 作为 Stage B 最小统一入口**
   在 `runtime-execution/application/usecases/motion/` 引入 `MotionControlUseCase`，统一承接 homing、ready-zero、jog、PTP control 与 monitoring 查询。为减少重写风险，它内部复用现有 workflow motion use cases，但 owner surface 与 include root 切到 `runtime-execution/application/*`。

3. **host runtime motion seam 固定在 runtime/host/runtime/motion**
   `WorkflowMotionRuntimeServicesProvider` 作为 host 侧 provider/orchestrator 装配点，负责用 `IMotionRuntimePort` 构造 `MotionControlServiceImpl` / `MotionStatusServiceImpl`。

4. **M7 只保留规划 public surface**
   `motion-planning` 只继续公开规划事实与规划入口；`MotionControlServiceImpl`、`MotionStatusServiceImpl`、runtime/control ports 不再由 M7 owner。必要兼容层仅允许头文件 alias，不允许新的 owner 声明或 `.cpp` 编译入口。

5. **gateway 只做最小重接**
   `apps/runtime-gateway` 的 `TcpMotionFacade` / `tcp_facade_builder.h` 只改依赖注入与 builder 装配，统一切到 `MotionControlUseCase`，不修改 TCP JSON 对外协议。

## Validation Strategy

### Required Checks

- `modules/motion-planning/tests/unit/domain/motion/NoRuntimeControlLeakTest.cpp`
- `modules/runtime-execution/runtime/host/tests/unit/runtime/motion/MotionControlMigrationTest.cpp`
- `modules/motion-planning/tests` 既有 `MotionPlannerOwnerPathTest`、`MotionPlanningFacadeTest`、`MotionPlannerConstraintTest`
- `modules/runtime-execution/runtime/host/tests/integration/HostBootstrapSmokeTest.cpp`
- `modules/workflow/tests/process-runtime-core/unit/motion/MotionRuntimeAssemblyFactoryTest.cpp`
- `scripts/validation/assert-module-boundary-bridges.ps1`
- `scripts/validation/run-local-validation-gate.ps1`

### Intended Command Sequence

1. `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot D:/Projects/wt-spike153 -ReportDir tests/reports/module-boundary-bridges-phase2-current`
2. `cmake --build D:/Projects/wt-spike153/build-phase2 --config Debug --target siligen_motion_planning_unit_tests siligen_unit_tests siligen_runtime_host_unit_tests --parallel`
3. `cmake --build D:/Projects/wt-spike153/build-phase2 --config Debug --target process_runtime_core_motion_runtime_assembly_test workflow_integration_motion_runtime_assembly_smoke --parallel`
4. `D:/Projects/wt-spike153/build-phase2/bin/Debug/siligen_motion_planning_unit_tests.exe --gtest_filter=MotionPlannerTest.*:MotionPlannerConstraintTest.*:InterpolationProgramPlannerTest.*:MotionPlannerOwnerPathTest.*:MotionPlanningOwnerBoundaryTest.*:NoRuntimeControlLeakTest.*`
5. `D:/Projects/wt-spike153/build-phase2/bin/Debug/siligen_unit_tests.exe --gtest_filter=DispensingProcessServiceWaitForMotionCompleteTest.*:DispensingWorkflowUseCaseTest.*`
6. `D:/Projects/wt-spike153/build-phase2/bin/Debug/siligen_runtime_host_unit_tests.exe --gtest_filter=MotionControlMigrationTest.*:WorkflowMotionRuntimeServicesProviderTest.*`
7. `D:/Projects/wt-spike153/build-phase2/bin/Debug/process_runtime_core_motion_runtime_assembly_test.exe`
8. `D:/Projects/wt-spike153/build-phase2/bin/Debug/workflow_integration_motion_runtime_assembly_smoke.exe`
9. `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/run-local-validation-gate.ps1 -ReportRoot tests/reports/local-validation-gate-phase2-current`

## Current Validation Baseline

- `build-phase2` 已成功 configure，并可重复构建 `siligen_motion_planning_unit_tests`、`siligen_unit_tests`、`siligen_runtime_host_unit_tests`、`process_runtime_core_motion_runtime_assembly_test`、`workflow_integration_motion_runtime_assembly_smoke`
- `assert-module-boundary-bridges.ps1` 当前报告为 `passed`
- Stage B 关键回归已通过：
  - 规划 owner 边界：16/16
  - 邻接 consumer：33/33
  - host/runtime seam：5/5
  - runtime assembly smoke：2 个可执行 `exit 0`

## Residual Blockers

- 本轮 `run-local-validation-gate.ps1` 已复跑，但 `overall_status=failed`
- 唯一失败步骤是 `test-contracts-ci`
- 精确阻断位于 `apps/hmi-app/tests/unit/test_offline_preview_builder.py`，由 `no-loose-mock` 静态门禁触发
- 结论：Stage B / US2 的专项构建、测试和边界门禁已闭环；剩余 root-entry gate 阻断不属于本阶段 owner seam 范围，必须在证据中按“跨域静态测试阻断”单独记录
