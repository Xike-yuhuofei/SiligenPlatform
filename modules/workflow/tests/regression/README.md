# workflow regression tests

本目录是 `workflow` 模块 regression 登记面的正式入口。

## 当前状态

- 当前不承载 source-bearing regression 测试源码。
- foreign-owner 的模式切换 / execution wiring 哨兵已迁到 `modules/runtime-execution/tests/unit/dispensing/DispensingWorkflowModeResolutionTest.cpp`。
- 目录保留为 M0 regression shell，后续只允许登记真正属于 workflow owner 的回归目标。

## 约束

- regression 面优先登记可重复、可定向执行的 smoke 目标。
- 本任务不扩展 workflow owner 实现，不新增 public surface。
- 不再通过 `../canonical/` 复用 foreign owner 或 canonical 测试源码。
