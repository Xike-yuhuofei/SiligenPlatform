# Implementation Plan: M7 MotionPlan Owner Boundary Repair

**Branch**: `refactor/motion/ARCH-201-m7-owner-boundary-repair` | **Date**: 2026-03-27 | **Spec**: `/specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/spec.md`  
**Input**: Feature specification from `/specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/spec.md`

## Summary

本特性分三步执行：先完成 `MotionPlanner` owner 收口（M7），再剥离 runtime/control 语义到 M9，最后收敛 public surface、目录骨架与 CMP 边界契约。目标是让 M7 回归“规划结果生产者”，M9 回归“执行结果消费者/驱动者”。

## Technical Context

**Language/Version**: C++17、CMake 3.20+、PowerShell 7、Python 3.11  
**Primary Dependencies**: ruckig、GoogleTest、spdlog、仓库根级验证脚本  
**Storage**: N/A（仅仓库文件与测试报告目录）  
**Testing**: `modules/*/tests` + `runtime/host/tests` + `.\test.ps1` + `scripts/validation/run-local-validation-gate.ps1`  
**Target Platform**: Windows 开发环境 + CI（无机台/mocks）  
**Project Type**: 多模块 C++ 工程（modules owner + apps host 装配）  
**Performance Goals**: 规划结果数值行为无回归（轨迹长度、关键速度约束和触发时序在既定容差内）  
**Constraints**: 不引入新的双向模块依赖；兼容层必须有退出路径；不破坏既有 root 入口  
**Scale/Scope**: `modules/motion-planning`、`modules/process-path`、`modules/runtime-execution`、`scripts/validation`、`specs/refactor/motion/ARCH-201-m7-owner-boundary-repair`  

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] Branch name is compliant with `<type>/<scope>/<ticket>-<short-desc>`
- [x] Planned paths use canonical workspace roots; no new legacy default paths
- [x] Build/test validation approach is explicit via root entry points (`.\build.ps1`, `.\test.ps1`, `.\ci.ps1`) or justified subset
- [x] Compatibility/fallback behavior is explicit and time-bounded if legacy paths are involved

## Project Structure

### Documentation (this feature)

```text
specs/refactor/motion/ARCH-201-m7-owner-boundary-repair/
├── spec.md
├── plan.md
└── tasks.md
```

### Source Code (repository root)

```text
modules/
├── motion-planning/
│   ├── contracts/
│   ├── application/
│   ├── domain/motion/
│   └── tests/
├── process-path/
│   └── domain/trajectory/
└── runtime-execution/
    ├── contracts/
    ├── application/
    ├── runtime/host/
    ├── adapters/device/
    └── tests/

scripts/
└── validation/
```

**Structure Decision**: 保持 canonical roots，不新增 legacy 根目录；通过 owner 归位与依赖收口修复结构偏差。

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| 临时兼容 include（旧 MotionPlanner 路径） | 降低并行分支合并冲击 | 直接硬切会导致未迁移分支批量构建失败且不可分阶段验证 |
