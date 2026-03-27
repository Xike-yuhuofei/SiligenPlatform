# Implementation Plan: ARCH-201 Stage B - 边界收敛与 US2 最小闭环

**Branch**: `refactor/motion/ARCH-201-m7-owner-boundary-repair` | **Date**: 2026-03-27 | **Spec**: `/specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/spec.md`
**Input**: Feature specification from `/specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/spec.md`

## Summary

本阶段只做一条主线：先把当前混合工作树收敛回 `ARCH-201 Stage B` 允许范围，再完成 US2 的最小闭环，让 `M7 motion-planning` 只保留规划事实与规划输出，让 `M9 runtime-execution` 承接 runtime/control owner。`workflow` 在本阶段只允许保留 orchestration / compatibility shell，不并行推进 US3 全量骨架一致化或 US4 CMP 最终收口。

## Technical Context

**Language/Version**: C++17、CMake 3.20+、PowerShell 7、Python 3.11
**Primary Dependencies**: GoogleTest、spdlog、ruckig、仓库根级验证脚本
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

1. `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot . -ReportDir tests/reports/module-boundary-bridges`
2. `cmake --build build --config Debug --target siligen_motion_planning_unit_tests`
3. `cmake --build build --config Debug --target siligen_runtime_host_unit_tests`
4. `cmake --build build --config Debug --target siligen_transport_gateway`（仅在 gateway 变更纳入 Stage B 时）
5. `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/run-local-validation-gate.ps1 -ReportRoot tests/reports/local-validation-gate`

## Known Blockers

当前工作树在 park 白名单外改动后，顶层 reconfigure 仍被仓库前置状态阻断。已知阻塞点包括：

- `modules/process-planning/contracts` 缺少 `CMakeLists.txt`
- `modules/job-ingest/contracts` 缺少 `CMakeLists.txt`
- `modules/topology-feature/contracts` 缺少 `CMakeLists.txt`
- `modules/trace-diagnostics` canonical public headers 缺失
- `build/modules/process-planning/contracts` 存在 binary dir 二次占用冲突

结论：Stage B 代码、门禁和文档可以继续收口，但完整 build/test 结果必须在证据中明确标记为“被当前工作树前置状态阻断”。
