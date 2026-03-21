---
name: ss-safety-guard
description: |
  高风险操作安全护栏技能。对可能导致数据丢失、历史覆盖或环境破坏的操作执行前审查，
  并要求明确确认与回滚预案。
---

# /ss-safety-guard

## 步骤 0：上下文归一化（必须先做）

```powershell
$CTX = .\tools\scripts\resolve-workflow-context.ps1 | ConvertFrom-Json
```

## 高风险操作清单

1. `git reset --hard`
2. `git push --force`
3. `rm -rf`
4. 大范围批量替换或删除
5. 直接修改生产配置/密钥/数据库

## 执行要求

在执行高风险操作前，必须先回答：

1. 为什么必须现在执行？
2. 可逆吗？回滚怎么做？
3. 影响范围是什么？
4. 是否有更低风险替代方案？

并且必须通过守卫脚本执行命令：

```powershell
.\tools\scripts\invoke-guarded-command.ps1 -Command "<your-command>"
```

如果脚本返回 `exit code 2`，表示高风险命令被拦截。此时：

1. 先输出风险声明与回滚方案
2. 得到用户明确同意后，才允许执行：
```powershell
.\tools\scripts\invoke-guarded-command.ps1 -Command "<your-command>" -Approve
```

## 输出

若继续执行，先给出：

1. 风险声明
2. 回滚步骤
3. 用户确认结论

## 规则

1. 无明确确认，不执行高风险操作
2. 未给回滚方案，不执行不可逆操作
3. 禁止绕过 `invoke-guarded-command.ps1` 直接执行高风险命令
