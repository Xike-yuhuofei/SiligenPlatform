# tests

dxf-geometry 的模块级验证入口应收敛到此目录。

## 当前最小正式矩阵

- `unit/services/dxf/DxfPbPreparationServiceTest.cpp`
  - 对应 target：`siligen_dxf_geometry_unit_tests`
  - 冻结 PB 生成服务的缓存命中、外部根解析、命令覆盖与失败处理。
- `contract/DxfPbPreparationServiceContractsTest.cpp`
  - 对应 target：`siligen_dxf_geometry_contract_tests`
  - 冻结 `DxfPbPreparationService` 公开构造面、`EnsurePbReady` 与 `CleanupPreparedInput` 的最小稳定行为。
- `golden/DxfPbPreparationServiceCommandGoldenTest.cpp`
  - 对应 target：`siligen_dxf_geometry_golden_tests`
  - 冻结非默认 `DxfPreprocessConfig` 组装出的参数快照，防止命令旗标顺序和默认值漂移。
- `integration/DxfPbPreparationServiceIntegrationTest.cpp`
  - 对应 target：`siligen_dxf_geometry_integration_tests`
  - 覆盖工作区 `scripts/engineering-data/dxf_to_pb.py` 解析、配置旗标透传，以及 prepared `.pb` cleanup 闭环。
- `contract/PbPathSourceAdapterContractTest.cpp`
  - 对应 target：`dxf_geometry_pb_path_source_adapter_contract_test`
  - 冻结 `PbPathSourceAdapter` 在 protobuf on/off 两种构建面的显式失败契约。
- `python/test_engineering_data_residual_quarantine.py`
  - 对应命令：`python -m pytest modules/dxf-geometry/tests/python/test_engineering_data_residual_quarantine.py -q`
  - 冻结 `engineering_data.contracts.simulation_input` 现在是 `runtime_execution` owner + `engineering_data.processing.simulation_geometry` helper 组合出的 stable compat shell，并证明旧 `simulation_runtime_export.py` 已删除。
- `python/test_engineering_data_cli_lane.py`
  - 对应命令：`python -m pytest modules/dxf-geometry/tests/python/test_engineering_data_cli_lane.py -q`
  - 冻结 `engineering_data` 两个模块内 CLI (`dxf_to_pb` / `export_simulation_input`) 仍可用，并验证 workspace `export_simulation_input.py` / `path_to_trajectory.py` 继续是正式入口。

## 当前不进入 live suite

- `regression/`
  - 仅为 workspace layout 要求的 canonical skeleton；当前不承载 source-bearing 测试，也不注册 regression target。

## 边界

- 模块内 `tests/` 负责 `dxf-geometry` owner 级证明，不用仓库级 `tests/integration` 替代。
- `unit/contract/golden/integration` 必须保持独立 target，不再混编成单一 `unit_tests` 容器。
- `golden` 只冻结脱敏参数快照，不把临时目录路径写成权威真值。
- `python/` 负责 `engineering_data` 中 DXF preprocess / simulation compat shell 的 owner closeout，并证明 trajectory / preview 已迁出本模块、simulation runtime residual 已退出 live tree。
- 本地 preview owner gate 已迁到 `apps/planner-cli/tests/python/`；trajectory owner gate 已迁到 `modules/motion-planning/application/motion_planning/` 与 `scripts/engineering-data/path_to_trajectory.py`。
- trajectory compat shell 与模块内旧 CLI 已从 live surface 删除。
