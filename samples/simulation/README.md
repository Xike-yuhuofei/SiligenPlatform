# simulation examples

这里放工作区级运动仿真样例。

当前用途：

- canonical `simulation-input.json` 样例
- 示例程序/文档共用输入
- 跨包回归时的人类可读样本

当前高信号样例：

- `rect_diag.simulation-input.json`
  长路径、多段连续执行的 scheme C 回归输入
  对应导出样例：`samples/replay-data/rect_diag.scheme_c.recording.json`
- `sample_trajectory.json`
  线段 + 圆弧混合路径，以及 compare output + delayed IO + valve delay 组合回归输入
  对应导出样例：`samples/replay-data/sample_trajectory.scheme_c.recording.json`
- `invalid_empty_segments.simulation-input.json`
  结构化失败结果回归输入
- `following_error_quantized.simulation-input.json`
  可选 `motion_realism` 场景，覆盖跟随误差近似与编码器量化

说明：

- `rect_diag`、`sample_trajectory` 的 canonical replay export 位于 `samples/replay-data/`。
- `invalid_empty_segments`、`following_error_quantized` 当前没有独立 replay 文件；当 legacy executable 缺失时，`run_simulated_line.py` 会回退到对应 baseline summary fixture，保持分层 gate 可执行。

## Fault Scenario Mapping

Fault scenario contract 现在固定为 `fault-scenario.v1`，每条场景都必须声明：

- `owner_scope`
- `source_asset_refs`
- `injector_id`
- `default_seed`
- `clock_profile`
- `supported_hooks`

当前 simulated-line matrix 固定为 `fault-matrix.simulated-line.v1`，deterministic replay 默认值为：

- `seed = 0`
- `clock_profile = deterministic-monotonic`
- `repeat_count = 2`

允许的 hook surface 只包括：

- `controller.readiness`
- `controller.abort`
- `io.disconnect`

- `invalid_empty_segments.simulation-input.json`
  - fault id: `fault.simulated.invalid-empty-segments`
  - 默认结果：`failed`
  - 适用层：`L2-offline-integration`、`L3-simulated-e2e`
  - injector: `simulated-line.input-asset`
  - source_asset_refs:
    `sample.simulation.invalid_empty_segments`、`baseline.simulation.invalid_empty_segments`
  - supported_hooks:
    `controller.readiness`
- `following_error_quantized.simulation-input.json`
  - fault id: `fault.simulated.following_error_quantized`
  - 默认结果：`deferred`
  - 适用层：`L3-simulated-e2e`、`L4-performance`
  - injector: `simulated-line.input-asset`
  - source_asset_refs:
    `sample.simulation.following_error_quantized`、`baseline.simulation.following_error_quantized`
  - supported_hooks:
    `controller.abort`、`io.disconnect`
