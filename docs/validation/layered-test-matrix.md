# Layered Test Matrix

更新时间：`2026-04-02`

## 1. 目标

冻结仓库分层测试体系的 canonical layer、execution lane、升级规则、shared asset roots 和 closeout evidence 出口。

## 2. Layer Matrix

| Layer | 目标 | 默认承载面 | 默认 lane | 升级规则 |
|---|---|---|---|---|
| `L0-structure-gate` | 校验布局、根级入口、冻结门禁 | `scripts/migration/`、`scripts/validation/` | `quick-gate` | 任意非平凡改动必经 |
| `L1-module-contract` | 校验 owner 输入、输出、错误分类和状态契约 | `tests/contracts/`、module `*/tests` | `quick-gate` | 影响单模块语义输出时必须进入 |
| `L2-offline-integration` | 校验跨模块链路、协议和共享资产复用 | `tests/integration/`、`tests/integration/scenarios/` | `quick-gate` / `full-offline-gate` | 影响协议、执行准备、两类以上 owner 时必须进入 |
| `L3-simulated-e2e` | 校验 simulated-line 主场景与故障恢复 | `tests/e2e/simulated-line/` | `full-offline-gate` | 影响 success/block/rollback/recovery/archive 主链时升级 |
| `L4-performance` | 校验 preview/execution/single-flight 阈值与漂移 | `tests/performance/` | `nightly-performance` | 影响耗时、缓存、大样本路径时升级 |
| `L5-limited-hil` | 校验模拟无法充分证明的真实硬件风险 | `tests/e2e/hardware-in-loop/` | `limited-hil` | 只有离线层放行且 admission/preflight 满足后才能进入 |
| `L6-closeout` | 汇总已执行层、残余风险与文档影响 | `tests/reports/`、closeout 文档 | closeout | 只聚合证据，不单独给出通过结论 |

## 3. Execution Lanes

| Lane | 默认入口 | 允许层 | gate decision | fail policy | timeout / retry / fail-fast |
|---|---|---|---|---|---|
| `quick-gate` | `build.ps1`、`test.ps1`、`run-local-validation-gate.ps1` | `L0-L2` | `blocking` | `fail-fast` | `900s / 0 / 1` |
| `full-offline-gate` | `test.ps1`、`ci.ps1` | `L0-L4` | `blocking` | `collect-and-report` | `2700s / 1 / 0` |
| `nightly-performance` | `ci.ps1`、`tests/performance/collect_dxf_preview_profiles.py` | `L0,L2-L4` | `blocking` | `collect-and-report` | `3600s / 1 / 0` |
| `limited-hil` | `tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1` | `L0,L2-L3,L5-L6` | `blocking` | `manual-signoff-required` | `1800s / 0 / 1` |

说明：

- 根级 `test.ps1 -IncludeHardwareSmoke/-IncludeHil*` 与 `ci.ps1 -IncludeHilCaseMatrix` 只保留为 root opt-in surface，不作为正式 controlled HIL release path。
- `nightly-performance` 正式 authority 是 `tests/performance/collect_dxf_preview_profiles.py --include-start-job --gate-mode nightly-performance --threshold-config tests/baselines/performance/dxf-preview-profile-thresholds.json`。
- `nightly-performance` 正式 blocking 样本固定为 `small=rect_diag.dxf`、`medium=rect_medium_ladder.dxf`、`large=rect_large_ladder.dxf`。
- `nightly-performance` 默认按 `<repo-root>\build\bin\*` -> `%LOCALAPPDATA%\SiligenSuite\control-apps-build\bin\*` 解析 gateway executable。

## 3.1 Suite / Label Taxonomy

| Suite | 默认层 | size label | canonical labels |
|---|---|---|---|
| `apps` | `L0-structure-gate` | `small` | `suite:apps`, `kind:entrypoint`, `size:small` |
| `contracts` | `L0-L1` | `small` | `suite:contracts`, `kind:contract`, `size:small` |
| `protocol-compatibility` | `L2-offline-integration` | `medium` | `suite:protocol-compatibility`, `kind:integration`, `size:medium` |
| `e2e` | `L3-simulated-e2e` | `large` | `suite:e2e`, `kind:e2e`, `size:large` |
| `performance` | `L4-performance` | `large` | `suite:performance`, `kind:performance`, `size:large` |

## 4. Routing Rules

