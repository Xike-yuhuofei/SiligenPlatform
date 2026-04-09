# dxf-geometry/contracts

`modules/dxf-geometry/contracts/` 是 `M2 dxf-geometry` 的模块契约入口。

## 正式职责

- 记录 DXF 几何输入、`.dxf -> .pb` 预处理与 PB parser 相关的模块内契约与输入引用规则。
- 以 `DxfPbPreparationService` 与 `PbPathSourceAdapter` 作为当前稳定 C++ seam 的契约锚点。
- 作为 `modules/dxf-geometry/` 的契约锚点，供 `Wave 2` cutover 与门禁校验引用。

## 不应承载

- 跨模块稳定公共契约（应放入 `shared/contracts/engineering/`）。
- 运行时执行、设备适配或 HMI 展示层语义。
- 已无 live consumer 的 selector/factory/auto adapter surface。
- 未声明迁移路径与退出条件的 legacy 回写说明。

## 当前事实来源

- `modules/dxf-geometry/application/include/dxf_geometry/application/services/dxf/DxfPbPreparationService.h`
- `modules/dxf-geometry/adapters/include/dxf_geometry/adapters/planning/dxf/PbPathSourceAdapter.h`
- `modules/dxf-geometry/application/engineering_data/contracts/simulation_input.py`
  - 只保留 `simulation_input` stable compat shell；simulation runtime concrete 已迁到 `modules/runtime-execution/application/runtime_execution/`，geometry helper 仍由 `application/engineering_data/processing/simulation_geometry.py` 提供。
- `shared/contracts/engineering/`
  - preview JSON 与 simulation input JSON 的 canonical schema authority 仍在此处；本地 HTML preview renderer 已迁到 `apps/planner-cli/tools/planner_cli_preview/`。
- `modules/motion-planning/application/motion_planning/trajectory_generation.py`
  - trajectory owner 已迁到 `motion-planning`；`dxf-geometry` 不再把 trajectory shell 计入模块契约 authority。
