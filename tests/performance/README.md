# tests/performance

正式 performance 验证承载面。

当前 canonical 子域：`tests/performance/`

## DXF 预览画像脚本

- `tests/performance/collect_dxf_preview_profiles.py`
- 默认采集当前仓库已跟踪的 DXF samples：`small=rect_diag.dxf`、`medium=rect_medium_ladder.dxf`、`large=rect_large_ladder.dxf`
- canonical `medium` / `large` 由 `tests/performance/generate_canonical_dxf_samples.py` 生成并固化在 `samples/dxf/`
- 如需临时覆盖某个 sample，仍可显式通过 `--sample <label>=<PATH>` 提供，但正式 `nightly-performance` blocking gate 只认仓库内 canonical samples
- 输出 `JSON + Markdown` 到 `tests/reports/performance/dxf-preview-profiles/`
- `tests/performance/collect_dxf_preview_profiles.py` 同时是 `nightly-performance` 的正式 authority；当显式传 `--gate-mode nightly-performance --threshold-config tests/baselines/performance/dxf-preview-profile-thresholds.json` 时，threshold gate 为 blocking
- 默认 gateway executable 解析顺序与根级 build 保持一致：`<repo-root>\build\bin\*` -> `%LOCALAPPDATA%\SiligenSuite\control-apps-build\bin\*`
- 固定输出三张表：
  - `Preview`：authority 侧 `artifact.create -> plan.prepare -> preview.snapshot`
  - `Execution`：开启 `--include-start-job` 后的 `preview.confirm -> dxf.job.start -> dxf.job.status -> dxf.job.stop`
  - `Single Flight`：同一 artifact 并发发起 `plan.prepare`，观测 authority single-flight
- 并补充 shared evidence bundle：
  - `case-index.json`
  - `validation-evidence-bundle.json`
  - `evidence-links.md`
  - `failure-details.json`（仅失败/阻断等非通过结论）

### 场景语义

- `cold`
  - 每次重启 gateway 后执行完整采集，观察 `.pb` 冷启动、authority build 和首轮 execution assembly
- `hot`
  - 复用同一 backend 和 artifact，观察 authority cache 以及 execution assembly 热路径
- `singleflight`
  - 同一 artifact 并发发起 `plan.prepare`
  - 单测才是 execution single-flight 的硬证据；这里的 real-runtime 并发结果只作为观测性证据
- `control_cycle`
  - 仅在 `--include-start-job --include-control-cycles` 下采集
  - 复用同一 authority / artifact，记录 `pause/resume` 与 `stop/reset/rerun` 的控制环耗时
  - 当前正式摘要字段固定为 `pause_to_running_ms`、`stop_to_idle_ms`、`rerun_total_ms`、`execution_total_ms`、`working_set_mb`、`private_memory_mb`、`handle_count`、`timeout_count`
- `long_run`
  - 在同一 backend 和 artifact 上持续复跑到 `--long-run-minutes` 指定时长
  - 先执行一次 `plan.prepare -> preview.snapshot` 建立 preview setup，随后在循环内仅执行 `preview.confirm -> dxf.job.start -> dxf.job.status -> dxf.job.stop`
  - 当前正式摘要字段固定为 `execution_total_ms`、`working_set_mb`、`private_memory_mb`、`handle_count`、`thread_count`、`timeout_count`

### `--include-start-job` 启动约定

- 若显式传入 `--launch-spec`，脚本完全尊重该契约，不额外改写启动配置。
- 若未传入 `--launch-spec` 且同时开启 `--include-start-job --dry-run`：
  - 脚本固定使用当前工作区解析出的 `--gateway-exe/--config-path`，优先取 `<repo-root>\build\bin\*`，其次取 `%LOCALAPPDATA%\SiligenSuite\control-apps-build\bin\*`，而不是 HMI 外部 launch spec。
  - 当 `--config-path` 指向的配置仍是 `Hardware.mode=Real` 时，脚本会在 `tests/reports/performance/dxf-preview-profiles/_runtime/` 下自动生成临时 mock 配置，并以该配置启动 gateway。
  - gateway 启动后，脚本会在采样前执行一次 mock `connect -> home.auto`，确保 `dxf.job.start` 的 dry-run/mock 路径具备可复跑前置条件。
