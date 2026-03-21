---
name: ss-qa-regression
description: |
  测试-修复-回归闭环技能。根据测试计划和代码变更执行验证，发现问题后给出修复
  建议并复测，形成可追溯 QA 报告。
---

# /ss-qa-regression

## 步骤 0：上下文归一化（必须先做）

```powershell
$CTX = .\tools\scripts\resolve-workflow-context.ps1 | ConvertFrom-Json
```

## 输入

- 最新测试计划（按 `docs/process-model/plans/{ticket}-test-plan-*.md` 逆序取最新）
- 当前分支改动

```powershell
$TEST_PLAN = Get-ChildItem "docs/process-model/plans/$($CTX.Ticket)-test-plan-*.md" -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1
```

## 执行步骤

1. 检查工作树状态：`git status --porcelain`
2. 若非干净工作树，必须先处理（提交/暂存/中止），不得直接进入 QA
3. 执行受影响范围测试（优先根级命令）
4. 记录失败用例与复现步骤
5. 修复后复测，确认无回归

## 推荐命令

- `.\test.ps1 -Profile CI -Suite apps -FailOnKnownFailure`
- 按需增加集成或模块级命令

## 输出

写入：`docs/process-model/reviews/{branchSafe}-qa-{timestamp}.md`

```powershell
$OUT = "docs/process-model/reviews/$($CTX.BranchSafe)-qa-$($CTX.Timestamp).md"
```

报告模板（字段固定）：

1. 环境信息（分支、commit、执行时间）
2. 执行命令与退出码
3. 测试范围
4. 失败项与根因
5. 修复项与验证证据（包含复测命令）
6. 未修复项与风险接受结论
7. 发布建议（Go / No-Go）

## 规则

1. 没有复测证据不得标记“已修复”
2. 对未修复项必须给出明确阻断结论或风险接受结论
3. 工作树不干净时禁止直接开始 QA
