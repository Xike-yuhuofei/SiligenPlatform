# Validation

## 1. 根级入口

- `build.ps1`
- `test.ps1`
- `ci.ps1`
- `scripts/validation/run-local-validation-gate.ps1`

分层路由参数：

- `-Lane quick-gate|full-offline-gate|nightly-performance|limited-hil`
- `-RiskProfile low|medium|high|hardware-sensitive`
- `-DesiredDepth quick|full-offline|nightly|hil`
- `-ChangedScope <scope>`
- `-SkipLayer <layer-id>` + `-SkipJustification <reason>`

联机矩阵 root opt-in 开关：

- `test.ps1 -IncludeHilCaseMatrix`
- `ci.ps1 -IncludeHilCaseMatrix`
- `scripts/validation/run-local-validation-gate.ps1 -IncludeHilCaseMatrix`

受控门禁 / 发布门禁默认纳入 `hil-case-matrix`：

- `tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1`
- `release-check.ps1`

正式 controlled HIL 路径：

- `tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1`

说明：

- 根级 `test.ps1 -IncludeHardwareSmoke/-IncludeHil*` 与 `ci.ps1 -IncludeHilCaseMatrix` 只保留为 root opt-in surface。
- 正式 `limited-hil` 结论只认 `run_hil_controlled_test.ps1` 产出的 `offline-prereq -> hardware-smoke -> hil-closed-loop -> optional hil-case-matrix -> hil-controlled-gate -> hil-controlled-release-summary` 证据链。
- 若 `run_hil_controlled_test.ps1` 需要发布 fixed latest authority，则必须带非空 `-Executor`；未签名诊断回合必须显式使用 `-PublishLatestOnPass:$false`。
- 一旦 offline prerequisites 已通过，后续 HIL 步骤即使阻塞，也必须继续发布 `hil-controlled-gate-summary.*` 与 `hil-controlled-release-summary.md`，不能只停在中间 summary。

如需临时隔离 `hil-case-matrix` 影响，可显式传：

- `tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1 -IncludeHilCaseMatrix:$false`
- `release-check.ps1 -IncludeHilCaseMatrix:$false`

## 2. 正式承载面

- 联机测试矩阵基线：`docs/validation/online-test-matrix-v1.md`
- 冻结规范：`docs/architecture/dsp-e2e-spec/`
- 正式发版前测试清单：`docs/validation/release-test-checklist.md`
- trace owner：`modules/trace-diagnostics/`
- HMI owner：`modules/hmi-application/`
- HMI 宿主壳：`apps/hmi-app/`
- 正式验证根：`tests/`
- 稳定样本根：`samples/`
- 系统收敛汇总：`docs/architecture/system-acceptance-report.md`

## 3. 必要回链字段

- `stage_id`
- `artifact_id`
- `module_id`
- `workflow_state`
- `execution_state`
- `event_name`
- `failure_code`
- `evidence_path`

## 4. 默认报告位置

- `tests/reports/workspace-validation.*`
- `tests/reports/case-index.json`
- `tests/reports/validation-evidence-bundle.json`
- `tests/reports/evidence-links.md`
- `tests/reports/failure-details.json`
- `tests/reports/report-manifest.json`
- `tests/reports/report-index.json`
- `tests/reports/ci/`
- `tests/reports/local-validation-gate/<timestamp>/`
- `tests/reports/.../dsp-e2e-spec-docset/`
- `tests/reports/hil-controlled-test/<timestamp>/offline-prereq/workspace-validation.json|md`
- `tests/reports/hil-controlled-test/<timestamp>/hardware-smoke/hardware-smoke-summary.json|md`
- `tests/reports/hil-controlled-test/<timestamp>/hil-closed-loop-summary.json|md`
- `tests/reports/hil-controlled-test/<timestamp>/hil-controlled-gate-summary.json|md`
- `tests/reports/hil-controlled-test/<timestamp>/hil-controlled-release-summary.md`
- `tests/reports/hil-controlled-test/latest-source.txt`
- `tests/reports/performance/dxf-preview-profiles/latest.json|md`

## 4.1 Baseline Governance

