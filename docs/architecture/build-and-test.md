# Build And Test

更新时间：`2026-03-25`

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
- 冻结文档集校验统一由 `scripts/migration/validate_dsp_e2e_spec_docset.py` 执行。
- `ci.ps1` 必须同时发布 `workspace-validation`、`dsp-e2e-spec-docset` 与 `local-validation-gate` 目录。

## 3. 证据输出

| 入口 | 默认输出 |
|---|---|
| `test.ps1` | `tests/reports/workspace-validation.json`, `tests/reports/workspace-validation.md` |
| `ci.ps1` | `tests/reports/ci/` |
| `run-local-validation-gate.ps1` | `tests/reports/local-validation-gate/<timestamp>/` |
| 冻结文档集校验 | `tests/reports/**/dsp-e2e-spec-docset/*.md`, `*.json` |
| legacy 退出门禁 | `tests/reports/**/legacy-exit/legacy-exit-checks.*` |

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
