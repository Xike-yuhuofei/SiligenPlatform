# domain/dispensing

`modules/dispense-packaging/domain/dispensing/` 当前同时承载两类内容：

- `canonical owner/core surface`
  - `value-objects/` 中与 `TriggerPlan`、`DispensingPlan`、`DispensingExecutionPlan`、
    `AuthorityTriggerLayout` 等 `M8` 执行准备事实直接相关的头文件。
  - `ports/` 中仍保留的兼容头，但这些头的正式 owner 已转到
    `runtime-execution/contracts`，这里不能再被表述成新的 owner 来源。
- `residual containers`
  - `domain-services/` 里的 execution/process-control/valve/CMP concrete。
  - `planning/` 里的 planning concrete。
  - `compensation/`、`model/`、`simulation/` 里的仓库存在物，不应被误判为 live public
    owner surface。

## 当前构建口径

- `siligen_dispense_packaging_domain_dispensing`
  - 当前只应被理解为 owner/core header 与 contract surface。
- `siligen_dispense_packaging_domain_dispensing_planning_residual_headers` 与
  `siligen_dispense_packaging_domain_dispensing_execution_residual_headers`
  - 仅为 residual/internal target 提供 fat include 与 runtime bridge，不得重新挂回 owner/core target。
- `siligen_dispense_packaging_planning_owner_residual`
  - 当前只保留 owner-adjacent 的 authority/path utility concrete。
- `siligen_dispense_packaging_compensation_residual`
  - 作为 execution/planning residual 共享的 module-local compensation bridge。
- `siligen_dispense_packaging_execution_residual`
  - 显式承接 execution/process-control/valve/CMP residual。
- legacy DXF planning concrete 不再出现在 domain live build graph；当前仅允许作为
  `siligen_dispense_packaging_planning_legacy_dxf_quarantine_support` 测试支撑 target，
  供 planning residual regression 隔离回归使用。
- `UnifiedTrajectoryPlannerService` wrapper 已删除；`DispensingPlannerService.cpp`
  内残余的 process-path / motion-planning 编排仅保留为私有 helper，原 wrapper 行为证据改由
  process-path owner tests 承接。
- `PathOptimizationStrategy` 已删除；轮廓优化残余当前只保留
  `ContourOptimizationService.cpp` 这一条 quarantine seam。

## 边界说明

- `架构真值`
  - `M8` 只能 owner `DispenseTimingPlan`、`ExecutionPackage` 等执行准备事实。
  - `M8` 不应越权成为运行时执行 owner，也不应重新持有路径/轨迹规划 owner 语义。
- `当前实现事实`
  - 本目录仍混有 residual concrete，说明迁移尚未完全结束。
- `本轮处理口径`
  - 只显式标记 owner/core 与 residual 的边界。
  - 不在本轮发起跨模块迁移，不把 residual 目录删除后伪装成已完成 owner 收口。
- `禁止项`
  - 不允许通过 raw include path 暴露 `workflow` include root 或
    `runtime-execution/contracts/runtime` include root；如需跨模块头，应通过 canonical target
    传递。
