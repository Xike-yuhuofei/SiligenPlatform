# tests

`modules/dispense-packaging/tests/` 是当前模块级验证入口。

## 当前 source-bearing 测试面

- `unit/domain/dispensing/`
  - 当前保留 `M8` owner/core 与 authority planning 相关单测，不再混入显式 residual planner / process-control / controller / shared util 回归。
- `contract/domain/dispensing/`
  - owner 边界守护与物理目录语义一致。
- `regression/domain/dispensing/`
  - residual acceptance 与 planning / execution residual 回归统一收口，不再混入 `unit`。
- `regression/shared/types/`
  - shared-adjacent 回归与 `M8` owner unit 分离，避免误记为模块 closeout 证据。
- `unit/application/services/dispensing/`
  - 当前只保留模块内 preview 邻接单测。
- `contract/application/services/dispensing/`
  - workflow-facing public seam contract 测试与其 fixture 同目录收口。
- `unit/application/usecases/dispensing/`
  - 当前 valve use case 邻接回归。

## 当前 live targets

- `siligen_dispense_packaging_unit_tests`
  - 当前聚焦模块内 unit 范围，不再混入 boundary / seam / residual acceptance。
- `siligen_dispense_packaging_boundary_tests`
  - 当前 owner 边界守护入口；物理源文件位于 `contract/`。
- `siligen_dispense_packaging_workflow_seam_tests`
  - 当前 workflow-facing seam 守护入口；物理源文件位于 `contract/`。
- `siligen_dispense_packaging_residual_regression_tests`
  - 当前 residual acceptance 守护入口。
- `siligen_dispense_packaging_planning_residual_regression_tests`
  - 当前 planning residual 回归入口；物理源文件位于 `regression/domain/dispensing/planning/`。
- `siligen_dispense_packaging_execution_residual_regression_tests`
  - 当前 execution / process-control residual 回归入口；物理源文件位于 `regression/domain/dispensing/execution/`。
- `siligen_dispense_packaging_shared_adjacent_regression_tests`
  - 当前 shared-adjacent 回归入口；物理源文件位于 `regression/shared/types/`。

## 占位目录

当前没有 source-bearing `integration/` suite，也不再保留空壳占位目录。
如后续出现真实 integration lane，应以实际测试源文件和 target 为准重新建立目录，而不是先放 `.gitkeep`。
