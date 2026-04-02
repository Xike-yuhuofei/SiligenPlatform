# Build And Test

更新时间：`2026-04-02`

## 1. 目标

冻结工作区唯一正式的根级 build/test/CI/本地门禁入口，并明确证据发布位置。

## 2. 根级入口

```powershell
.\build.ps1
.\test.ps1 -Profile Local -Suite all
.\ci.ps1 -Suite all
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1
```

统一规则：

- 正式入口只认 `build.ps1`、`test.ps1`、`ci.ps1`。
- 三个入口统一接受 `-Lane`、`-RiskProfile`、`-DesiredDepth`、`-ChangedScope`、`-SkipLayer`、`-SkipJustification`，并把这些 metadata 透传到 `python -m test_kit.workspace_validation`。
- 冻结文档集校验统一由 `scripts/migration/validate_dsp_e2e_spec_docset.py` 执行。
- `ci.ps1` 必须同时发布 `workspace-validation`、`dsp-e2e-spec-docset` 与 `local-validation-gate` 目录。

## 2.1 Layer / Lane 术语冻结

- `quick-gate`：默认离线快速门禁，只允许 `L0-L2`
- `full-offline-gate`：完整离线回归，允许 `L0-L4`
- `nightly-performance`：性能与漂移观测，允许 `L0,L2-L4`
- `limited-hil`：受限联机，允许 `L0,L2-L3,L5-L6`
- 任意 `skip layer` 都必须同时给出 `skip justification`
- `tests/performance/collect_dxf_preview_profiles.py --include-start-job --gate-mode nightly-performance --threshold-config tests/baselines/performance/dxf-preview-profile-thresholds.json` 是 `nightly-performance` 的正式 authority path
- `nightly-performance` 正式 blocking 样本固定为 `small=rect_diag.dxf`、`medium=rect_medium_ladder.dxf`、`large=rect_large_ladder.dxf`
- `nightly-performance` 默认按 `<repo-root>\build\bin\*` -> `%LOCALAPPDATA%\SiligenSuite\control-apps-build\bin\*` 解析 gateway executable，与根级 `build.ps1` 的 canonical build root 保持一致
- `tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1` 是 `limited-hil` 的正式 controlled path；根级 `-IncludeHardwareSmoke/-IncludeHil*` 只保留为 opt-in surface
- `limited-hil` 若要发布 fixed latest authority，必须以非空 `-Executor` 运行 `run_hil_controlled_test.ps1`

## 2.2 Gate Policy / Budget

| lane | decision | fail policy | timeout_budget_seconds | retry_budget | fail_fast_case_limit |
|---|---|---|---:|---:|---:|
| `quick-gate` | `blocking` | `fail-fast` | 900 | 0 | 1 |
| `full-offline-gate` | `blocking` | `collect-and-report` | 2700 | 1 | 0 |
| `nightly-performance` | `blocking` | `collect-and-report` | 3600 | 1 | 0 |
| `limited-hil` | `blocking` | `manual-signoff-required` | 1800 | 0 | 1 |

统一要求：

- `ci.ps1` 必须显式输出当前 lane policy。
- `workspace-validation.json|md` 必须回显选中的 gate decision、timeout/retry/fail-fast 预算。
- `case-index.json` 与 `validation-evidence-bundle.json` 必须保留 suite/size/labels taxonomy，不得只剩自由文本描述。
- `nightly-performance` 必须同时产出 `threshold_gate`、`report-manifest.json` 与 `report-index.json`；当 threshold config 包含 execution thresholds 时，正式 gate 必须显式带 `--include-start-job`，缺少 threshold config、execution evidence 或 schema 不兼容时 hard-fail。
- `limited-hil` 必须同时满足 offline prerequisites、admission metadata、safety preflight 和 operator override reason 规则，才允许给出 `passed`。
- `limited-hil` 的 fixed latest publish 还必须带非空 `Executor`，否则不得覆盖 `tests/reports/hil-controlled-test/latest-source.txt`。
- `limited-hil` 在 offline prerequisites 已通过后，即使 hardware smoke / hil-closed-loop / hil-case-matrix 阻塞，也必须继续生成 `hil-controlled-gate-summary.*` 与 `hil-controlled-release-summary.md`，禁止只保留半截 HIL 工件。

