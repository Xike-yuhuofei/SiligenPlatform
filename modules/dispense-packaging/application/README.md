# application

`modules/dispense-packaging/application/` 是 `M8` 对外暴露 packaging / validation consumer surface 的唯一入口。

- planning owner 实现收敛在 `PlanningAssemblyServices.cpp` 与
  `WorkflowPlanningAssemblyOperationsProvider.cpp`。
- 不再保留 `DispensePlanningFacade` 作为 live owner 或兼容入口。
- 规划算法实现不得回流到本目录；上游路径/轨迹规划分别由 `M6`、`M7` owner 提供。
