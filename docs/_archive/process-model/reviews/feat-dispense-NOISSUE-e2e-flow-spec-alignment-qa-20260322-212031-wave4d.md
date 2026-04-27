# QA Report

## 1. 环境信息

1. 分支：`feat/dispense/NOISSUE-e2e-flow-spec-alignment`
2. 基线提交：`f6e5668d`
3. 执行时间：`2026-03-22`
4. 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-212031`
5. 说明：本轮只做 `Wave 4D` repo-side launcher blocker payoff，不补外部输入

## 2. 执行命令与退出码

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4d-closeout\legacy-exit-pre`
   - exit code：`0`
2. `.\tools\scripts\install-python-deps.ps1 -SkipApps`
   - exit code：`0`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\verify-engineering-data-cli.ps1 -ReportDir integration\reports\verify\wave4d-closeout\launcher`
   - exit code：`0`
4. `rg -n "engineering-data-generate-dxf-preview" README.md docs/runtime tools/scripts/verify-engineering-data-cli.ps1`
   - exit code：`1`
   - 解释：零命中，视为通过
5. `powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4d-closeout\legacy-exit-post`
   - exit code：`0`

## 3. 结果摘要

1. `engineering-data-cli-check.md` 显示 required checks 全部通过：
   - `import engineering_data`
   - `python -m engineering_data.cli.dxf_to_pb --help`
   - `python -m engineering_data.cli.path_to_trajectory --help`
   - `python -m engineering_data.cli.export_simulation_input --help`
   - `python -m engineering_data.cli.generate_preview --help`
2. 当前机器上的 console script 也全部可见：
   - `engineering-data-dxf-to-pb`
   - `engineering-data-path-to-trajectory`
   - `engineering-data-export-simulation-input`
   - `engineering-data-generate-preview`
3. 错误 canonical 名称 `engineering-data-generate-dxf-preview` 在 active docs 和 helper 中零命中
4. `legacy-exit-pre/post` 均为全绿

## 4. 结论

1. `Wave 4C` 的 launcher blocker 已被 `Wave 4D` repo-side payoff 清除。
2. 当前仍未补齐外部输入，因此不能据此宣称仓外迁移完成。
3. 下一步应补 field scripts / release package / rollback package 输入，并按修正后的口径重跑外部观察。
