# engineering-data

`engineering-data` 是 Siligen 工程对象与离线预处理的 canonical 包，负责稳定的 DXF/几何/路径/轨迹预处理能力，不接管实时控制。

## Canonical 范围

以下逻辑现在归 `packages/engineering-data`：

- DXF -> `PathBundle(.pb)` 导出
- `PathBundle` -> simulation input JSON 导出
- DXF HTML/JSON 预览 artifact 生成
- 离线路径点集 -> 轨迹采样 JSON
- 稳定工程对象模型：`Point2D`、`LineSegment`、`ArcSegment`、`PlanningReport`、`TrajectoryArtifact`
- Python protobuf 生成代码：`engineering_data.proto.dxf_primitives_pb2`

## 兼容壳范围

以下入口继续保留为兼容面，但真实实现都转发到 `engineering-data`：

- 旧 CLI 名称：`dxf-to-pb`、`path-to-trajectory`、`export-simulation-input`、`generate-dxf-preview`
- 旧导入路径：`dxf_pipeline.cli.*`、`dxf_pipeline.contracts.*`、`dxf_pipeline.preview.*`
- 旧脚本/服务壳：`dxf_pipeline.services.*`

说明：

- 安装 `engineering-data` 后会同时暴露 canonical CLI 名称和 legacy CLI 名称
- 不把 `dxf-pipeline` 全部历史脚本原样搬进来
- `control-core` 不再依赖 `dxf-pipeline` 内部实现路径
- 需要兼容的 `.pb`、preview JSON、simulation JSON 由测试覆盖

## 构建与入口

- 安装入口：`packages/engineering-data/pyproject.toml`
- Canonical CLI：
  - `engineering-data-dxf-to-pb`
  - `engineering-data-path-to-trajectory`
  - `engineering-data-export-simulation-input`
  - `engineering-data-generate-preview`
- Legacy CLI alias（由 `engineering-data` 安装同时提供）：
  - `dxf-to-pb`
  - `path-to-trajectory`
  - `export-simulation-input`
  - `generate-dxf-preview`
- 工作区脚本入口：
  - `packages/engineering-data/scripts/dxf_to_pb.py`
  - `packages/engineering-data/scripts/path_to_trajectory.py`
  - `packages/engineering-data/scripts/export_simulation_input.py`
  - `packages/engineering-data/scripts/generate_preview.py`

## 边界说明

- `engineering-data` 只负责工程对象和离线预处理
- 实时控制、运行时调度、设备执行仍留在 `control-core` / runtime 侧
- 若一项能力同时服务预处理和运行时，先抽稳定模型，再保留两侧 wrapper

当前后续拆分点：

- `contracts.simulation_input` 后续拆成 path projection mapper / serializer
- `trajectory.offline_path_to_trajectory` 后续拆成 sampling / ruckig adapter / artifact writer
- `models.geometry` 后续可继续细分为 primitive / contour / topology 子模块
