# NOISSUE Wave 4E Remediation Plan

- 状态：Ready
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-225619`
- 上游状态：`docs/process-model/plans/NOISSUE-wave4e-observation-status-20260322-225619.md`

## 1. 目标

1. 消除 `release-package = No-Go`
2. 补齐 `rollback-package = input-gap`
3. 在四个 scope 全部为 `Go` 前，不生成 `Wave 4E closeout`

## 2. 子任务拆解

### 2.1 子任务 A：release-package 外部 alias 清理

1. 目标对象：
   - `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\config\machine_config.ini`
2. 执行动机：
   - 当前 `release-package` 唯一 blocker 就是 deleted alias 路径命中。
3. 回滚方案：
   - 先把目标文件备份到 `%LOCALAPPDATA%\SiligenSuite\wave4e-remediation-backup\machine_config.ini.20260322-225619.bak`
   - 若观察或现场使用失败，按原路径恢复该备份文件
4. 执行约束：
   - 先给出风险声明
   - 通过 `tools/scripts/invoke-guarded-command.ps1` 执行
   - 无用户明确确认不得执行
5. 期望结果：
   - `config\machine_config.ini` 不再存在于发布根
   - `config\machine\machine_config.ini` 保持不变

### 2.2 子任务 B：rollback-package 真实输入补齐

1. 当前复查范围内未发现可接受目录：
   - `C:\Users\Xike\AppData\Local\SiligenSuite`
   - `C:\Users\Xike\Desktop`
   - `C:\Users\Xike\Downloads`
2. 可接受输入：
   - rollback bundle 解包目录
   - 现场 backup pack 目录
   - rollback SOP snapshot 目录
3. 不可接受输入：
   - 单个截图
   - 仅口头说明
   - 与回滚无关的普通发布目录

### 2.3 子任务 C：重跑 Wave 4E 观察

1. 重新登记或复用 intake：
   - `.\tools\scripts\register-external-observation-intake.ps1 -Scope release-package -SourcePath <release-root> -ReportDir integration\reports\verify\wave4e-rerun\intake`
   - `.\tools\scripts\register-external-observation-intake.ps1 -Scope rollback-package -SourcePath <rollback-root> -ReportDir integration\reports\verify\wave4e-rerun\intake`
2. 重跑观察：
   - `.\tools\scripts\run-external-migration-observation.ps1 -PythonExe python -ReportRoot integration\reports\verify\wave4e-rerun`
3. 通过门槛：
   - `external launcher = Go`
   - `field scripts = Go`
   - `release package = Go`
   - `rollback package = Go`

## 3. 收口门禁

1. 任一 scope 不是 `Go`，则：
   - `external observation execution = No-Go`
   - `external migration complete declaration = No-Go`
   - `next-wave planning = No-Go`
2. 只有四个 scope 全绿后，才允许补写：
   - `Wave 4E closeout`
   - Wave 4E QA review
   - Wave 4E doc-sync review
