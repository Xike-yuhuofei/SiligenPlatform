# workflow integration tests

本目录是 `workflow` 模块 integration 承载面的正式入口。

## 当前登记目标

- 当前不再承载 owner-owned planning / execution integration 测试源码。
- `workflow_integration_tests` 仅作为 workflow 级 integration 空壳 target 与命名登记入口保留。
- `PlanningRequestTest`、`PlanningUseCaseExportPortTest`、`PlanningFailureSurfaceTest` 已迁到 `modules/dispense-packaging/tests/unit/application/usecases/dispensing/`。
- `DispensingWorkflowUseCaseTest` 已迁到 `modules/runtime-execution/tests/unit/dispensing/`。

## 约束

- 本目录只负责 integration 级构建入口、命名与登记。
- 当前 target 不直接编译测试源码，避免 foreign owner 语义回流到 `workflow/tests`。
- 不再反向复用 `tests/canonical/` 源文件。
- 后续若新增真正属于 workflow facade 的集成测试，可在本目录恢复源码注册；但不得把 planning / execution owner 测试回写到此处。
