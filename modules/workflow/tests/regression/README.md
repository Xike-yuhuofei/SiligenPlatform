# workflow regression tests

本目录是 `workflow` 模块 regression 承载面的正式入口。

## 当前登记目标

- `workflow_regression_deterministic_path_execution_smoke`
  - 复用 `tests/unit/` 的确定性路径执行测试源码。
  - 覆盖 canonical workflow 路径执行链最容易因链接、运行时依赖或装配退化而失稳的场景。
- `workflow_regression_boundary_cutover_smoke`
  - 覆盖 `process-runtime-core` 测试根删除、runtime concrete shim 删除、`InitializeSystemUseCase` contract cutover 与 workflow build graph 收口。
- `workflow_regression_planning_ingress_smoke`
  - 覆盖 `PbPathSourceAdapter` / `DXFAdapterFactory` 的 canonical 入口行为，防止 protobuf 与 DXF legacy fallback 静默回流。

## 约束

- regression 面优先登记可重复、可定向执行的 smoke 目标。
- 本任务不扩展 workflow owner 实现，不新增 public surface。
- 更细的路径规划、初始化和 planning ingress 回归矩阵，留给后续任务在本目录继续扩充。
