# NOISSUE Wave 4D Closeout

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-212031`
- 上游收口：`docs/process-model/plans/NOISSUE-wave4c-closeout-20260322-212031.md`
- 关联验证目录：`integration/reports/verify/wave4d-closeout/`

## 1. 总体裁决

1. `repo-side launcher blocker payoff = Go`
2. `Wave 4C launcher finding superseded = Go`
3. `Wave 4C rerun readiness = Go`
4. `external migration complete declaration = No-Go`
5. `next-wave planning = No-Go`

## 2. 本阶段实际完成内容

### 2.1 观察标准修正

1. `docs/runtime/external-migration-observation.md`
   - canonical preview CLI 更正为 `engineering-data-generate-preview`
   - launcher gate 从“PATH 硬门槛”修正为“目标 Python 解释器可运行 canonical module CLI”
2. current-fact 扫描已确认错误命令名在 active docs 中零命中

### 2.2 repo-side launcher blocker payoff

1. `tools/scripts/install-python-deps.ps1 -SkipApps`
   - 已补当前机器的 canonical `engineering-data` 安装面
2. `tools/scripts/verify-engineering-data-cli.ps1`
   - 新增独立 helper
   - required checks 全部通过
   - console script 也在当前机器上全部可见

### 2.3 外部输入 intake 准备

1. `integration/reports/verify/wave4d-closeout/intake/README.md`
2. `field-scripts-template.md`
3. `release-package-template.md`
4. `rollback-package-template.md`

## 3. 实际执行命令

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4d-closeout\legacy-exit-pre`
2. `.\tools\scripts\install-python-deps.ps1 -SkipApps`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\verify-engineering-data-cli.ps1 -ReportDir integration\reports\verify\wave4d-closeout\launcher`
4. `rg -n "engineering-data-generate-dxf-preview" README.md docs/runtime tools/scripts/verify-engineering-data-cli.ps1`
5. `powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4d-closeout\legacy-exit-post`

## 4. 关键证据

1. `integration/reports/verify/wave4d-closeout/launcher/engineering-data-cli-check.md`
   - `required_checks_passed=True`
   - `console_scripts_passed=True`
   - `overall_status=passed`
2. `integration/reports/verify/wave4d-closeout/legacy-exit-pre/legacy-exit-checks.md`
   - `passed_rules=19 failed_rules=0`
3. `integration/reports/verify/wave4d-closeout/legacy-exit-post/legacy-exit-checks.md`
   - `passed_rules=19 failed_rules=0`
4. `rg -n "engineering-data-generate-dxf-preview" README.md docs/runtime tools/scripts/verify-engineering-data-cli.ps1`
   - 零命中

## 5. 阶段结论

1. `Wave 4C` 中“canonical launcher 当前不可见”的 repo-side 误判已被 `Wave 4D` supersede。
2. 当前 repo-side 已经具备按修正口径重跑外部观察的条件。
3. 当前仍不能宣称仓外迁移完成，因为 field scripts / release package / rollback package 输入仍未补齐。
4. 下一步应直接补三类外部输入，并基于 `Wave 4D` intake 模板重开外部观察。

## 6. 非阻断记录

1. 当前工作树仍明显不干净，本阶段没有回退无关改动。
2. 本阶段没有重跑新的全量 build/test。
3. 当前 console script 恰好也可见，但这只是附加事实，不是默认 gate 的唯一依据。
