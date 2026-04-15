# dxf-geometry

目标 owner：`M2 dxf-geometry`

当前正式事实来源：`modules/dxf-geometry/{application,adapters,contracts}/` 与 `shared/contracts/engineering/`

## Live Surface

- 构建入口：`CMakeLists.txt`（目标：`siligen_module_dxf_geometry`）。
- C++ canonical seam：`dxf_geometry/application/services/dxf/DxfPbPreparationService.h`（当前稳定公开 `EnsurePbReady` 与 `CleanupPreparedInput`）。
- C++ parser seam：`dxf_geometry/adapters/planning/dxf/PbPathSourceAdapter.h`（由 `siligen_dxf_geometry_pb_path_source_adapter` 暴露）。
- Python stable contract：`engineering_data.contracts.simulation_input`。
  该 surface 现在只是 stable compat shell；runtime concrete owner 已迁到 `modules/runtime-execution/application/runtime_execution/`，geometry helper 仍在 `engineering_data.processing.simulation_geometry`。
- 不再保留 `application/services/dxf/DxfPbPreparationService.h` 旧 include wrapper。

## Live Owner Graph

- `modules/dxf-geometry/` 是 `M2` 的 canonical owner 根。
- `modules/dxf-geometry/application/engineering_data/` 当前只承载 DXF preprocess、simulation geometry helper 与 simulation-input stable compat shell。
- simulation-input runtime concrete 已迁到 `modules/runtime-execution/application/runtime_execution/`；`dxf-geometry` 不再承载 runtime defaults、trigger 投影或 payload 组装实现。
- trajectory owner 已迁到 `modules/motion-planning/application/motion_planning/trajectory_generation.py`；`scripts/engineering-data/path_to_trajectory.py` 是唯一正式 workspace 入口。
- preview owner 已迁到 `apps/planner-cli` 下的 `planner_cli_preview` 子包；`scripts/engineering-data/generate_preview.py` 只是 workspace 稳定入口。

## 统一骨架状态

- 当前 source-bearing roots：`application/`、`adapters/`、`tests/`
- `domain`、`services`、`examples` 保留为 canonical skeleton；当前 shell-only，不参与 live build。
- `tests/regression/` 仅为 workspace layout 要求的 canonical skeleton；当前不承载 source-bearing 测试，也不注册 regression target。

## 测试基线

- `modules/dxf-geometry/tests/` 负责 `dxf-geometry` owner 级 `unit`、`contract`、`golden`、`integration` 分层证明。
- 当前最小正式矩阵聚焦 `DxfPbPreparationService` 与 `PbPathSourceAdapter` 的 live seam。
- `modules/dxf-geometry/tests/python/` 是 `engineering_data` 中 DXF preprocess / simulation-input contract 的模块内 Python gate，并承担 trajectory / preview 已迁出本模块 live surface 的证明。
  仓库级 `tests/contracts/**` 与 `scripts/validation/verify-engineering-data-cli.ps1` 继续作为跨 owner consumer 证据，而不是替代本模块 closeout。
- 仓库级 `tests/` 只消费跨 owner 场景，不替代本模块内 contract。
