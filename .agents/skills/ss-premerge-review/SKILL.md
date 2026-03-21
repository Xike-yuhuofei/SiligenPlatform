---
name: ss-premerge-review
description: |
  预合并代码审查技能。对当前分支相对 base 的差异进行结构化审查，先报告高风险
  问题，再给可执行修复建议，输出可追溯审查报告。
---

# /ss-premerge-review

## 步骤 0：上下文归一化（必须先做）

```powershell
$CTX = .\tools\scripts\resolve-workflow-context.ps1 | ConvertFrom-Json
```

## 输入

- 当前分支
- 目标 base 分支（默认仓库默认分支）

## 步骤 1：确定 base 分支（必须明确）

```powershell
$BASE = (gh pr view --json baseRefName -q .baseRefName 2>$null)
if ([string]::IsNullOrWhiteSpace($BASE)) {
  $BASE = (gh repo view --json defaultBranchRef -q .defaultBranchRef.name 2>$null)
}
if ([string]::IsNullOrWhiteSpace($BASE)) { $BASE = "main" }
```

## 执行步骤

1. 同步远端基线：`git fetch origin $BASE`
2. 获取 diff：`git diff origin/$BASE...HEAD`
3. 按严重级别输出发现项（P0/P1/P2）
4. 每条发现项包含：文件、风险、触发条件、建议修复
5. 识别文档漂移并标注需更新文档

## 输出

写入：`docs/process-model/reviews/{branchSafe}-premerge-{timestamp}.md`

```powershell
$OUT = "docs/process-model/reviews/$($CTX.BranchSafe)-premerge-$($CTX.Timestamp).md"
```

报告结构：

1. Findings（按严重级别排序）
2. Open Questions
3. Residual Risks
4. 建议下一步（QA 或返工）

## 规则

1. Findings 必须优先于摘要
2. 无问题时必须明确写“未发现阻断项”，并列出残余风险
3. 不执行 push / merge / PR 创建
4. 未执行 `git fetch origin $BASE` 时，不允许输出最终审查结论
