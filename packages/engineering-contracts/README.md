# engineering-contracts

`engineering-contracts` 固化 DXF 工程数据跨包、跨语言协作时的 canonical 协议定义，避免 `dxf-pipeline`、`apps/hmi-app`、`packages/simulation-engine` 继续通过隐式字段约定协作。

## Canonical 定义位置

- `.pb` / `PathBundle` canonical 定义：`proto/v1/dxf_primitives.proto`
- 预览 JSON canonical 定义：`schemas/v1/preview-artifact.schema.json`
- simulation input JSON canonical 定义：`schemas/v1/simulation-input.schema.json`

## Producer / Consumer

| Contract | Producer | Consumer | Notes |
| --- | --- | --- | --- |
| `PathBundle` (`.pb`) | `engineering_data.processing.dxf_to_pb` / `engineering_data.cli.dxf_to_pb` | `engineering-data` 仿真导出、`control-core` DXF 规划适配 | 当前 wire format 延续 `Backend_CPP/proto/dxf_primitives.proto` |
| Preview Artifact JSON | `engineering_data.preview.html_preview.generate_preview()` / `engineering_data.cli.generate_preview --json` | `apps/hmi-app` `DxfPipelinePreviewClient` | HMI 默认走 canonical CLI，legacy `dxf-pipeline` 只保留兼容壳 |
| Simulation Input JSON | `engineering_data.contracts.simulation_input.bundle_to_simulation_payload` / `engineering_data.cli.export_simulation_input` | `packages/simulation-engine` loader / runtime bridge | 必须保持当前 consumer 可直接读取 |

## 文档

- 版本策略：`docs/versioning.md`
- 跨格式映射：`docs/mappings.md`
- Fixture 说明：`fixtures/README.md`

## 验证

兼容性测试位于 `tests/test_engineering_contracts.py`，覆盖：

- canonical proto 与现有 `Backend_CPP` proto 一致
- `.pb` fixture 能按现有 protobuf 生成代码解析
- `engineering-data` 当前 preview / simulation 导出结果符合 canonical schema
- `apps/hmi-app` 当前 preview consumer 可消费 canonical preview fixture
- `packages/simulation-engine` 当前可执行程序可消费 canonical simulation fixture
