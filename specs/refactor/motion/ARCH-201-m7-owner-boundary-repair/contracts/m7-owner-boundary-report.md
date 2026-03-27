# M7 Owner Boundary Report

## Scope

- Owner module: `modules/motion-planning` (`M7`)
- Upstream module: `modules/process-path` (`M6`)
- Runtime/control owner: `modules/runtime-execution` (`M9`)
- 本阶段范围：Stage B（边界收敛 + US2 最小闭环）

## Owner Closure Rules

1. `MotionPlanner` 实现文件必须位于 `modules/motion-planning/domain/motion/domain-services/`
2. `motion-planning` 构建入口不得编译 `process-path` 下的 `MotionPlanner.cpp`
3. `MotionPlanningFacade` 只能引用 M7 的 `MotionPlanner` 头文件
4. `process-path` 旧路径只允许兼容层，不允许 owner 实现
5. `IMotionRuntimePort`、`IIOControlPort`、`MotionControlServiceImpl`、`MotionStatusServiceImpl` 的 owner 必须位于 `modules/runtime-execution`
6. `modules/motion-planning/domain/motion/*` 与 `modules/workflow/domain/include/domain/motion/*` 中的同名 runtime/control 头文件只允许保留 shim/alias
7. `apps/runtime-gateway` 的 motion 注入只能通过 `runtime_execution/application/usecases/motion/MotionControlUseCase.h`

## Enforcement

- `scripts/validation/assert-module-boundary-bridges.ps1` 新增 M7 旧路径检测规则
- `scripts/validation/assert-module-boundary-bridges.ps1` 同时覆盖 M7 -> M9 runtime/control owner 迁移后的 required/forbidden 规则
