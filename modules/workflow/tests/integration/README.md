# workflow integration tests

本目录是 `workflow` 模块 integration 承载面的正式入口。

## 当前登记目标

- `siligen_dispensing_semantics_tests`
  - 直接编译 workflow 自有的 integration 级测试源码。
  - 覆盖 `DispensingWorkflowUseCase` 与 `PlanningUseCaseExportPort` 这类仍由 workflow 负责的跨 owner 装配语义。

## 约束

- 本目录只负责 integration 级构建入口、命名与登记。
- 不再反向复用 `tests/canonical/` 源文件。
- 后续若新增更贴近 workflow facade 的集成测试，应继续落在本目录注册，不得回写 bridge 测试入口。
