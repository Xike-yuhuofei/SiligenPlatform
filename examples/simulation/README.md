# simulation examples

这里放工作区级运动仿真样例。

当前用途：

- canonical `simulation-input.json` 样例
- 示例程序/文档共用输入
- 跨包回归时的人类可读样本

当前高信号样例：

- `rect_diag.simulation-input.json`
  长路径、多段连续执行的 scheme C 回归输入
  对应导出样例：`examples/replay-data/rect_diag.scheme_c.recording.json`
- `sample_trajectory.json`
  线段 + 圆弧混合路径，以及 compare output + delayed IO + valve delay 组合回归输入
  对应导出样例：`examples/replay-data/sample_trajectory.scheme_c.recording.json`
- `invalid_empty_segments.simulation-input.json`
  结构化失败结果回归输入
- `following_error_quantized.simulation-input.json`
  可选 `motion_realism` 场景，覆盖跟随误差近似与编码器量化
