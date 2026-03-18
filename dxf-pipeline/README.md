# dxf-pipeline

`dxf-pipeline` 现在是兼容入口层，保留旧 CLI 和旧 Python 导入路径；canonical 的工程对象与离线预处理实现已经收口到 `D:\Projects\SiligenSuite\packages\engineering-data`。

## 输入边界

- 当前导入输入固定为 `DXF R12`。
- 处理链路应优先面向 `R12` 可表达的二维几何：`LINE`、`ARC`、`CIRCLE`、旧式 `POLYLINE` / `VERTEX`、`POINT`。
- 默认假设输入不依赖高版本 DXF 特性；若上游导入了高版本实体，应在进入 `dxf-pipeline` 前完成降级或预处理。

## 当前定位

- 保留旧 CLI 名称：`dxf-to-pb`、`path-to-trajectory`、`export-simulation-input`、`generate-dxf-preview`
- 保留旧导入路径：`dxf_pipeline.cli.*`、`dxf_pipeline.contracts.*`、`dxf_pipeline.preview.*`
- 兼容源码工作区中的历史调用方式
- 不再承载 canonical 算法实现

## Canonical 实现位置

- `D:\Projects\SiligenSuite\packages\engineering-data`
- `D:\Projects\SiligenSuite\packages\engineering-contracts`

## 兼容说明

- 新功能优先落在 `engineering-data`
- 本目录内的 CLI / contracts / preview / proto 仅作 compatibility shim
- 若需要稳定模型、脚本入口或测试夹具，请优先使用 `engineering-data` / `engineering-contracts`

## 常用命令

```powershell
$env:PYTHONPATH="D:\Projects\SiligenSuite\packages\engineering-data\src"
python -m engineering_data.cli.dxf_to_pb --help
python -m engineering_data.cli.export_simulation_input --help
```

## 导出 packages/simulation-engine 输入 JSON

`dxf-pipeline` 旧命令仍可用，但内部会转发到 `engineering-data`。

最小示例：

```powershell
$env:PYTHONPATH="D:\Projects\SiligenSuite\dxf-pipeline\src"
python -m dxf_pipeline.cli.export_simulation_input `
  --input "D:\Projects\SiligenSuite\dxf-pipeline\tests\fixtures\imported\uploads-dxf\live\d1a4fe0c-664e-4177-057f-34a21e3feed3_1770211294022_rect_diag.pb" `
  --output "D:\Projects\SiligenSuite\dxf-pipeline\examples\rect_diag.simulation-input.json"
```

可选地传入 `--trigger-points-csv`，把带 `x_mm,y_mm` 的点位 CSV 投影为全局累计长度触发位置；若未提供触发源，则导出空数组 `triggers: []`。
