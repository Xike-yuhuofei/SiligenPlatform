# QA Report

## 1. 环境信息

1. 分支：`feat/dispense/NOISSUE-e2e-flow-spec-alignment`
2. 执行时间：`2026-03-22`
3. 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-221623`
4. 说明：本轮验证 `Wave 4E` intake/observation 自动化，不伪造真实仓外输入

## 2. 计划中的验证项

1. intake 脚本能为三类 scope 生成标准 `.md` 和 `.json`
2. observation 脚本能在 `Go`/`No-Go`/`input-gap` 条件下生成单项结果、`summary.md` 和 `blockers.md`
3. launcher 观察继续复用 `verify-engineering-data-cli.ps1`，不重做 `Wave 4D` 实现

## 3. 实际执行命令与退出码

1. 临时根目录：`%TEMP%\ss-wave4e-automation-20260322-221623`
2. intake 登记：
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\register-external-observation-intake.ps1 -Scope field-scripts -SourcePath <good-field> -ReportDir <good-report>\intake`
   - exit code：`0`
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\register-external-observation-intake.ps1 -Scope release-package -SourcePath <good-release> -ReportDir <good-report>\intake`
   - exit code：`0`
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\register-external-observation-intake.ps1 -Scope rollback-package -SourcePath <good-rollback> -ReportDir <good-report>\intake`
   - exit code：`0`
3. 正向观察：
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\run-external-migration-observation.ps1 -PythonExe python -FieldScriptsPath <good-field> -ReleasePackagePath <good-release> -RollbackPackagePath <good-rollback> -ReportRoot <good-report>`
   - exit code：`0`
4. 负向观察：
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\run-external-migration-observation.ps1 -PythonExe python -FieldScriptsPath <bad-field> -ReleasePackagePath <good-release> -RollbackPackagePath <good-rollback> -ReportRoot <bad-report>`
   - exit code：`1`
5. 缺口观察：
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\run-external-migration-observation.ps1 -PythonExe python -ReportRoot <gap-report>`
   - exit code：`1`
6. 一致性校验：
   - `rg -n "observation_result =|external migration complete declaration =|next-wave planning =|field-scripts-no-go|input-gap" <good-report>\observation <bad-report>\observation <gap-report>\observation`
   - exit code：`0`

## 4. 结果摘要

1. 正向样例生成了完整 intake `.md/.json`、四个 scope 观察结果、`summary.md` 和 `blockers.md`，且 summary 为 `Go`
2. 负向样例在 `bad-field\deploy.ps1` 命中 `dxf-to-pb` 后，`field-scripts.md` 被裁决为 `No-Go`，`blockers.md` 列出 `field-scripts-no-go`
3. 缺失输入样例在未提供三类外部路径时，`field scripts / release package / rollback package` 均被裁决为 `input-gap`
4. launcher 观察继续复用 `verify-engineering-data-cli.ps1`，当前环境 required checks 通过

## 5. 结论

1. `Wave 4E` 的 intake/observation 自动化已具备可执行性。
2. 自动化脚本能稳定区分 `Go`、`No-Go` 和 `input-gap`，且会同步生成 `summary.md` 与 `blockers.md`。
3. 当前仓库仍未拿到真实仓外输入，因此这些验证只证明自动化流程可用，不构成仓外迁移完成结论。