- `hardware-sensitive` 请求不能止步于 `quick-gate`。
- 任意显式 `skip layer` 都必须同时记录 `skip justification`。
- `nightly-performance` 缺少 threshold config、schema 不兼容或 required samples/scenarios 不满足时必须 hard-fail。
- 当 threshold config 包含 execution thresholds 时，`nightly-performance` 缺少 `--include-start-job` 产生的 execution evidence 也必须 hard-fail。
- `limited-hil` 只消费已冻结的 offline/HIL artifacts，不重解释设备语义。
- `limited-hil` evidence bundle 必须带 `schema_version=validation-evidence-bundle.v2`、`report-manifest.json`、`report-index.json`、`metadata.admission`、`safety_preflight_passed` 与 override reason 规则。
- `limited-hil` 若要发布 fixed latest authority，必须由 `run_hil_controlled_test.ps1 -Executor "<operator>"` 生成 signed `hil-controlled-release-summary.md`，否则不得覆盖 `tests/reports/hil-controlled-test/latest-source.txt`。
- `limited-hil` 在 offline prerequisites 已通过后，即使 HIL 步骤阻塞，也必须继续发布 `hil-controlled-gate-summary.*` 与 `hil-controlled-release-summary.md` 作为 blocked authority。
- `tests/reports/` 只保存 evidence，不得反写为 baseline。

## 5. Shared Assets

- DXF canonical samples：`samples/dxf/rect_diag.dxf`、`samples/dxf/rect_medium_ladder.dxf`、`samples/dxf/rect_large_ladder.dxf`
- simulated-line canonical inputs：`samples/simulation/*.json`
- canonical replay exports：`samples/replay-data/*.scheme_c.recording.json`
- simulated-line baselines：`tests/baselines/*.simulation-baseline.json`、`*.scheme_c.recording-baseline.json`
- preview snapshot baseline：`tests/baselines/preview/rect_diag.preview-snapshot-baseline.json`
- performance threshold config：`tests/baselines/performance/dxf-preview-profile-thresholds.json`
  - 当前冻结范围：`small`、`medium`、`large` samples，`cold/hot/singleflight`，`include-start-job=true`
- baseline governance manifest：`tests/baselines/baseline-manifest.json`
- shared fixture root：`shared/testing/`

## 5.1 Deprecated Roots

- `data/baselines/simulation/`：已弃用，redirect 到 `tests/baselines/`
- `data/baselines/engineering/`：已弃用，redirect 到 `shared/contracts/engineering/fixtures/cases/rect_diag/`
- duplicate-source guard：若上述目录重新出现资产文件，`validate_workspace_layout.py` 与 shared asset contract 必须阻断

## 6. Evidence Layout

- workspace validation：`tests/reports/**/workspace-validation.json|md`
- shared case index：`tests/reports/**/case-index.json`
- shared bundle：`tests/reports/**/validation-evidence-bundle.json`
- report manifest / index：`tests/reports/**/report-manifest.json`、`tests/reports/**/report-index.json`
- shared links：`tests/reports/**/evidence-links.md`
- failure details：`tests/reports/**/failure-details.json`
- performance authority：`tests/reports/performance/dxf-preview-profiles/latest.json|md` 与 `<timestamp>/validation-evidence-bundle.json`
- controlled HIL authority：
  - `tests/reports/hil-controlled-test/<timestamp>/offline-prereq/workspace-validation.json|md`
  - `tests/reports/hil-controlled-test/<timestamp>/hardware-smoke/hardware-smoke-summary.json|md`
  - `tests/reports/hil-controlled-test/<timestamp>/hil-closed-loop-summary.json|md`
  - `tests/reports/hil-controlled-test/<timestamp>/hil-controlled-gate-summary.json|md`
  - `tests/reports/hil-controlled-test/<timestamp>/hil-controlled-release-summary.md`
  - optional `tests/reports/hil-controlled-test/<timestamp>/hil-case-matrix/case-matrix-summary.json|md`
  - `tests/reports/hil-controlled-test/latest-source.txt`

## 7. Verification Evidence

- 合同回归：
  - `tests/contracts/test_layered_validation_contract.py`
  - `tests/contracts/test_shared_test_assets_contract.py`
  - `tests/contracts/test_validation_evidence_bundle_contract.py`
  - `tests/contracts/test_layered_validation_lane_policy_contract.py`
  - `tests/contracts/test_performance_threshold_gate_contract.py`
- integration smokes：
  - `tests/integration/scenarios/run_layered_validation_smoke.py`
  - `tests/integration/scenarios/run_shared_asset_reuse_smoke.py`
  - `tests/integration/scenarios/run_baseline_governance_smoke.py`
  - `tests/integration/scenarios/run_fault_matrix_smoke.py`
  - `tests/integration/scenarios/run_lane_policy_smoke.py`
  - `tests/integration/scenarios/run_hil_controlled_gate_smoke.py`
- Phase 10 实现证据：
  - `tests/reports/adhoc/performance-bundle-smoke/latest.json`
  - `tests/reports/adhoc/performance-bundle-smoke/20260401T103051Z/validation-evidence-bundle.json`
  - `tests/reports/adhoc/hil-bundle-smoke/hil-closed-loop-summary.json`
  - `tests/reports/adhoc/hil-bundle-smoke/validation-evidence-bundle.json`
