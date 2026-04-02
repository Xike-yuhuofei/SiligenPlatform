# Phase 12 Layered Test Activation Closeout

更新时间：`2026-04-02`

更新说明：本稿记录的是 Phase 12 收口状态；其中“多样本 nightly-performance”与“signed controlled HIL authority”已在 `docs/validation/phase13-multisample-performance-and-signed-hil-closeout-20260402.md` 中继续收口。

## 1. 目标

把 `TASK-006` 已完成的分层测试平台投入正式运行：

- 冻结 `nightly-performance` 的 canonical threshold config
- 执行一次正式 `nightly-performance` blocking gate
- 执行一次 `limited-hil` 60s quick gate，并在阻塞时保留完整 blocked evidence
- 当 quick gate 放行后，执行正式 `limited-hil` `1800s` formal gate

## 2. 本轮变更

- `tests/performance/collect_dxf_preview_profiles.py`
  - 默认 gateway executable 解析顺序对齐根级 build reality：`<repo-root>\build\bin\*` -> `%LOCALAPPDATA%\SiligenSuite\control-apps-build\bin\*`
- `tests/baselines/performance/dxf-preview-profile-thresholds.json`
  - 从 smoke 级 `30000 ms` 阈值收紧为基于 `small` sample 的正式校准阈值
  - 正式 gate 现在要求 `--include-start-job`
- `tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1`
  - offline prerequisites 直接绑定 `-Suite @("contracts","e2e","protocol-compatibility")`
  - offline prerequisites 已通过后，即使 HIL 步骤阻塞，也继续发布 `hil-controlled-gate-summary.*` 与 `hil-controlled-release-summary.md`

## 3. 验证结果

### 3.1 Nightly Performance

命令：

```powershell
python .\tests\performance\collect_dxf_preview_profiles.py `
  --sample-labels small `
  --cold-iterations 2 `
  --hot-warmup-iterations 1 `
  --hot-iterations 2 `
  --singleflight-rounds 2 `
  --singleflight-fanout 3 `
  --include-start-job `
  --gate-mode nightly-performance `
  --threshold-config .\tests\baselines\performance\dxf-preview-profile-thresholds.json
```

结果：`passed`

关键证据：

- `tests/reports/performance/dxf-preview-profiles/20260401T153350Z/report.json`
- `tests/reports/performance/dxf-preview-profiles/20260401T153350Z/validation-evidence-bundle.json`
- `tests/reports/performance/dxf-preview-profiles/latest.json`

冻结阈值对应的本轮观测：

- `small.cold.artifact_ms.p95_ms = 443.282`
- `small.cold.prepare_total_ms.p95_ms = 33.9`
- `small.hot.prepare_total_ms.p95_ms = 16.85`
- `small.hot.execution_total_ms.p95_ms = 312.0`
- `small.singleflight.prepare_total_ms.p95_ms = 31.25`

结论：

- `nightly-performance` 已具备正式 blocking gate 语义
- 当前 threshold config 只冻结 `small` sample，不包含 `medium/large`

### 3.2 Limited HIL Quick Gate

命令：

```powershell
.\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -SkipBuild `
  -UseTimestampedReportDir `
  -HilDurationSeconds 60 `
  -ReportDir tests\reports\verify\phase12-limited-hil `
  -PublishLatestOnPass:$false
```

结果：`blocked`

关键证据：

- `tests/reports/verify/phase12-limited-hil/20260401T153504Z/offline-prereq/workspace-validation.json`
- `tests/reports/verify/phase12-limited-hil/20260401T153504Z/hardware-smoke/hardware-smoke-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260401T153504Z/hil-closed-loop-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260401T153504Z/hil-case-matrix/case-matrix-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260401T153504Z/hil-controlled-gate-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260401T153504Z/hil-controlled-release-summary.md`

关键事实：

- offline prerequisites：`33 passed / 0 failed / 0 known_failure / 0 skipped`
- hardware smoke：`passed`
- `hil-closed-loop`：`failed`
- `hil-case-matrix`：`failed`
- controlled gate：`failed`
- release summary：`阻塞`

阻塞原因：

- `hil-closed-loop-summary.json` 的 `failure_classification` 为 `validation:assertion_failed`
- 直接失败点为 `tcp-connect`
- 原始错误为：`MC_Open failed with error code: -7`

结论：

- 当前环境已经满足 `limited-hil` 的离线前置层与 evidence governance
- 但真实控制卡连接在 quick gate 阶段阻塞，formal `1800s` gate 本轮不继续执行

### 3.3 Limited HIL Follow-up Quick Gate

背景：

- `2026-04-02` 确认控制卡恢复正常上电后，对 `limited-hil` quick gate 做 follow-up 重试

命令：

```powershell
.\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -SkipBuild `
  -UseTimestampedReportDir `
  -HilDurationSeconds 60 `
  -ReportDir tests\reports\verify\phase12-limited-hil `
  -PublishLatestOnPass:$false
