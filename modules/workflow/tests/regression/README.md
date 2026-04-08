# workflow regression tests

本目录是 `workflow` 模块 regression 承载面的正式入口。

## 当前登记目标

- `workflow_regression_dispensing_mode_resolution_smoke`
  - 直接编译本目录内的回归测试源码。
  - 作为 workflow 侧 legacy execute wiring 清理后的最小回归哨兵，专门守住模式切换与宿主接线不反弹。

## 约束

- regression 面优先登记可重复、可定向执行的 smoke 目标。
- 本任务不扩展 workflow owner 实现，不新增 public surface。
- 不再通过 `../canonical/` 复用 foreign owner 或 canonical 测试源码。
