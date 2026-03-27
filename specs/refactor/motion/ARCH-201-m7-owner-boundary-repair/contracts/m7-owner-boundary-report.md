# M7 Owner Boundary Report

## Scope

- Owner module: `modules/motion-planning` (`M7`)
- Upstream module: `modules/process-path` (`M6`)
- 本阶段范围：Phase 1/2 + US1

## Owner Closure Rules

1. `MotionPlanner` 实现文件必须位于 `modules/motion-planning/domain/motion/domain-services/`
2. `motion-planning` 构建入口不得编译 `process-path` 下的 `MotionPlanner.cpp`
3. `MotionPlanningFacade` 只能引用 M7 的 `MotionPlanner` 头文件
4. `process-path` 旧路径只允许兼容层，不允许 owner 实现

## Enforcement

- `scripts/validation/assert-module-boundary-bridges.ps1` 新增 M7 旧路径检测规则
