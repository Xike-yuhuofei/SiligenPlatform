# QA Report

## 1. 环境信息

1. 目标分支：`main`（PR #2 合并后）
2. 复验工作区：`D:\Projects\SS-dispense-align-wave5c-main`
3. 执行日期：`2026-03-23`
4. 工作流上下文：`ticket=NOISSUE`，`timestamp=20260323-012030`

## 2. 执行命令与退出码

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps`
   - exit code：`0`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -FailOnKnownFailure`
   - exit code：`0`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\verify-engineering-data-cli.ps1 -ReportDir integration\reports\verify\wave5c-main-postmerge-20260323-012010\launcher -PythonExe python`
   - exit code：`0`

## 3. 结果摘要

1. apps suite：
   - `passed=16 failed=0 known_failure=0 skipped=0`
   - evidence：`D:\Projects\SS-dispense-align-wave5c-main\integration\reports\workspace-validation.md`
2. launcher helper：
   - `required_checks_passed=True`
   - `console_scripts_passed=True`
   - `overall_status=passed`
   - evidence：`D:\Projects\SS-dispense-align-wave5c-main\integration\reports\verify\wave5c-main-postmerge-20260323-012010\launcher\engineering-data-cli-check.md`

## 4. 结论

1. `Wave 5C main post-merge minimal validation = Go`
2. 当前复验范围内无新增阻断项。
