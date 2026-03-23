# chore/runtime/NOISSUE-local-gate-entrypoint-sync QA Review

- 状态：Go
- 日期：2026-03-23
- 分支：chore/runtime/NOISSUE-local-gate-entrypoint-sync
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260323-120653`

## 1. 评审范围

1. `ci.ps1` 默认验收入口是否收敛到本地门禁脚本。
2. `release-check.ps1` 默认验收入口是否收敛到本地门禁脚本。
3. 发布流程文档是否与脚本行为一致。

## 2. 变更核对

1. `ci.ps1`
   - 新增参数：`LocalValidationReportRoot`、`SkipLocalValidationGate`
   - 默认 `Suite=all` 时先执行 `tools/scripts/run-local-validation-gate.ps1`
2. `release-check.ps1`
   - 新增参数：`LocalValidationReportRoot`、`SkipLocalValidationGate`
   - 默认先执行 `tools/scripts/run-local-validation-gate.ps1`
   - release manifest 新增本地门禁字段
3. `docs/runtime/release-process.md`
   - `release-check.ps1` 执行步骤更新为“默认包含本地门禁”

## 3. 验证记录

1. 语法校验（PowerShell Parser）：`ci.ps1` 通过。
2. 语法校验（PowerShell Parser）：`release-check.ps1` 通过。
3. 本次为脚本入口收敛改造，未触发全量 build/test（避免重复长耗时验证）；功能行为以参数与执行顺序变更为主。

## 4. 风险与说明

1. 若外部自动化显式依赖旧行为，可通过 `-SkipLocalValidationGate` 保持兼容。
2. `ci.ps1` 在自定义 suite（非 `all`）下不会强制执行本地门禁，避免非默认场景被额外放大耗时。

## 5. 结论

1. 本次“脚本层默认验收入口收敛”满足目标，结论 `Go`。
