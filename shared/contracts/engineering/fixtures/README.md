# Fixtures

本目录下的 fixture 分成两类：

- canonical producer case：为工程契约包提供稳定基线，可复用到 preview / simulation / HMI / e2e
- process-path regression case：只服务 `modules/process-path` 的 owner 级回归，不承担 canonical producer 语义

## canonical producer case

当前 canonical 基线是 `cases/rect_diag`，owner 仍是 `shared/contracts/engineering`，来源与生成方式如下：

- `rect_diag.dxf`：迁移初始样本来自 `dxf-pipeline/tests/fixtures/.../rect_diag.dxf`，现以本目录副本为基线
- `rect_diag.pb`：迁移初始样本来自 legacy live fixture，现由 `engineering_data.proto.dxf_primitives_pb2` 负责解析
- `preview-artifact.json`：由 `scripts/engineering-data/generate_preview.py --json` 针对 `rect_diag.dxf` 生成并固化
- `simulation-input.json`：由 `engineering_data.contracts.simulation_input.bundle_to_simulation_payload()`（实现 owner：`modules/runtime-execution/application/runtime_execution/simulation_input.py`）针对 `rect_diag.pb` 导出并固化

这些 fixture 的目的不是替代生产实现，而是为契约包提供：

- 稳定的回归样本
- canonical producer 输出基线
- `apps/hmi-app` / `tests/e2e/simulated-line` 兼容验证输入

## process-path regression case

以下 case 只服务 `M6 process-path` owner 回归，不参与 canonical producer 输出：

- `cases/point_mixed/point_mixed.pb`
- `cases/fragmented_chain/fragmented_chain.pb`
- `cases/multi_contour_reorder/multi_contour_reorder.pb`
- `cases/intersection_split/intersection_split.pb`

约束：

- 这些 case 默认只提交 `.pb` 与 case README，不补 `preview-artifact.json` / `simulation-input.json`
- 每个 case README 都必须写清楚用途、期望 diagnostics、是否属于 canonical producer
- 新增 process-path regression case 时，优先复用 `tools/generate_process_path_regression_cases.py` 生成，避免手写二进制样本漂移
