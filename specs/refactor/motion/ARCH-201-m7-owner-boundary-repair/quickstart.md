# Quickstart: M7 Owner Boundary Repair

## 1. 前置

1. 确认当前分支为 `refactor/motion/ARCH-201-m7-owner-boundary-repair`
2. 运行上下文解析：
   - `pwsh -NoLogo -NoProfile -File scripts/validation/resolve-workflow-context.ps1`

## 2. 核心验证（US1）

1. 边界门禁：
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot . -ReportDir tests/reports/module-boundary-bridges`
2. 运行 motion-planning 单测（包含 owner 归位测试）：
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File test.ps1 -Profile CI -Suite contracts -ReportDir tests/reports/m7-owner-boundary-repair -FailOnKnownFailure`

## 3. 全量本地门禁（可选）

- `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/run-local-validation-gate.ps1 -ReportRoot tests/reports/local-validation-gate`

## 4. 验收检查点

1. `modules/motion-planning/domain/motion/CMakeLists.txt` 不再引用 `process-path/.../MotionPlanner.cpp`
2. `MotionPlanningFacade.cpp` 使用 `domain/motion/domain-services/MotionPlanner.h`
3. `process-path/domain/trajectory/CMakeLists.txt` 不再反向暴露 motion-planning include 根
