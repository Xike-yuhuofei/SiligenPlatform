# engineering-contracts

`engineering-contracts` 固化 DXF 工程数据跨模块、跨语言协作时的 canonical 协议定义，避免 `engineering-data`、`apps/hmi-app` 与仿真回归链继续通过隐式字段约定协作。

## Canonical 定义位置

- `.pb` / `PathBundle` canonical 定义：`proto/v1/dxf_primitives.proto`
- 预览 JSON canonical 定义：`schemas/v1/preview-artifact.schema.json`
- simulation input JSON canonical 定义：`schemas/v1/simulation-input.schema.json`

## Producer / Consumer

| Contract | Producer | Consumer | Notes |
| --- | --- | --- | --- |
| `PathBundle` (`.pb`) | `engineering_data.processing.dxf_to_pb` / `engineering_data.cli.dxf_to_pb` | `engineering-data` 仿真导出、`control-core` DXF 规划适配 | 当前 wire format 延续 `Backend_CPP/proto/dxf_primitives.proto` |
| Preview Artifact JSON | `scripts/engineering-data/generate_preview.py --json`（实现 owner：`apps/planner-cli/tools/planner_cli_preview`） | canonical preview artifact consumer / HMI preview gate 辅助校验 | HMI 运行时主链路改走 gateway preview snapshot；不再依赖本地 preview integration shim |
| Simulation Input JSON | `engineering_data.contracts.simulation_input.bundle_to_simulation_payload` / `engineering_data.cli.export_simulation_input` | `tests/e2e/simulated-line/` 回归链路与相关仿真执行器 | stable surface 保持不变；runtime concrete owner 已迁到 `modules/runtime-execution/application/runtime_execution/`，geometry helper 仍由 `modules/dxf-geometry/application/engineering_data/processing/simulation_geometry.py` 提供 |

## 文档

- 版本策略：`docs/versioning.md`
- 跨格式映射：`docs/mappings.md`
- Fixture 说明：`fixtures/README.md`

## 验证

兼容性测试位于 `tests/test_engineering_contracts.py`，覆盖：

- owner proto 与工作区公开 canonical `data/schemas/engineering/dxf/v1/dxf_primitives.proto` 一致
- 若存在外部 `Backend_CPP/proto/dxf_primitives.proto`，额外校验 owner proto 与其一致
- `.pb` fixture 能按现有 protobuf 生成代码解析
- `engineering-data` 当前 preview / simulation 导出结果符合 canonical schema
- canonical preview artifact consumer 可消费 canonical preview fixture
- simulated-line 当前可执行链路可消费 canonical simulation fixture
