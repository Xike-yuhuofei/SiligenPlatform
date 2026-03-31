# tests/performance

正式 performance 验证承载面。

当前 canonical 子域：`tests/performance/`

## DXF 预览画像脚本

- `tests/performance/collect_dxf_preview_profiles.py`
- 默认采集三档 DXF：`small=rect_diag.dxf`、`medium=bra.dxf`、`large=Demo.dxf`
- 输出 `JSON + Markdown` 到 `tests/reports/performance/dxf-preview-profiles/`
- 固定输出三张表：
  - `Preview`：authority 侧 `artifact.create -> plan.prepare -> preview.snapshot`
  - `Execution`：开启 `--include-start-job` 后的 `preview.confirm -> dxf.job.start -> dxf.job.status -> dxf.job.stop`
  - `Single Flight`：同一 artifact 并发发起 `plan.prepare`，观测 authority single-flight

### 场景语义

- `cold`
  - 每次重启 gateway 后执行完整采集，观察 `.pb` 冷启动、authority build 和首轮 execution assembly
- `hot`
  - 复用同一 backend 和 artifact，观察 authority cache 以及 execution assembly 热路径
- `singleflight`
  - 同一 artifact 并发发起 `plan.prepare`
  - 单测才是 execution single-flight 的硬证据；这里的 real-runtime 并发结果只作为观测性证据

### `--include-start-job` 启动约定

- 若显式传入 `--launch-spec`，脚本完全尊重该契约，不额外改写启动配置。
- 若未传入 `--launch-spec` 且同时开启 `--include-start-job --dry-run`：
  - 脚本固定使用工作区内 `--gateway-exe/--config-path` 解析出的 gateway，而不是 HMI 外部 launch spec。
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
- 报告环境头
  - `gateway_config_path` / `gateway_hardware_mode` / `gateway_auto_provisioned_mock` 用于说明本次 execution smoke 是否走了自动 mock 配置
- `baseline-json`
  - 只做漂移对比与告警，不默认让脚本失败
  - 超过 `--regression-threshold-pct` 的正向漂移会写入 JSON `baseline_comparison.entries[].status=regression`

## Wave 5 仓库级 performance 回链检查点（US6 / M10-M11）

| Checkpoint | 通过标准 | 证据入口 |
|---|---|---|
| `US6-PERF-CP1-surface-anchored` | performance 验证入口稳定承载于 `tests/performance/`，并纳入 `tests/CMakeLists.txt` 的仓库级验证锚点校验 | `tests/performance/README.md`、`tests/CMakeLists.txt` |
| `US6-PERF-CP2-wave5-gates-aligned` | 仓库级性能验证遵循 Wave 5 门禁口径，入口统一通过根级验证链路触发 | `.\\test.ps1`、`python .\\scripts\\migration\\validate_workspace_layout.py`、`python -m test_kit.workspace_validation`、`validation-gates.md` |
| `US6-PERF-CP3-legacy-source-downgraded` | 不再存在平行的 legacy performance 验证根，仓库级 performance owner 已完全收敛到 `tests/performance/` | `tests/performance/README.md`、`wave-mapping.md`、`dsp-e2e-spec-s10-frozen-directory-index.md` |
