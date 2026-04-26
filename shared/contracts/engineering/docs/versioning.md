# 版本策略

## 范围

本包只管理工程数据边界协议，不管理实现包内部类。

当前 `v1` 覆盖三类契约：

- `proto/v1/dxf_primitives.proto`
- `schemas/v1/preview-artifact.schema.json`
- `schemas/v1/simulation-input.schema.json`

## Owner

- 主生产者：
  - `.pb`：`engineering_data.processing.dxf_to_pb` / `engineering_data.cli.dxf_to_pb`
  - preview JSON：`scripts/engineering-data/generate_preview.py --json`（实现 owner：`apps/planner-cli/tools/planner_cli_preview`）
  - simulation input JSON：`engineering_data.contracts.simulation_input.bundle_to_simulation_payload` / `engineering_data.cli.export_simulation_input`（实现 owner：`modules/runtime-execution/application/runtime_execution/`；schema authority 仍在本包）
- 主消费者：
  - `.pb`：`dxf-pipeline`、`control-core`
  - preview JSON：`apps/hmi-app`
  - simulation input JSON：`tests/e2e/simulated-line`

任何协议变更都必须同时更新：

1. canonical proto/schema
2. `docs/mappings.md`
3. `fixtures/`
4. `tests/test_engineering_contracts.py`

## Proto 版本策略

`.pb` 使用 protobuf wire compatibility 规则：

- 允许新增字段，但只能追加新 tag
- 禁止复用、重排、删除既有 field number
- 禁止静默修改既有字段语义
- 新增 enum 值只能追加，不能改写已有数值

`PathHeader.schema_version` 是 payload 自描述版本。

- 当前 canonical `PathBundle` schema version：`2`
- 只有发生 wire-level breaking change 时才提升该值

当前 `v1` canonical proto 路径下承载的是最新单轨 schema，而不是历史 payload 兼容窗口。

- 历史 `schema_version=1` `PathBundle` 已退出 live 支持
- 当前 producer 必须输出 `schema_version=2`
- 当前 consumer 必须对非 `2` 的 payload fail-closed
- 旧 payload 的唯一升级路径是重新从 `.dxf` 正式生成，禁止仓内静默兼容读取

## JSON 版本策略

preview JSON 与 simulation input JSON 当前 payload 内没有独立版本字段，因此版本锚点由 canonical 文件路径承载：

- `schemas/v1/preview-artifact.schema.json`
- `schemas/v1/simulation-input.schema.json`

规则如下：

- 向后兼容的新增可选字段：保留在 `v1`
- 字段重命名、删除、类型变化、新增必填字段：升级到 `v2`
- 语义变化但字段名不变：必须升级主版本，禁止静默复用旧字段

## 兼容发布要求

一个变更只有在满足下列条件后才能发布：

1. `dxf-pipeline` 当前导出结果能通过 canonical 校验
2. `apps/hmi-app` 或 `tests/e2e/simulated-line` 现有消费方至少有一个真实兼容测试通过
3. 若存在跨格式映射变化，`docs/mappings.md` 必须显式说明

## 非兼容变更示例

- 将 `ArcPrimitive.start_angle_deg` 改成弧度但仍保留原字段名
- 将 simulation input 的 `trigger_position` 从“累计路径长度 mm”改成坐标点
- 将 preview JSON canonical 从 CLI 输出悄悄替换成 HTML 内嵌 metadata
