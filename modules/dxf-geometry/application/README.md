# application

`modules/dxf-geometry/application/` 承载 `M2 dxf-geometry` 当前 live application surface。

## 当前 source-bearing roots

- `services/dxf/`
  - C++ `DxfPbPreparationService`、`PbCommandResolver`、`PbProcessRunner` 的实现。
- `engineering_data/processing/`
  - DXF preprocess 与 simulation geometry helper。
- `engineering_data/contracts/simulation_input.py`
  - 只保留 simulation-input stable compat shell；runtime concrete owner 已迁到
    `runtime-execution`。
- `engineering_data/cli/`
  - 模块内 `dxf_to_pb` / `export_simulation_input` CLI 包装入口。
- `engineering_data/models/`
  - 当前只保留 geometry helper dataclass，不再继续承载 trajectory owner 语义。

## 非 live / 禁止回流

- `engineering_data/preview/`
  - preview owner 已迁出，不应再回流本模块。
- `engineering_data/trajectory/`
  - trajectory owner 已迁到 `motion-planning`，本目录不应再承载 trajectory shell。
- `engineering_data/residual/`
  - 仅保留 residual quarantine 标记，不恢复为 live implementation root。
