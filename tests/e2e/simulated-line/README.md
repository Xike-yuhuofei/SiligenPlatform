# simulated-line

这里放 canonical 仿真执行链的联动回归。

更新时间：`2026-04-13`

当前入口：

- `run_simulated_line.py`
- `run_simulated_line_matrix.py`
- `run_controlled_production_test.ps1`
- `run_controlled_production_test.py`
- `verify_controlled_production_test.py`

它当前验证以下回归链：

- compat historical summary：
  优先消费 `simulate_dxf_path.exe`；若 legacy executable 缺失，则回退到
  `rect_diag.simulation-baseline.json` 的 canonical summary fixture，
  仅保留历史兼容回归，不再阻断于已移除的 legacy binary。
- scheme C canonical replay / summary：
  优先消费 `simulate_scheme_c_closed_loop.exe`；若 legacy executable 缺失，则回退到：
  - `samples/replay-data/rect_diag.scheme_c.recording.json`
  - `samples/replay-data/sample_trajectory.scheme_c.recording.json`
  - `invalid_empty_segments.scheme_c.recording-baseline.json`
  - `following_error_quantized.scheme_c.recording-baseline.json`

scheme C 每个场景都会执行两次并比较输出 JSON，确保 repeated run deterministic。

其中 `following_error_quantized` 会额外校验：

- `summary.following_error_sample_count`
- `summary.max_following_error_mm`
- `summary.mean_following_error_mm`

默认输入优先复用根级 `samples/simulation/*.json`，不依赖额外 bridge 根或已删除 legacy 宿主。
共享样本、baseline 与 fault scenario 必须回链到：

- `samples/simulation/`
- `tests/baselines/`
- `shared/testing/`

Phase 9 起，simulated-line fault/injector authority 固定在 `shared/testing/test-kit/src/test_kit/fault_injection.py`。
默认 matrix id 为 `fault-matrix.simulated-line.v1`，双替身 surface 固定为：

- `fake_controller`: `readiness`、`abort`、`preflight`、`control_cycle`
- `fake_io`: `disconnect`、`alarm`

DXF truth matrix 口径：

- compat full-chain case 统一来自 `shared/contracts/engineering/fixtures/dxf-truth-matrix.json`
- 当前 full-chain canonical producer case 为 `rect_diag`、`bra`、`arc_circle_quadrants`
- `run_simulated_line.py --mode compat` 在未显式筛选时，会枚举全部 full-chain case，而不再只跑 `rect_diag`
- `run_simulated_line.py --mode both` 也会先跑上述 compat case，再补 scheme C regression
- `run_simulated_line_matrix.py` 仍显式钉住 `rect_diag`，用于冻结 fault matrix / control-cycle authority；它不是 full-chain case sweep 入口

`run_simulated_line.py` 现在支持：

- `--compat-case-id <case>` 可重复，用于只运行指定 full-chain compat case
- `--fault-id <fault_id>` 可重复，用于只运行指定 fault 场景
- `--seed <int>`
- `--clock-profile <profile>`
- `--hook-readiness ready|not-ready`
- `--hook-abort none|before-run|after-first-pass`
- `--hook-disconnect none|after-first-pass`
- `--hook-preflight none|fail-after-preview-ready`
- `--hook-alarm none|during-execution`
- `--hook-control-cycle none|pause-resume-once|stop-reset-rerun`

`run_controlled_production_test.py` 会把这些选项透传为环境变量：

- `SILIGEN_SIMULATED_LINE_FAULT_IDS`
- `SILIGEN_SIMULATED_LINE_SEED`
- `SILIGEN_SIMULATED_LINE_CLOCK_PROFILE`
- `SILIGEN_SIMULATED_LINE_READINESS`
- `SILIGEN_SIMULATED_LINE_ABORT`
- `SILIGEN_SIMULATED_LINE_DISCONNECT`
- `SILIGEN_SIMULATED_LINE_PREFLIGHT`
- `SILIGEN_SIMULATED_LINE_ALARM`
- `SILIGEN_SIMULATED_LINE_CONTROL_CYCLE`

`run_simulated_line.py` 支持通过 `--report-dir <dir>` 同步落盘：

- `simulated-line-summary.json`
- `simulated-line-summary.md`
- `case-index.json`
- `validation-evidence-bundle.json`
- `evidence-links.md`
- `failure-details.json`（失败、阻断、已知失败、跳过或延后时）

truth matrix 示例命令：

```powershell
python .\tests\e2e\simulated-line\run_simulated_line.py --mode compat
```

```powershell
python .\tests\e2e\simulated-line\run_simulated_line.py --mode compat --compat-case-id bra
```

```powershell
python .\tests\e2e\simulated-line\run_simulated_line.py --mode both --compat-case-id arc_circle_quadrants
```

`run_simulated_line_matrix.py` 会复用 `run_simulated_line.py` 固化异常矩阵，当前覆盖：

- `readiness-not-ready`
- `abort-before-run`
- `fault-invalid-empty-segments`
- `fault-following-error-quantized`
- `preview-ready-but-preflight-failed`
- `alarm-during-execution`
- `abort-after-first-pass`
- `pause-resume-cycle`
- `stop-reset-cycle`
- `disconnect-after-first-pass`

它同样会产出：

- `simulated-line-matrix-summary.json`
- `simulated-line-matrix-summary.md`
- `case-index.json`
- `validation-evidence-bundle.json`
- `evidence-links.md`

每条 case record 还必须带上：

- `fault_ids`
- `deterministic_replay.seed`
- `deterministic_replay.clock_profile`
- `deterministic_replay.repeat_count`
- `deterministic_replay.passed`

bundle metadata 必须带上：

- `fault_matrix_id`
- `selected_fault_ids`
- `deterministic_seed`
- `clock_profile`
- `double_surface`
- `hooks`
- `hook_preflight`
- `hook_alarm`
- `hook_control_cycle`
- `lifecycle_transitions`

`run_controlled_production_test.ps1` 会串行执行 root `e2e` build/test，并把 `workspace-validation.*` 与 `simulated-line-summary.*` 一起输出到统一报告目录。
