# regression-baselines

`tests/baselines/` 是工作区级 golden / regression baseline 的 canonical root。

## 当前 inventory

- `rect_diag.simulation-baseline.json`：compat simulated-line 摘要基线；legacy compat executable 缺失时也作为 canonical summary fixture
- `rect_diag.scheme_c.recording-baseline.json`：scheme C 最小闭环摘要基线
- `sample_trajectory.scheme_c.recording-baseline.json`：scheme C 线段 + 圆弧混合路径摘要基线
- `invalid_empty_segments.scheme_c.recording-baseline.json`：scheme C 结构化失败摘要基线
- `following_error_quantized.scheme_c.recording-baseline.json`：scheme C 跟随误差摘要基线
- `preview/rect_diag.preview-snapshot-baseline.json`：real preview snapshot 几何基线
- `performance/dxf-preview-profile-thresholds.json`：`nightly-performance` 的正式 threshold config baseline
- `baseline-manifest.json`：baseline provenance、reviewer workflow 与 stale policy 真值
- `baseline-manifest.schema.json`：manifest schema 真值

## Governance Rules

- baseline 只能从 `samples/` 或稳定协议 fixture 回链，禁止从 `tests/reports/` 反写。
- 任何 baseline refresh 都必须与 `baseline-manifest.json` 同改，并更新 `review_by`。
- `data/baselines/**` 已降级为 deprecated redirect，不得再保留可比对资产文件。
- manifest 中 `deprecated_source_paths` 只用于迁移审计，不代表允许双写。

## Reviewer Workflow

- 先运行 owning regression producer，生成新的 evidence。
- 解释语义差异，确认是预期变更而不是误回归。
- 同一变更内更新 baseline 文件和 `baseline-manifest.json`。
- 至少需要 owner scope + `shared/testing` 双 review 后再合并。

## Scope

- 本目录只保存 golden / regression baselines。
- 最近一次运行输出、临时调试快照、人工导出报告必须留在 `tests/reports/`。
