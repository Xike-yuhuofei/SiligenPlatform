# tests

workflow 的模块级验证入口应收敛到此目录。

- `unit/` 现为 workflow 自有单元测试的 canonical 承载面，正式承载 `siligen_unit_tests`、`siligen_pr1_tests`、`siligen_dispensing_semantics_tests`。
- `workflow integration` 登记面保留给 workflow-owned facade 集成测试；已删除的 runtime assembly smoke 不得在本目录恢复。
- `regression/` 正式承载 `workflow_regression_deterministic_path_execution_smoke`、`workflow_regression_boundary_cutover_smoke`、`workflow_regression_planning_ingress_smoke`。
- 历史 `process-runtime-core` 测试根已删除，不再作为 live build/test 入口。