- 上述 bootstrap 只负责把 mock runtime 拉到可执行状态；其耗时不计入 `Execution` 表。

### 常用跑法

- `small` smoke
  ```powershell
  python tests/performance/collect_dxf_preview_profiles.py `
    --sample-labels small `
    --cold-iterations 1 `
    --hot-warmup-iterations 1 `
    --hot-iterations 1 `
    --singleflight-rounds 1 `
    --singleflight-fanout 3
  ```
- `nightly-performance` threshold gate
  ```powershell
  python tests/performance/collect_dxf_preview_profiles.py `
    --sample-labels small medium large `
    --cold-iterations 1 `
    --hot-warmup-iterations 1 `
    --hot-iterations 2 `
    --singleflight-rounds 2 `
    --singleflight-fanout 4 `
    --include-start-job `
    --include-control-cycles `
    --pause-resume-cycles 3 `
    --stop-reset-rounds 3 `
    --long-run-minutes 5 `
    --gate-mode nightly-performance `
    --threshold-config tests/baselines/performance/dxf-preview-profile-thresholds.json
  ```
- `small` + execution smoke
  ```powershell
  python tests/performance/collect_dxf_preview_profiles.py `
    --sample-labels small `
    --cold-iterations 1 `
    --hot-warmup-iterations 1 `
    --hot-iterations 1 `
    --singleflight-rounds 1 `
    --singleflight-fanout 3 `
    --include-start-job
  ```
