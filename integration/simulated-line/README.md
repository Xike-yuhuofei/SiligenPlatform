# simulated-line

这里放 `packages/simulation-engine` 驱动的仿真联动回归。

当前入口：

- `run_simulated_line.py`
- `run_controlled_production_test.ps1`

它当前验证以下回归链：

- compatibility engine：`simulate_dxf_path.exe`
  仅保留 `rect_diag.simulation-baseline.json` 历史对照
- scheme C minimal closed-loop：`simulate_scheme_c_closed_loop.exe`
  对齐以下 scheme C 基线：
  - `rect_diag.scheme_c.recording-baseline.json`
  - `sample_trajectory.scheme_c.recording-baseline.json`
  - `invalid_empty_segments.scheme_c.recording-baseline.json`
  - `following_error_quantized.scheme_c.recording-baseline.json`

scheme C 每个场景都会执行两次并比较输出 JSON，确保 repeated run deterministic。

其中 `following_error_quantized` 会额外校验：

- `summary.following_error_sample_count`
- `summary.max_following_error_mm`
- `summary.mean_following_error_mm`

默认输入优先复用根级 `examples/simulation/*.json`，不依赖 runtime-host 或 `apps/*`。

`run_simulated_line.py` 支持通过 `--report-dir <dir>` 同步落盘：

- `simulated-line-summary.json`
- `simulated-line-summary.md`

`run_controlled_production_test.ps1` 会串行执行 root `simulation` build/test，并把 `workspace-validation.*` 与 `simulated-line-summary.*` 一起输出到统一报告目录。
