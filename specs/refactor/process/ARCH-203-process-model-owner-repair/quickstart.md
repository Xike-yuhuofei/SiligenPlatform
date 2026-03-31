# Quickstart: ARCH-203 Process Model Owner Boundary Repair

## 1. 基线确认

1. 确认当前 worktree 位于 `D:\Projects\SiligenSuite-wt-arch203`
2. 确认当前分支为 `refactor/process/ARCH-203-process-model-owner-repair`
3. 确认 `docs/process-model/reviews/` 下已经存在以下 10 份基线文档：
   - 9 份 `2026-03-31` 模块评审
   - 1 份 `architecture-review-recheck-report-20260331-082102.md`

建议检查命令：

```powershell
git branch --show-current
Get-ChildItem docs/process-model/reviews/*20260331*.md | Select-Object Name
```

当前分支已导入这批文档；若后续 worktree 再次缺失这批文档，只允许继续做规划资产维护，不允许进入 implementation。

## 2. 阅读顺序

1. `spec.md`
2. `plan.md`
3. `research.md`
4. `data-model.md`
5. `contracts/module-owner-boundary-contract.md`
6. `contracts/consumer-access-contract.md`
7. `contracts/remediation-wave-contract.md`

## 3. 执行波次

1. `Wave 0`: 导入并冻结 `2026-03-31` 评审与复核基线
2. `Wave 1`: 收口 `workflow <-> runtime-execution` seam 与 public surface
3. `Wave 2`: 收口 `process-planning -> coordinate-alignment -> process-path -> motion-planning -> dispense-packaging` 主链交接
4. `Wave 3`: 收口 `topology-feature`、`hmi-application` 与宿主消费者接入
5. `Wave 4`: 同步文档、构建、测试入口、模块库存与 evidence

## 4. 推荐验证命令

边界门禁：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot . -ReportDir tests/reports/module-boundary-bridges
```

根级构建入口：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite contracts
```

根级测试入口：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite contracts -ReportDir tests/reports/arch-203 -FailOnKnownFailure
```

根级 CI 入口：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\ci.ps1 -Suite contracts -ReportDir tests/reports/ci-arch-203
```

## 5. 当前 implementation 基线

- `plan.md`、`research.md`、`data-model.md`、`contracts/`、`tasks.md` 已齐备
- `docs/process-model/reviews/` 下的 10 份 `2026-03-31` baseline 评审/复核文档已进入当前分支工作区
- `tests/reports/module-boundary-bridges-current/` 已给出 `finding_count = 0` 的边界门禁结果
- `apps/hmi-app/config/gateway-launch.json` 已补齐，HMI formal launcher contract guard 不再是 closeout 阻断
- `.\build.ps1`、`.\test.ps1`、`.\ci.ps1` 与 `scripts/validation/run-local-validation-gate.ps1` 已在当前 worktree 通过；见 `tests/reports/arch-203/arch-203-root-entry-summary.md`
- 若在新的 clean worktree 上复跑，仍需先为 `scripts/bootstrap/bootstrap-third-party.ps1` 提供有效的 bundle 来源（`SILIGEN_THIRD_PARTY_ARTIFACT_ROOT` 或 `SILIGEN_THIRD_PARTY_BASE_URI`）