- Phase 11 closeout evidence：
  - `tests/reports/adhoc/phase11-closeout/workspace-validation.json`
  - `tests/reports/adhoc/phase11-closeout/validation-evidence-bundle.json`
  - `tests/reports/adhoc/phase11-closeout/report-manifest.json`
  - `tests/reports/adhoc/phase11-closeout/report-index.json`
  - `tests/reports/adhoc/phase11-hil-controlled-gate-smoke/positive/hil-controlled-gate-summary.json`
  - `tests/reports/adhoc/phase11-hil-controlled-gate-smoke/positive/hil-controlled-release-summary.md`
  - `tests/reports/adhoc/phase11-hil-controlled-gate-smoke/negative-missing-offline/hil-controlled-gate-summary.json`
  - `tests/reports/adhoc/phase11-hil-controlled-gate-smoke/negative-missing-override-reason/hil-controlled-gate-summary.json`
  - `phase11-closeout` root validation result：`31 passed / 0 failed / 0 known_failure / 0 skipped`
  - `phase11-hil-controlled-gate-smoke` synthetic acceptance result：正例 `gate=0 render=0`；负例 `missing-offline=10`、`missing-override-reason=1`
- Phase 12 activation evidence：
  - nightly-performance calibration:
    - `tests/reports/performance/dxf-preview-profiles/20260401T153350Z/report.json`
    - `tests/reports/performance/dxf-preview-profiles/20260401T153350Z/validation-evidence-bundle.json`
    - `tests/reports/performance/dxf-preview-profiles/latest.json`
  - limited-hil initial quick gate blocked:
    - `tests/reports/verify/phase12-limited-hil/20260401T153504Z/offline-prereq/workspace-validation.json`
    - `tests/reports/verify/phase12-limited-hil/20260401T153504Z/hil-closed-loop-summary.json`
    - `tests/reports/verify/phase12-limited-hil/20260401T153504Z/hil-case-matrix/case-matrix-summary.json`
    - `tests/reports/verify/phase12-limited-hil/20260401T153504Z/hil-controlled-gate-summary.json`
    - `tests/reports/verify/phase12-limited-hil/20260401T153504Z/hil-controlled-release-summary.md`
  - limited-hil follow-up quick gate pass:
    - `tests/reports/verify/phase12-limited-hil/20260402T005612Z/offline-prereq/workspace-validation.json`
    - `tests/reports/verify/phase12-limited-hil/20260402T005612Z/hil-closed-loop-summary.json`
    - `tests/reports/verify/phase12-limited-hil/20260402T005612Z/hil-case-matrix/case-matrix-summary.json`
    - `tests/reports/verify/phase12-limited-hil/20260402T005612Z/hil-controlled-gate-summary.json`
    - `tests/reports/verify/phase12-limited-hil/20260402T005612Z/hil-controlled-release-summary.md`
  - limited-hil formal gate pass:
    - `tests/reports/verify/phase12-limited-hil/20260402T010321Z/offline-prereq/workspace-validation.json`
    - `tests/reports/verify/phase12-limited-hil/20260402T010321Z/hil-closed-loop-summary.json`
    - `tests/reports/verify/phase12-limited-hil/20260402T010321Z/hil-case-matrix/case-matrix-summary.json`
    - `tests/reports/verify/phase12-limited-hil/20260402T010321Z/hil-controlled-gate-summary.json`
    - `tests/reports/verify/phase12-limited-hil/20260402T010321Z/hil-controlled-release-summary.md`
  - limited-hil official archive publish:
    - `tests/reports/verify/phase12-limited-hil/20260402T013939Z/hil-controlled-gate-summary.json`
    - `tests/reports/verify/phase12-limited-hil/20260402T013939Z/hil-controlled-release-summary.md`
    - `tests/reports/hil-controlled-test/latest-source.txt`
  - closeout:
    - `docs/validation/phase12-layered-test-activation-closeout-20260401.md`
- Phase 13 multisample / signed authority evidence：
  - nightly-performance multi-sample gate:
    - `tests/reports/performance/dxf-preview-profiles/20260402T023650Z/report.json`
    - `tests/reports/performance/dxf-preview-profiles/20260402T023650Z/validation-evidence-bundle.json`
    - `tests/reports/performance/dxf-preview-profiles/20260402T023650Z/report-manifest.json`
    - `tests/reports/performance/dxf-preview-profiles/20260402T023650Z/report-index.json`
  - signed limited-hil latest authority:
    - `tests/reports/verify/phase13-limited-hil/20260402T023734Z/hil-controlled-gate-summary.json`
    - `tests/reports/verify/phase13-limited-hil/20260402T023734Z/hil-controlled-release-summary.md`
    - `tests/reports/hil-controlled-test/latest-source.txt`
  - closeout:
    - `docs/validation/phase13-multisample-performance-and-signed-hil-closeout-20260402.md`