## 3. 证据输出

| 入口 | 默认输出 |
|---|---|
| `test.ps1` | `tests/reports/workspace-validation.json`, `tests/reports/workspace-validation.md` |
| `ci.ps1` | `tests/reports/ci/` |
| `run-local-validation-gate.ps1` | `tests/reports/local-validation-gate/<timestamp>/` |
| 冻结文档集校验 | `tests/reports/**/dsp-e2e-spec-docset/*.md`, `*.json` |
| legacy 退出门禁 | `tests/reports/**/legacy-exit/legacy-exit-checks.*` |
| 模块桥接边界门禁 | `tests/reports/**/module-boundary-bridges/module-boundary-bridges.*` |
| 分层证据 bundle | `tests/reports/**/case-index.json`, `validation-evidence-bundle.json`, `evidence-links.md`, `failure-details.json` |
| `nightly-performance` threshold gate | `tests/reports/performance/dxf-preview-profiles/latest.json`, `<timestamp>/validation-evidence-bundle.json`, `report-manifest.json`, `report-index.json` |
| `limited-hil` controlled path | `tests/reports/hil-controlled-test/**/offline-prereq/`, `hardware-smoke/`, `hil-closed-loop-summary.*`, `hil-controlled-gate-summary.*`, `hil-controlled-release-summary.md`, `latest-source.txt` |

## 4. S09 场景类

| scenario_class | 覆盖入口 | 重点 |
|---|---|---|
| `success` | `test.ps1 -Suite contracts` | 主成功链与归档闭环 |
| `block` | `test.ps1 -Suite contracts` | `PreflightBlocked` 只阻断执行，不污染规划链 |
| `rollback` | `test.ps1 -Suite contracts` | `ArtifactSuperseded` 与回退链 |
| `recovery` | `test.ps1 -Suite contracts` | fault 恢复链 |
| `archive` | `test.ps1 -Suite contracts` | `ExecutionRecordBuilt` 到 `WorkflowArchived` |

## 5. 与冻结文档集的绑定

- `S06` 规定目标根与路径处置。
- `S09` 规定 success、block、rollback、recovery、archive 五类 acceptance baseline。
- `S10` 规定审阅顺序与证据入口。
- `system-acceptance-report.md` 汇总当前 closeout 结论。

## 6. 冻结文档评审命令

```powershell
python .\scripts\migration\validate_dsp_e2e_spec_docset.py `
  --report-dir tests/reports/manual-freeze-review `
  --report-stem dsp-e2e-spec-docset-review
```

评审要点：

- 命令退出码必须为 `0`。
- 报告 `tests/reports/manual-freeze-review/dsp-e2e-spec-docset-review.md` 中 `finding_count` 必须为 `0`。
- 若失败，按报告中的 `rule_id` 和 `file` 修复冻结文档后重跑。

## 7. 统一术语检查步骤

1. 执行冻结文档评审命令，确认报告无 `required-term-missing`。
2. 对 `S01`、`S02`、`S03`、`S10` 抽查关键术语：

```powershell
rg -n "ExecutionPackageBuilt|ExecutionPackageValidated|PreflightBlocked|ArtifactSuperseded|WorkflowArchived" `
  .\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s01-stage-io-matrix.md `
  .\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s02-stage-responsibility-acceptance.md `
  .\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s03-stage-errorcode-rollback.md `
  .\docs\architecture\dsp-e2e-spec\dsp-e2e-spec-s10-frozen-directory-index.md
```

3. 任一术语缺失时，不允许进入下一波次评审或 closeout 判定。
