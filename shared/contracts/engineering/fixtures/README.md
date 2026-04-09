# Fixtures

当前 fixture 以 `rect_diag` 为基线案例，当前 canonical owner 是 `shared/contracts/engineering`，来源与生成方式如下：

- `rect_diag.dxf`：迁移初始样本来自 `dxf-pipeline/tests/fixtures/.../rect_diag.dxf`，现以本目录副本为基线
- `rect_diag.pb`：迁移初始样本来自 legacy live fixture，现由 `engineering_data.proto.dxf_primitives_pb2` 负责解析
- `preview-artifact.json`：由 `scripts/engineering-data/generate_preview.py --json` 针对 `rect_diag.dxf` 生成并固化
- `simulation-input.json`：由 `engineering_data.contracts.simulation_input.bundle_to_simulation_payload()`（实现 owner：`modules/runtime-execution/application/runtime_execution/simulation_input.py`）针对 `rect_diag.pb` 导出并固化

这些 fixture 的目的不是替代生产实现，而是为契约包提供：

- 稳定的回归样本
- canonical producer 输出基线
- `apps/hmi-app` / `tests/e2e/simulated-line` 兼容验证输入
