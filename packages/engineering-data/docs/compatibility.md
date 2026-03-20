# engineering-data 兼容说明

## Canonical 范围

以下能力现在以 `packages/engineering-data` 为 canonical 实现：

- `engineering_data.processing.dxf_to_pb`
- `engineering_data.contracts.simulation_input`
- `engineering_data.preview.html_preview`
- `engineering_data.trajectory.offline_path_to_trajectory`
- `engineering_data.models.{geometry,trajectory}`
- `engineering_data.proto.dxf_primitives_pb2`

这些模块只负责工程对象和离线预处理，不接管实时控制、插补执行或设备驱动。

## 兼容壳范围

以下旧入口继续保留，但只做转发：

- `dxf_pipeline.cli.*`
- `dxf_pipeline.contracts.*`
- `dxf_pipeline.preview.html_preview`
- `dxf_pipeline.services.*`
- `proto.dxf_primitives_pb2`

兼容壳职责：

- 维持旧 CLI 名称和旧 Python 导入路径
- 兼容包物理位置统一位于 `packages/engineering-data/src`
- 不再承载 canonical 算法实现

## Cutover 约束

- 不再支持把 `dxf-pipeline/src` 作为 `PYTHONPATH` source root
- 工作区内调用方必须直接指向 `engineering_data.*` 或 `packages/engineering-data/scripts/*`
- 工作区外如仍使用 legacy import/CLI 名称，可在兼容窗口内继续安装 `engineering-data` 获取这些入口

## 后续拆分点

- `models.geometry` 后续可细分为 `dxf_primitives`、`path_segments`、`topology`
- `trajectory.offline_path_to_trajectory` 后续可拆为 `sampling`、`ruckig_adapter`、`json_export`
- `contracts.simulation_input` 里“段投影”和“JSON 组装”后续可分成独立 mapper / serializer