```

结果：`passed`

关键证据：

- `tests/reports/verify/phase12-limited-hil/20260402T005612Z/offline-prereq/workspace-validation.json`
- `tests/reports/verify/phase12-limited-hil/20260402T005612Z/hardware-smoke/hardware-smoke-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260402T005612Z/hil-closed-loop-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260402T005612Z/hil-case-matrix/case-matrix-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260402T005612Z/hil-controlled-gate-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260402T005612Z/hil-controlled-release-summary.md`

关键事实：

- offline prerequisites：`33 passed / 0 failed / 0 known_failure / 0 skipped`
- hardware smoke：`passed`
- `hil-closed-loop`：`passed`
- `hil-case-matrix`：`passed`
- controlled gate：`passed`
- release summary：`通过`

结论：

- 上一轮 `tcp-connect / MC_Open failed with error code: -7` 阻塞未再复现
- `limited-hil` quick gate 已解除阻塞，并满足进入 formal `1800s` gate 的前提

### 3.4 Limited HIL Formal Gate

命令：

```powershell
.\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -SkipBuild `
  -UseTimestampedReportDir `
  -HilDurationSeconds 1800 `
  -ReportDir tests\reports\verify\phase12-limited-hil `
  -PublishLatestOnPass:$false
```

结果：`passed`

关键证据：

- `tests/reports/verify/phase12-limited-hil/20260402T010321Z/offline-prereq/workspace-validation.json`
- `tests/reports/verify/phase12-limited-hil/20260402T010321Z/hardware-smoke/hardware-smoke-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260402T010321Z/hil-closed-loop-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260402T010321Z/hil-case-matrix/case-matrix-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260402T010321Z/hil-controlled-gate-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260402T010321Z/hil-controlled-release-summary.md`

关键事实：

- offline prerequisites：`33 passed / 0 failed / 0 known_failure / 0 skipped`
- hardware smoke：`passed`
- `hil-closed-loop`：`passed`（`iterations=599`）
- `hil-case-matrix`：`20/20 passed`
- controlled gate：`passed`
- release summary：`通过`

结论：

- `limited-hil` formal `1800s` gate 已正式通过
- Phase 12 的 controlled HIL activation 已从“quick gate blocked closeout”推进到“quick + formal gate passed”

### 3.5 Limited HIL Official Archive Publish

背景：

- 为了形成正式发布副本，本轮追加执行一次不带 `-SkipBuild` 的 formal `1800s` controlled HIL
- 本轮保留默认 `PublishLatestOnPass=true`，将通过证据同步到固定目录 `tests/reports/hil-controlled-test`

命令：

```powershell
.\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
  -Profile Local `
  -UseTimestampedReportDir `
  -HilDurationSeconds 1800 `
  -ReportDir tests\reports\verify\phase12-limited-hil `
  -PublishLatestReportDir tests\reports\hil-controlled-test
```

结果：`passed`

关键证据：

- `tests/reports/verify/phase12-limited-hil/20260402T013939Z/offline-prereq/workspace-validation.json`
- `tests/reports/verify/phase12-limited-hil/20260402T013939Z/hardware-smoke/hardware-smoke-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260402T013939Z/hil-closed-loop-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260402T013939Z/hil-case-matrix/case-matrix-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260402T013939Z/hil-controlled-gate-summary.json`
- `tests/reports/verify/phase12-limited-hil/20260402T013939Z/hil-controlled-release-summary.md`
- `tests/reports/hil-controlled-test/latest-source.txt`

关键事实：

- build apps：`passed`
- offline prerequisites：`33 passed / 0 failed / 0 known_failure / 0 skipped`
- hardware smoke：`passed`
- `hil-closed-loop`：`passed`
- `hil-case-matrix`：`20/20 passed`
- controlled gate：`passed`
- 固定目录 `tests/reports/hil-controlled-test/` 已与本次时间戳目录对齐发布

结论：

- Phase 12 的 controlled HIL 已具备“时间戳证据 + fixed latest publish”双轨归档
- 后续 release / field review 可以直接消费 `tests/reports/hil-controlled-test/` 作为当前 latest authority 副本

## 4. 最小回归

- `python .\tests\contracts\test_performance_threshold_gate_contract.py`
- `python .\tests\contracts\test_validation_evidence_bundle_contract.py`
- `python .\tests\contracts\test_layered_validation_lane_policy_contract.py`
- `python .\tests\performance\collect_dxf_preview_profiles.py ... --include-start-job --gate-mode nightly-performance ...`
- `.\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 -Profile Local -SkipBuild -HilDurationSeconds 60 ...`
- `.\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 -Profile Local -SkipBuild -HilDurationSeconds 1800 ...`
- `.\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 -Profile Local -HilDurationSeconds 1800 -PublishLatestReportDir tests\reports\hil-controlled-test`

## 5. 残余风险

- `nightly-performance` 当前仅冻结 `small` sample；`medium/large` 仍未进入正式 blocking config
- `limited-hil` release summary 中执行人仍为 `待填写`，需要现场执行人补签

## 6. 最终判断

- `TASK-006` 的平台建设边界维持完成
- Phase 12 的 `nightly-performance` 激活已完成
- Phase 12 的 `limited-hil` quick gate 与 formal gate 已完成并通过
- 当前只剩现场执行人补签与后续更大样本/更长周期验证，不需要回到平台建设流
