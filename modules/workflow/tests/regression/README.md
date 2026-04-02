# workflow regression tests

本目录是 `workflow` 模块 regression 承载面的正式入口。

## 当前登记目标

- `workflow_regression_deterministic_path_execution_smoke`
  - 复用 `process-runtime-core` 的确定性路径执行测试源码。
  - 作为 canonical workflow 路径执行链的最小回归哨兵，优先覆盖 owner 激活后最容易因链接、运行时依赖或装配退化而失稳的场景。

## 约束

- regression 面优先登记可重复、可定向执行的 smoke 目标。
- 本任务不扩展 workflow owner 实现，不新增 public surface。
- 更细的路径规划、初始化和装配回归矩阵，留给后续任务在本目录继续扩充。