- manifest：`tests/baselines/baseline-manifest.json`
- schema：`tests/baselines/baseline-manifest.schema.json`
- canonical baseline root：`tests/baselines/`
- deprecated duplicate roots：`data/baselines/simulation/`、`data/baselines/engineering/`
- `workspace-validation.json` 会记录：
  - `canonical_asset_inventory`
  - `baseline_governance_summary`
  - `baseline_manifest_entry_count`
  - duplicate / deprecated / stale / unused baseline issue counts

## 4.2 Layer / Lane

- `L0-structure-gate`：布局、根入口和冻结门禁
- `L1-module-contract`：模块 owner 契约
- `L2-offline-integration`：跨模块链路与共享资产
- `L3-simulated-e2e`：simulated-line 主场景
- `L4-performance`：preview/execution/single-flight 性能画像
- `L5-limited-hil`：受限 HIL
- `L6-closeout`：收尾汇总

## 4.3 Lane Policy 真值

- `quick-gate`：`blocking`
- `full-offline-gate`：`blocking`
- `nightly-performance`：`blocking`
- `limited-hil`：`blocking`

补充约束：

- `nightly-performance` 正式入口是 `tests/performance/collect_dxf_preview_profiles.py --include-start-job --gate-mode nightly-performance --threshold-config tests/baselines/performance/dxf-preview-profile-thresholds.json`。
- `nightly-performance` 正式 blocking 样本固定为 `small=rect_diag.dxf`、`medium=rect_medium_ladder.dxf`、`large=rect_large_ladder.dxf`。
- `baseline-json` 仍只做 advisory drift compare，不替代 threshold gate。
- `nightly-performance` 默认按 `<repo-root>\build\bin\*` -> `%LOCALAPPDATA%\SiligenSuite\control-apps-build\bin\*` 解析 gateway executable；若 threshold config 包含 execution thresholds，未带 `--include-start-job` 视为正式 gate 不完整。
- `limited-hil` evidence bundle 必须带 `schema_version=validation-evidence-bundle.v2`、`report-manifest.json`、`report-index.json`、`metadata.admission`、`safety_preflight_passed` 和 override reason 规则。
- `limited-hil` 若要更新 `tests/reports/hil-controlled-test/latest-source.txt`，必须带非空 `Executor` 并生成 signed `hil-controlled-release-summary.md`。

## 4.4 Phase 12 Closeout

- `docs/validation/phase12-layered-test-activation-closeout-20260401.md`
- 该批次冻结了正式 `nightly-performance` threshold config，并记录了 `limited-hil` 的初次 blocked evidence、follow-up quick gate pass、formal `1800s` gate pass，以及 fixed latest publish 到 `tests/reports/hil-controlled-test/`。

## 4.5 Phase 13 Closeout

- `docs/validation/phase13-multisample-performance-and-signed-hil-closeout-20260402.md`
- 该批次把 `nightly-performance` 正式 blocking 覆盖扩到 repo-local `small/medium/large` canonical DXF，并将 signed controlled HIL latest authority 切到 `tests/reports/verify/phase13-limited-hil/20260402T023734Z/`。

## 4.6 Test System Rollout Plan

- `docs/validation/test-system-rollout-plan-20260402.md`
- 该文档把“测试体系平台已完成、覆盖仍需收口”的现状拆成 `Phase A-E` 的阶段任务，覆盖模块矩阵补齐、专项 authority 策略收敛，以及 `stress / soak` 正式化。

## 4.7 Test System Gap Matrix

- `docs/validation/test-system-gap-matrix-20260402.md`
- 该文档冻结 `Phase A / T001-T003` 的当前真值：模块 `empty / thin / healthy` 状态、模块内与仓库级 tests 的 owner 边界，以及 `P0-05`、`P1-01`、`P1-04`、`P1-05` 的专项策略定位。

## 5. Wave 8 引用检查

- 波次：`Wave 8`
- 布局门禁：`python .\scripts\migration\validate_workspace_layout.py --wave "Wave 8"`
- 仓库级验证入口：`python -m test_kit.workspace_validation --suite packages --profile ci`
- 测试矩阵基线：`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md`
- 联机矩阵落地：`docs/validation/online-test-matrix-v1.md`
- 冻结目录索引：`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s10-frozen-directory-index.md`
