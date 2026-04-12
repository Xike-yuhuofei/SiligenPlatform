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
- `siligen_dispense_packaging_execution_residual`
  - 显式承接 execution/process-control/valve/CMP residual。
- `siligen_dispense_packaging_planning_residual`
  - 显式承接 planning residual，并保留对上游 planning concrete 的依赖。

## 边界说明

- `架构真值`
  - `M8` 只能 owner `DispenseTimingPlan`、`ExecutionPackage` 等执行准备事实。
  - `M8` 不应越权成为运行时执行 owner，也不应重新持有路径/轨迹规划 owner 语义。
- `当前实现事实`
  - 本目录仍混有 residual concrete，说明迁移尚未完全结束。
- `本轮处理口径`
  - 只显式标记 owner/core 与 residual 的边界。
  - 不在本轮发起跨模块迁移，不把 residual 目录删除后伪装成已完成 owner 收口。
