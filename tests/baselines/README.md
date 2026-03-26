# regression-baselines

这里存放工作区级场景回归基线。

当前基线：

- `rect_diag.simulation-baseline.json`：canonical `rect_diag` 仿真摘要基线
- `rect_diag.scheme_c.recording-baseline.json`：scheme C 最小闭环 `scheme_c.recording.v1` 摘要基线
- `sample_trajectory.scheme_c.recording-baseline.json`：scheme C 线段 + 圆弧混合路径与 replay 组合摘要基线
- `invalid_empty_segments.scheme_c.recording-baseline.json`：scheme C 结构化失败结果摘要基线
- `following_error_quantized.scheme_c.recording-baseline.json`：scheme C 可选 `motion_realism` 摘要基线

scheme C 摘要基线当前统一覆盖：

- 终态、tick/time、motion/timeline/trace 计数
- 最终轴位置
- `following_error_sample_count`
- `max_following_error_mm`
- `mean_following_error_mm`
