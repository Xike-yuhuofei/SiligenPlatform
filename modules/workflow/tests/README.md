# tests

workflow 的模块级验证入口应收敛到此目录。

- `canonical` 是 workflow canonical 测试源码承载面，只覆盖当前 owner graph 下仍属于 workflow 的主体验证入口。
- `unit/` 不再允许承载 source-bearing 测试；历史 workflow `unit` 目录只保留空目录占位，避免 foreign owner 测试继续回流。
- `integration` 现作为 workflow 级集成烟测登记面与空壳构建入口，不再承载 planning / execution foreign-owner 测试源码。
- `regression` 现作为 workflow 级回归登记面与空壳构建入口；foreign-owner 模式哨兵已迁到 `modules/runtime-execution/tests/unit/dispensing/`。
- `PlanningRequestTest`、`PlanningUseCaseExportPortTest`、`PlanningFailureSurfaceTest` 已迁到 `modules/dispense-packaging/tests`。
- `DispensingWorkflowUseCaseTest` 已迁到 `modules/runtime-execution/tests`。
- `DispensingWorkflowModeResolutionTest` 已迁到 `modules/runtime-execution/tests/unit/dispensing/`。
- 历史 bridge 测试入口不再属于 canonical tests surface。
