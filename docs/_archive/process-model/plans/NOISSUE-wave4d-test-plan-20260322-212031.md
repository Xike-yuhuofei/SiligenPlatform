# NOISSUE Wave 4D Test Plan - Launcher Gate Correction

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-212031`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave4d-scope-20260322-212031.md`
- 关联 arch：`docs/process-model/plans/NOISSUE-wave4d-arch-plan-20260322-212031.md`

## 1. 验证原则

1. 只做 repo-side blocker payoff，不补外部输入。
2. 先跑 `legacy-exit-pre`，再安装依赖和 launcher helper，最后跑 `legacy-exit-post`。
3. console script 可见性默认只做记录，不是必需 gate。

## 2. current-fact 扫描

```powershell
rg -n "engineering-data-generate-dxf-preview" README.md docs/runtime tools/scripts/verify-engineering-data-cli.ps1
```

预期：

1. 零命中。

## 3. launcher helper 验证

```powershell
.\tools\scripts\install-python-deps.ps1 -SkipApps
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\verify-engineering-data-cli.ps1 -ReportDir integration\reports\verify\wave4d-closeout\launcher
```

预期：

1. `engineering_data` import 通过。
2. 4 个 canonical module CLI `--help` 通过。
3. 输出 `engineering-data-cli-check.json/.md`。

## 4. 阶段前后门禁

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4d-closeout\legacy-exit-pre
powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4d-closeout\legacy-exit-post
```

预期：

1. 前后门禁均为全绿。
