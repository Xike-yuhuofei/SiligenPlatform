# tests

`modules/dispense-packaging/tests/` 仅承载模块级验证入口与 closeout 证据。

## 当前 source-bearing 测试面

- `unit/domain/dispensing/`
  - 当前保留 `M8` owner/core 与 authority planning 相关单测，不再混入显式 residual planner / process-control / controller / shared util 回归。
- `contract/domain/dispensing/`
  - owner 边界守护与物理目录语义一致。
- `regression/domain/dispensing/`
  - residual acceptance 收口在此目录，不再混入 `unit`。
- `regression/domain/dispensing/planning/`
  - planning residual 与 legacy DXF quarantine audit 的物理回归面；其中 legacy DXF concrete 仅允许通过 test-only `siligen_dispense_packaging_planning_legacy_dxf_quarantine_support` 进入。
- `regression/domain/dispensing/execution/`
  - execution / process-control residual 回归面。
- `regression/shared/types/`
  - shared-adjacent 回归与 `M8` owner unit 分离，避免误记为模块 closeout 证据。
- `unit/application/services/dispensing/`
  - 当前只保留模块内 preview 邻接单测。
- `contract/application/services/dispensing/`
  - workflow-facing public seam contract 测试与其 fixture 同目录收口。
- `unit/application/usecases/dispensing/`
  - 当前 valve use case 邻接回归。

## 当前 live targets

- `siligen_dispense_packaging_boundary_tests` 与 `siligen_dispense_packaging_workflow_seam_tests` 是当前 phase 4 closeout gate。
- `siligen_dispense_packaging_unit_tests` 当前聚焦模块内 owner-adjacent/unit 邻接回归，不替代 closeout gate。
- `siligen_dispense_packaging_residual_regression_tests` 当前只承载结构性 residual acceptance / topology 守护。
- `siligen_dispense_packaging_planning_residual_regression_tests` 当前承载 planning residual 与 legacy DXF quarantine audit。
- `siligen_dispense_packaging_execution_residual_regression_tests` 当前承载 execution / process-control residual 回归。
- `siligen_dispense_packaging_shared_adjacent_regression_tests` 当前承载 shared-adjacent 回归。

## 占位目录

- `integration`
  当前只保留 canonical skeleton 占位目录，用于满足 workspace layout gate；它不承载 source-bearing suite，也不参与 closeout 证据。
- 如后续出现真实 integration lane，应以实际测试源文件和 target 为准扩展该目录，而不是把占位目录误记为 live 测试面。
