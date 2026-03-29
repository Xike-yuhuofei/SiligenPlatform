# application

`modules/dispense-packaging/application/` 是 `M8` 对外暴露 packaging / validation consumer surface 的唯一入口。

- `DispensePlanningFacade` 只负责 `ExecutionPackage` 组装、preview payload 组装与离线校验相关 owner 事实。
- 规划算法实现不得回流到本目录；上游路径/轨迹规划分别由 `M6`、`M7` owner 提供。