- `large` single-flight 观测
  ```powershell
  python tests/performance/collect_dxf_preview_profiles.py `
    --sample-labels large `
    --cold-iterations 0 `
    --hot-iterations 0 `
    --singleflight-rounds 1 `
    --singleflight-fanout 8
  ```
- 基线对比
  ```powershell
  python tests/performance/collect_dxf_preview_profiles.py `
    --sample-labels small medium large `
    --include-start-job `
    --baseline-json tests/reports/performance/dxf-preview-profiles/latest.json `
    --regression-threshold-pct 10
  ```

### 报告路径

- 最新报告固定写入：
  - `tests/reports/performance/dxf-preview-profiles/latest.json`
  - `tests/reports/performance/dxf-preview-profiles/latest.md`
- 每次运行还会额外写入时间戳目录：
  - `tests/reports/performance/dxf-preview-profiles/<UTC_TIMESTAMP>/report.json`
  - `tests/reports/performance/dxf-preview-profiles/<UTC_TIMESTAMP>/report.md`

### 字段说明

- `dxf.plan.prepare.performance_profile`
  - 只承载 authority 侧耗时和去重字段，不混 execution 字段
- `dxf.job.start.performance_profile`
  - 是 execution 画像的唯一公开出口
  - 固定字段：`execution_cache_hit`、`execution_joined_inflight`、`execution_wait_ms`、`motion_plan_ms`、`assembly_ms`、`export_ms`、`execution_total_ms`
- `control_cycle.summary` / `long_run.summary`
  - 顶层 `count` 固定表示控制环轮次或 long-run 迭代数
  - 顶层 `sample_count` 固定表示资源采样点数量，不再覆盖 `count` 语义
- 报告环境头
  - `gateway_config_path` / `gateway_hardware_mode` / `gateway_auto_provisioned_mock` 用于说明本次 execution smoke 是否走了自动 mock 配置
- `baseline-json`
  - 只做漂移对比与告警，不默认让脚本失败
  - 超过 `--regression-threshold-pct` 的正向漂移会写入 JSON `baseline_comparison.entries[].status=regression`
- `threshold-config`
  - canonical 路径固定为 `tests/baselines/performance/dxf-preview-profile-thresholds.json`
  - 当前正式 blocking 样本为 `small`、`medium`、`large`
  - 当前冻结场景为 `cold`、`hot`、`singleflight`、`control_cycle`
  - `small.long_run` 已纳入正式阈值冻结面，用于守住长稳资源趋势与 timeout 漂移
  - 当前正式 gate 需要 `--include-start-job`，因为 threshold config 已包含 execution thresholds
  - `nightly-performance` 正式入口还要求 `--include-control-cycles --pause-resume-cycles 3 --stop-reset-rounds 3 --long-run-minutes 5`
  - 只有 `threshold_gate` 会在 `nightly-performance` 下成为 blocking 判定
  - `threshold_gate` 结果会同时写入 `report.json/.md`、`validation-evidence-bundle.json`、`report-manifest.json` 与 `report-index.json`

## Phase 12 Calibration Evidence

- 正式校准批次：`tests/reports/performance/dxf-preview-profiles/20260401T153350Z/`
- 当前校准结果：
  - `small.cold.artifact_ms.p95_ms = 443.282`
  - `small.cold.prepare_total_ms.p95_ms = 33.9`
  - `small.hot.prepare_total_ms.p95_ms = 16.85`
  - `small.hot.execution_total_ms.p95_ms = 312.0`
  - `small.singleflight.prepare_total_ms.p95_ms = 31.25`
- 当前结论：`threshold_gate=passed`

## Phase 13 Multi-Sample Evidence

- 正式多样本 gate：`tests/reports/performance/dxf-preview-profiles/20260402T023650Z/`
- 当前多样本结果：
  - `small.cold.artifact_ms.p95_ms = 454.733`
  - `small.cold.prepare_total_ms.p95_ms = 35.0`
  - `small.hot.prepare_total_ms.p95_ms = 20.8`
  - `small.hot.execution_total_ms.p95_ms = 330.0`
  - `small.singleflight.prepare_total_ms.p95_ms = 34.3`
  - `medium.cold.artifact_ms.p95_ms = 454.877`
  - `medium.cold.prepare_total_ms.p95_ms = 119.0`
  - `medium.hot.prepare_total_ms.p95_ms = 21.8`
  - `medium.hot.execution_total_ms.p95_ms = 2245.0`
  - `medium.singleflight.prepare_total_ms.p95_ms = 120.95`
  - `large.cold.artifact_ms.p95_ms = 541.127`
  - `large.cold.prepare_total_ms.p95_ms = 1522.0`
  - `large.hot.prepare_total_ms.p95_ms = 20.9`
  - `large.hot.execution_total_ms.p95_ms = 27917.0`
  - `large.singleflight.prepare_total_ms.p95_ms = 1505.6`
- 当前结论：`threshold_gate=passed`

## Phase 14 Long-Run Memory Repair Evidence

- 正式修复回归批次：`tests/reports/performance/dxf-preview-profiles/20260403T083352Z/`
- 当前多样本 `long_run` 结果：
  - `small.long_run.working_set_mb.delta_max = 11.461`
  - `small.long_run.private_memory_mb.delta_max = 10.047`
  - `medium.long_run.working_set_mb.delta_max = 19.855`
  - `medium.long_run.private_memory_mb.delta_max = 18.406`
  - `large.long_run.working_set_mb.delta_max = 66.429`
  - `large.long_run.private_memory_mb.delta_max = 65.832`
- 当前结论：`threshold_gate=passed`

## Wave 5 仓库级 performance 回链检查点（US6 / M10-M11）

| Checkpoint | 通过标准 | 证据入口 |
|---|---|---|
| `US6-PERF-CP1-surface-anchored` | performance 验证入口稳定承载于 `tests/performance/`，并纳入 `tests/CMakeLists.txt` 的仓库级验证锚点校验 | `tests/performance/README.md`、`tests/CMakeLists.txt` |
| `US6-PERF-CP2-wave5-gates-aligned` | 仓库级性能验证遵循 Wave 5 门禁口径，入口统一通过根级验证链路触发 | `.\\test.ps1 -Lane nightly-performance -Suite performance`、`python .\\scripts\\migration\\validate_workspace_layout.py`、`scripts\\validation\\invoke-workspace-tests.ps1`、`validation-gates.md` |
| `US6-PERF-CP3-legacy-source-downgraded` | 不再存在平行的 legacy performance 验证根，仓库级 performance owner 已完全收敛到 `tests/performance/` | `tests/performance/README.md`、`wave-mapping.md`、`dsp-e2e-spec-s10-frozen-directory-index.md` |

## Closeout Notes

- `baseline-json` 仍然保留，但只表达 advisory drift compare。
- `nightly-performance` 的正式 blocking 语义已经切到 `threshold_gate`，不能再把 `baseline-json` 当作唯一 gate。
