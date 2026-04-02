# Phase 13 Multisample Performance And Signed HIL Closeout

更新时间：`2026-04-02`

## 1. 目标

在 Phase 12 已完成正式激活的基础上，继续收口两项剩余缺口：

- 把 `nightly-performance` 从仅覆盖 `small` 提升为覆盖 repo-local `small/medium/large` canonical DXF 的正式 blocking gate
- 把 controlled HIL fixed latest publish 从 unsigned authority 收紧为必须带执行人签名的正式证据

## 2. 本轮变更

- `samples/dxf/`
  - 新增 `rect_medium_ladder.dxf`
  - 新增 `rect_large_ladder.dxf`
  - 新增生成脚本 `tests/performance/generate_canonical_dxf_samples.py`
- `shared/testing/test-kit/src/test_kit/asset_catalog.py`
  - `default_performance_samples()` 扩展为 `small/medium/large`
  - 新增 `sample.dxf.rect_medium_ladder`、`sample.dxf.rect_large_ladder`
  - 新增 `baseline.performance.dxf_preview_thresholds`
- `tests/baselines/performance/dxf-preview-profile-thresholds.json`
  - `required_samples` 扩展为 `small`、`medium`、`large`
  - 正式阈值扩展到三档样本的 `cold/hot/singleflight`
- `tests/performance/collect_dxf_preview_profiles.py`
  - evidence bundle asset refs 不再只特判 `rect_diag`，改为按 canonical sample registry 回链
- `tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1`
  - 当 `PublishLatestOnPass=true` 时强制要求非空 `-Executor`
  - 参数快照现在会明确输出 `executor`

## 3. 验证结果

### 3.1 Nightly Performance Multi-Sample Gate

正式命令：

```powershell
python .\tests\performance\collect_dxf_preview_profiles.py `
  --sample-labels small medium large `
  --cold-iterations 1 `
  --hot-warmup-iterations 1 `
  --hot-iterations 2 `
  --singleflight-rounds 2 `
  --singleflight-fanout 4 `
  --include-start-job `
  --gate-mode nightly-performance `
  --threshold-config tests/baselines/performance/dxf-preview-profile-thresholds.json
```

结果：`passed`

正式证据：

- `tests/reports/performance/dxf-preview-profiles/20260402T023650Z/report.json`
- `tests/reports/performance/dxf-preview-profiles/20260402T023650Z/validation-evidence-bundle.json`
- `tests/reports/performance/dxf-preview-profiles/20260402T023650Z/report-manifest.json`
- `tests/reports/performance/dxf-preview-profiles/20260402T023650Z/report-index.json`
- `tests/reports/performance/dxf-preview-profiles/latest.json`

关键指标：

- `small.cold.artifact_ms.p95_ms = 454.733`
- `small.hot.execution_total_ms.p95_ms = 330.0`
- `medium.cold.prepare_total_ms.p95_ms = 119.0`
- `medium.hot.execution_total_ms.p95_ms = 2245.0`
- `large.cold.prepare_total_ms.p95_ms = 1522.0`
- `large.hot.execution_total_ms.p95_ms = 27917.0`
- `large.singleflight.prepare_total_ms.p95_ms = 1505.6`

结论：

- `nightly-performance` 已不再停留在 `small` single-sample gate
- repo-local `small/medium/large` canonical DXF 已进入正式 blocking config

### 3.2 Signed Controlled HIL Latest Authority

正式命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -UseTimestampedReportDir `
  -HilDurationSeconds 1800 `
  -Executor "Codex + Xike" `
  -ReportDir tests\reports\verify\phase13-limited-hil `
  -PublishLatestReportDir tests\reports\hil-controlled-test
```

结果：`passed`

正式证据：

- `tests/reports/verify/phase13-limited-hil/20260402T023734Z/offline-prereq/workspace-validation.json`
- `tests/reports/verify/phase13-limited-hil/20260402T023734Z/hardware-smoke/hardware-smoke-summary.json`
- `tests/reports/verify/phase13-limited-hil/20260402T023734Z/hil-closed-loop-summary.json`
- `tests/reports/verify/phase13-limited-hil/20260402T023734Z/hil-case-matrix/case-matrix-summary.json`
- `tests/reports/verify/phase13-limited-hil/20260402T023734Z/hil-controlled-gate-summary.json`
- `tests/reports/verify/phase13-limited-hil/20260402T023734Z/hil-controlled-release-summary.md`
- `tests/reports/hil-controlled-test/latest-source.txt`

关键事实：

- `offline prerequisites = 33 passed / 0 failed / 0 known_failure / 0 skipped`
- `hardware smoke = passed`
- `hil-closed-loop = passed`（`iterations=587`）
- `hil-case-matrix = 20/20 passed`
- `controlled gate = passed`
- `hil-controlled-release-summary.md` 已记录 `执行人=Codex + Xike`
- `latest-source.txt` 已指向 `20260402T023734Z`

结论：

- fixed latest authority 已从 Phase 12 unsigned publish 切换到 Phase 13 signed publish
- 后续未带 `-Executor` 的诊断回合不得再覆盖 `tests/reports/hil-controlled-test/latest-source.txt`

## 4. 最小回归

- `python .\tests\contracts\test_shared_test_assets_contract.py`
- `python .\tests\contracts\test_performance_threshold_gate_contract.py`
- `python .\tests\integration\scenarios\run_baseline_governance_smoke.py`
- `python .\tests\integration\scenarios\run_shared_asset_reuse_smoke.py`
- `python .\tests\performance\collect_dxf_preview_profiles.py ... --sample-labels small medium large ...`
- `.\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 -Profile Local -HilDurationSeconds 1800 -Executor "Codex + Xike" ...`

## 5. 残余风险

- `small.hot.prepare_total_ms` 在三轮 calibration 中存在低毫秒级抖动，当前已通过较宽松阈值吸收，但后续若要继续收紧阈值，应先提高 hot iterations 再重校准
- 当前 controlled HIL 仍是 `1800s` formal gate，不等同于 `60 min+` soak
- 本轮没有扩展 `disconnect recovery`、`HMI runtime action matrix` 或更长周期真机回归

## 6. 最终判断

- Phase 13 已完成 `nightly-performance` 多样本正式化
- Phase 13 已完成 signed controlled HIL latest authority 收口
- 分层测试平台当前剩余工作已从“正式通道缺口”转为“更长周期 / 更深场景”的扩面问题
